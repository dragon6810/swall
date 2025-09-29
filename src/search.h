#ifndef _BRAIN_H
#define _BRAIN_H

#include <stdint.h>

#include "board.h"
#include "move.h"

move_t search(board_t* board, int timems);

#endif