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

const bitboard_t magics[MAGIC_COUNT][BOARD_AREA] =
{
  { // rook
    0x0080008040001120,0x0040004410002000,0x4480085000802004,0x4080048008001000,0x1A00080201201084,0x0200090200100844,0x2080048051000600,0x0A0000408400A512,
    0x2000800220C01080,0x0186400420035002,0x00850011C1006002,0x0701801002880081,0x4000800800828400,0x4800808084000200,0x0081008100060004,0x0085000051000482,
    0x0801208000804000,0x0100820021004200,0x0008220052008040,0x2000120009420020,0x0011010048000490,0x0042008044008002,0x3000010100020004,0xC0110200009C0141,
    0x0001800080204000,0x08045001C0002008,0x2005410100102004,0x8940082100100300,0x2008040080080080,0x00000C0801201040,0x0000040101000200,0x4912942200044089,
    0x0450400021800088,0x1080412008401000,0x000241001B002000,0x0122080082801000,0x0180800C01800800,0x8224800200800400,0x1400020001010004,0x0001000445000082,
    0x2480002009424001,0x0AC008100120A000,0x3001002001110041,0x01004049A2020010,0x0018002040140400,0x2002004410420009,0x1881000A00010004,0x0101004100820004,
    0x8080800040002180,0x0100310040058100,0x0060002880100080,0x5810080084100180,0x0880080004008280,0x0000240080120080,0x0108100806010400,0x80A180034B000080,
    0x4085044020108005,0x302100A080400A31,0x102042200A001082,0x0312210004081001,0x00220020084410C6,0x4A2700080C002205,0x4102002804008102,0x0840005024008102,
  },
  { // bishop
    0x0020081001024210,0x8010900480838409,0x0012080201221100,0x04844102A0023004,0x180202100005E801,0x0244900420009000,0x20218808080C0804,0x4003040202020200,
    0x10000C2028112102,0x0010300148050452,0xA1000C010C010200,0x80C8040502091040,0x9000041460081020,0x4600108220200504,0x30022104420A4100,0x003204A407045080,
    0x0822C00820010220,0x0260000484808608,0x0008021B00410200,0x000200602A004000,0x40010028200828C0,0x012080010080C020,0x4402001401820800,0x0002400600420800,
    0x1010400010020208,0x0530250808080080,0x22008800100120A6,0x400340400C010200,0x000A002082008051,0x0441020010405010,0x08060C08408C1100,0x8024102025091100,
    0x0196424000101128,0x00040402010C3004,0x8000282800500080,0x0100040400080120,0x0008146400804100,0x00E1004602010100,0x0004044041040104,0x8000820148220101,
    0x0882080554004000,0x0054210908005018,0x1031031082011000,0x00C4002024208806,0x1210401048800500,0x10420850110028A0,0x2042A40102010410,0x00100120C4828300,
    0x0018440208410001,0x0444840508230600,0x9000A08401884000,0x80101810840C00C0,0x0001442012049000,0x000A410408048000,0x8108200420820010,0x0082100E008100A0,
    0x0402021202020200,0x008600D108080242,0x1204408046009000,0x8010008000618800,0x008C040008210100,0xE428082004291200,0x000040840802004E,0x3020091002008021,
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

    printf("{\n");
    for(p=0; p<MAGIC_COUNT; p++)
    {
        printf("  { // %s\n    ", p == MAGIC_ROOK ? "rook" : "bishop");
        for(square=0; square<BOARD_AREA; square++)
        {
            while(1)
            {
                magic = magic_rand() & magic_rand() & magic_rand();
                if(!magic_ismagic(p, square, magic))
                    continue;

                printf("0x%016llX,", magic);
                if(square % BOARD_LEN == BOARD_LEN - 1)
                {
                    printf("\n");
                    if(square < BOARD_AREA - 1)
                        printf("    ");
                }
                break;
            }
        }
        printf("  },\n");
    }
    printf("}\n");
}
