#include "brain.h"

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

int16_t brain_eval(board_t* board)
{
    int i;
    team_e t;
    piece_e p;

    bitboard_t pmask;
    int16_t scores[TEAM_COUNT];

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
        }
    }

    return 
        board->tomove == TEAM_WHITE ? scores[TEAM_WHITE] - scores[TEAM_BLACK] 
                                    : scores[TEAM_BLACK] - scores[TEAM_WHITE];
}

int16_t brain_search(board_t* board, int16_t alpha, int16_t beta, int depth, move_t* outmove)
{
    moveset_t *cur;

    moveset_t *moves;
    int16_t eval;
    move_t bestmove;
    mademove_t mademove;

    if(!depth)
        return brain_eval(board);
    moves = move_alllegal(board);

    if(!moves)
    {
        if(board->check[board->tomove])
            return INT16_MIN + 1; // checkmate
        return 0; // stalemate
    }

    for(cur=moves; cur; cur=cur->next)
    {
        move_domove(board, cur->move, &mademove);
        eval = -brain_search(board, -beta, -alpha, depth - 1, NULL);
        move_undomove(board, &mademove);

        if(eval >= beta)
        {
            move_freeset(moves);
            return beta;
        }

        if(eval > alpha)
        {
            alpha = eval;
            bestmove = cur->move;
        }
    }

    move_freeset(moves);

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
