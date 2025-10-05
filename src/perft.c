#include "perft.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "move.h"

// are they in wrong order?
static bool perft_moveorder(move_t moves[2])
{
    int i;

    char strs[2][MAX_LONGALG];

    for(i=0; i<2; i++)
        move_tolongalg(moves[i], strs[i]);

    return strcmp(strs[0], strs[1]) > 0;
}

static void perft_order(moveset_t* moves)
{
    int i;

    bool swapped;
    move_t pair[2];

    do
    {
        for(i=1, swapped=false; i<moves->count; i++)
        {
            pair[0] = moves->moves[i-1];
            pair[1] = moves->moves[i];
            if(perft_moveorder(pair))
            {
                swapped = true;
                moves->moves[i] ^= moves->moves[i-1];
                moves->moves[i-1] ^= moves->moves[i];
                moves->moves[i] ^= moves->moves[i-1];
            }
        }
    }
    while (swapped);
}

uint64_t nnodes;
clock_t start;

int perft_r(board_t* board, int depthfromroot, int depth)
{
    int i;

    moveset_t moves;
    int count, total;
    mademove_t made;
    char str[MAX_LONGALG];

    nnodes++;

    if(!depthfromroot)
    {
        nnodes = 0;
        start = clock();
    }

    if(!depth)
        return 1;

    move_gensetup(board);
    move_alllegal(board, &moves, false);
    if(!depthfromroot)
        perft_order(&moves);

    for(i=0, total=0; i<moves.count; i++)
    {
        move_make(board, moves.moves[i], &made);
        move_tolongalg(moves.moves[i], str);
        total += count = perft_r(board, depthfromroot + 1, depth - 1);
        move_unmake(board, &made);

        if(depthfromroot)
            continue;

        move_tolongalg(moves.moves[i], str);
        printf("%s: %d\n", str, count);
    }

    if(!depthfromroot)
        printf("info nps %llu\n", (uint64_t) ((double) nnodes / ((double) (clock() - start) / CLOCKS_PER_SEC)));

    return total;
}

void perft(board_t* board, int depth)
{
    int n;

    n = perft_r(board, 0, depth);
    printf("\nNodes searched:%d\n\n", n);
}