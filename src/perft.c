#include "perft.h"

#include <stdio.h>

#include "move.h"

void perft_order(moveset_t* moves, int counts[MAX_MOVE])
{
    int i, j;

    move_t bestmv;
    int bestidx;

    for(i=0; i<moves->count; i++)
    {
        bestidx = i;
        bestmv = moves->moves[i] & ~MOVEBITS_TYP_MASK;

        for(j=i+1; j<moves->count; j++)
        {
            if((moves->moves[j] & ~MOVEBITS_TYP_MASK) < bestmv)
            {
                bestmv = moves->moves[i] & ~MOVEBITS_TYP_MASK;
                bestidx = j;
            }
        }

        if(bestidx == i)
            continue;

        moves->moves[i] ^= moves->moves[bestidx];
        moves->moves[bestidx] ^= moves->moves[i];
        moves->moves[i] ^= moves->moves[bestidx];

        counts[i] ^= counts[bestidx];
        counts[bestidx] ^= counts[i];
        counts[i] ^= counts[bestidx];
    }
}

int perft_r(board_t* board, int depthfromroot, int depth)
{
    int i;

    moveset_t moves;
    int counts[MAX_MOVE], total;
    mademove_t made;
    char str[MAX_LONGALG];

    if(!depth)
        return 1;

    move_alllegal(board, &moves, false);
    for(i=0, total=0; i<moves.count; i++)
    {
        move_make(board, moves.moves[i], &made);
        move_tolongalg(moves.moves[i], str);
        total += counts[i] = perft_r(board, depthfromroot + 1, depth - 1);
        move_unmake(board, &made);
    }

    if(depthfromroot)
        return total;

    perft_order(&moves, counts);
    for(i=0; i<moves.count; i++)
    {
        move_tolongalg(moves.moves[i], str);
        printf("%s: %d\n", str, counts[i]);
    }

    return total;
}

void perft(board_t* board, int depth)
{
    int n;

    n = perft_r(board, 0, depth);
    printf("\nNodes searched:%d\n\n", n);
}