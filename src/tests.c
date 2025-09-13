#include "tests.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "move.h"

int ncap, nenpas, ncastle, nprom, ncheck;

static int tests_movegen_r(board_t* board, int depth, move_t* move)
{
    moveset_t *cur;

    board_t newboard;
    moveset_t *set;
    int dst;
    int count;

    memcpy(&newboard, board, sizeof(board_t));

    if(move)
        dst = (*move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    if(!depth && move 
    && ((board->pboards[TEAM_BLACK][PIECE_NONE] | board->pboards[TEAM_WHITE][PIECE_NONE]) & (uint64_t) 1 << dst))
        ncap++;

    if(!depth && move && ((*move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_ENPAS)
        nenpas++;

    if(!depth && move && ((*move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_CASTLE)
        ncastle++;

    if(!depth && move && (((*move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMQ
    || ((*move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMR
    || ((*move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMB
    || ((*move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS) == MOVETYPE_PROMN))
        nprom++;
    
    if(move)
        move_domove(&newboard, *move);

    if(!depth && board->check[board->tomove])
        ncheck++;

    if(!depth)
        return 1;

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
        ncap = nenpas = ncastle = nprom = ncheck = 0;
    
        start = clock();
        count = tests_movegen_r(&board, i, NULL);
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