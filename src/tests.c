#include "tests.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "move.h"

int ncap, nenpas, ncastle, nprom, ncheck;

static int tests_movegen_r(board_t* board, int depth)
{
    moveset_t *cur;

    moveset_t *set;
    mademove_t mademove;
    int dst;
    int count;

    if(!depth)
        return 1;

    set = move_alllegal(board);

    for(cur=set, count=0; cur; cur=cur->next)
    {
        move_domove(board, cur->move, &mademove);

        dst = (cur->move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
        if(depth == 1 && (board->pboards[TEAM_BLACK][PIECE_NONE] | board->pboards[TEAM_WHITE][PIECE_NONE]) & (uint64_t) 1 << dst)
            ncap++;

        if(depth == 1 && ((cur->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_ENPAS)
        {
            ncap++;
            nenpas++;
        }

        if(depth == 1 && ((cur->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_CASTLE)
            ncastle++;

        if(depth == 1 && (((cur->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMQ
        || ((cur->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMR
        || ((cur->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMB
        || ((cur->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMN))
            nprom++;

        if(depth == 1 && board->check[board->tomove])
            ncheck++;

        count += tests_movegen_r(board, depth - 1);
        move_undomove(board, &mademove);
    }

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

    board_loadfen(&board, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ");
    board_findpieces(&board);
    move_findattacks(&board);
    move_findpins(&board);

    for(i=1; i<=4; i++)
    {
        ncap = nenpas = ncastle = nprom = ncheck = 0;
    
        start = clock();
        count = tests_movegen_r(&board, i);
        stop = clock();

        printf("depth %d: %d moves (%fms).\n", i, count, 
            (double) (stop - start) / CLOCKS_PER_SEC * 1000.0);
        printf("\tcaptures: %d.\n", ncap);
        printf("\ten passants: %d.\n", nenpas);
        printf("\tcastles: %d.\n", ncastle);
        printf("\tpromotions: %d.\n", nprom);
        printf("\tchecks: %d.\n", ncheck);
    }

    printf("time spent generating moves: %lfms (%d%%).\n", 
        msmovegen, (int) (msmovegen / (msmovegen + msmove) * 100));
    printf("time spent making moves: %lfms (%d%%).\n", 
        msmove, (int) (msmove / (msmovegen + msmove) * 100));
}