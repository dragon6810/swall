#include "brain.h"

#include <stdio.h>
#include <stdlib.h>

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

int16_t brain_eval(board_t* board)
{
    int i;
    team_e t;
    piece_e p;

    bitboard_t pmask;
    int16_t scores[TEAM_COUNT];
    int r, f;

    for(t=0; t<TEAM_COUNT; t++)
    {
        scores[t] = 0;

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

            scores[t] += psqrttable[p][r * BOARD_LEN + f] * 2;
        }
    }

    return 
        board->tomove == TEAM_WHITE ? scores[TEAM_WHITE] - scores[TEAM_BLACK] 
                                    : scores[TEAM_BLACK] - scores[TEAM_WHITE];
}

int16_t brain_search(board_t* board, int16_t alpha, int16_t beta, int depth, move_t* outmove)
{
    int i;

    moveset_t moves;
    int16_t eval;
    move_t bestmove;
    mademove_t mademove;

    if(!depth)
        return brain_eval(board);

    move_alllegal(board, &moves);

    if(!moves.count)
    {
        if(board->check[board->tomove])
            return INT16_MIN + 1; // checkmate
        return 0; // stalemate
    }

    for(i=0; i<moves.count; i++)
    {
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

    brain_search(board, INT16_MIN + 1, INT16_MAX, 6, &move);
    move_domove(board, move, &mademove);
}
