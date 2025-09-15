#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "brain.h"
#include "move.h"
#include "tests.h"

#define MAX_INPUT 512

board_t board = {};

void uci_cmd_position(const char* args)
{
    char move[5];
    move_t *pmove;
    moveset_t moves;
    mademove_t mademove;

    while(*args && *args <= 32)
        args++;

    if(!strncmp(args, "startpos", 8))
    {
        args += 8;
        board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }
    else if (!strncmp(args, "fen", 3))
    {
        args += 3;
        while(*args && *args <= 32)
            args++;

        args += board_loadfen(&board, args);
    }
    else
    {
        board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }

    while(1)
    {
        while(*args && *args <= 32)
            args++;

        if(*args < 'a' || *args > 'h') break;
        move[0] = *args++;
        if(*args < '1' || *args > '8') break;
        move[1] = *args++;
        if(*args < 'a' || *args > 'h') break;
        move[1] = *args++;
        if(*args < '1' || *args > '8') break;
        move[2] = *args++;

        move_alllegal(&board, &moves, false);
        if(!(pmove = move_findmove(&moves, move)))
            continue;

        if((*pmove >> MOVEBITS_TYP_BITS) >= MOVETYPE_PROMQ
        && (*pmove >> MOVEBITS_TYP_BITS) <= MOVETYPE_PROMN)
        {
            *pmove &= ~MOVEBITS_TYP_MASK;

            move[4] = *args++;
            switch(move[4])
            {
            case 'q':
                *pmove |= ((uint16_t)MOVETYPE_PROMQ) << MOVEBITS_TYP_BITS;
                break;
            case 'r':
                *pmove |= ((uint16_t)MOVETYPE_PROMR) << MOVEBITS_TYP_BITS;
                break;
            case 'b':
                *pmove |= ((uint16_t)MOVETYPE_PROMB) << MOVEBITS_TYP_BITS;
                break;
            case 'n':
                *pmove |= ((uint16_t)MOVETYPE_PROMN) << MOVEBITS_TYP_BITS;
                break;
            default:
                break;
            }
        }

        move_domove(&board, *pmove, &mademove);
    }
}

void uci_cmd_isready(void)
{
    board_findpieces(&board);
    move_findattacks(&board);
    move_findpins(&board);

    printf("readyok\n");
}

void uci_cmd_uci(void)
{
    printf("id name swall\n");
    printf("id author Henry Dunn\n");
    printf("uciok\n");
}

void uci_main(void)
{
    char line[MAX_INPUT];
    char *c;

    board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    while(1)
    {
        if(!fgets(line, sizeof(line), stdin))
            break;
        
        c = line;
        while(*c++ != '\n');
        *c = 0;

        if(c == line)
            continue;

        if(!strncmp(line, "uci", 3))
            uci_cmd_uci();
        else if(!strncmp(line, "isready", 7))
            uci_cmd_uci();
        else if(!strncmp(line, "position", 8))
            uci_cmd_position(line + 8);
        else if(!strncmp(line, "d", 1))
            board_print(&board);
            
    }

    /*
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

            move_alllegal(&board, &moves, false);
            
            if(!(pmove = move_findmove(&moves, move)))
            {
                printf("invalid move.\n");
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
                    continue;
                }
            }

            move_domove(&board, *pmove, &mademove);

            board_print(&board);

            brain_dobestmove(&board);
            board_print(&board);

            continue;
        }

        if(buf[0])
            printf("no matching command.\n");
    }
    */
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, ""); 
    setvbuf(stdout, NULL, _IONBF, 0);

    move_init();
    tests_movegen();

    uci_main();
    
    return 0;
}