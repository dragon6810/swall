#include "brain.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "book.h"
#include "zobrist.h"

#define MAX_KILLER 8
#define MAX_DEPTH 64

int16_t pscore[PIECE_COUNT] =
{
    0,   // PIECE_NONE
    0,   // PIECE_KING
    900, // PIECE_QUEEN
    400, // PIECE_ROOK
    350, // PIECE_BISHOP
    300, // PIECE_KNIGHT
    100, // PIECE_PAWN
};

// https://www.chessprogramming.org/Simplified_Evaluation_Function
int16_t psqrttable[2][PIECE_COUNT][BOARD_AREA] =
{
    // early game
    {
        // PIECE_NONE
        {

        },
        // PIECE_KING
        {
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -30,-40,-40,-50,-50,-40,-40,-30,
            -20,-30,-30,-40,-40,-30,-30,-20,
            -10,-20,-20,-20,-20,-20,-20,-10,
            20, 20,  0,  0,  0,  0, 20, 20,
            20, 30, 10,  0,  0, 10, 30, 20
        },
        // PIECE_QUEEN
        {
            -20,-10,-10, -5, -5,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5,  5,  5,  5,  0,-10,
            -5,  0,  5,  5,  5,  5,  0, -5,
            0,  0,  5,  5,  5,  5,  0, -5,
            -10,  5,  5,  5,  5,  5,  0,-10,
            -10,  0,  5,  0,  0,  0,  0,-10,
            -20,-10,-10, -5, -5,-10,-10,-20
        },
        // PIECE_ROOK
        {
            0,  0,  0,  0,  0,  0,  0,  0,
            5, 10, 10, 10, 10, 10, 10,  5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            0,  0,  0,  5,  5,  0,  0,  0
        },
        // PIECE_BISHOP
        {
            -20,-10,-10,-10,-10,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5, 10, 10,  5,  0,-10,
            -10,  5,  5, 10, 10,  5,  5,-10,
            -10,  0, 10, 10, 10, 10,  0,-10,
            -10, 10, 10, 10, 10, 10, 10,-10,
            -10,  5,  0,  0,  0,  0,  5,-10,
            -20,-10,-10,-10,-10,-10,-10,-20,
        },
        // PIECE_KNIGHT
        {
            -50,-40,-30,-30,-30,-30,-40,-50,
            -40,-20,  0,  0,  0,  0,-20,-40,
            -30,  0, 10, 15, 15, 10,  0,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  0, 15, 20, 20, 15,  0,-30,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -40,-20,  0,  5,  5,  0,-20,-40,
            -50,-40,-30,-30,-30,-30,-40,-50,
        },
        // PIECE_PAWN
        {
            0,  0,  0,  0,  0,  0,  0,  0,
            50, 50, 50, 50, 50, 50, 50, 50,
            10, 10, 20, 30, 30, 20, 10, 10,
            5,  5, 10, 25, 25, 10,  5,  5,
            0,  0,  0, 20, 20,  0,  0,  0,
            5, -5,-10,  0,  0,-10, -5,  5,
            5, 10, 10,-20,-20, 10, 10,  5,
            0,  0,  0,  0,  0,  0,  0,  0
        },
    },
    // end game
    {
        // PIECE_NONE
        {

        },
        // PIECE_KING
        {
            -50,-40,-30,-20,-20,-30,-40,-50,
            -30,-20,-10,  0,  0,-10,-20,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 30, 40, 40, 30,-10,-30,
            -30,-10, 20, 30, 30, 20,-10,-30,
            -30,-30,  0,  0,  0,  0,-30,-30,
            -50,-30,-30,-30,-30,-30,-30,-50
        },
        // PIECE_QUEEN
        {
            -20,-10,-10, -5, -5,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5,  5,  5,  5,  0,-10,
            -5,  0,  5,  5,  5,  5,  0, -5,
             0,  0,  5,  5,  5,  5,  0, -5,
            -10,  5,  5,  5,  5,  5,  0,-10,
            -10,  0,  5,  0,  0,  0,  0,-10,
            -20,-10,-10, -5, -5,-10,-10,-20
        },
        // PIECE_ROOK
        {
             0,  0,  0,  0,  0,  0,  0,  0,
             5, 10, 10, 10, 10, 10, 10,  5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
            -5,  0,  0,  0,  0,  0,  0, -5,
             0,  0,  0,  5,  5,  0,  0,  0
        },
        // PIECE_BISHOP
        {
            -20,-10,-10,-10,-10,-10,-10,-20,
            -10,  0,  0,  0,  0,  0,  0,-10,
            -10,  0,  5, 10, 10,  5,  0,-10,
            -10,  5,  5, 10, 10,  5,  5,-10,
            -10,  0, 10, 10, 10, 10,  0,-10,
            -10, 10, 10, 10, 10, 10, 10,-10,
            -10,  5,  0,  0,  0,  0,  5,-10,
            -20,-10,-10,-10,-10,-10,-10,-20,
        },
        // PIECE_KNIGHT
        {
            -50,-40,-30,-30,-30,-30,-40,-50,
            -40,-20,  0,  0,  0,  0,-20,-40,
            -30,  0, 10, 15, 15, 10,  0,-30,
            -30,  5, 15, 20, 20, 15,  5,-30,
            -30,  0, 15, 20, 20, 15,  0,-30,
            -30,  5, 10, 15, 15, 10,  5,-30,
            -40,-20,  0,  5,  5,  0,-20,-40,
            -50,-40,-30,-30,-30,-30,-40,-50,
        },
        // PIECE_PAWN
        {
            0,  0,  0,  0,  0,  0,  0,  0,
            70, 70, 70, 70, 70, 70, 70, 70,
            50, 50, 50, 50, 50, 50, 50, 50,
            30, 30, 30, 30, 30, 30, 30, 30,
            20, 20, 20, 20, 20, 20, 20, 20,
            10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10,
             0,  0,  0,  0,  0,  0,  0,  0
        },
    },
};

// team is the team trying to get their opponent's king into corner
static int16_t brain_kingcornerbonus(board_t* board, team_e team, float endgame)
{
    bitboard_t king;
    uint8_t kingsqr, r, f;
    float x, y;
    float sqrdst;

    king = board->pboards[!team][PIECE_KING];
    kingsqr = __builtin_ctzll(king);
    f = kingsqr % BOARD_LEN;
    r = kingsqr / BOARD_LEN;
    x = ((float) f / (float) BOARD_LEN - 0.5) * 2;
    y = ((float) r / (float) BOARD_LEN - 0.5) * 2;

    sqrdst = x * x + y * y;
    return sqrdst * 100 * endgame;
}

// positive in favor of board->tomove
static int16_t brain_eval(board_t* board)
{
    const int startmaterial = 0
        + pscore[PIECE_QUEEN] 
        + pscore[PIECE_ROOK] * 2 
        + pscore[PIECE_BISHOP] * 2 + pscore[PIECE_KNIGHT] * 2 
        + pscore[PIECE_PAWN] * 8;

    int i;
    team_e t;

    int16_t material[TEAM_COUNT], scores[TEAM_COUNT], eval;
    int r, f;
    piece_e p;
    float a, b, endgameweight;

    for(t=0; t<TEAM_COUNT; t++)
    {
        scores[t] = material[t] = 0;

        for(i=0; i<board->npiece[t]; i++)
        {
            p = board->sqrs[board->ptable[t][i]] & SQUARE_MASK_TYPE;
            material[t] += pscore[p];
        }
    }

    endgameweight = 1 - ((float) (material[TEAM_WHITE] + material[TEAM_BLACK]) / (float) (startmaterial * 2));

    for(t=0; t<TEAM_COUNT; t++)
    {
        for(i=0; i<board->npiece[t]; i++)
        {
            r = board->ptable[t][i] / BOARD_LEN;
            f = board->ptable[t][i] % BOARD_LEN;
            p = board->sqrs[board->ptable[t][i]] & SQUARE_MASK_TYPE;

            if(t == TEAM_BLACK)
                r = BOARD_LEN - 1 - r;

            a = psqrttable[0][p][r * BOARD_LEN + f];
            b = psqrttable[1][p][r * BOARD_LEN + f];
            scores[t] += (a + (b - a) * endgameweight) * 2;
        }

        scores[t] += material[t];
        scores[t] += brain_kingcornerbonus(board, t, endgameweight);
    }

    eval = board->tomove == TEAM_WHITE ? scores[TEAM_WHITE] - scores[TEAM_BLACK] : scores[TEAM_BLACK] - scores[TEAM_WHITE];
    return eval;
}

int ntranspos = 0, ncutnodes = 0, npvnodes = 0;
clock_t searchstart;
bool searchcanceled;
int searchtime;
move_t bestknown;
int nkillers[MAX_DEPTH] = {};
move_t killers[MAX_DEPTH][MAX_KILLER] = {};

static int16_t brain_moveguess(board_t* board, move_t mv, int plies)
{
    int i;
    
    int16_t score;
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
    if(plies >= 0)
        for(i=0; i<nkillers[plies]; i++)
            if(killers[plies][i] == mv)
                return 9940;

    src = (mv & MOVEBITS_SRC_MASK) >> MOVEBITS_SRC_BITS;
    dst = (mv & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (mv & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    dstmask = (uint64_t) 1 << dst;

    score = 0;
    if(type >= MOVETYPE_PROMQ && type <= MOVETYPE_PROMN)
        score += pscore[PIECE_QUEEN + (type - MOVETYPE_PROMQ)];

    psrc = board->sqrs[src] & SQUARE_MASK_TYPE;
    pdst = board->sqrs[dst] & SQUARE_MASK_TYPE;

    // capture opponent
    if(pdst)
        score += pscore[pdst];

    // opponent captures us
    if(board->attacks[!board->tomove][PIECE_NONE] & dstmask)
        score -= pscore[psrc];

    return score;
}

static void brain_scoremoves(board_t* board, moveset_t* moves, int16_t outscores[MAX_MOVE], int plies)
{
    int i;

    for(i=0; i<moves->count; i++)
        outscores[i] = brain_moveguess(board, moves->moves[i], plies);
}

static void brain_sortmoves(moveset_t* moves, int16_t* scores)
{
    int i, j;

    int16_t bestscore;
    int bestidx;
    move_t tempmv;
    int16_t tempscore;

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

static int16_t brain_quiesencesearch(board_t* board, int16_t alpha, int16_t beta)
{
    int i;

    int16_t eval, besteval;
    moveset_t moves;
    mademove_t mademove;
    int16_t scores[MAX_MOVE];

    if((double) (clock() - searchstart) / CLOCKS_PER_SEC * 1000 >= searchtime)
    {
        searchcanceled = true;
        return 0;
    }

    besteval = eval = brain_eval(board);
    if(besteval >= beta)
        return besteval;
    if(besteval > alpha)
        alpha = besteval;

    move_alllegal(board, &moves, true);
    brain_scoremoves(board, &moves, scores, -1);
    brain_sortmoves(&moves, scores);

    for(i=0; i<moves.count; i++)
    {
        move_make(board, moves.moves[i], &mademove);
        eval = -brain_quiesencesearch(board, -beta, -alpha);
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

    if(board->check[board->tomove])
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
static int16_t brain_search(board_t* board, int16_t alpha, int16_t beta, int depth, int rootdepth, int next, move_t* outmove)
{
    int i;

    transpos_t *transpos;
    moveset_t moves;
    int16_t scores[MAX_MOVE];
    int16_t eval;
    move_t bestmove;
    mademove_t mademove;
    transpos_type_e transpostype;
    bool needsfullsearch;
    bool capture;
    int ext;

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
        return brain_quiesencesearch(board, alpha, beta);

    move_alllegal(board, &moves, false);
    brain_scoremoves(board, &moves, scores, rootdepth);
    brain_sortmoves(&moves, scores);

    if(!moves.count)
    {
        eval = 0; // stalemate
        if(board->check[board->tomove])
            eval = -10000 + rootdepth; // checkmate

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
        if(i > 2 && depth - 1 > 2 && !ext)
        {
            eval = -brain_search(board, -beta, -alpha, depth - 2, rootdepth + 1, next - ext, NULL);
            needsfullsearch = eval > alpha;
        }

        if(needsfullsearch)
            eval = -brain_search(board, -beta, -alpha, depth - 1 + ext, rootdepth + 1, next - ext, NULL);
        
        move_unmake(board, &mademove);

        if(searchcanceled)
            return 0;

        if(eval > alpha)
        {
            if(!capture && nkillers[rootdepth] < MAX_KILLER)
                killers[rootdepth][nkillers[rootdepth]++] = moves.moves[i];

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

move_t brain_runsearch(board_t* board, int timems)
{
    int i;

    move_t move;
    char str[MAX_LONGALG];
    int score;

    ntranspos = ncutnodes = npvnodes = 0;
    searchstart = clock();
    searchtime = timems - 10;
    searchcanceled = false;
    bestknown = 0;
    memset(nkillers, 0, sizeof(nkillers));

    if(book_findmove(board, &move))
        return move;
    
    for(i=1, move=0; i<MAX_DEPTH; i++)
    {
        score = brain_search(board, INT16_MIN + 1, INT16_MAX, i, 0, 16, &move);
        move_tolongalg(move, str);
        printf("depth: %d. best move: %s. score: %d.\n", i, str, score);

        if(searchcanceled)
            break;

        bestknown = move;
    }

    printf("searched to depth %d with %d transpositions, %d cut nodes, and %d pv nodes.\n", i - 1, ntranspos, ncutnodes, npvnodes);

    return move;
}
