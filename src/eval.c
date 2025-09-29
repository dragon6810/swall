#include "eval.h"

static const score_t startmaterial = 0
        + eval_pscore[PIECE_QUEEN] 
        + eval_pscore[PIECE_ROOK] * 2 
        + eval_pscore[PIECE_BISHOP] * 2 
        + eval_pscore[PIECE_KNIGHT] * 2 
        + eval_pscore[PIECE_PAWN] * 8;

static inline void eval_positionbonus(board_t* board, float endgame, score_t bonus[TEAM_COUNT])
{
    int i;
    team_e t;

    piece_e piece;
    int square;
    float a, b;

    for(t=0; t<TEAM_COUNT; t++)
    {
        bonus[t] = 0;
        for(i=0; i<board->npiece[t]; i++)
        {
            square = board->ptable[t][i];
            piece = board->sqrs[square] & SQUARE_MASK_TYPE;
            if(t == TEAM_BLACK)
                square = BOARD_AREA - square;
            
            a = eval_psqrtable[0][piece][square];
            b = eval_psqrtable[1][piece][square];

            bonus[t] += a + (b - a) * endgame;
        }
    }
}

static inline float eval_endgameweight(score_t totalmaterial)
{
    return 1.0 - ((float) totalmaterial / (float) startmaterial);
}

static inline void eval_countmaterial(board_t* board, score_t material[TEAM_COUNT])
{
    int i;
    team_e t;

    for(t=0; t<TEAM_COUNT; t++)
    {
        material[t] = 0;
        for(i=0; i<board->npiece[t]; i++)
            material[t] += eval_pscore[board->sqrs[board->ptable[t][i]] & SQUARE_MASK_TYPE];
    }
}

// positive in favor of board->tomove
score_t evaluate(board_t* board)
{
    team_e t;

    score_t material[TEAM_COUNT], position[TEAM_COUNT], scores[TEAM_COUNT], eval;
    float endgameweight;

    eval_countmaterial(board, material);
    endgameweight = eval_endgameweight(material[TEAM_WHITE] + material[TEAM_BLACK]);
    eval_positionbonus(board, endgameweight, position);

    for(t=0; t<TEAM_COUNT; t++)
        scores[t] = material[t] + position[t];

    if(board->tomove == TEAM_WHITE)
        eval = scores[TEAM_WHITE] - scores[TEAM_BLACK];
    else
        eval = scores[TEAM_BLACK] - scores[TEAM_WHITE];
    
    return eval;
}
