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
#define LMR_REDUCTION 3

#define INFO_PERIOD_CLOCKS (100 * (CLOCKS_PER_SEC / 1000))

#define SCORE_MATE 24000
#define MATE_THRESH (SCORE_MATE - MAX_DEPTH)

int search_killeridx[MAX_DEPTH];
move_t search_killers[MAX_DEPTH][MAX_KILLER];
score_t search_history[TEAM_COUNT][BOARD_AREA][BOARD_AREA];
move_t search_counters[TEAM_COUNT][BOARD_AREA][BOARD_AREA];

int ntranspos = 0, ncutnodes = 0, npvnodes = 0;
clock_t searchstart;
bool searchcanceled;
int searchtime;
move_t bestknown;
clock_t lastinfo;

int curdepth = 0;
int seldepth = 0;
score_t curscore = 0;
uint64_t nnodes;

static inline void search_printinfo(board_t* board)
{
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
    printf(" hashfull %d", (int) ((double) board->ttable.occupancy / (double) board->ttable.size * 1000));
    printf("\n");
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
        searchcanceled = true;
        return 0;
    }

    besteval = eval = evaluate(board);
    if(besteval >= beta)
        return besteval;
    if(besteval > alpha)
        alpha = besteval;

    move_gensetup(board);
    move_alllegal(board, &moves, true);
    
    pick_sort(board, &moves, 0, plies, -1, alpha, beta, &picker);

    while((move = pick(&picker)))
    {
        move_make(board, move, &mademove);
        eval = -brain_quiesencesearch(board, plies + 1, -beta, -alpha);
        move_unmake(board, &mademove);

        if(searchcanceled)
            return 0;

        if(eval > besteval)
            besteval = eval;
        if(eval > alpha)
            alpha = eval;
        if(alpha >= beta)
        {
            ncutnodes++;
            return alpha;
        }
    }

    npvnodes++;

    return besteval;
}

static inline int brain_calcext(board_t* board, move_t move, int next)
{
    int ext;

    int dst, type;
    piece_e ptype;

    ext = 0;

    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    ptype = board->sqrs[dst] & SQUARE_MASK_TYPE;

    if(board->check)
        ext++;

    if(ptype == PIECE_PAWN && (dst / BOARD_LEN == 1 || dst / BOARD_LEN == BOARD_LEN - 2))
        ext++;

    if(type >= MOVETYPE_PROMQ && type <= MOVETYPE_PROMN)
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
    score_t eval;
    move_t bestmove;
    mademove_t mademove;
    transpos_type_e transpostype;
    bool needsfullsearch;
    bool capture;
    bool checknull;
    int ext;

    nnodes++;
    if(plies > seldepth)
        seldepth = plies;

    if(clock() - lastinfo >= INFO_PERIOD_CLOCKS)
        search_printinfo(board);

    if((double) (clock() - searchstart) / CLOCKS_PER_SEC * 1000 >= searchtime)
    {
        searchcanceled = true;
        return 0;
    }

    // three-fold repitition or fifty-move
    if(board->stalemate)
        return 0;

    transpos = transpose_find(&board->ttable, board->hash, depth, alpha, beta, false);
    if(transpos)
    {
        ntranspos++;
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

        // i think that since mate needs plies, we can't store transposition here
        return eval;
    }
    
    // not in check, not in king-and-pawn endgame, and depth > 3
    checknull = !board->check
    && ((board->pboards[board->tomove][PIECE_KING] | board->pboards[board->tomove][PIECE_PAWN])
    != board->pboards[board->tomove][PIECE_NONE])
    && depth > NULL_REDUCTION;

    if(checknull)
    {
        move_makenull(board, &mademove);
        eval = -search_r(board, 0, -beta, -beta + 1, plies + 1, depth - 1 - NULL_REDUCTION, next, NULL);
        move_unmakenull(board, &mademove);

        if(eval >= beta)
        {
            ncutnodes++;
            transpose_store(&board->ttable, board->hash, depth, eval, 0, TRANSPOS_LOWER);
            return eval;
        }
    }

    pick_sort(board, &moves, prev, plies, depth, alpha, beta, &picker);

    i = 0;
    bestmove = 0;
    transpostype = TRANSPOS_UPPER;
    while((move = pick(&picker)))
    {
        move_make(board, move, &mademove);

        capture = mademove.captured;
        ext = brain_calcext(board, move, next);

        needsfullsearch = true;
        if(i > 2 && depth > LMR_REDUCTION && !ext && !capture)
        {
            eval = -search_r(board, move, -beta, -alpha, plies + 1, depth - 1 - LMR_REDUCTION, next, NULL);
            needsfullsearch = eval > alpha;
        }

        if(needsfullsearch)
            eval = -search_r(board, move, -beta, -alpha, plies + 1, depth - 1 + ext, next - ext, NULL);
        
        move_unmake(board, &mademove);

        if(searchcanceled)
            return 0;

        if(eval > alpha)
        {
            transpostype = TRANSPOS_PV;

            alpha = eval;
            bestmove = move;
        }

        // move was so good that opponent will never let us get to this point
        if(alpha >= beta)
        {
            ncutnodes++;
            if(!capture)
            {
                search_killers[depth][(search_killeridx[depth]++) % MAX_KILLER] = move;
                search_history[board->tomove][move & MOVEBITS_SRC_MASK][(move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS] += depth;
                search_counters[board->tomove][prev & MOVEBITS_SRC_MASK][(prev & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS] = bestmove;
            }
            transpose_store(&board->ttable, board->hash, depth, alpha, bestmove, TRANSPOS_LOWER);
            return alpha;
        }

        i++;
    }
    
    npvnodes++;

    if(outmove)
        *outmove = bestmove;

    transpose_store(&board->ttable, board->hash, depth, alpha, bestmove, transpostype);
    return alpha;
}

move_t search(board_t* board, int timems)
{
    int i;

    move_t move;
    score_t score;

    ntranspos = ncutnodes = npvnodes = 0;
    searchstart = clock();
    searchtime = timems - 10;
    searchcanceled = false;
    bestknown = 0;
    nnodes = 0;
    seldepth = 0;

    if(book_findmove(board, &move))
        return move;

    lastinfo = clock();
    
    for(i=1, move=0; i<MAX_DEPTH; i++)
    {
        curdepth = i;

        memset(search_killeridx, 0, sizeof(search_killeridx));
        memset(search_killers, 0, sizeof(search_killers));
        memset(search_history, 0, sizeof(search_history));
        memset(search_counters, 0, sizeof(search_counters));

        score = search_r(board, 0, SCORE_MIN, SCORE_MAX, 0, i, 16, &move);
        
        if(searchcanceled)
            break;

        curscore = score;
        search_printinfo(board);

        bestknown = move;
    }

    return move;
}
