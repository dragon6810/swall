#include "zobrist.h"

#include <stdlib.h>

#include "board.h"

uint64_t randa, randb, randc, randd;
uint64_t sqrhashes[PIECE_COUNT][BOARD_AREA], tomovehash, castlehash[16], enpashash[BOARD_LEN];

#define rot(x,k) (((x)<<(k))|((x)>>(64-(k))))
static uint64_t zobrist_rand(void)
{
    uint64_t e;

    e = randa - rot(randb, 7);
    randa = randb ^ rot(randc, 13);
    randb = randc + rot(randd, 37);
    randc = randd + e;
    randd = e + randa;
    return randd;
}
#undef rot

static void zobrist_initrand(void)
{
    const uint64_t seed = 5113671424;
    int i;

    randa = 0xF1EA5EED;
    randb = randc = randd = seed;
    for(i=0; i<20; i++)
        zobrist_rand();
}

void zobrist_init(void)
{
    int i, j;

    zobrist_initrand();

    for(i=0; i<PIECE_COUNT; i++)
        for(j=0; j<BOARD_AREA; j++)
            sqrhashes[i][j] = zobrist_rand();
    tomovehash = zobrist_rand();
    for(i=0; i<16; i++)
        castlehash[i] = zobrist_rand();
    for(i=0; i<BOARD_LEN; i++)
        enpashash[i] = zobrist_rand();
}

uint64_t zobrist_hash(board_t* board)
{
    int t, p;

    uint64_t hash;
    piece_e type;

    hash = 0;
    for(t=0; t<TEAM_COUNT; t++)
    {
        for(p=0; p<board->npiece[t]; p++)
        {
            for(type=PIECE_KING; type<PIECE_COUNT; type++)
                if(board->pboards[t][type] & (uint64_t) 1 << board->ptable[t][p])
                    break;

            hash ^= sqrhashes[type][board->ptable[t][p]];
        }
    }

    return hash;
}
