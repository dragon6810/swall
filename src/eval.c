#include "eval.h"

static const score_t startmaterial = 0
        + eval_pscore[PIECE_QUEEN] 
        + eval_pscore[PIECE_ROOK] * 2 
        + eval_pscore[PIECE_BISHOP] * 2 
        + eval_pscore[PIECE_KNIGHT] * 2 
        + eval_pscore[PIECE_PAWN] * 8;

static inline bitboard_t eval_passedpawnmask(team_e team, uint8_t square)
{
    const bitboard_t file = 0x1010101010101010;
    const bitboard_t full = UINT64_MAX;

    int8_t r, f;
    bitboard_t mask;

    r = square / BOARD_LEN;
    f = square + BOARD_LEN;

    mask = 0;
    if(f)
        mask |= file << (f - 1);
    mask |= file << f;
    if(f < BOARD_LEN - 1)
        mask |= file << (f + 1);

    if(team == TEAM_WHITE)
        mask &= full << ((r + 1) * BOARD_LEN);
    else
        mask &= full >> ((BOARD_LEN - (r + 1)) * BOARD_LEN);

    return mask;
}

static const score_t passedpawnbonus[BOARD_LEN] = { 0, 0, 10, 20, 40, 60, 80, 0 };

static inline void eval_pawnstructure(board_t* board, score_t bonus[TEAM_COUNT])
{
    int i;
    team_e t;

    int8_t square, r;
    bitboard_t mask;

    for(t=0; t<TEAM_COUNT; t++)
    {
        bonus[t] = 0;
        for(i=0; i<board->npiece[t]; i++)
        {
            square = board->ptable[t][i];
            if((board->sqrs[square] & SQUARE_MASK_TYPE) != PIECE_PAWN)
                continue;

            r = square / BOARD_LEN;
            mask = eval_passedpawnmask(t, square);

            if(board->pboards[!t][PIECE_PAWN] & mask)
                continue;

            if(t == TEAM_BLACK)
                r = BOARD_LEN - r;

            bonus[t] += passedpawnbonus[r];
        }
    }
}

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

    score_t material[TEAM_COUNT], position[TEAM_COUNT], pawns[TEAM_COUNT], scores[TEAM_COUNT], eval;
    float endgameweight;

    eval_countmaterial(board, material);
    endgameweight = eval_endgameweight(material[TEAM_WHITE] + material[TEAM_BLACK]);
    eval_positionbonus(board, endgameweight, position);
    eval_pawnstructure(board, pawns);

    for(t=0; t<TEAM_COUNT; t++)
        scores[t] = material[t] + position[t] + pawns[t];;

    if(board->tomove == TEAM_WHITE)
        eval = scores[TEAM_WHITE] - scores[TEAM_BLACK];
    else
        eval = scores[TEAM_BLACK] - scores[TEAM_WHITE];
    
    return eval;
}
