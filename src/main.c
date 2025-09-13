#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "move.h"
#include "tests.h"

#define MAX_INPUT 64

board_t board = {};

int main(int argc, char** argv)
{
    int i;

    char buf[MAX_INPUT];
    char movestr[2][3];
    int r, f;
    uint8_t sqrs[2];
    move_t move, *pmove;
    moveset_t *moves;
    mademove_t mademove;

    setlocale(LC_ALL, ""); 

    move_init();

    tests_movegen();

    board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board_findpieces(&board);
    move_findattacks(&board);
    move_findpins(&board);

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
            {
                printf("invalid coordinates.\n");
                continue;
            }

            for(i=0; i<2; i++)
            {
                r = movestr[i][1] - '1';
                f = movestr[i][0] - 'a';
                sqrs[i] = r * BOARD_LEN + f;
            }

            move = 0;
            move |= sqrs[0];
            move |= ((uint16_t) sqrs[1]) << MOVEBITS_DST_BITS;

            moves = move_legalmoves(&board, NULL, sqrs[0]);
            move_printset(moves);
            
            if(!(pmove = move_findmove(moves, move)))
            {
                printf("invalid move.\n");
                move_freeset(moves);
                moves = NULL;
                continue;
            }

            if((*pmove >> MOVEBITS_TYP_BITS) >= MOVETYPE_PROMQ
            && (*pmove >> MOVEBITS_TYP_BITS) <= MOVETYPE_PROMN)
            {
                *pmove &= ~MOVEBITS_TYP_MASK;

                printf("what piece would you like to promote to?\n");
                printf("type \"queen\", \"rook\", \"bishop\", or \"knight\".\n");

                fgets(buf, MAX_INPUT, stdin);
                buf[strlen(buf) - 1] = 0; // chop off \n
                
                if(!strcmp(buf, "queen"))
                    *pmove |= ((uint16_t)MOVETYPE_PROMQ) << MOVEBITS_TYP_BITS;
                else if(!strcmp(buf, "rook"))
                    *pmove |= ((uint16_t)MOVETYPE_PROMR) << MOVEBITS_TYP_BITS;
                else if(!strcmp(buf, "bishop"))
                    *pmove |= ((uint16_t)MOVETYPE_PROMB) << MOVEBITS_TYP_BITS;
                else if(!strcmp(buf, "knight"))
                    *pmove |= ((uint16_t)MOVETYPE_PROMN) << MOVEBITS_TYP_BITS;
                else
                {
                    printf("invalid promotion option.\n");
                    move_freeset(moves);
                    moves = NULL;
                    continue;
                }
            }

            move_domove(&board, *pmove, &mademove);

            board_print(&board);

            move_freeset(moves);
            moves = NULL;

            continue;
        }

        if(buf[0])
            printf("no matching command.\n");
    }
    
    return 0;
}