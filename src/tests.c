#include "tests.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "move.h"

double msmovegen = 0, msmove = 0, msundo = 0;

static int tests_movegen_r(board_t* board, int depth)
{
    int i;

    moveset_t set;
    mademove_t mademove;
    int count, nleaves;
    clock_t start;

    if(!depth)
        return 1;

    start = clock();
    move_alllegal(board, &set, false);
    msmovegen += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;

    if(depth == 1)
        return set.count;

    for(i=count=0; i<set.count; i++)
    {
        start = clock();
        move_domove(board, set.moves[i], &mademove);
        msmove += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;
        
        nleaves = tests_movegen_r(board, depth - 1);

        start = clock();
        move_undomove(board, &mademove);
        msundo += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;

        count += nleaves;
    }

    return count;
}

void tests_movegen(void)
{
    int i;

    board_t board;
    clock_t start, stop;
    int count;

    memset(&board, 0, sizeof(board_t));

    //board_loadfen(&board, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    board_loadfen(&board, "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
    board_findpieces(&board);
    move_findattacks(&board);
    move_findpins(&board);

    for(i=1; i<=4; i++)
    {
        start = clock();
        count = tests_movegen_r(&board, i);
        stop = clock();

        printf("depth %d: %d moves (%fms).\n", i, count, 
            (double) (stop - start) / CLOCKS_PER_SEC * 1000.0);
    }

    printf("time spent generating moves: %lfms (%d%%).\n", 
        msmovegen, (int) (msmovegen / (msmovegen + msmove + msundo) * 100));
    printf("time spent making moves: %lfms (%d%%).\n", 
        msmove, (int) (msmove / (msmovegen + msmove + msundo) * 100));
    printf("time spent unmaking moves: %lfms (%d%%).\n", 
        msundo, (int) (msundo / (msmovegen + msmove + msundo) * 100));
}