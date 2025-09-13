#include "move.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int sweeptable[BOARD_AREA][DIR_COUNT];

double msmove = 0;

void move_domove(board_t* board, move_t move, mademove_t* outmove)
{
    int type, src, dst;
    team_e team;
    piece_e ptype, newtype, captype;
    bitboard_t srcmask, dstmask, cstlsrc, cstldst;
    int enpasoffs;
    clock_t start;

    start = clock();

    outmove->move = move;
    outmove->captured = PIECE_NONE;
    outmove->enpas = board->enpas;
    outmove->kcastle[0] = board->kcastle[0];
    outmove->kcastle[1] = board->kcastle[1];
    outmove->qcastle[0] = board->qcastle[0];
    outmove->qcastle[1] = board->qcastle[1];
    
    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    srcmask = (uint64_t) 1 << src;
    dstmask = (uint64_t) 1 << dst;

    team = board->tomove;

    for(ptype=PIECE_KING; ptype<PIECE_COUNT; ptype++)
        if(board->pboards[team][ptype] & srcmask)
            break;
    assert(ptype < PIECE_COUNT);

    enpasoffs = board->tomove == TEAM_WHITE ? diroffs[DIR_S] : diroffs[DIR_N];
    if(type == MOVETYPE_ENPAS)
    {
        outmove->captured = PIECE_PAWN;
        board->pboards[!team][PIECE_NONE] ^= (uint64_t) 1 << (board->enpas + enpasoffs);
        board->pboards[!team][PIECE_PAWN] ^= (uint64_t) 1 << (board->enpas + enpasoffs);
    }

    // double pawn push
    board->enpas = 0xFF;
    if(ptype == PIECE_PAWN && abs(dst - src) == BOARD_LEN * 2)
        board->enpas = dst + enpasoffs;

    if(ptype == PIECE_KING)
        board->kcastle[team] = board->qcastle[team] = false;
    if(ptype == PIECE_ROOK)
    {
        if(src % BOARD_LEN == BOARD_LEN - 1)
            board->kcastle[team] = false;
        if(!(src % BOARD_LEN))
            board->qcastle[team] = false;
    }

    // move the rook
    if(type == MOVETYPE_CASTLE)
    {
        // kingside
        if(dst > src)
        {
            cstlsrc = (uint64_t) 1 << (dst + 1);
            cstldst = (uint64_t) 1 << (dst - 1);
        }
        // queenside
        else
        {
            cstlsrc = (uint64_t) 1 << (dst - 2);
            cstldst = (uint64_t) 1 << (dst + 1);
        }

        board->pboards[team][PIECE_ROOK] ^= cstlsrc;
        board->pboards[team][PIECE_NONE] ^= cstlsrc;
        board->pboards[team][PIECE_ROOK] ^= cstldst;
        board->pboards[team][PIECE_NONE] ^= cstldst;
    }

    switch(type)
    {
    case MOVETYPE_PROMQ:
    case MOVETYPE_PROMR:
    case MOVETYPE_PROMB:
    case MOVETYPE_PROMN:
        newtype = PIECE_QUEEN + type - MOVETYPE_PROMQ;
        break;
    default:
        newtype = ptype;
    }

    board->pboards[team][ptype] ^= srcmask;
    board->pboards[team][PIECE_NONE] ^= srcmask;
    board->pboards[team][newtype] |= dstmask;
    board->pboards[team][PIECE_NONE] |= dstmask;

    if(board->pboards[!team][PIECE_NONE] & dstmask)
    {
        // we can never capture the king, so start at queen instead
        for(captype=PIECE_QUEEN; captype<PIECE_COUNT; captype++)
        {
            if(!(board->pboards[!team][captype] & dstmask))
                continue;

            board->pboards[!team][captype] ^= dstmask;
            board->pboards[!team][PIECE_NONE] ^= dstmask;
            outmove->captured = captype;
            break;
        }
    }

    board->tomove = !board->tomove;
    
    board_findpieces(board);
    move_findattacks(board);
    move_findpins(board);
    board_findcheck(board);

    if(board->check[!board->tomove])
    {
        printf("legality pruning failed!\n");
        printf("%s moved %c%c -> %c%c into check:\n", !board->tomove ? "black" : "white",
            'a' + src % BOARD_LEN, '1' + src / BOARD_LEN, 'a' + dst % BOARD_LEN, '1' + dst / BOARD_LEN);
        board_print(board);
        printf("%s attack:\n", !board->tomove ? "white" : "black");
        board_printbits(board->attacks[board->tomove]);
        printf("%s king:\n", !board->tomove ? "black" : "white");
        board_printbits(board->pboards[!board->tomove][PIECE_KING]);
        exit(1);
    }

    msmove += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;
}

// there's a lot of repeated code from move_domove.
// maybe figure out how to generalize this.
void move_undomove(board_t* board, const mademove_t* move)
{
    int type, src, dst;
    team_e team;
    piece_e ptype, oldtype;
    bitboard_t srcmask, dstmask, cstlsrc, cstldst;
    int enpasoffs;
    clock_t start;

    start = clock();

    board->enpas = move->enpas;
    board->kcastle[0] = move->kcastle[0];
    board->kcastle[1] = move->kcastle[1];
    board->qcastle[0] = move->qcastle[0];
    board->qcastle[1] = move->qcastle[1];
    
    src = move->move & MOVEBITS_SRC_MASK;
    dst = (move->move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    srcmask = (uint64_t) 1 << src;
    dstmask = (uint64_t) 1 << dst;

    team = !board->tomove;

    for(ptype=PIECE_KING; ptype<PIECE_COUNT; ptype++)
        if(board->pboards[team][ptype] & dstmask)
            break;
    if(ptype >= PIECE_COUNT)
    {
        printf("undo %c%c -> %c%c.\n", 
        'a' + src % BOARD_LEN, '1' + src / BOARD_LEN, 'a' + dst % BOARD_LEN, '1' + dst / BOARD_LEN);
        board_print(board);
        abort();
    }

    if(type == MOVETYPE_CASTLE)
    {
        // kingside
        if(dst > src)
        {
            cstlsrc = (uint64_t) 1 << (dst + 1);
            cstldst = (uint64_t) 1 << (dst - 1);
        }
        // queenside
        else
        {
            cstlsrc = (uint64_t) 1 << (dst - 2);
            cstldst = (uint64_t) 1 << (dst + 1);
        }

        board->pboards[team][PIECE_ROOK] ^= cstlsrc;
        board->pboards[team][PIECE_NONE] ^= cstlsrc;
        board->pboards[team][PIECE_ROOK] ^= cstldst;
        board->pboards[team][PIECE_NONE] ^= cstldst;
    }

    switch(type)
    {
    case MOVETYPE_PROMQ:
    case MOVETYPE_PROMR:
    case MOVETYPE_PROMB:
    case MOVETYPE_PROMN:
        oldtype = PIECE_PAWN;
        break;
    default:
        oldtype = ptype;
    }

    board->pboards[team][PIECE_NONE] ^= dstmask;
    board->pboards[team][ptype] ^= dstmask;
    board->pboards[team][PIECE_NONE] ^= srcmask;
    board->pboards[team][oldtype] ^= srcmask;

    if(type == MOVETYPE_ENPAS)
    {
        enpasoffs = team == TEAM_WHITE ? diroffs[DIR_S] : diroffs[DIR_N];
        board->pboards[!team][PIECE_NONE] ^= (uint64_t) 1 << (board->enpas + enpasoffs);
        board->pboards[!team][PIECE_PAWN] ^= (uint64_t) 1 << (board->enpas + enpasoffs);
    }
    else if(move->captured)
    {
        board->pboards[!team][move->captured] ^= dstmask;
        board->pboards[!team][PIECE_NONE] ^= dstmask;
    }
    
    board->tomove = team;

    board_findpieces(board);
    move_findattacks(board);
    move_findpins(board);
    board_findcheck(board);

    msmove += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;
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

static inline bool move_sqrinpin(pinline_t* pin, uint8_t sqr)
{
    bitboard_t sqrmask;

    sqrmask = (uint64_t) 1 << sqr;
    return (sqrmask & pin->bits) != 0;
}

static bool move_islegal(board_t* board, move_t move, piece_e ptype, team_e team)
{
    int i;
    uint8_t dcur;
    pinline_t *curpin;

    bitboard_t mask;
    uint8_t src, dst, type, dstart, dend;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    if(ptype == PIECE_KING)
    {
        dstart = dend = dst;
        if(type == MOVETYPE_CASTLE)
        {
            if(src < dst)
            {
                dstart = src;
                dend = dst;
            }
            else
            {
                dstart = dst;
                dend = src;
            }
        }
        
        mask = 0;
        for(dcur=dstart; dcur<=dend; dcur++)
            mask |= (uint64_t) 1 << dcur;

        return !(board->attacks[!team] & mask);
    }

    for(i=0, curpin=board->pins[!team]; i<board->npins[!team]; i++, curpin++)
    {
        // we're capturing the source, so don't worry about it.
        if(dst == curpin->start)
            continue;

        // leaving an open pin open.
        if(!curpin->nblocks && !move_sqrinpin(curpin, dst))
            return false;

        // stepping out of a pin we were in.
        if(curpin->nblocks && move_sqrinpin(curpin, src) && !move_sqrinpin(curpin, dst))
            return false;
    }

    return true;
}

static moveset_t* move_addiflegal(board_t* board, moveset_t* moves, move_t move, piece_e ptype, team_e team)
{
    moveset_t *newmove;

    if(!move_islegal(board, move, ptype, team))
        return moves;

    newmove = malloc(sizeof(moveset_t));
    newmove->move = move;
    newmove->next = moves;

    return newmove;
}

// if bits is not null, it will ignore set and fill out the bitboard
static moveset_t* move_sweep
(
    moveset_t* set, board_t* board, 
    uint8_t src, dir_e dir, int max, 
    bool nocapture, movetype_e type, 
    piece_e ptype, team_e team
)
{
    int i;

    int idx;
    move_t move;

    for(i=0, idx=src+diroffs[dir]; i<max; i++, idx+=diroffs[dir])
    {
        // friendly
        if(((uint64_t) 1 << idx) & board->pboards[team][PIECE_NONE])
            break;

        // enemy, but we can't capture
        if(nocapture && (((uint64_t) 1 << idx) & board->pboards[!team][PIECE_NONE]))
            break;

        move = src;
        move |= idx << MOVEBITS_DST_BITS;
        move |= ((uint16_t)type) << MOVEBITS_TYP_BITS;
        set = move_addiflegal(board, set, move, ptype, team);

        if(((uint64_t) 1 << idx) & board->pboards[!team][PIECE_NONE])
            break;
    }

    return set;
}

static void move_sweepatk(board_t* board, bitboard_t* bits, uint8_t src, dir_e dir, int max, team_e team)
{
    int i;

    int idx;
    bitboard_t mask;

    for(i=0, idx=src+diroffs[dir]; i<max; i++, idx+=diroffs[dir])
    {
        mask = (uint64_t) 1 << idx;
        *bits |= mask;

        if((board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) & mask
        && !(board->pboards[!team][PIECE_KING] & mask)) // should "go through" enemy king so they cant just backstep
            break;
    }
}

static void move_pawnatk(board_t* board, uint8_t src, team_e team)
{
    int i;
    
    int f;
    int dst;
    dir_e dirs[2];

    if(team == TEAM_WHITE)
    {
        dirs[0] = DIR_NE;
        dirs[1] = DIR_NW;
    }
    else
    {
        dirs[0] = DIR_SE;
        dirs[1] = DIR_SW;
    }

    f = src % BOARD_LEN;
    for(i=0; i<2; i++)
    {
        if(!i && f == BOARD_LEN-1)
            continue;
        if(i && !f)
            continue;

        dst = src + diroffs[dirs[i]];
        board->attacks[team] |= (uint64_t) 1 << dst;
    }
}

static void move_knightatk(board_t* board, uint8_t src, team_e team)
{
    int i;
    
    int dst;

    for(i=0; i<8; i++)
    {
        dst = move_knightoffs(i, src);
        if(dst < 0)
            continue;

        board->attacks[team] |= (uint64_t) 1 << dst;
    }
}

static void move_bishopatk(board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=DIR_NE; i<DIR_COUNT; i++)
        move_sweepatk(board, &board->attacks[team], src, i, sweeptable[src][i], team);
}

static void move_rookatk(board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=0; i<DIR_NE; i++)
        move_sweepatk(board, &board->attacks[team], src, i, sweeptable[src][i], team);
}

static void move_queenatk(board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=0; i<DIR_COUNT; i++)
        move_sweepatk(board, &board->attacks[team], src, i, sweeptable[src][i], team);
}

static void move_kingatk(board_t* board, uint8_t src, team_e team)
{
    int i;
    
    for(i=0; i<DIR_COUNT; i++)
        if(sweeptable[src][i])
            move_sweepatk(board, &board->attacks[team], src, i, 1, team);
}

void move_findattacks(board_t* board)
{
    int i, j;

    bitboard_t pmask;

    for(i=0; i<TEAM_COUNT; i++)
    {
        board->attacks[i] = 0;
        for(j=0; j<board->npiece[i]; j++)
        {
            pmask = (uint64_t) 1 << board->ptable[i][j];

            if(pmask & board->pboards[i][PIECE_KING])
                move_kingatk(board, board->ptable[i][j], i);
            else if(pmask & board->pboards[i][PIECE_QUEEN])
                move_queenatk(board, board->ptable[i][j], i);
            else if(pmask & board->pboards[i][PIECE_ROOK])
                move_rookatk(board, board->ptable[i][j], i);
            else if(pmask & board->pboards[i][PIECE_BISHOP])
                move_bishopatk(board, board->ptable[i][j], i);
            else if(pmask & board->pboards[i][PIECE_KNIGHT])
                move_knightatk(board, board->ptable[i][j], i);
            else if(pmask & board->pboards[i][PIECE_PAWN])
                move_pawnatk(board, board->ptable[i][j], i);
        }
    }
}

static void move_sweeppin(board_t* board, team_e team, dir_e dir, uint8_t src)
{
    int i;

    int idx;
    int max;
    pinline_t line;

    bitboard_t imask;

    memset(&line, 0, sizeof(pinline_t));

    max = sweeptable[src][dir];
    for(i=0, idx=src+diroffs[dir], line.nblocks=0; i<max; i++, idx+=diroffs[dir])
    {
        imask = (uint64_t) 1 << idx;
        line.bits |= imask;

        if(!((board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) & imask))
            continue;

        // hit a friendly! en passant might mess this up.
        if(board->pboards[team][PIECE_NONE] & imask)
            return;

        // enemy king, end of pin.
        if(board->pboards[!team][PIECE_KING] & imask)
            break;

        if(line.nblocks)
            return; // two enemies before the king
        line.nblocks++;
    }

    // edge of the board and no king
    if(i >= max)
        return;

    if(board->npins[team] >= PIECE_MAX)
    {
        printf("move_sweeppin: max pins reached! max is %d.\n", PIECE_MAX);
        exit(1);
    }

    line.start = src;
    line.end = idx;
    board->pins[team][board->npins[team]++] = line;
}

static void move_pawnpin(board_t* board, team_e team, uint8_t src)
{
    int i;

    int dst;
    bitboard_t dstmask;
    pinline_t *line;
    dir_e dirs[2];

    if(team == TEAM_WHITE)
    {
        dirs[0] = DIR_NW;
        dirs[1] = DIR_NE;
    }
    else
    {
        dirs[0] = DIR_SW;
        dirs[1] = DIR_SE;
    }

    for(i=0; i<2; i++)
    {
        dst = src + diroffs[dirs[i]];
        if(abs(dst / BOARD_LEN - src / BOARD_LEN) != 1)
            continue;

        dstmask = (uint64_t) 1 << dst;
        if(!(dstmask & board->pboards[!team][PIECE_KING]))
            continue;

        if(board->npins[team] >= PIECE_MAX)
        {
            printf("move_pawnpin: max pins reached! max is %d.\n", PIECE_MAX);
            exit(1);
        }

        line = &board->pins[team][board->npins[team]++];
        line->bits = dstmask;
        line->end = dst;
        line->start = src;
        line->nblocks = 0;

        break;
    }
}

static void move_knightpin(board_t* board, team_e team, uint8_t src)
{
    int i;

    int dst;
    bitboard_t dstmask;
    pinline_t *line;

    for(i=0; i<8; i++)
    {
        dst = move_knightoffs(i, src);
        if(dst < 0)
            continue;

        dstmask = (uint64_t) 1 << dst;
        if(!(dstmask & board->pboards[!team][PIECE_KING]))
            continue;

        if(board->npins[team] >= PIECE_MAX)
        {
            printf("move_knightpin: max pins reached! max is %d.\n", PIECE_MAX);
            exit(1);
        }

        line = &board->pins[team][board->npins[team]++];
        line->bits = dstmask;
        line->end = dst;
        line->start = src;
        line->nblocks = 0;

        break;
    }
}

void move_findpins(board_t* board)
{
    int i, j;
    dir_e dir;

    bitboard_t pmask;

    for(i=0; i<TEAM_COUNT; i++)
    {
        board->npins[i] = 0;
        for(j=0; j<board->npiece[i]; j++)
        {
            pmask = (uint64_t) 1 << board->ptable[i][j];

            if(!(board->pboards[i][PIECE_NONE] & pmask))
                continue;

            if(board->pboards[i][PIECE_QUEEN] & pmask)
                for(dir=0; dir<DIR_COUNT; dir++) move_sweeppin(board, i, dir, board->ptable[i][j]);
            else if(board->pboards[i][PIECE_ROOK] & pmask)
                for(dir=0; dir<DIR_NE; dir++) move_sweeppin(board, i, dir, board->ptable[i][j]);
            else if(board->pboards[i][PIECE_BISHOP] & pmask)
                for(dir=DIR_NE; dir<DIR_COUNT; dir++) move_sweeppin(board, i, dir, board->ptable[i][j]);
            else if(board->pboards[i][PIECE_KNIGHT] & pmask)
                move_knightpin(board, i, board->ptable[i][j]);
            else if(board->pboards[i][PIECE_PAWN] & pmask)
                move_pawnpin(board, i, board->ptable[i][j]);
        }
    }
}

static moveset_t* move_pawnmoves(moveset_t* set, board_t* board, uint8_t src, team_e team)
{
    int i;
    int type;

    bool dbl;
    int dst;
    int starttype, stoptype;
    dir_e dir, atk[2];
    move_t move;
    bitboard_t dstmask;

    starttype = stoptype = MOVETYPE_DEFAULT;

    if(team == TEAM_WHITE)
    {
        if(src / BOARD_LEN == BOARD_LEN-2)
        {
            starttype = MOVETYPE_PROMQ;
            stoptype = MOVETYPE_PROMN;
        }
        dbl = (src / BOARD_LEN == 1);
        dir = DIR_N;
        atk[0] = DIR_NW;
        atk[1] = DIR_NE;
    }
    else
    {
        if(src / BOARD_LEN == 1)
        {
            starttype = MOVETYPE_PROMQ;
            stoptype = MOVETYPE_PROMN;
        }
        dbl = (src / BOARD_LEN == BOARD_LEN - 2);
        dir = DIR_S;
        atk[0] = DIR_SW;
        atk[1] = DIR_SE;
    }
    
    dst = src + diroffs[dir];
    dstmask = (uint64_t) 1 << dst;
    if(!((board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) & dstmask))
    {
        for(type=starttype; type<=stoptype; type++)
        {
            move = src;
            move |= ((uint16_t)dst) << MOVEBITS_DST_BITS;
            move |= ((uint16_t)type) << MOVEBITS_TYP_BITS;
            set = move_addiflegal(board, set, move, PIECE_PAWN, team);
        }

        dst += diroffs[dir];
        dstmask = (uint64_t) 1 << dst;
        if(dbl && !((board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) & dstmask))
        {
            move = src;
            move |= ((uint16_t)dst) << MOVEBITS_DST_BITS;
            set = move_addiflegal(board, set, move, PIECE_PAWN, team);
        }
    }

    for(i=0; i<2; i++)
    {
        dst = src + diroffs[atk[i]];
        // edge of board puts us in different rank
        if(dst / BOARD_LEN != (src + diroffs[dir]) / BOARD_LEN)
            continue;
        dstmask = (uint64_t) 1 << dst;

        if(board->enpas == dst)
        {
            move = 0;
            move |= src;
            move |= dst << MOVEBITS_DST_BITS;
            move |= ((uint16_t)MOVETYPE_ENPAS) << MOVEBITS_TYP_BITS;
            set = move_addiflegal(board, set, move, PIECE_PAWN, team);
            
            // no need to check normal attacks since logically the square must be clear
            continue;
        }

        if(!(board->pboards[!team][PIECE_NONE] & dstmask))
            continue;
        
        for(type=starttype; type<=stoptype; type++)
        {
            move = 0;
            move |= src;
            move |= dst << MOVEBITS_DST_BITS;
            move |= ((uint16_t)type) << MOVEBITS_TYP_BITS;
            set = move_addiflegal(board, set, move, PIECE_PAWN, team);
        }
    }

    return set;
}

static moveset_t* move_knightmoves(moveset_t* set, board_t* board, uint8_t src, team_e team)
{
    int i;

    int dst;
    move_t move;
    
    for(i=0; i<8; i++)
    {
        dst = move_knightoffs(i, src);
        if(dst < 0)
            continue;

        if(board->pboards[team][PIECE_NONE] & (uint64_t) 1 << dst)
            continue;

        move = src;
        move |= dst << MOVEBITS_DST_BITS;
        set = move_addiflegal(board, set, move, PIECE_KNIGHT, team);
    }

    return set;
}

static moveset_t* move_bishopmoves(moveset_t* set, board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=DIR_NE; i<DIR_COUNT; i++)
        set = move_sweep(set, board, src, i, sweeptable[src][i], false, MOVETYPE_DEFAULT, PIECE_BISHOP, team);

    return set;
}

static moveset_t* move_rookmoves(moveset_t* set, board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=0; i<DIR_NE; i++)
        set = move_sweep(set, board, src, i, sweeptable[src][i], false, MOVETYPE_DEFAULT, PIECE_ROOK, team);

    return set;
}

static moveset_t* move_kingmoves(moveset_t* set, board_t* board, uint8_t src, team_e team)
{
    int i;

    move_t move;
    bitboard_t allpiece;

    allpiece = board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE];

    for(i=0; i<DIR_COUNT; i++)
        if(sweeptable[src][i])
            set = move_sweep(set, board, src, i, 1, false, MOVETYPE_DEFAULT, PIECE_KING, team);

    if(board->kcastle[team] && !(allpiece & ((uint64_t) 1 << (src + 1) | (uint64_t) 1 << (src + 2))))
    {
        move = src;
        move |= ((uint16_t)src+2) << MOVEBITS_DST_BITS;
        move |= ((uint16_t)MOVETYPE_CASTLE) << MOVEBITS_TYP_BITS;
        set = move_addiflegal(board, set, move, PIECE_KING, team);
    }

    if(board->qcastle[team]
    && !(allpiece & ((uint64_t) 1 << (src - 1) | (uint64_t) 1 << (src - 2) | (uint64_t) 1 << (src - 3))))
    {
        move = src;
        move |= ((uint16_t)src-2) << MOVEBITS_DST_BITS;
        move |= ((uint16_t)MOVETYPE_CASTLE) << MOVEBITS_TYP_BITS;
        set = move_addiflegal(board, set, move, PIECE_KING, team);
    }

    return set;
}

static moveset_t* move_queenmoves(moveset_t* set, board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=0; i<DIR_COUNT; i++)
        set = move_sweep(set, board, src, i, sweeptable[src][i], false, MOVETYPE_DEFAULT, PIECE_QUEEN, team);

    return set;
}

double msmovegen = 0;

moveset_t* move_legalmoves(board_t* board, moveset_t* moves, uint8_t src)
{
    clock_t start;
    bitboard_t srcmask;

    start = clock();

    srcmask = (uint64_t) 1 << src;
         if(board->pboards[TEAM_WHITE][PIECE_KING] & srcmask)
        moves = move_kingmoves(moves, board, src, TEAM_WHITE);
    else if(board->pboards[TEAM_BLACK][PIECE_KING] & srcmask)
        moves = move_kingmoves(moves, board, src, TEAM_BLACK);

    else if(board->pboards[TEAM_WHITE][PIECE_QUEEN] & srcmask)
        moves = move_queenmoves(moves, board, src, TEAM_WHITE);
    else if(board->pboards[TEAM_BLACK][PIECE_QUEEN] & srcmask)
        moves = move_queenmoves(moves, board, src, TEAM_BLACK);

    else if(board->pboards[TEAM_WHITE][PIECE_ROOK] & srcmask)
        moves = move_rookmoves(moves, board, src, TEAM_WHITE);
    else if(board->pboards[TEAM_BLACK][PIECE_ROOK] & srcmask)
        moves = move_rookmoves(moves, board, src, TEAM_BLACK);

    else if(board->pboards[TEAM_WHITE][PIECE_BISHOP] & srcmask)
        moves = move_bishopmoves(moves, board, src, TEAM_WHITE);
    else if(board->pboards[TEAM_BLACK][PIECE_BISHOP] & srcmask)
        moves = move_bishopmoves(moves, board, src, TEAM_BLACK);

    else if(board->pboards[TEAM_WHITE][PIECE_KNIGHT] & srcmask)
        moves = move_knightmoves(moves, board, src, TEAM_WHITE);
    else if(board->pboards[TEAM_BLACK][PIECE_KNIGHT] & srcmask)
        moves = move_knightmoves(moves, board, src, TEAM_BLACK);

    else if(board->pboards[TEAM_WHITE][PIECE_PAWN] & srcmask)
        moves = move_pawnmoves(moves, board, src, TEAM_WHITE);
    else if(board->pboards[TEAM_BLACK][PIECE_PAWN] & srcmask)
        moves = move_pawnmoves(moves, board, src, TEAM_BLACK);

    msmovegen += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;

    return moves;
}

moveset_t* move_alllegal(board_t* board)
{
    int i;

    moveset_t *moves;

    moves = NULL;
    for(i=0; i<board->npiece[board->tomove]; i++)
        moves = move_legalmoves(board, moves, board->ptable[board->tomove][i]);

    return moves;
}

move_t* move_findmove(moveset_t* set, move_t move)
{
    moveset_t *cur;

    cur = set;
    while(cur)
    {
        if((cur->move & ~MOVEBITS_TYP_MASK) == (move & ~MOVEBITS_TYP_MASK))
            return &cur->move;
        cur = cur->next;
    }

    return NULL;
}

void move_printset(moveset_t* set)
{
    int r, f;
    moveset_t *cur;

    // this is very inefficient. use a hashmap maybe? it doesnt really matter anyway.

    for(r=BOARD_LEN-1; r>=0; r--)
    {
        for(f=0; f<BOARD_LEN; f++)
        {
            cur = set;
            while(cur)
            {
                if(((cur->move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS) == r * BOARD_LEN + f)
                    break;
                cur = cur->next;
            }

            if(cur)
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

void move_freeset(moveset_t* set)
{
    moveset_t *next, *cur;

    cur = set;
    next = NULL;

    while(cur)
    {
        next = cur->next;
        free(cur);
        cur = next;
    }
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
        for(dir=0; dir<DIR_COUNT; dir++)
            sweeptable[i][dir] = move_maxsweep(dir, i);
}
