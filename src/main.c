#include <stdio.h>

#include "board.h"

int main(int argc, char** argv)
{
    board_t board = {};

    board_print(&board);
    
    return 0;
}