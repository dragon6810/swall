#ifndef _BRAIN_H
#define _BRAIN_H

#include <stdint.h>

#include "board.h"
#include "move.h"

// positive in favor of board->tomove
int16_t brain_eval(board_t* board);
int16_t brain_search(board_t* board, int16_t alpha, int16_t beta, int depth, move_t* outmove);
void brain_dobestmove(board_t* board);

#endif