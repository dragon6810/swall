#include "magic.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"

typedef enum
{
    MAGIC_ROOK=0,
    MAGIC_BISHOP,
    MAGIC_COUNT,
} magicpiece_e;

// one per square, per piece
typedef struct magicset_s
{
    bitboard_t mask;
    uint64_t magic, shift;
    uint64_t nboards;
    bitboard_t *boards;
} magicset_t;

magicset_t magicsets[MAGIC_COUNT][BOARD_AREA] = {};

// { file, rank }
int diroffsrf[DIR_COUNT][2] =
{
    {  1,  0 }, // DIR_E
    {  0,  1 }, // DIR_N
    { -1,  0 }, // DIR_W
    {  0, -1 }, // DIR_S
    {  1,  1 }, // DIR_NE
    { -1,  1 }, // DIR_NW
    { -1, -1 }, // DIR_SW
    {  1, -1 }, // DIR_SE
};

static bitboard_t magic_findmoves(magicpiece_e p, uint8_t pos, bitboard_t blockers)
{
    int i;
    dir_e d;

    dir_e start, end;
    bitboard_t mask;
    int square;

    if(p == MAGIC_ROOK)
    {
        start = DIR_E;
        end = DIR_S;
    }
    else
    {
        start = DIR_NE;
        end = DIR_SE;
    }

    mask = 0;
    for(d=start; d<=end; d++)
    {
        for(i=1, square=pos+diroffs[d]; i<=sweeptable[pos][d]; i++, square+=diroffs[d])
        {
            mask |= (uint64_t) 1 << square;
            if(blockers & ((uint64_t) 1 << square))
                break;
        }
    }

    return mask;
}

static bitboard_t magic_scattertomask(bitboard_t blockers, bitboard_t mask)
{
    bitboard_t dstbit;
    bitboard_t scattered;

    scattered = 0;
    while(mask)
    {
        dstbit = mask & -mask;
        if(blockers & 1)
            scattered |= dstbit;
        blockers >>= 1;
        mask ^= dstbit;
    }

    return scattered;
}

static void magic_genblockers(magicset_t* set, magicpiece_e p, uint8_t pos)
{
    uint64_t i;

    uint8_t n;
    bitboard_t mask, blockers;

    n = 0;
    mask = set->mask;
    while(mask)
    {
        mask &= mask - 1;
        n++;
    }

    set->nboards = (bitboard_t) 1 << n;
    set->boards = malloc(sizeof(bitboard_t) * set->nboards);

    for(i=0; i<set->nboards; i++)
    {
        blockers = magic_scattertomask(i, set->mask);
        set->boards[i] = magic_findmoves(p, pos, blockers);
    }
}

static bitboard_t magic_getborder(uint8_t pos)
{
    const bitboard_t sides[4] = { 0x00000000000000FF, 0x0101010101010101, 0x8080808080808080, 0xFF00000000000000, };

    int i;

    bitboard_t posmask, mask;

    posmask = (bitboard_t) 1 << pos;

    mask = 0;
    for(i=0; i<4; i++)
        if(!(posmask & sides[i]))
            mask |= sides[i];
    
    return mask;
}

static void magic_makeset(magicpiece_e piece, uint8_t pos)
{
    magicset_t *set;
    bitboard_t fullmask, border;

    set = &magicsets[piece][pos];

    fullmask = magic_findmoves(piece, pos, 0);
    border = magic_getborder(pos);

    set->mask = fullmask & ~border;

    magic_genblockers(set, piece, pos);
}

void magic_init(void)
{
    int i;
    magicpiece_e p;

    for(p=0; p<MAGIC_COUNT; p++)
        for(i=0; i<BOARD_AREA; i++)
            magic_makeset(p, i);
}

void magic_findmagic(void)
{
    printf("find magic\n");
}
