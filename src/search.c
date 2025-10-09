#include "search.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "book.h"
#include "eval.h"
#include "pick.h"
#include "zobrist.h"

#define NULL_REDUCTION 3
#define LMR_REDUCTION 2
#define LMP_THRESHOLD 8
#define LMP_MAXDEPTH 3

#define INFO_PERIOD_CLOCKS (100 * (CLOCKS_PER_SEC / 1000))

#define SCORE_MATE 24000
#define MATE_THRESH (SCORE_MATE - MAX_DEPTH)

#define DELTA_MARGIN (eval_pscore[PIECE_QUEEN] + 256)
#define ASPIRATION_MARGIN 50

int search_killeridx[MAX_DEPTH];
move_t search_killers[MAX_DEPTH][MAX_KILLER];
score_t search_history[TEAM_COUNT][BOARD_AREA][BOARD_AREA];
move_t search_counters[TEAM_COUNT][BOARD_AREA][BOARD_AREA];
ttable_t search_ttable;
_Atomic bool search_active;
_Atomic bool search_cancel;

move_t curpv[MAX_DEPTH][MAX_DEPTH];
int pvcount[MAX_DEPTH];

clock_t searchstart;
int searchtime;
move_t bestknown;
clock_t lastinfo;

int curdepth = 0;
int seldepth = 0;
score_t curscore = 0;
uint64_t nnodes, nnonterminal;

static inline void search_printinfo(board_t* board)
{
    int i;

    char str[MAX_LONGALG];

    lastinfo = clock();

    printf("info");
    printf(" depth %d", curdepth);
    printf(" seldepth %d", seldepth);
    printf(" time %llu", (uint64_t) ((double) (lastinfo - searchstart) / CLOCKS_PER_SEC * 1000));
    if(curscore < MATE_THRESH && curscore > -MATE_THRESH)
        printf(" score cp %d", curscore);
    else if(curscore >= MATE_THRESH)
        printf(" score mate %d", (SCORE_MATE - curscore - 1) / 2 + 1);
    else
        printf(" score mate %d", (-SCORE_MATE - curscore + 1) / 2 - 1);
    printf(" nodes %llu", nnodes);
    printf(" nps %llu", (uint64_t) ((double) nnodes / ((double) (clock() - searchstart) / CLOCKS_PER_SEC)));
    printf(" hashfull %d", (int) ((double) search_ttable.occupancy / (double) search_ttable.size * 1000));

    if(pvcount[0])
    {
        printf(" pv");
        for(i=0; i<pvcount[0]; i++)
        {
            move_tolongalg(curpv[0][i], str);
            printf(" %s", str);
        }
    }

    printf("\n");

    printf("info string outdegree %f\n", ((double) nnodes - 1) / (double) nnonterminal);
}

static score_t brain_quiesencesearch(board_t* board, int plies, score_t alpha, score_t beta)
{
    score_t eval, besteval;
    moveset_t moves;
    picker_t picker;
    move_t move;
    mademove_t mademove;
    
    nnodes++;
    if(plies > seldepth)
        seldepth = plies;

    if((double) (clock() - searchstart) / CLOCKS_PER_SEC * 1000 >= searchtime)
    {
        search_cancel = true;
        return 0;
    }

    besteval = eval = evaluate(board);
    if(besteval >= beta)
        return besteval;
    if(besteval > alpha)
        alpha = besteval;

    if(eval + DELTA_MARGIN < alpha)
        return eval;

    move_gensetup(board);
    move_alllegal(board, &moves, true);
    
    pick_sort(board, &moves, 0, plies, -1, alpha, beta, &picker);

    if(moves.count)
        nnonterminal++;

    while((move = pick(&picker)))
    {
        move_make(board, move, &mademove);
        eval = -brain_quiesencesearch(board, plies + 1, -beta, -alpha);
        move_unmake(board, &mademove);

        if(search_cancel)
            return 0;

        if(eval > besteval)
            besteval = eval;
        if(eval > alpha)
            alpha = eval;
        if(alpha >= beta)
            return alpha;
    }

    return besteval;
}

static inline int brain_calcext(board_t* board, move_t move, int next)
{
    int ext;

    int dst;
    piece_e ptype;

    ext = 0;

    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;

    ptype = board->sqrs[dst] & SQUARE_MASK_TYPE;

    if(board->check)
        ext++;

    if(ptype == PIECE_PAWN && (dst / BOARD_LEN == 1 || dst / BOARD_LEN == BOARD_LEN - 2))
        ext++;

    if(ext > next)
        ext = next;

    return ext;
}

// if search is canceled, dont trust the results!
static score_t search_r(board_t* board, move_t prev, score_t alpha, score_t beta, int plies, int depth, int next, move_t* outmove)
{
    int i;
    move_t move;

    transpos_t *transpos;
    moveset_t moves;
    picker_t picker;
    score_t eval, margin;
    move_t bestmove;
    mademove_t mademove;
    transpos_type_e transpostype;
    movetype_e movetype;
    bool capture, promotes, nonpv, givescheck;
    int reduction;
    int ext;
    score_t childalpha, childbeta;

    pvcount[plies] = 0;

    nnodes++;
    if(plies > seldepth)
        seldepth = plies;

    if((double) (clock() - searchstart) / CLOCKS_PER_SEC * 1000 >= searchtime)
    {
        search_cancel = true;
        return 0;
    }

    // three-fold repitition or fifty-move
    if(board->stalemate)
        return 0;

    transpos = transpose_find(&search_ttable, board->hash, depth, alpha, beta, false);
    if(transpos)
    {
        if(outmove)
            *outmove = transpos->bestmove;
        return transpos->eval;
    }

    if(!depth)
        return brain_quiesencesearch(board, plies, alpha, beta);

    move_gensetup(board);
    move_alllegal(board, &moves, false);

    if(!moves.count)
    {
        eval = 0; // stalemate
        if(board->check)
            eval = -SCORE_MATE + plies; // checkmate
        else
            transpose_store(&search_ttable, board->hash, depth, 0, 0, TRANSPOS_PV);

        return eval;
    }

    nnonterminal++;

    // null move pruning: when not in check, not in king-and-pawn endgame, and depth is high enough
    // we can assume doing nothing is generally worse than doing something. use a null move as a lower bound for the moves.
    // if we can already cause a fail-high cutoff, we can assume the moves will only be better and exit here.
    if(!board->check
    && ((board->pboards[board->tomove][PIECE_KING] | board->pboards[board->tomove][PIECE_PAWN])
    != board->pboards[board->tomove][PIECE_NONE])
    && depth > NULL_REDUCTION)
    {
        move_makenull(board, &mademove);
        eval = -search_r(board, 0, -beta, -beta + 1, plies + 1, depth - 1 - NULL_REDUCTION, next, NULL);
        move_unmakenull(board, &mademove);

        // doing nothing was good enough to cause a cutoff, doing something would
        // probably only be better
        if(eval >= beta)
        {
            if(eval > -MATE_THRESH && eval < MATE_THRESH)
                transpose_store(&search_ttable, board->hash, depth, eval, 0, TRANSPOS_LOWER);
            return eval;
        }
    }

    pick_sort(board, &moves, prev, plies, depth, alpha, beta, &picker);

    i = 0;
    bestmove = 0;
    transpostype = TRANSPOS_UPPER;
    while((move = pick(&picker)))
    {
        reduction = 0;
        movetype = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;
        capture = ((board->sqrs[(move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS] & SQUARE_MASK_TYPE) != PIECE_NONE)
        || movetype == MOVETYPE_ENPAS;
        promotes = movetype >= MOVETYPE_PROMQ && movetype <= MOVETYPE_PROMN;
        givescheck = move_givescheck(board, move);
        nonpv = i || !picker.tt;

        // futility pruning
        // if the move probably can't raise alpha (static eval + margin), don't even search it.
        // only do it towards leaves, and if there is no capture, check, or mate.
        // also don't do it if side to move is in check.
        if(depth <= 3 
        && !board->check && !givescheck && !capture && !promotes
        && alpha < MATE_THRESH && beta > -MATE_THRESH)
        {
            margin = 128 * depth;
            eval = evaluate(board);
            if(eval + margin <= alpha)
            {
                i++;
                continue;
            }
        }

        childalpha = -beta;
        childbeta = -alpha;

        // PV Search
        // if the current node isn't a pv node, we assume its probably worse.
        // do it with a null window, and if its better than we thought research.
        if(nonpv)
            childalpha = -alpha - 1;

        move_make(board, move, &mademove);

        ext = brain_calcext(board, move, next);

        // trust move ordering is decent, search later moves to a shallower depth
        if(i >= 2 && depth > LMR_REDUCTION && !ext && !capture)
            reduction += LMR_REDUCTION;

        // initial search
        eval = -search_r(board, move, childalpha, childbeta, plies + 1, depth - 1 + ext - reduction, next - ext, NULL);

        // we did a null window search, but it was good!
        // full window.
        if(nonpv && eval > alpha)
            childalpha = -beta;

        // we did a reduced or null window search but it was good, so research.
        if((reduction || nonpv) && eval > alpha)
            eval = -search_r(board, move, childalpha, childbeta, plies + 1, depth - 1 + ext, next - ext, NULL);

        move_unmake(board, &mademove);

        if(search_cancel)
            return 0;

        if(eval > alpha)
        {
            transpostype = TRANSPOS_PV;

            if(!plies)
                curscore = eval;

            alpha = eval;
            bestmove = move;

            curpv[plies][0] = move;
            pvcount[plies] = 1;
            if(pvcount[plies + 1])
            {
                memcpy(&curpv[plies][1], &curpv[plies + 1][0], pvcount[plies + 1] * sizeof(move_t));
                pvcount[plies] += pvcount[plies + 1];
            }
        }

        // move was so good that opponent will never let us get to this point
        // this means that alpha is only a lower bound for this node.
        if(alpha >= beta)
        {
            if(!capture)
            {
                search_killers[plies][(search_killeridx[plies]++) % MAX_KILLER] = move;
                search_history[board->tomove][move & MOVEBITS_SRC_MASK][(move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS] += depth;
                search_counters[board->tomove][prev & MOVEBITS_SRC_MASK][(prev & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS] = bestmove;
            }
            if(eval > -MATE_THRESH && eval < MATE_THRESH)
                transpose_store(&search_ttable, board->hash, depth, alpha, bestmove, TRANSPOS_LOWER);
            return alpha;
        }

        i++;
    }

    if(outmove)
        *outmove = bestmove;

    if(eval > -MATE_THRESH && eval < MATE_THRESH)
        transpose_store(&search_ttable, board->hash, depth, alpha, bestmove, transpostype);
    return alpha;
}

move_t search(board_t* board, int timems)
{
    int i;

    move_t move;
    score_t alpha, beta, score;

    if(search_active)
        return 0;
    search_active = true;

    searchstart = clock();
    searchtime = timems - 10;
    search_cancel = false;
    bestknown = 0;
    nnodes = nnonterminal = 0;
    seldepth = 0;

    if(book_findmove(board, &move))
    {
        search_active = false;
        return move;
    }

    lastinfo = clock();
    
    alpha = SCORE_MIN;
    beta = SCORE_MAX;
    for(i=1, move=0; i<MAX_DEPTH; i++)
    {
        curdepth = i;

        if(i > 1)
        {
            alpha = score - ASPIRATION_MARGIN;
            beta = score + ASPIRATION_MARGIN;
        }

runsearch:
        memset(search_killeridx, 0, sizeof(search_killeridx));
        memset(search_killers, 0, sizeof(search_killers));
        memset(search_history, 0, sizeof(search_history));
        memset(search_counters, 0, sizeof(search_counters));
        memset(pvcount, 0, sizeof(pvcount));

        score = search_r(board, 0, SCORE_MIN, SCORE_MAX, 0, i, 16, &move);
        
        if(search_cancel)
            break;

        if(score <= alpha)
            alpha = SCORE_MIN;
        if(score >= beta)
            beta = SCORE_MAX;
        if(score <= alpha || score >= beta)
            goto runsearch;

        curscore = score;
        search_printinfo(board);

        bestknown = move;
    }

    search_active = false;
    return move;
}

void search_init(void)
{
    transpose_alloc(&search_ttable, 64 * 1024);
}
