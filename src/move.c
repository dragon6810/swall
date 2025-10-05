#include "move.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "magic.h"

int sweeptable[BOARD_AREA][DIR_COUNT];
bitboard_t kingatk[BOARD_AREA];
bitboard_t knightatk[BOARD_AREA];
bitboard_t pawnatk[TEAM_COUNT][BOARD_AREA];
bitboard_t pawnpush[TEAM_COUNT][BOARD_AREA];
bitboard_t pawndbl[TEAM_COUNT][BOARD_AREA];
bitboard_t emptysweeps[BOARD_AREA][DIR_COUNT];

void move_tolongalg(move_t move, char str[MAX_LONGALG])
{
    int src, dst;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;

    str[0] = 'a' + src % BOARD_LEN;
    str[1] = '1' + src / BOARD_LEN;
    str[2] = 'a' + dst % BOARD_LEN;
    str[3] = '1' + dst / BOARD_LEN;

    switch((move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS)
    {
    case MOVETYPE_PROMQ:
        str[4] = 'q';
        break;
    case MOVETYPE_PROMR:
        str[4] = 'r';
        break;
    case MOVETYPE_PROMB:
        str[4] = 'b';
        break;
    case MOVETYPE_PROMN:
        str[4] = 'n';
        break;
    default:
        str[4] = 0;
    }

    str[5] = 0;
}

static inline void move_pawnatk(board_t* restrict board, uint8_t src, team_e team)
{
    board->attacks |= pawnatk[team][src];
}

static inline void move_knightatk(board_t* restrict board, uint8_t src, team_e team)
{
    board->attacks |= knightatk[src];
}

static inline void move_bishopatk(board_t* restrict board, uint8_t src, team_e team)
{
    bitboard_t block;

    block = (board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) ^ board->pboards[!team][PIECE_KING];
    board->attacks |= magic_lookup(MAGIC_BISHOP, src, block);
}

static inline void move_rookatk(board_t* restrict board, uint8_t src, team_e team)
{
    bitboard_t block;

    block = (board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) ^ board->pboards[!team][PIECE_KING];
    board->attacks |= magic_lookup(MAGIC_ROOK, src, block);
}

static inline void move_queenatk(board_t* restrict board, uint8_t src, team_e team)
{
    bitboard_t block;

    block = (board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) ^ board->pboards[!team][PIECE_KING];
    board->attacks |= magic_lookup(MAGIC_ROOK, src, block) | magic_lookup(MAGIC_BISHOP, src, block);
}

static inline void move_kingatk(board_t* restrict board, uint8_t src, team_e team)
{
    board->attacks |= kingatk[src];
}

void move_findattacks(board_t* restrict board)
{
    piece_e p;

    bitboard_t bb;
    team_e team;
    int square;

    team = !board->tomove;

    board->attacks = 0;
    for(p=PIECE_KING; p<PIECE_COUNT; p++)
    {
        bb = board->pboards[team][p];
        while(bb)
        {
            square = __builtin_ctzll(bb);
            bb &= bb - 1;

            switch(p)
            {
            case PIECE_KING:
                move_kingatk(board, square, team);
                break;
            case PIECE_QUEEN:
                move_queenatk(board, square, team);
                break;
            case PIECE_ROOK:
                move_rookatk(board, square, team);
                break;
            case PIECE_BISHOP:
                move_bishopatk(board, square, team);
                break;
            case PIECE_KNIGHT:
                move_knightatk(board, square, team);
                break;
            case PIECE_PAWN:
                move_pawnatk(board, square, team);
                break;
            default:
                break;
            }
        }
    }
}

static inline void move_pawnthreat(board_t* restrict board, uint8_t kingpos)
{
    team_e team;
    bitboard_t mask;

    if(board->dblcheck)
        return;

    team = board->tomove;

    mask = pawnatk[team][kingpos] & board->pboards[!team][PIECE_PAWN];

    if(!mask)
        return;

    if(board->isthreat)
    {
        board->dblcheck = true;
        return;
    }

    board->isthreat = true;
    board->threat = mask;
}

static inline void move_knightthreat(board_t* restrict board, uint8_t kingpos)
{
    team_e team;
    bitboard_t mask;

    if(board->dblcheck)
        return;

    team = board->tomove;

    mask = knightatk[kingpos] & board->pboards[!team][PIECE_KNIGHT];

    if(!mask)
        return;

    if(board->isthreat)
    {
        board->dblcheck = true;
        return;
    }

    board->isthreat = true;
    board->threat = mask;
}

void move_findpins(board_t* restrict board)
{
    int i;
    dir_e dir;

    team_e team;
    uint8_t kingpos, nblockers;
    bitboard_t queenmask, rookmask, bishopmask, pinnermask, moves, sweepmask, blockermask;

    board->isthreat = board->dblcheck = false;
    for(i=0; i<BOARD_AREA; i++)
        board->pinmasks[i] = UINT64_MAX;

    team = board->tomove;
    kingpos = __builtin_ctzll(board->pboards[team][PIECE_KING]);
    queenmask = board->pboards[!team][PIECE_QUEEN];
    rookmask = board->pboards[!team][PIECE_ROOK];
    bishopmask = board->pboards[!team][PIECE_BISHOP];
    pinnermask = queenmask | rookmask | bishopmask;

    move_knightthreat(board, kingpos);
    move_pawnthreat(board, kingpos);

    moves = 0;
    moves |= magic_lookup(MAGIC_ROOK, kingpos, board->pboards[!team][PIECE_NONE]);
    moves |= magic_lookup(MAGIC_BISHOP, kingpos, board->pboards[!team][PIECE_NONE]);

    if(!(moves & pinnermask))
        return;

    for(dir=0; dir<DIR_COUNT; dir++)
    {
        sweepmask = emptysweeps[kingpos][dir] & moves;

        if(dir < DIR_NE && !(sweepmask & rookmask) && !(sweepmask & queenmask))
            continue;
        if(dir >= DIR_NE && !(sweepmask & bishopmask) && !(sweepmask & queenmask))
            continue;

        // we actually hit a queen, rook, or bishop, but we could have multiple or no pieces in the way
        blockermask = sweepmask & board->pboards[team][PIECE_NONE];
        nblockers = __builtin_popcountll(blockermask);

        if(!nblockers)
        {
            if(board->isthreat)
            {
                board->dblcheck = true;
                continue;
            }

            board->isthreat = true;
            board->threat = sweepmask;
            continue;
        }

        if(nblockers != 1)
            continue;

        board->pinmasks[__builtin_ctzll(blockermask)] = sweepmask;
    }
}

static inline void move_bitboardtomoves(board_t* restrict board, moveset_t* restrict set, uint8_t src, bitboard_t moves)
{
    uint8_t dst;
    move_t move;

    while(moves)
    {
        dst = __builtin_ctzll(moves);
        moves &= moves - 1;

        move = src;
        move |= (move_t) dst << MOVEBITS_DST_BITS;
        set->moves[set->count++] = move;
    }
}

static inline bool move_enpaslegal(board_t* restrict board, uint8_t src)
{
    team_e team;
    uint8_t kingpos, cappawn;
    bitboard_t rookatk, bishopatk, blockers;

    team = board->tomove;
    cappawn = board->enpas + PAWN_OFFS(!team);
    kingpos = __builtin_ctzll(board->pboards[team][PIECE_KING]);

    blockers = (board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);
    blockers ^= (bitboard_t) 1 << src;
    blockers ^= (bitboard_t) 1 << cappawn;
    blockers ^= (bitboard_t) 1 << board->enpas;

    rookatk = magic_lookup(MAGIC_ROOK, kingpos, blockers);
    if(rookatk & (board->pboards[!team][PIECE_ROOK] | board->pboards[!team][PIECE_QUEEN]))
        return false;

    bishopatk = magic_lookup(MAGIC_BISHOP, kingpos, blockers);
    if(bishopatk & (board->pboards[!team][PIECE_BISHOP] | board->pboards[!team][PIECE_QUEEN]))
        return false;

    return true;
}

static inline void move_pawnmoves(moveset_t* restrict set, board_t* restrict board, uint8_t src, team_e team, bool caponly)
{
    const bitboard_t promotionmask = 0xFF000000000000FF;

    int i;

    uint8_t dst;
    int starttype, stoptype;
    move_t move;
    bitboard_t moves;

    moves = 0;
    if(!caponly)
    {
        moves |= pawnpush[team][src] & ~(board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);
        if(moves)
            moves |= pawndbl[team][src] & ~(board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);
    }
    moves |= pawnatk[team][src] & board->pboards[!team][PIECE_NONE];

    if(board->isthreat)
        moves &= board->threat;
    moves &= board->pinmasks[src];

    starttype = stoptype = MOVETYPE_DEFAULT;
    if(moves & promotionmask)
    {
        starttype = MOVETYPE_PROMQ;
        stoptype = MOVETYPE_PROMN;
    }

    while(moves)
    {
        dst = __builtin_ctzll(moves);
        moves &= moves - 1;

        for(i=starttype; i<=stoptype; i++)
        {
            move = src;
            move |= (move_t) dst << MOVEBITS_DST_BITS;
            move |= (move_t) i << MOVEBITS_TYP_BITS;
            set->moves[set->count++] = move;
        }
    }

    if(board->enpas == 0xFF)
        return;

    moves = pawnatk[team][src] & ((bitboard_t) 1 << board->enpas);
    if(!moves)
        return;

    if(board->isthreat)
        moves &= board->threat;
    moves &= board->pinmasks[src];

    if(!moves)
        return;

    if(!move_enpaslegal(board, src))
        return;

    move = src;
    move |= (move_t) board->enpas << MOVEBITS_DST_BITS;
    move |= (move_t) MOVETYPE_ENPAS << MOVEBITS_TYP_BITS;
    set->moves[set->count++] = move;
}

static inline void move_knightmoves(moveset_t* restrict set, board_t* restrict board, uint8_t src, team_e team, bool caponly)
{
    bitboard_t moves;

    moves = knightatk[src];
    if(caponly)
        moves &= board->pboards[!team][PIECE_NONE];
    moves &= ~board->pboards[team][PIECE_NONE];

    if(board->isthreat)
        moves &= board->threat;
    moves &= board->pinmasks[src];

    move_bitboardtomoves(board, set, src, moves);
}

static inline bitboard_t move_getslideratk(board_t* restrict board, magicpiece_e type, uint8_t src, bool caponly)
{
    team_e team;
    bitboard_t moves;

    team = board->sqrs[src] >> SQUARE_BITS_TEAM;

    moves = magic_lookup(type, src, board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);
    if(caponly)
        moves &= board->pboards[!team][PIECE_NONE];
    moves &= ~board->pboards[team][PIECE_NONE];
    
    if(board->isthreat)
        moves &= board->threat;
    moves &= board->pinmasks[src];

    return moves;
}

static inline void move_bishopmoves(moveset_t* restrict set, board_t* restrict board, uint8_t src, team_e team, bool caponly)
{
    bitboard_t moves;

    moves = move_getslideratk(board, MAGIC_BISHOP, src, caponly);
    move_bitboardtomoves(board, set, src, moves);
}

static inline void move_rookmoves(moveset_t* restrict set, board_t* restrict board, uint8_t src, team_e team, bool caponly)
{
    bitboard_t moves;

    moves = move_getslideratk(board, MAGIC_ROOK, src, caponly);
    move_bitboardtomoves(board, set, src, moves);
}

static inline void move_queenmoves(moveset_t* restrict set, board_t* restrict board, uint8_t src, team_e team, bool caponly)
{
    bitboard_t moves;

    moves = move_getslideratk(board, MAGIC_ROOK, src, caponly) | move_getslideratk(board, MAGIC_BISHOP, src, caponly);
    move_bitboardtomoves(board, set, src, moves);
}

static inline void move_kingmoves(moveset_t* restrict set, board_t* restrict board, uint8_t src, team_e team, bool caponly)
{
    uint8_t dst;
    bitboard_t moves;
    move_t move;
    bitboard_t travelmask, allpiece;

    moves = kingatk[src];
    moves &= ~board->pboards[team][PIECE_NONE];
    moves &= ~board->attacks;
    if(caponly)
        moves &= board->pboards[!team][PIECE_NONE];

    move_bitboardtomoves(board, set, src, moves);

    if(caponly || board->check)
        return;

    allpiece = board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE];
    
    dst = src + 2;
    travelmask = (uint64_t) 1 << (src + 1) | (uint64_t) 1 << dst;
    if(board->kcastle[team] && !(allpiece & travelmask) && !(board->attacks & travelmask))
    {
        move = src;
        move |= (move_t) dst << MOVEBITS_DST_BITS;
        move |= (move_t) MOVETYPE_CASTLE << MOVEBITS_TYP_BITS;
        set->moves[set->count++] = move;
    }

    dst = src - 2;
    travelmask = (uint64_t) 1 << (src - 1) | (uint64_t) 1 << dst;
    if(board->qcastle[team] && !(allpiece & (travelmask | (uint64_t) 1 << (src - 3))) && !(board->attacks & travelmask))
    {
        move = src;
        move |= (move_t) dst << MOVEBITS_DST_BITS;
        move |= (move_t) MOVETYPE_CASTLE << MOVEBITS_TYP_BITS;
        set->moves[set->count++] = move;
    }
}

static inline void move_legalmoves(board_t* restrict board, moveset_t* restrict moves, uint8_t src, bool caponly)
{
    piece_e piece;

    piece = board->sqrs[src] & SQUARE_MASK_TYPE;

    if(piece == PIECE_KING)
    {
        move_kingmoves(moves, board, src, board->sqrs[src] >> SQUARE_BITS_TEAM, caponly);
        return;
    }

    // you can only move your king in double check
    if(board->dblcheck)
        return;

    switch(piece)
    {
    case PIECE_QUEEN:
        move_queenmoves(moves, board, src, board->sqrs[src] >> SQUARE_BITS_TEAM, caponly);
        break;
    case PIECE_ROOK:
        move_rookmoves(moves, board, src, board->sqrs[src] >> SQUARE_BITS_TEAM, caponly);
        break;
    case PIECE_BISHOP:
        move_bishopmoves(moves, board, src, board->sqrs[src] >> SQUARE_BITS_TEAM, caponly);
        break;
    case PIECE_KNIGHT:
        move_knightmoves(moves, board, src, board->sqrs[src] >> SQUARE_BITS_TEAM, caponly);
        break;
    case PIECE_PAWN:
        move_pawnmoves(moves, board, src, board->sqrs[src] >> SQUARE_BITS_TEAM, caponly);
        break;
    default:
        break;
    }
}

void move_gensetup(board_t* restrict board)
{
    move_findattacks(board);
    board_findcheck(board);
    move_findpins(board);
}

void move_alllegal(board_t* restrict board, moveset_t* restrict outmoves, bool caponly)
{
    piece_e p;

    bitboard_t bb;
    int square;

    outmoves->count = 0;
    if(board->stalemate)
        return;

    for(p=PIECE_KING; p<PIECE_COUNT; p++)
    {
        bb = board->pboards[board->tomove][p];
        while(bb)
        {
            square = __builtin_ctzll(bb);
            bb &= bb - 1;

            move_legalmoves(board, outmoves, square, caponly);
        }
    }
}

move_t* move_findmove(moveset_t* set, move_t move)
{
    int i;

    for(i=0; i<set->count; i++)
        if((set->moves[i] & ~MOVEBITS_TYP_MASK) == (move & ~MOVEBITS_TYP_MASK))
            return &set->moves[i];

    return NULL;
}

void move_printset(moveset_t* set)
{
    int i, r, f;

    // this is very inefficient. use a hashmap maybe? it doesnt really matter anyway.

    for(r=BOARD_LEN-1; r>=0; r--)
    {
        for(f=0; f<BOARD_LEN; f++)
        {
            for(i=0; i<set->count; i++)
                if(((set->moves[i] & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS) == r * BOARD_LEN + f)
                    break;

            if(i<set->count)
                printf("# ");
            else
            {
                if((r + f) & 1)
                    printf("- ");
                else
                    printf("+ ");
            }
        }
        printf("\n");
    }
}

static void move_emptysweeps(uint8_t src)
{
    int i;
    dir_e d;

    int square;
    bitboard_t sweep;

    for(d=0; d<DIR_COUNT; d++)
    {
        sweep = 0;
        for(i=1; i<=sweeptable[src][d]; i++)
        {
            square = src + diroffs[d] * i;
            sweep |= (bitboard_t) 1 << square;
        }

        emptysweeps[src][d] = sweep;
    }
}

static void move_pawnboards(uint8_t src)
{
    int i;
    team_e t;

    int r, f;
    int targetrank, homerank;

    f = src % BOARD_LEN;
    r = src / BOARD_LEN;

    for(t=0; t<TEAM_COUNT; t++)
    {
        pawnatk[t][src] = pawnpush[t][src] = pawndbl[t][src] = 0;
        targetrank = r + PAWN_OFFS(t) / BOARD_LEN;
        homerank = t == TEAM_WHITE ? 1 : BOARD_LEN - 2;

        if(targetrank < 0 || targetrank >= BOARD_LEN)
            continue;

        pawnpush[t][src] |= (bitboard_t) 1 << (src + PAWN_OFFS(t));
        if(r == homerank)
            pawndbl[t][src] |= (bitboard_t) 1 << (src + 2 * PAWN_OFFS(t));
        
        for(i=-1; i<=1; i+=2)
        {
            if(f + i < 0 || f + i >= BOARD_LEN)
                continue;

            pawnatk[t][src] |= (bitboard_t) 1 << (src + PAWN_OFFS(t) + i);
        }
    }
}

// 0 <= idx < 8
// -1 means invalid
static int move_knightoffs(int idx, int src)
{
    int r, f;

    dir_e dir;
    int offs;

    dir = idx >> 1;
    offs = (idx & 1) * 2 - 1;

    r = src / BOARD_LEN;
    f = src % BOARD_LEN;

    if(dir == DIR_E)
        f += 2;
    if(dir == DIR_N)
        r += 2;
    if(dir == DIR_W)
        f -= 2;
    if(dir == DIR_S)
        r -= 2;

    if(dir == DIR_E || dir == DIR_W)
        r += offs;
    if(dir == DIR_N || dir == DIR_S)
        f += offs;

    if(r < 0 || r >= BOARD_LEN || f < 0 || f >= BOARD_LEN)
        return -1;

    return r * BOARD_LEN + f;
}

static bitboard_t move_knightboard(uint8_t src)
{
    int i;

    int dst;
    bitboard_t bits;

    bits = 0;
    for(i=0; i<8; i++)
    {
        dst = move_knightoffs(i, src);
        if(dst < 0)
            continue;

        bits |= (bitboard_t) 1 << dst;
    }

    return bits;
}

static bitboard_t move_kingboard(uint8_t src)
{
    dir_e d;

    bitboard_t bits;

    bits = 0;
    for(d=0; d<DIR_COUNT; d++)
    {
        if(!sweeptable[src][d])
            continue;

        bits |= (bitboard_t) 1 << (src + diroffs[d]);
    }

    return bits;
}

static int move_maxsweep(dir_e dir, uint8_t src)
{
    int r, f;
    int dsts[DIR_NE];

    r = src / BOARD_LEN;
    f = src % BOARD_LEN;

    dsts[DIR_E] = BOARD_LEN - (f + 1);
    dsts[DIR_N] = BOARD_LEN - (r + 1);
    dsts[DIR_W] = f;
    dsts[DIR_S] = r;

    if(dir < DIR_NE)
        return dsts[dir];

    switch(dir)
    {
    case DIR_NE:
        if(dsts[DIR_E] < dsts[DIR_N])
            return dsts[DIR_E];
        return dsts[DIR_N];
    case DIR_NW:
        if(dsts[DIR_N] < dsts[DIR_W])
            return dsts[DIR_N];
        return dsts[DIR_W];
    case DIR_SW:
        if(dsts[DIR_W] < dsts[DIR_S])
            return dsts[DIR_W];
        return dsts[DIR_S];
    case DIR_SE:
        if(dsts[DIR_S] < dsts[DIR_E])
            return dsts[DIR_S];
        return dsts[DIR_E];
    default:
        return -1;
    }
}

void move_init(void)
{
    int i;
    dir_e dir;

    for(i=0; i<BOARD_AREA; i++)
    {
        for(dir=0; dir<DIR_COUNT; dir++)
            sweeptable[i][dir] = move_maxsweep(dir, i);
        kingatk[i] = move_kingboard(i);
        knightatk[i] = move_knightboard(i);
        move_pawnboards(i);
        move_emptysweeps(i);
    }

    move_makeinit();
}
