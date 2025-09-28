#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "board.h"
#include "book.h"
#include "brain.h"
#include "move.h"
#include "perft.h"
#include "zobrist.h"

#define MAX_INPUT 4096

board_t board = {};

int tryparsemove(const char* str)
{
    int i;

    const char* start;
    char move[5];
    moveset_t moves;
    mademove_t mademove;
    move_t bmove, *pmove;

    start = str;

    while(*str && *str <= 32)
        str++;

    if(*str < 'a' || *str > 'h') return 0;
    move[0] = *str++;
    if(*str < '1' || *str > '8') return 0;
    move[1] = *str++;
    if(*str < 'a' || *str > 'h') return 0;
    move[2] = *str++;
    if(*str < '1' || *str > '8') return 0;
    move[3] = *str++;

    move[4] = 0;
    if(*str == 'q' || *str == 'r' || *str == 'b' || *str == 'n')
        move[4] = *str++;

    bmove = (move[0] - 'a') + (move[1] - '1') * BOARD_LEN;
    bmove |= (uint16_t) ((move[2] - 'a') + (move[3] - '1') * BOARD_LEN) << MOVEBITS_DST_BITS;

    move_alllegal(&board, &moves, false);
    for(i=0, pmove=NULL; i<moves.count; i++)
    {
        if((moves.moves[i] & ~MOVEBITS_TYP_MASK) != (bmove & ~MOVEBITS_TYP_MASK))
            continue;

        if((moves.moves[i] >> MOVEBITS_TYP_BITS) >= MOVETYPE_PROMQ 
        && (moves.moves[i] >> MOVEBITS_TYP_BITS) <= MOVETYPE_PROMN)
        {
            // this is a scummy way to do this
            if((moves.moves[i] >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMQ && move[4] == 'q');
            else if((moves.moves[i] >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMR && move[4] == 'r');
            else if((moves.moves[i] >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMB && move[4] == 'b');
            else if((moves.moves[i] >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMN && move[4] == 'n');
            else
                continue;
        }

        pmove = &moves.moves[i];
        break;
    }

    if(!pmove)
        return 0;

    move_make(&board, *pmove, &mademove);

    return str - start;
}

void uci_cmd_perft(const char* args)
{
    char depthstr[MAX_INPUT];
    const char *depthend;
    int depth;

    while(*args && *args <= 32)
        args++;

    depthend = args;
    while(*depthend && *depthend >= '0' && *depthend <= '9')
        depthend++;
    
    memcpy(depthstr, args, depthend - args);
    depthstr[depthend - args] = 0;

    depth = atoi(args);
    if(depth <= 0)
        return;

    perft(&board, depth);
}

void uci_cmd_go(const char* args)
{
    move_t move;
    char str[MAX_LONGALG];
    char arg[MAX_INPUT];
    const char *argend;
    int times[TEAM_COUNT], searchtime;

    while(*args && *args <= 32)
        args++;
    
    if(!strncmp(args, "perft", 5))
    {
        uci_cmd_perft(args + 5);
        return;
    }

    times[TEAM_WHITE] = times[TEAM_BLACK] = searchtime = 0;
    while(1)
    {
        while(*args && *args <= 32)
            args++;

        if(!*args)
            break;

        if(!strncmp(args, "wtime", 5))
        {
            args += 5;
            while(*args && *args <= 32)
                args++;

            argend = args;
            while(*argend && *argend >= '0' && *argend <= '9')
                argend++;
            
            memcpy(arg, args, argend - args);
            arg[argend - args] = 0;

            times[TEAM_WHITE] = atoi(arg);
            args += argend - args;

            if(times[TEAM_WHITE] <= 0)
            {
                times[TEAM_WHITE] = 0;
                continue;
            }

            continue;
        }

        else if(!strncmp(args, "btime", 5))
        {
            args += 5;
            while(*args && *args <= 32)
                args++;

            argend = args;
            while(*argend && *argend >= '0' && *argend <= '9')
                argend++;
            
            memcpy(arg, args, argend - args);
            arg[argend - args] = 0;

            times[TEAM_BLACK] = atoi(arg);
            args += argend - args;

            if(times[TEAM_BLACK] <= 0)
            {
                times[TEAM_BLACK] = 0;
                continue;
            }

            continue;
        }

        else if(!strncmp(args, "movetime", 8))
        {
            args += 8;
            while(*args && *args <= 32)
                args++;

            argend = args;
            while(*argend && *argend >= '0' && *argend <= '9')
                argend++;
            
            memcpy(arg, args, argend - args);
            arg[argend - args] = 0;

            searchtime = atoi(arg);
            args += argend - args;

            if(searchtime <= 0)
            {
                searchtime = 0;
                continue;
            }

            continue;
        }

        else
            break;
    }

    if(!searchtime)
    {
        searchtime = 100;
        if(times[board.tomove])
            searchtime = times[board.tomove] / 25;
    }

    move = brain_runsearch(&board, searchtime);
    move_tolongalg(move, str);

    printf("bestmove %s\n", str);
}

void uci_cmd_position(const char* args)
{
    int res, movew;

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

        res = board_loadfen(&board, args);
        if(res < 0)
            return;
        args += res;
    }
    else
    {
        board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }

    board_update(&board);

    transpose_alloc(&board.ttable, 64 * 1024);
    transpose_alloc(&board.ttableold, 64 * 1024);

    while(*args && *args <= 32)
            args++;

    if(strncmp(args, "moves", 5))
        return;
    args += 5;

    while(1)
    {
        movew = tryparsemove(args);
        if(!movew)
            break;

        args += movew;
        while(*args && *args <= 32)
            args++;
    }
}

void uci_cmd_isready(void)
{
    board_update(&board);

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
    board_update(&board);
    transpose_alloc(&board.ttable, 64 * 1024);
    transpose_alloc(&board.ttableold, 64 * 1024);

    while(1)
    {
        if(!fgets(line, sizeof(line), stdin))
            break;
        
        c = line;
        while(*c++ != '\n');
        *c = 0;

        if(c == line)
            continue;

        if(tryparsemove(line))
            continue;
        else if(!strncmp(line, "uci", 3))
            uci_cmd_uci();
        else if(!strncmp(line, "isready", 7))
            uci_cmd_isready();
        else if(!strncmp(line, "position", 8))
            uci_cmd_position(line + 8);
        else if(!strncmp(line, "d", 1))
            board_print(&board);
        else if(!strncmp(line, "go", 2))
            uci_cmd_go(line + 2);
        else if(!strncmp(line, "quit", 4))
            break;
    }

    transpose_free(&board.ttable);
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, ""); 
    setvbuf(stdout, NULL, _IONBF, 0);

    srand(time(NULL));

    move_init();
    book_load("baron30.bin");
    //tests_movegen();

    uci_main();
    
    return 0;
}