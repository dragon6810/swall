#include "search.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "book.h"
#include "eval.h"
#include "zobrist.h"

#define MAX_KILLER 2
#define MAX_DEPTH 64

#define INFO_PERIOD_CLOCKS (100 * (CLOCKS_PER_SEC / 1000))

#define SCORE_MATE 24000
#define MATE_THRESH (SCORE_MATE - MAX_DEPTH)

int ntranspos = 0, ncutnodes = 0, npvnodes = 0;
clock_t searchstart;
bool searchcanceled;
int searchtime;
move_t bestknown;
int nkillers[MAX_DEPTH] = {};
move_t killers[MAX_DEPTH][MAX_KILLER] = {};
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
        printf(" score mate %d", (SCORE_MATE - curscore) / 2 + 1);
    else
        printf(" score mate %d", (-SCORE_MATE + curscore) / 2 - 1);
    printf(" nodes %llu", nnodes);
    printf(" nps %llu", (uint64_t) ((double) nnodes / ((double) (clock() - searchstart) / CLOCKS_PER_SEC)));
    printf(" hashfull %d", (int) ((double) board->ttable.occupancy / (double) board->ttable.size * 1000));
    printf("\n");
}

static inline void search_sortmoves(moveset_t* moves, score_t* scores)
{
    int i, j;

    score_t bestscore;
    int bestidx;
    move_t tempmv;
    score_t tempscore;

    for(i=0; i<moves->count; i++)
    {
        bestidx = i;
        bestscore = scores[i];

        for(j=i+1; j<moves->count; j++)
        {
            if(scores[j] > bestscore)
            {
                bestscore = scores[j];
                bestidx = j;
            }
        }

        if(bestidx == i)
            continue;

        tempmv = moves->moves[i];
        moves->moves[i] = moves->moves[bestidx];
        moves->moves[bestidx] = tempmv;

        tempscore = scores[i];
        scores[i] = scores[bestidx];
        scores[bestidx] = tempscore;
    }
}

static inline score_t brain_moveguess(board_t* board, move_t mv, int plies, int depth)
{
    int i;
    
    score_t score;
    bitboard_t dstmask;
    int src, dst, type;
    piece_e psrc, pdst;
    transpos_t *transpos;

    if(!plies && mv == bestknown)
        return 9960;

    transpos = transpose_find(&board->ttable, board->hash, 0, INT16_MIN + 1, INT16_MAX, true);
    if(transpos && transpos->bestmove == mv)
        return 9950; // a little less than mate

    // killers
    if(depth >= 0)
        for(i=0; i<nkillers[depth]; i++)
            if(killers[depth][i] == mv)
                return 9940;

    src = (mv & MOVEBITS_SRC_MASK) >> MOVEBITS_SRC_BITS;
    dst = (mv & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (mv & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    dstmask = (uint64_t) 1 << dst;

    score = 0;
    if(type >= MOVETYPE_PROMQ && type <= MOVETYPE_PROMN)
        score += eval_pscore[PIECE_QUEEN + (type - MOVETYPE_PROMQ)];

    psrc = board->sqrs[src] & SQUARE_MASK_TYPE;
    pdst = board->sqrs[dst] & SQUARE_MASK_TYPE;

    // capture opponent
    if(pdst)
        score += eval_pscore[pdst];

    // opponent captures us
    if(board->attacks & dstmask)
        score -= eval_pscore[psrc];

    return score;
}

static inline void search_order(board_t* board, moveset_t* moves, int plies, int depth)
{
    int i;

    score_t scores[MAX_MOVE];

    for(i=0; i<moves->count; i++)
        scores[i] = brain_moveguess(board, moves->moves[i], plies, depth);
    search_sortmoves(moves, scores);
}

static score_t brain_quiesencesearch(board_t* board, int plies, score_t alpha, score_t beta)
{
    int i;

    score_t eval, besteval;
    moveset_t moves;
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
    search_order(board, &moves, -1, -1);

    for(i=0; i<moves.count; i++)
    {
        move_make(board, moves.moves[i], &mademove);
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

static int brain_calcext(board_t* board, move_t move, int next)
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
static score_t search_r(board_t* board, score_t alpha, score_t beta, int plies, int depth, int next, move_t* outmove)
{
    int i;

    transpos_t *transpos;
    moveset_t moves;
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
    
    // not in check, and not in king-and-pawn endgame
    checknull = !board->check 
    && ((board->pboards[board->tomove][PIECE_KING] | board->pboards[board->tomove][PIECE_PAWN])
    != board->pboards[board->tomove][PIECE_NONE])
    && depth > 3;

    if(checknull)
    {
        move_makenull(board, &mademove);
        eval = -search_r(board, -beta, -beta + 1, plies + 1, depth - 1 - 3, next, NULL);
        move_unmakenull(board, &mademove);

        if(eval >= beta)
        {
            ncutnodes++;
            transpose_store(&board->ttable, board->hash, depth, eval, 0, TRANSPOS_LOWER);
            return eval;
        }
    }

    move_alllegal(board, &moves, false);
    search_order(board, &moves, plies, depth);

    if(!moves.count)
    {
        eval = 0; // stalemate
        if(board->check)
            eval = -SCORE_MATE + plies; // checkmate

        transpose_store(&board->ttable, board->hash, depth, eval, 0, TRANSPOS_PV);
        return eval;
    }

    bestmove = 0;
    transpostype = TRANSPOS_UPPER;
    for(i=0; i<moves.count; i++)
    {
        capture = board->sqrs[(moves.moves[i] & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS];

        move_make(board, moves.moves[i], &mademove);

        ext = brain_calcext(board, moves.moves[i], next);

        needsfullsearch = true;
        if(i > 2 && depth > 3 && !ext && !capture)
        {
            eval = -search_r(board, -beta, -alpha, plies + 1, depth - 2, next - ext, NULL);
            needsfullsearch = eval > alpha;
        }

        if(needsfullsearch)
            eval = -search_r(board, -beta, -alpha, plies + 1, depth - 1 + ext, next - ext, NULL);
        
        move_unmake(board, &mademove);

        if(searchcanceled)
            return 0;

        if(eval > alpha)
        {
            if(!capture && nkillers[depth] < MAX_KILLER)
                killers[depth][nkillers[depth]++] = moves.moves[i];

            transpostype = TRANSPOS_PV;

            alpha = eval;
            bestmove = moves.moves[i];
        }

        // move was so good that opponent will never let us get to this point
        if(alpha >= beta)
        {
            ncutnodes++;
            transpose_store(&board->ttable, board->hash, depth, alpha, bestmove, TRANSPOS_LOWER);
            return alpha;
        }
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

        memset(nkillers, 0, sizeof(nkillers));

        score = search_r(board, SCORE_MIN, SCORE_MAX, 0, i, 16, &move);
        
        if(searchcanceled)
            break;

        curscore = score;
        search_printinfo(board);

        bestknown = move;
    }

    return move;
}
