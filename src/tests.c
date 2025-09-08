#include "tests.h"

#include <time.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "move.h"

static int tests_movegen_r(board_t* board, int depth, move_t* move)
{
    moveset_t *cur;

    board_t newboard;
    moveset_t *set;
    int count;

    if(!depth)
        return 1;

    memcpy(&newboard, board, sizeof(board_t));

    if(move)
        move_domove(&newboard, *move);

    set = move_alllegal(&newboard);

    for(cur=set, count=0; cur; cur=cur->next)
        count += tests_movegen_r(&newboard, depth - 1, &cur->move);

    move_freeset(set);

    return count;
}

void tests_movegen(void)
{
    int i;

    board_t board;
    clock_t start, stop;
    int count;

    memset(&board, 0, sizeof(board_t));

    board_loadfen(&board, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board_findpieces(&board);
    move_findattacks(&board);
    move_findpins(&board);

    for(i=1; i<=5; i++)
    {
        start = clock();
        count = tests_movegen_r(&board, i, NULL);
        stop = clock();

        printf("depth %d: %d moves (%fms).\n", i, count, 
            (double) (stop - start) / CLOCKS_PER_SEC * 1000.0);

    }
}