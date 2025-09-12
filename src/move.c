#include "move.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int sweeptable[BOARD_AREA][DIR_COUNT];

double mspingen = 0;

void move_domove(board_t* board, move_t move)
{
    int type, src, dst;
    team_e team;
    piece_e ptype, newtype;
    clock_t start;

    start = clock();
    
    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    for(team=0; team<TEAM_COUNT; team++)
        if(((uint64_t) 1 << src) & board->pboards[team][PIECE_NONE])
            break;

    // moving from empty square
    if(team >= TEAM_COUNT)
        return;

    for(ptype=0; ptype<PIECE_COUNT; ptype++)
        if(board->pboards[team][ptype] & ((uint64_t) 1 << src))
            break;

    // bitboard desync
    if(ptype >= PIECE_COUNT)
        return;

    if(type == MOVETYPE_ENPAS)
        board->pboards[!team][PIECE_PAWN] ^= (uint64_t) 1 << board->enpas;

    // double pawn push
    board->enpas = 0xFF;
    if(ptype == PIECE_PAWN && abs(dst - src) == BOARD_LEN * 2)
        board->enpas = dst + (team == TEAM_WHITE ? diroffs[DIR_S] : diroffs[DIR_N]);

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
            board->pboards[team][PIECE_ROOK] ^= (uint64_t) 1 << (dst - 1);
            board->pboards[team][PIECE_ROOK] ^= (uint64_t) 1 << (dst + 1);
        }
        // queenside
        else
        {
            board->pboards[team][PIECE_ROOK] ^= (uint64_t) 1 << (dst - 2);
            board->pboards[team][PIECE_ROOK] ^= (uint64_t) 1 << (dst + 1);
        }
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

    board->pboards[team][ptype] &= ~((uint64_t) 1 << src);
    board->pboards[team][PIECE_NONE] &= ~((uint64_t) 1 << dst);
    board->pboards[team][newtype] |= (uint64_t) 1 << dst;
    board->pboards[team][PIECE_NONE] |= (uint64_t) 1 << dst;

    move_findattacks(board);
    move_findpins(board);
    board_findcheck(board);

    mspingen += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;

    board->tomove = !board->tomove;
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

static bool move_islegal(board_t* board, move_t move)
{
    int i;
    uint8_t dcur;
    pinline_t *curpin;

    uint8_t src, dst, type, dstart, dend;
    team_e team;
    piece_e ptype;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    for(team=0; team<TEAM_COUNT; team++)
        if(((uint64_t) 1 << src) & board->pboards[team][PIECE_NONE])
            break;

    if(team >= TEAM_COUNT)
        return false;

    for(ptype=0; ptype<PIECE_COUNT; ptype++)
        if(((uint64_t) 1 << src) & board->pboards[team][ptype])
            break;

    if(ptype >= PIECE_COUNT)
        return false;

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

        for(dcur=dstart; dcur<=dend; dcur++)
            if(((uint64_t) 1 << dcur) & board->attacks[!team])
                return false;

        return true;
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

static moveset_t* move_addiflegal(board_t* board, moveset_t* moves, move_t move)
{
    moveset_t *newmove;

    if(!move_islegal(board, move))
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
    bool nocapture, movetype_e type
)
{
    int i;

    team_e team;
    int idx;
    move_t move;

    for(team=0; team<TEAM_COUNT; team++)
        if(((uint64_t) 1 << src) & board->pboards[team][PIECE_NONE])
            break;
    if(team >= TEAM_COUNT)
        return set;

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
        set = move_addiflegal(board, set, move);

        if(((uint64_t) 1 << idx) & board->pboards[!team][PIECE_NONE])
            break;
    }

    return set;
}

static void move_sweepatk(board_t* board, bitboard_t* bits, uint8_t src, dir_e dir, int max)
{
    int i;

    int idx;

    for(i=0, idx=src+diroffs[dir]; i<max; i++, idx+=diroffs[dir])
    {
        *bits |= (uint64_t) 1 << idx;

        if((board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE]) & PIECE_MASK_TYPE)
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
        move_sweepatk(board, &board->attacks[team], src, i, sweeptable[src][i]);
}

static void move_rookatk(board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=0; i<DIR_NE; i++)
        move_sweepatk(board, &board->attacks[team], src, i, sweeptable[src][i]);
}

static void move_queenatk(board_t* board, uint8_t src, team_e team)
{
    int i;

    for(i=0; i<DIR_COUNT; i++)
        move_sweepatk(board, &board->attacks[team], src, i, sweeptable[src][i]);
}

static void move_kingatk(board_t* board, uint8_t src, team_e team)
{
    int i;
    
    for(i=0; i<DIR_COUNT; i++)
        if(sweeptable[src][i])
            move_sweepatk(board, &board->attacks[team], src, i, sweeptable[src][i]);
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
            if(!(pmask & board->pboards[i][PIECE_NONE]))
                continue;

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
            if(board->pboards[i][PIECE_ROOK] & pmask)
                for(dir=0; dir<DIR_NE; dir++) move_sweeppin(board, i, dir, board->ptable[i][j]);
            if(board->pboards[i][PIECE_BISHOP] & pmask)
                for(dir=DIR_NE; dir<DIR_COUNT; dir++) move_sweeppin(board, i, dir, board->ptable[i][j]);
        }
    }
}

static moveset_t* move_pawnmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;
    int type;

    bool dbl;
    int dst;
    int starttype, stoptype;
    dir_e dir, atk[2];
    move_t move;
    bitboard_t srcmask, dstmask;
    team_e team;

    srcmask = (uint64_t) 1 << src;
    starttype = stoptype = MOVETYPE_DEFAULT;

    if(board->pboards[TEAM_BLACK][PIECE_PAWN] & srcmask)
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

        team = TEAM_BLACK;
    }
    else
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

        team = TEAM_WHITE;
    }

    dst = src + diroffs[dir];
    dstmask = (uint64_t) 1 << dst;
    if(!((board->ptable[dst][TEAM_WHITE] | board->ptable[dst][TEAM_BLACK]) & dstmask))
    {
        for(type=starttype; type<=stoptype; type++)
        {
            move = src;
            move |= ((uint16_t)dst) << MOVEBITS_DST_BITS;
            move |= ((uint16_t)type) << MOVEBITS_TYP_BITS;
            set = move_addiflegal(board, set, move);
        }

        dst += diroffs[dir];
        dstmask = (uint64_t) 1 << dst;
        if(dbl && !((board->ptable[dst][TEAM_WHITE] | board->ptable[dst][TEAM_BLACK]) & dstmask))
        {
            move = src;
            move |= ((uint16_t)dst) << MOVEBITS_DST_BITS;
            set = move_addiflegal(board, set, move);
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
            set = move_addiflegal(board, set, move);
            
            // no need to check normal attacks since logically the square must be clear
            continue;
        }

        if(!(board->ptable[dst][!team] & dstmask))
            continue;
        
        for(type=starttype; type<=stoptype; type++)
        {
            move = 0;
            move |= src;
            move |= dst << MOVEBITS_DST_BITS;
            move |= ((uint16_t)type) << MOVEBITS_TYP_BITS;
            set = move_addiflegal(board, set, move);
        }
    }

    return set;
}

static moveset_t* move_knightmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    int dst;
    move_t move;
    team_e team;

    if(board->pboards[TEAM_WHITE][PIECE_KNIGHT] & (uint64_t) 1 << src)
        team = TEAM_WHITE;
    else
        team = TEAM_BLACK;
    
    for(i=0; i<8; i++)
    {
        dst = move_knightoffs(i, src);
        if(dst < 0)
            continue;

        if(board->pboards[team][PIECE_NONE] & (uint64_t) 1 << dst)
            continue;

        move = src;
        move |= dst << MOVEBITS_DST_BITS;
        set = move_addiflegal(board, set, move);
    }

    return set;
}

static moveset_t* move_bishopmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=DIR_NE; i<DIR_COUNT; i++)
        set = move_sweep(set, board, src, i, sweeptable[src][i], false, MOVETYPE_DEFAULT);

    return set;
}

static moveset_t* move_rookmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=0; i<DIR_NE; i++)
        set = move_sweep(set, board, src, i, sweeptable[src][i], false, MOVETYPE_DEFAULT);

    return set;
}

static moveset_t* move_kingmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    team_e team;
    move_t move;
    bitboard_t allpiece;

    team = TEAM_WHITE;
    if(board->pboards[TEAM_BLACK][PIECE_NONE] & (uint64_t) 1 << src)
        team = TEAM_BLACK;

    allpiece = board->pboards[TEAM_WHITE][PIECE_NONE] | board->pboards[TEAM_BLACK][PIECE_NONE];

    for(i=0; i<DIR_COUNT; i++)
        if(sweeptable[src][i])
            set = move_sweep(set, board, src, i, 1, false, MOVETYPE_DEFAULT);

    if(board->kcastle[team] && !(allpiece & ((uint64_t) 1 << (src + 1) | (uint64_t) 1 << (src + 2))))
    {
        move = src;
        move |= ((uint16_t)src+2) << MOVEBITS_DST_BITS;
        move |= ((uint16_t)MOVETYPE_CASTLE) << MOVEBITS_TYP_BITS;
        set = move_addiflegal(board, set, move);
    }

    if(board->qcastle[team]
    && !(allpiece & ((uint64_t) 1 << (src - 1) | (uint64_t) 1 << (src - 2) | (uint64_t) 1 << (src - 3))))
    {
        move = src;
        move |= ((uint16_t)src-2) << MOVEBITS_DST_BITS;
        move |= ((uint16_t)MOVETYPE_CASTLE) << MOVEBITS_TYP_BITS;
        set = move_addiflegal(board, set, move);
    }

    return set;
}

static moveset_t* move_queenmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=0; i<DIR_COUNT; i++)
        set = move_sweep(set, board, src, i, sweeptable[src][i], false, MOVETYPE_DEFAULT);

    return set;
}

double mspseudo = 0;

moveset_t* move_legalmoves(board_t* board, moveset_t* moves, uint8_t src)
{
    clock_t start;
    bitboard_t srcmask;

    start = clock();

    srcmask = (uint64_t) 1 << src;
    if((board->pboards[TEAM_WHITE][PIECE_KING] | board->pboards[TEAM_BLACK][PIECE_KING]) & srcmask)
        moves = move_kingmoves(moves, board, src);
    else if((board->pboards[TEAM_WHITE][PIECE_QUEEN] | board->pboards[TEAM_BLACK][PIECE_QUEEN]) & srcmask)
        moves = move_queenmoves(moves, board, src);
    else if((board->pboards[TEAM_WHITE][PIECE_ROOK] | board->pboards[TEAM_BLACK][PIECE_ROOK]) & srcmask)
        moves = move_rookmoves(moves, board, src);
    else if((board->pboards[TEAM_WHITE][PIECE_BISHOP] | board->pboards[TEAM_BLACK][PIECE_BISHOP]) & srcmask)
        moves = move_bishopmoves(moves, board, src);
    else if((board->pboards[TEAM_WHITE][PIECE_KNIGHT] | board->pboards[TEAM_BLACK][PIECE_KNIGHT]) & srcmask)
        moves = move_knightmoves(moves, board, src);
    else if((board->pboards[TEAM_WHITE][PIECE_PAWN] | board->pboards[TEAM_BLACK][PIECE_PAWN]) & srcmask)
        moves = move_pawnmoves(moves, board, src);

    mspseudo += (double) (clock() - start) / CLOCKS_PER_SEC * 1000.0;

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
