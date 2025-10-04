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

static void move_pawnatk(board_t* board, uint8_t src, team_e team)
{
    bitboard_t atk;

    atk = pawnatk[team][src];
    board->attacks[team][PIECE_NONE] |= atk;
    board->attacks[team][PIECE_PAWN] |= atk;
}

static void move_knightatk(board_t* board, uint8_t src, team_e team)
{
    bitboard_t atk;

    atk = knightatk[src];
    board->attacks[team][PIECE_NONE] |= atk;
    board->attacks[team][PIECE_KNIGHT] |= atk;
}

static void move_bishopatk(board_t* board, uint8_t src, team_e team)
{
    bitboard_t block, atk;

    block = (board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) ^ board->pboards[!team][PIECE_KING];
    atk = magic_lookup(MAGIC_BISHOP, src, block);
    board->attacks[team][PIECE_NONE] |= atk;
    board->attacks[team][PIECE_BISHOP] |= atk;
}

static void move_rookatk(board_t* board, uint8_t src, team_e team)
{
    bitboard_t block, atk;

    block = (board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) ^ board->pboards[!team][PIECE_KING];
    atk = magic_lookup(MAGIC_ROOK, src, block);
    board->attacks[team][PIECE_NONE] |= atk;
    board->attacks[team][PIECE_ROOK] |= atk;
}

static void move_queenatk(board_t* board, uint8_t src, team_e team)
{
    bitboard_t block, atk;

    block = (board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) ^ board->pboards[!team][PIECE_KING];
    
    atk = 0;
    atk |= magic_lookup(MAGIC_ROOK, src, block);
    atk |= magic_lookup(MAGIC_BISHOP, src, block);
    board->attacks[team][PIECE_NONE] |= atk;
    board->attacks[team][PIECE_QUEEN] |= atk;
}

static void move_kingatk(board_t* board, uint8_t src, team_e team)
{
    bitboard_t atk;

    atk = kingatk[src];
    board->attacks[team][PIECE_NONE] |= atk;
    board->attacks[team][PIECE_KING] |= atk;
}

void move_findattacks(board_t* board)
{
    int i, j;

    piece_e p;

    for(i=0; i<TEAM_COUNT; i++)
    {
        for(j=0; j<PIECE_COUNT; j++)
            board->attacks[i][j] = 0;

        for(j=0; j<board->npiece[i]; j++)
        {
            p = board->sqrs[board->ptable[i][j]] & SQUARE_MASK_TYPE;
            
            switch (p)
            {
            case PIECE_KING:
                move_kingatk(board, board->ptable[i][j], i);
                break;
            case PIECE_QUEEN:
                move_queenatk(board, board->ptable[i][j], i);
                break;
            case PIECE_ROOK:
                move_rookatk(board, board->ptable[i][j], i);
                break;
            case PIECE_BISHOP:
                move_bishopatk(board, board->ptable[i][j], i);
                break;
            case PIECE_KNIGHT:
                move_knightatk(board, board->ptable[i][j], i);
                break;
            case PIECE_PAWN:
                move_pawnatk(board, board->ptable[i][j], i);
                break;
            default:
                break;
            }
        }
    }
}

void move_findpins(board_t* board)
{
    dir_e dir;

    team_e team;
    uint8_t kingpos, nblockers;
    bitboard_t queenmask, rookmask, bishopmask, pinnermask, moves, sweepmask;

    board->isenpaspin = false;
    board->npins = 0;

    team = board->tomove;
    kingpos = __builtin_ctzll(board->pboards[team][PIECE_KING]);
    queenmask = board->pboards[!team][PIECE_QUEEN];
    rookmask = board->pboards[!team][PIECE_ROOK];
    bishopmask = board->pboards[!team][PIECE_BISHOP];
    pinnermask = queenmask | rookmask | bishopmask;

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
        nblockers = __builtin_popcountll(sweepmask & board->pboards[team][PIECE_NONE]);

        if(nblockers != 1)
            continue;

        board->pins[board->npins++] = sweepmask;
    }
}

static void move_pawnthreat(board_t* board, uint8_t src, team_e team)
{
    bitboard_t moves;

    moves = pawnatk[team][src];
    if(!(moves & board->pboards[!team][PIECE_KING]))
        return;

    if(board->isthreat)
    {
        board->dblcheck = true;
        return;
    }

    board->isthreat = true;
    board->threat = (uint64_t) 1 << src;
}

static void move_knightthreat(board_t* board, uint8_t src, team_e team)
{
    bitboard_t moves;

    moves = knightatk[src];
    if(!(moves & board->pboards[!team][PIECE_KING]))
        return;

    if(board->isthreat)
    {
        board->dblcheck = true;
        return;
    }

    board->isthreat = true;
    board->threat = (uint64_t) 1 << src;
}

static void move_bishopthreat(board_t* board, uint8_t src, team_e team)
{
    dir_e d;

    bitboard_t mask;

    mask = magic_lookup(MAGIC_BISHOP, src, board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);

    if(!(mask & board->pboards[!team][PIECE_KING]))
        return;

    for(d=DIR_NE; d<DIR_COUNT; d++)
    {
        if(!(emptysweeps[src][d] & board->pboards[!team][PIECE_KING]))
            continue;
        
        if(board->isthreat)
        {
            board->dblcheck = true;
            return;
        }

        board->isthreat = true;
        board->threat = (mask & emptysweeps[src][d]) | ((bitboard_t) 1 << src);
        return;
    }
}

static void move_rookthreat(board_t* board, uint8_t src, team_e team)
{
    dir_e d;

    bitboard_t mask;

    mask = magic_lookup(MAGIC_ROOK, src, board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);

    if(!(mask & board->pboards[!team][PIECE_KING]))
        return;

    for(d=0; d<DIR_NE; d++)
    {
        if(!(emptysweeps[src][d] & board->pboards[!team][PIECE_KING]))
            continue;
        
        if(board->isthreat)
        {
            board->dblcheck = true;
            return;
        }

        board->isthreat = true;
        board->threat = (mask & emptysweeps[src][d]) | ((bitboard_t) 1 << src);
        return;
    }
}

static void move_queenthreat(board_t* board, uint8_t src, team_e team)
{
    dir_e d;

    bitboard_t mask;

    mask = 0;
    mask |= magic_lookup(MAGIC_ROOK, src, board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);
    mask |= magic_lookup(MAGIC_BISHOP, src, board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);

    if(!(mask & board->pboards[!team][PIECE_KING]))
        return;

    for(d=0; d<DIR_COUNT; d++)
    {
        if(!(emptysweeps[src][d] & board->pboards[!team][PIECE_KING]))
            continue;
        
        if(board->isthreat)
        {
            board->dblcheck = true;
            return;
        }

        board->isthreat = true;
        board->threat = (mask & emptysweeps[src][d]) | ((bitboard_t) 1 << src);
        return;
    }
}

void move_findthreats(board_t* board)
{
    int i;

    piece_e p;

    board->dblcheck = false;
    board->isthreat = false;
    board->threat = 0;

    if(!(board->attacks[!board->tomove][PIECE_NONE] & board->pboards[board->tomove][PIECE_KING]))
        return;

    for(i=0; i<board->npiece[!board->tomove]; i++)
    {
        p = board->sqrs[board->ptable[!board->tomove][i]] & SQUARE_MASK_TYPE;
        if(!(board->attacks[!board->tomove][p] & board->pboards[board->tomove][PIECE_KING]))
            continue;

        switch(p)
        {
        case PIECE_QUEEN:
            move_queenthreat(board, board->ptable[!board->tomove][i], !board->tomove);
            break;
        case PIECE_ROOK:
            move_rookthreat(board, board->ptable[!board->tomove][i], !board->tomove);
            break;
        case PIECE_BISHOP:
            move_bishopthreat(board, board->ptable[!board->tomove][i], !board->tomove);
            break;
        case PIECE_KNIGHT:
            move_knightthreat(board, board->ptable[!board->tomove][i], !board->tomove);
            break;
        case PIECE_PAWN:
            move_pawnthreat(board, board->ptable[!board->tomove][i], !board->tomove);
            break;
        default:
            break;
        }

        if(board->dblcheck)
            break;
    }
}

static void move_bitboardtomoves(board_t* board, moveset_t* set, uint8_t src, bitboard_t moves)
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

static inline bool move_enpaslegal(board_t* board, uint8_t src)
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

static void move_pawnmoves(moveset_t* set, board_t* board, uint8_t src, team_e team, bool caponly)
{
    const bitboard_t promotionmask = 0xFF000000000000FF;

    int i;

    uint8_t dst;
    int starttype, stoptype;
    move_t move;
    bitboard_t srcmask, moves;

    srcmask = (bitboard_t) 1 << src;

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
    for(i=0; i<board->npins; i++)
    {
        if(!(srcmask & board->pins[i]))
            continue;
        moves &= board->pins[i];
    }

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

    if(board->isenpaspin)
        return;

    moves = pawnatk[team][src] & ((bitboard_t) 1 << board->enpas);
    if(!moves)
        return;

    if(board->isthreat)
        moves &= board->threat;
    for(i=0; i<board->npins; i++)
    {
        if(!(srcmask & board->pins[i]))
            continue;
        moves &= board->pins[i];
    }

    if(!moves)
        return;

    if(!move_enpaslegal(board, src))
        return;

    move = src;
    move |= (move_t) board->enpas << MOVEBITS_DST_BITS;
    move |= (move_t) MOVETYPE_ENPAS << MOVEBITS_TYP_BITS;
    set->moves[set->count++] = move;
}

static void move_knightmoves(moveset_t* set, board_t* board, uint8_t src, team_e team, bool caponly)
{
    int i;

    bitboard_t srcmask, moves;

    srcmask = (bitboard_t) 1 << src;

    moves = knightatk[src];
    if(caponly)
        moves &= board->pboards[!team][PIECE_NONE];
    moves &= ~board->pboards[team][PIECE_NONE];
    if(board->threat)
        moves &= board->threat;

    for(i=0; i<board->npins; i++)
    {
        if(!(srcmask & board->pins[i]))
            continue;
        moves &= board->pins[i];
    }

    move_bitboardtomoves(board, set, src, moves);
}

static bitboard_t move_getslideratk(board_t* board, magicpiece_e type, uint8_t src, bool caponly)
{
    int i;

    team_e team;
    bitboard_t srcmask, moves;

    team = board->sqrs[src] >> SQUARE_BITS_TEAM;
    srcmask = (bitboard_t) 1 << src;

    moves = magic_lookup(type, src, board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]);
    if(caponly)
        moves &= board->pboards[!team][PIECE_NONE];
    moves &= ~board->pboards[team][PIECE_NONE];
    if(board->threat)
        moves &= board->threat;

    for(i=0; i<board->npins; i++)
    {
        if(!(srcmask & board->pins[i]))
            continue;
        moves &= board->pins[i];
    }

    return moves;
}

static void move_bishopmoves(moveset_t* set, board_t* board, uint8_t src, team_e team, bool caponly)
{
    bitboard_t moves;

    moves = move_getslideratk(board, MAGIC_BISHOP, src, caponly);
    move_bitboardtomoves(board, set, src, moves);
}

static void move_rookmoves(moveset_t* set, board_t* board, uint8_t src, team_e team, bool caponly)
{
    bitboard_t moves;

    moves = move_getslideratk(board, MAGIC_ROOK, src, caponly);
    move_bitboardtomoves(board, set, src, moves);
}

static void move_queenmoves(moveset_t* set, board_t* board, uint8_t src, team_e team, bool caponly)
{
    bitboard_t moves;

    moves = move_getslideratk(board, MAGIC_ROOK, src, caponly) | move_getslideratk(board, MAGIC_BISHOP, src, caponly);
    move_bitboardtomoves(board, set, src, moves);
}

static void move_kingmoves(moveset_t* set, board_t* board, uint8_t src, team_e team, bool caponly)
{
    uint8_t dst;
    bitboard_t moves;
    move_t move;
    bitboard_t travelmask, allpiece;

    moves = kingatk[src];
    moves &= ~board->pboards[team][PIECE_NONE];
    moves &= ~board->attacks[!team][PIECE_NONE];
    if(caponly)
        moves &= board->pboards[!team][PIECE_NONE];

    move_bitboardtomoves(board, set, src, moves);

    if(caponly || board->check[team])
        return;

    allpiece = board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE];
    
    dst = src + 2;
    travelmask = (uint64_t) 1 << (src + 1) | (uint64_t) 1 << dst;
    if(board->kcastle[team] && !(allpiece & travelmask) && !(board->attacks[!team][PIECE_NONE] & travelmask))
    {
        move = src;
        move |= (move_t) dst << MOVEBITS_DST_BITS;
        move |= (move_t) MOVETYPE_CASTLE << MOVEBITS_TYP_BITS;
        set->moves[set->count++] = move;
    }

    dst = src - 2;
    travelmask = (uint64_t) 1 << (src - 1) | (uint64_t) 1 << dst;
    if(board->qcastle[team] && !(allpiece & travelmask) && !(board->attacks[!team][PIECE_NONE] & travelmask))
    {
        move = src;
        move |= (move_t) dst << MOVEBITS_DST_BITS;
        move |= (move_t) MOVETYPE_CASTLE << MOVEBITS_TYP_BITS;
        set->moves[set->count++] = move;
    }
}

void move_legalmoves(board_t* board, moveset_t* moves, uint8_t src, bool caponly)
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

void move_alllegal(board_t* board, moveset_t* outmoves, bool caponly)
{
    int i;

    outmoves->count = 0;
    for(i=0; i<board->npiece[board->tomove]; i++)
        move_legalmoves(board, outmoves, board->ptable[board->tomove][i], caponly);
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
}
