#include <stdio.h>
#include <string.h>

#include "board.h"

#define MAX_INPUT 64

board_t board = {};

int main(int argc, char** argv)
{
    int i;

    char buf[MAX_INPUT];
    char movestr[2][3];
    int r, f;
    move_t move;

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

            continue;
        }

        if(sscanf(buf, " move %2s %2s", movestr[0], movestr[1]) == 2)
        {
            for(i=0; i<2; i++)
            {
                if(movestr[i][0] < 'a' || movestr[i][0] > 'h')
                    break;
                if(movestr[i][1] < '1' || movestr[i][1] > '8')
                    break;
            }
            if(i < 2)
                continue;

            for(i=0; i<2; i++)
            {
                r = movestr[i][1] - '1';
                f = movestr[i][0] - 'a';
                move[i] = r * BOARD_LEN + f;
            }

            board.pieces[move[1]] = board.pieces[move[0]];
            board.pieces[move[0]] = 0;
            board_print(&board);

            continue;
        }
    }
    
    return 0;
}