#include "eval.h"

static const score_t startmaterial = 
          eval_pscore[PIECE_QUEEN]
        + eval_pscore[PIECE_ROOK] * 2 
        + eval_pscore[PIECE_BISHOP] * 2 + eval_pscore[PIECE_KNIGHT] * 2 
        + eval_pscore[PIECE_PAWN] * 8;

static inline bitboard_t eval_isolatedpawnmask(team_e team, uint8_t square)
{
    int8_t f;
    bitboard_t mask;

    f = square % BOARD_LEN;

    mask = 0;
    if(f)
        mask |= board_files[f - 1];
    if(f < BOARD_LEN - 1)
        mask |= board_files[f + 1];

    return mask;
}

static inline bitboard_t eval_passedpawnmask(team_e team, uint8_t square)
{
    const bitboard_t full = UINT64_MAX;

    int8_t r, f;
    bitboard_t mask;

    r = square / BOARD_LEN;
    f = square % BOARD_LEN;

    mask = 0;
    if(f)
        mask |= board_files[f - 1];
    mask |= board_files[f];
    if(f < BOARD_LEN - 1)
        mask |= board_files[f + 1];

    if(team == TEAM_WHITE)
        mask &= full << ((r + 1) * BOARD_LEN);
    else
        mask &= full >> ((BOARD_LEN - (r - 1)) * BOARD_LEN);

    return mask;
}

static const score_t passedpawnbonus[BOARD_LEN] = { 0, 0, 10, 20, 40, 60, 80, 0 };

static inline void eval_pawnstructure(board_t* board, score_t bonus[TEAM_COUNT])
{
    const score_t isolatedpenalty = 50;

    team_e t;

    int8_t square, r;
    bitboard_t bb, mask;

    for(t=0; t<TEAM_COUNT; t++)
    {
        bonus[t] = 0;
        bb = board->pboards[t][PIECE_PAWN];
        while(bb)
        {
            square = __builtin_ctzll(bb);
            bb &= bb - 1;

            mask = eval_passedpawnmask(t, square);

            if(!(board->pboards[!t][PIECE_PAWN] & mask))
            {
                r = square / BOARD_LEN;
                if(t == TEAM_BLACK)
                    r = BOARD_LEN - r;

                bonus[t] += passedpawnbonus[r];
            }

            mask = eval_isolatedpawnmask(t, square);
            if(!(board->pboards[t][PIECE_PAWN] & mask))
                bonus[t] -= isolatedpenalty;
        }
    }
}

static inline void eval_positionbonus(board_t* board, float endgame, score_t bonus[TEAM_COUNT])
{
    team_e t;
    piece_e p;

    bitboard_t bb;
    int square;
    float a, b;

    for(t=0; t<TEAM_COUNT; t++)
    {
        bonus[t] = 0;
        for(p=PIECE_KING; p<PIECE_COUNT; p++)
        {
            bb = board->pboards[t][p];
            while(bb)
            {
                square = __builtin_ctzll(bb);
                bb &= bb - 1;

                if(t == TEAM_BLACK)
                    square = BOARD_AREA - square;
            
                a = eval_psqrtable[0][p][square];
                b = eval_psqrtable[1][p][square];

                bonus[t] += a + (b - a) * endgame;
            }
        }
    }
}

static inline float eval_endgameweight(score_t totalmaterial)
{
    float t;

    // 0 = startpos, 1 = king vs king
    t = 1.0 - ((float) totalmaterial / (float) startmaterial);

    // extremely unlikely, but t could be < 0 if someone promotes a lot
    if(t < 0)
        t = 0;

    // i tried adding some nonlinearity but it made it worse
    // worth looking into whether linear is actually good here
    return t;
}

static inline void eval_countmaterial(board_t* board, score_t material[TEAM_COUNT])
{
    team_e t;
    piece_e p;

    for(t=0; t<TEAM_COUNT; t++)
    {
        material[t] = 0;
        for(p=PIECE_KING; p<PIECE_COUNT; p++)
            material[t] += eval_pscore[p] * __builtin_popcountll(board->pboards[t][p]);
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
