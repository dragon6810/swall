#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "brain.h"
#include "move.h"
#include "tests.h"
#include "zobrist.h"

#define MAX_INPUT 512

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

void uci_cmd_go(const char* args)
{
    move_t move;
    int src, dst;
    char str[5];

    brain_search(&board, INT16_MIN + 1, INT16_MAX, 5, &move);

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;

    str[0] = 'a' + src % BOARD_LEN;
    str[1] = '1' + src / BOARD_LEN;
    str[2] = 'a' + dst % BOARD_LEN;
    str[3] = '1' + dst / BOARD_LEN;

    switch((move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS)
    {
    case MOVETYPE_PROMQ:
        str[4] = 'q';
        break;
    case MOVETYPE_PROMR:
        str[4] = 'r';
        break;
    case MOVETYPE_PROMB:
        str[4] = 'b';
        break;
    case MOVETYPE_PROMN:
        str[4] = 'n';
        break;
    default:
        str[4] = 0;
    }

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

    board_findpieces(&board);
    move_findattacks(&board);
    move_findpins(&board);
    board_findcheck(&board);

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

    printf("zobrist: %16llx\n", zobrist_hash(&board));
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
    board_findpieces(&board);
    move_findattacks(&board);
    move_findpins(&board);
    board_findcheck(&board);
    zobrist_init();

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
    }
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, ""); 
    setvbuf(stdout, NULL, _IONBF, 0);

    move_init();
    //tests_movegen();

    uci_main();
    
    return 0;
}