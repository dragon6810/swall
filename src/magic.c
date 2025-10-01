#include "magic.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    uint64_t magic;
    uint8_t shift;

    bitboard_t *boards;
} magicset_t;

magicset_t magicsets[MAGIC_COUNT][BOARD_AREA] = {};

// { file, rank }
const int diroffsrf[DIR_COUNT][2] =
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

#define MAX_MAGIC_BITS 12

const int nrelevent[MAGIC_COUNT][BOARD_AREA] =
{
    // rook
    {
        12, 11, 11, 11, 11, 11, 11, 12,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        11, 10, 10, 10, 10, 10, 10, 11,
        12, 11, 11, 11, 11, 11, 11, 12,
    },
    // bishop
    {
        6, 5, 5, 5, 5, 5, 5, 6,
        5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 7, 7, 7, 7, 5, 5,
        5, 5, 7, 9, 9, 7, 5, 5,
        5, 5, 7, 9, 9, 7, 5, 5,
        5, 5, 7, 7, 7, 7, 5 ,5,
        5 ,5 ,5 ,5 ,5 ,5 ,5, 5,
        6 ,5 ,5 ,5 ,5 ,5 ,5, 6,
    },
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
    int nboards;
    bitboard_t mask, blockers;

    n = 0;
    mask = set->mask;
    while(mask)
    {
        mask &= mask - 1;
        n++;
    }

    nboards = (bitboard_t) 1 << n;
    set->boards = malloc(sizeof(bitboard_t) * nboards);

    for(i=0; i<nboards; i++)
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
    bitboard_t fullmask, border, mask;
    int nbits;

    set = &magicsets[piece][pos];

    fullmask = magic_findmoves(piece, pos, 0);
    border = magic_getborder(pos);

    set->mask = mask = fullmask & ~border;

    // sanity check
    nbits = 0;
    while (mask)
    {
        mask &= (mask - 1);
        nbits++;
    }
    assert(nbits == nrelevent[piece][pos]);

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

bitboard_t seen[(uint64_t) 1 << MAX_MAGIC_BITS] = {};

static bool magic_ismagic(magicpiece_e p, uint8_t pos, bitboard_t magic)
{
    bitboard_t i;

    magicset_t *set;
    int nboards;
    uint64_t idx;
    bitboard_t blockers, moves;

    set = &magicsets[p][pos];

    nboards = (bitboard_t) 1 << nrelevent[p][pos];
    memset(seen, 0, sizeof(bitboard_t) * nboards);

    for(i=0; i<nboards; i++)
    {
        blockers = magic_scattertomask(i, set->mask);
        idx = (blockers * magic) >> (BOARD_AREA - nrelevent[p][pos]);
        
        if(idx >= nboards)
            return false;

        moves = magic_findmoves(p, pos, blockers);

        if(seen[idx] && seen[idx] != moves)
            return false;
        seen[idx] = moves;
    }

    return true;
}

uint64_t randa, randb, randc, randd;

#define rot(x,k) (((x)<<(k))|((x)>>(64-(k))))
static uint64_t magic_rand(void)
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

static void magic_initrand(void)
{
    const uint64_t seed = 5113671424;
    int i;

    randa = 0xF1EA5EED;
    randb = randc = randd = time(NULL) + seed;
    for(i=0; i<20; i++)
        magic_rand();
}

void magic_findmagic(void)
{
    magicpiece_e p;
    int square;
    uint64_t magic;

    magic_initrand();

    for(p=0; p<MAGIC_COUNT; p++)
    {
        for(square=0; square<BOARD_AREA; square++)
        {
            while(1)
            {
                magic = magic_rand() & magic_rand() & magic_rand();
                if(!magic_ismagic(p, square, magic))
                    continue;

                printf("%d/%d (0x%016llX)\n", square + p * BOARD_AREA + 1, BOARD_AREA * MAGIC_COUNT, magic);
                break;
            }
        }
    }
}
