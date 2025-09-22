#ifndef _BOOK_H
#define _BOOK_H

#include <stdint.h>

#include "board.h"
#include "move.h"

// exits silently if it can't open the path
void book_load(const char* path);
bool book_findmove(board_t* board, move_t* outmove);
void book_free(void);

#endif