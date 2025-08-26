#include <stdio.h>

#include "board.h"

int main(int argc, char** argv)
{
    board_t board = {};

    board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board_print(&board);
    
    return 0;
}