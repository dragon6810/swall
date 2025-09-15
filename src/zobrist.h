#ifndef _ZOBRIST_H
#define _ZOBRIST_H

#include <stdint.h>

#include "board.h"

void zobrist_init(void);
uint64_t zobrist_hash(board_t* board);

#endif