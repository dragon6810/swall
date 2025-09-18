#include "brain.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

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
int16_t psqrttable[PIECE_COUNT][BOARD_AREA] =
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
};

// team is the team trying to get their opponent's king into corner
int16_t brain_kingcornerbonus(board_t* board, team_e team, float endgame)
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
    return sqrdst * 100;
}

int16_t brain_eval(board_t* board)
{
    const int startmaterial = 0
        + pscore[PIECE_QUEEN] 
        + pscore[PIECE_ROOK] * 2 
        + pscore[PIECE_BISHOP] * 2 + pscore[PIECE_KNIGHT] * 2 
        + pscore[PIECE_PAWN] * 8;

    int i;
    team_e t;
    piece_e p;

    bitboard_t pmask;
    int16_t material[TEAM_COUNT], scores[TEAM_COUNT];
    int r, f;
    float endgameweight;

    for(t=0; t<TEAM_COUNT; t++)
    {
        material[t] = 0;

        for(i=0; i<board->npiece[t]; i++)
        {
            pmask = (uint64_t) 1 << board->ptable[t][i];
            for(p=PIECE_KING; p<PIECE_COUNT; p++)
            {
                if(!(board->pboards[t][p] & pmask))
                    continue;

                scores[t] += pscore[p];
                break;
            }

            r = board->ptable[t][i] / BOARD_LEN;
            f = board->ptable[t][i] % BOARD_LEN;
            if(t == TEAM_BLACK)
                r = BOARD_LEN - 1 - r;

            material[t] += psqrttable[p][r * BOARD_LEN + f] * 2;
        }

        endgameweight = 1 - ((float) material[t] / (float) startmaterial);

        scores[t] = material[t];
        scores[t] += brain_kingcornerbonus(board, t, endgameweight);
    }

    return 
        board->tomove == TEAM_WHITE ? scores[TEAM_WHITE] - scores[TEAM_BLACK] 
                                    : scores[TEAM_BLACK] - scores[TEAM_WHITE];
}

static int16_t brain_moveguess(board_t* board, move_t mv)
{
    int16_t score;

    bitboard_t srcmask, dstmask;
    int src, dst, type;
    piece_e psrc, pdst;

    src = (mv & MOVEBITS_SRC_MASK) >> MOVEBITS_SRC_BITS;
    dst = (mv & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (mv & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    srcmask = (uint64_t) 1 << src;
    dstmask = (uint64_t) 1 << dst;

    score = 0;
    if(type >= MOVETYPE_PROMQ)
        score += pscore[PIECE_QUEEN + type - MOVETYPE_PROMQ];

    for(psrc=PIECE_KING; psrc<PIECE_COUNT; psrc++)
        if(board->pboards[board->tomove][psrc] & srcmask)
            break;
    
    if(board->pboards[!board->tomove][PIECE_NONE] & dstmask)
    {
        for(pdst=PIECE_KING; pdst<PIECE_COUNT; pdst++)
            if(board->pboards[!board->tomove][pdst] & dstmask)
                break;

        score += pscore[pdst] - pscore[psrc];
    }

    if(board->attacks[!board->tomove] & dstmask)
        score -= pscore[psrc];

    return score;
}

static void brain_scoremoves(board_t* board, moveset_t* moves, int16_t outscores[MAX_MOVE])
{
    int i;

    for(i=0; i<moves->count; i++)
        outscores[i] = brain_moveguess(board, moves->moves[i]);
}

static int16_t brain_searchcap(board_t* board, int16_t alpha, int16_t beta)
{
    int i, j;

    moveset_t moves;
    int16_t scores[MAX_MOVE];
    int scoreowner;
    int16_t bestscore;
    int16_t eval;
    mademove_t mademove;
    move_t tempmv;
    int16_t tempscr;

    eval = brain_eval(board);
    if(eval >= beta)
        return beta;
    if(eval > alpha)
        alpha = eval;

    move_alllegal(board, &moves, true);
    brain_scoremoves(board, &moves, scores);

    for(i=0; i<moves.count; i++)
    {
        scoreowner = i;
        bestscore = scores[i];

        for(j=i+1; j<moves.count; j++)
        {
            if(scores[j] > bestscore)
            {
                bestscore = scores[j];
                scoreowner = j;
            }
        }

        if(scoreowner != i)
        {
            tempmv = moves.moves[i];
            moves.moves[i] = moves.moves[scoreowner];
            moves.moves[scoreowner] = tempmv;

            tempscr = scores[i];
            scores[i] = scores[scoreowner];
            scores[scoreowner] = tempscr;
        }

        move_domove(board, moves.moves[i], &mademove);
        eval = -brain_searchcap(board, -beta, -alpha);
        move_undomove(board, &mademove);

        if(eval >= beta)
            return beta;

        if(eval > alpha)
            alpha = eval;
    }

    return alpha;
}

int16_t brain_search(board_t* board, int16_t alpha, int16_t beta, int depth, move_t* outmove)
{
    int i, j;

    moveset_t moves;
    int16_t scores[MAX_MOVE];
    int scoreowner;
    int16_t bestscore;
    int16_t eval;
    move_t bestmove;
    mademove_t mademove;
    move_t tempmv;
    int16_t tempscr;

    if(!depth)
        return brain_searchcap(board, alpha, beta);

    move_alllegal(board, &moves, false);
    brain_scoremoves(board, &moves, scores);

    if(!moves.count)
    {
        if(board->check[board->tomove])
            return INT16_MIN + 1; // checkmate
        return 0; // stalemate
    }

    for(i=0; i<moves.count; i++)
    {
        scoreowner = i;
        bestscore = scores[i];

        for(j=i+1; j<moves.count; j++)
        {
            if(scores[j] > bestscore)
            {
                bestscore = scores[j];
                scoreowner = j;
            }
        }

        if(scoreowner != i)
        {
            tempmv = moves.moves[i];
            moves.moves[i] = moves.moves[scoreowner];
            moves.moves[scoreowner] = tempmv;

            tempscr = scores[i];
            scores[i] = scores[scoreowner];
            scores[scoreowner] = tempscr;
        }

        move_domove(board, moves.moves[i], &mademove);
        eval = -brain_search(board, -beta, -alpha, depth - 1, NULL);
        move_undomove(board, &mademove);

        if(eval >= beta)
            return beta;

        if(eval > alpha)
        {
            alpha = eval;
            bestmove = moves.moves[i];
        }
    }

    if(outmove)
        *outmove = bestmove;
    return alpha;
}

void brain_dobestmove(board_t* board)
{
    move_t move;
    mademove_t mademove;

    brain_search(board, INT16_MIN + 1, INT16_MAX, 4, &move);
    move_domove(board, move, &mademove);
}
