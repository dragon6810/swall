#include <stdio.h>
#include <string.h>

#include "board.h"

#define MAX_INPUT 64

board_t board = {};

int main(int argc, char** argv)
{
    char buf[MAX_INPUT];
    char src[3], dst[3];

    board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    board_print(&board);

    while(1)
    {
        fgets(buf, MAX_INPUT, stdin);
        buf[strlen(buf) - 1] = 0; // chop off \n

        if(!strcmp(buf, "help"))
        {
            printf("commands:\n");

            printf("    help\n");
            printf("        prints this message.\n");

            printf("    move <src> <dst>\n");
            printf("        move a piece from source square to dest square, if legal.\n");
            printf("        src and dst are expected to be in algebraic notation.\n");

            continue;;
        }

        if(sscanf(buf, " move %2s %2s", src, dst) == 2)
        {
            printf("%s -> %s\n", src, dst);

            continue;
        }
    }
    
    return 0;
}