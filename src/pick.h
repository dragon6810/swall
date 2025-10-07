#ifndef _PICK_H
#define _PICK_H

#include "board.h"
#include "move.h"
#include "search.h"

typedef enum
{
    PICK_TT=0,
    PICK_CHECKS,
    PICK_GOODCAP,
    PICK_COUNTER,
    PICK_KILLERS,
    PICK_QUIET,
    PICK_BADCAP,
} pickstate_e;

typedef struct
{
    move_t tt;
    moveset_t checks;
    moveset_t goodcap;
    move_t counter;
    uint8_t nkillers;
    move_t killers[MAX_KILLER];
    moveset_t quiet;
    moveset_t badcap;

    score_t goodscores[MAX_MOVE];
    score_t quietscores[MAX_MOVE];
    score_t badscores[MAX_MOVE];

    pickstate_e state;
    uint8_t idx;
} picker_t;

void pick_sort(board_t* restrict board, moveset_t* restrict moves, move_t prev,
int plies, uint8_t depth, score_t alpha, score_t beta, picker_t* restrict picker);
move_t pick(picker_t* restrict picker);

#endif