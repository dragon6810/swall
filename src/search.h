#ifndef _BRAIN_H
#define _BRAIN_H

#include <stdint.h>

#include "board.h"
#include "move.h"
#include "transpose.h"

// probably faster when this is a power of two, since compiler could swap a modulo for an and
#define MAX_KILLER 2
#define MAX_DEPTH 256
// can go greater than MAX_KILLER, modulo by MAX_KILLER of index
extern int search_killeridx[MAX_DEPTH];
extern move_t search_killers[MAX_DEPTH][MAX_KILLER];
extern score_t search_history[TEAM_COUNT][BOARD_AREA][BOARD_AREA];
extern move_t search_counters[TEAM_COUNT][BOARD_AREA][BOARD_AREA];
extern ttable_t search_ttable;

move_t search(board_t* board, int timems);
void search_init(void);

#endif