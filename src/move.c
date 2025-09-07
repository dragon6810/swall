#include "move.h"

#include <stdio.h>
#include <stdlib.h>

void move_domove(board_t* board, move_t move)
{
    int type, src, dst;
    piece_t piece, newp;
    team_e team;
    piece_e ptype;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;
    piece = board->pieces[src];
    team = TEAM_WHITE;
    if(piece & PIECE_MASK_COLOR)
        team = TEAM_BLACK;
    ptype = piece & PIECE_MASK_TYPE;

    // double pawn push
    board->enpas = 0xFF;
    if((board->pieces[src] & PIECE_MASK_TYPE) == PIECE_PAWN
    && abs(dst - src) == BOARD_LEN * 2)
        board->enpas = dst;

    if(ptype == PIECE_KING)
        board->kcastle[team] = board->qcastle[team] = false;
    if(ptype == PIECE_ROOK)
    {
        if(src % BOARD_LEN == BOARD_LEN - 1)
            board->kcastle[team] = false;
        if(!(src % BOARD_LEN))
            board->qcastle[team] = false;
    }

    if(type == MOVETYPE_ENPAS)
        board->pieces[src / BOARD_LEN * BOARD_LEN + dst % BOARD_LEN] = 0;
    if(type == MOVETYPE_CASTLE)
    {
        // kingside
        if(dst > src)
        {
            board->pieces[dst - 1] = board->pieces[dst + 1];
            board->pieces[dst + 1] = 0;
        }
        // queenside
        else
        {
            board->pieces[dst + 1] = board->pieces[dst - 2];
            board->pieces[dst - 2] = 0;
        }
    }

    switch(type)
    {
    case MOVETYPE_PROMQ:
    case MOVETYPE_PROMR:
    case MOVETYPE_PROMB:
    case MOVETYPE_PROMN:
        newp = piece & PIECE_MASK_COLOR;
        newp |= PIECE_QUEEN + type - MOVETYPE_PROMQ;
        break;
    default:
        newp = piece;
    }

    board->pieces[dst] = newp;
    board->pieces[src] = 0;

    board_findpieces(board);
    move_findattacks(board);
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

// if bits is not null, it will ignore set and fill out the bitboard
static moveset_t* move_sweep
(
    moveset_t* set, bitboard_t bits, board_t* board, 
    uint8_t src, dir_e dir, int max, 
    bool nocapture, movetype_e type
)
{
    int i;

    piece_t piece;
    int idx, add;
    moveset_t *newset, *newmove;

    piece = board->pieces[src];

    newset = set;

    if(!max)
        return newset;

    for(i=0, add=diroffs[dir], idx=src+add; i<max; i++, idx+=add)
    {
        // friendly
        if(((board->pieces[idx] & PIECE_MASK_TYPE) != PIECE_NONE)
        && ((board->pieces[idx] & PIECE_MASK_COLOR) == (piece & PIECE_MASK_COLOR))
        && !bits)
            break;

        // enemy, but we can't capture
        if(nocapture && (board->pieces[idx] & PIECE_MASK_TYPE) != PIECE_NONE)
            break;

        if(bits)
        {
            bits[idx / BOARD_LEN] |= 1 << (idx % BOARD_LEN);
        }
        else
        {
            newmove = malloc(sizeof(moveset_t));
            newmove->move = 0;
            newmove->move |= src;
            newmove->move |= idx << MOVEBITS_DST_BITS;
            newmove->move |= ((uint16_t)type) << MOVEBITS_TYP_BITS;
            newmove->next = newset;
            newset = newmove;
        }

        // enemy, but we captured.
        if((board->pieces[idx] & PIECE_MASK_TYPE) != PIECE_NONE)
            break;
    }

    return newset;
}

static void move_pawnatk(board_t* board, uint8_t src)
{
    int i;
    
    int f;
    team_e team;
    int dst;
    dir_e dirs[2];

    team = TEAM_WHITE;
    if(board->pieces[src] & PIECE_MASK_COLOR)
        team = TEAM_BLACK;

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
        board->attacks[team][dst / BOARD_LEN] |= 1 << (dst % BOARD_LEN);
    }
}

static void move_knightatk(board_t* board, uint8_t src)
{
    int i;
    
    team_e team;
    int dst;

    team = TEAM_WHITE;
    if(board->pieces[src] & PIECE_MASK_COLOR)
        team = TEAM_BLACK;

    for(i=0; i<8; i++)
    {
        dst = move_knightoffs(i, src);
        if(dst < 0)
            continue;

        board->attacks[team][dst / BOARD_LEN] |= 1 << (dst % BOARD_LEN);
    }
}

static void move_bishopatk(board_t* board, uint8_t src)
{
    int i;
    
    team_e team;

    team = TEAM_WHITE;
    if(board->pieces[src] & PIECE_MASK_COLOR)
        team = TEAM_BLACK;

    for(i=DIR_NE; i<DIR_COUNT; i++)
        move_sweep(NULL, board->attacks[team], board, src, i, move_maxsweep(i, src), false, MOVETYPE_DEFAULT);
}

static void move_rookatk(board_t* board, uint8_t src)
{
    int i;
    
    team_e team;

    team = TEAM_WHITE;
    if(board->pieces[src] & PIECE_MASK_COLOR)
        team = TEAM_BLACK;

    for(i=0; i<DIR_NE; i++)
        move_sweep(NULL, board->attacks[team], board, src, i, move_maxsweep(i, src), false, MOVETYPE_DEFAULT);
}

static void move_queenatk(board_t* board, uint8_t src)
{
    int i;
    
    team_e team;

    team = TEAM_WHITE;
    if(board->pieces[src] & PIECE_MASK_COLOR)
        team = TEAM_BLACK;

    for(i=0; i<DIR_COUNT; i++)
        move_sweep(NULL, board->attacks[team], board, src, i, move_maxsweep(i, src), false, MOVETYPE_DEFAULT);
}

static void move_kingatk(board_t* board, uint8_t src)
{
    int i;
    
    team_e team;

    team = TEAM_WHITE;
    if(board->pieces[src] & PIECE_MASK_COLOR)
        team = TEAM_BLACK;

    for(i=0; i<DIR_COUNT; i++)
        if(move_maxsweep(i, src))
            move_sweep(NULL, board->attacks[team], board, src, i, 1, false, MOVETYPE_DEFAULT);
}

void move_findattacks(board_t* board)
{
    int i, j;

    for(i=0; i<TEAM_COUNT; i++)
    {
        *((uint64_t*)&board->attacks[i]) = 0;
        for(j=0; j<board->npieces[i]; j++)
        {
            switch(board->pieces[board->quickp[i][j]] & PIECE_MASK_TYPE)
            {
            case PIECE_KING:
                move_kingatk(board, board->quickp[i][j]);
                break;
            case PIECE_QUEEN:
                move_queenatk(board, board->quickp[i][j]);
                break;
            case PIECE_ROOK:
                move_rookatk(board, board->quickp[i][j]);
                break;
            case PIECE_BISHOP:
                move_bishopatk(board, board->quickp[i][j]);
                break;
            case PIECE_KNIGHT:
                move_knightatk(board, board->quickp[i][j]);
                break;
            case PIECE_PAWN:
                move_pawnatk(board, board->quickp[i][j]);
                break;
            default:
                break;
            }
        }

        printf("atk %d:\n", i);
        board_printbits(board->attacks[i]);
    }
}

void move_findpins(board_t* board)
{

}


static moveset_t* move_pawnmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;
    int type;

    int max, dst;
    int starttype, stoptype;
    dir_e dir, atk[2];
    moveset_t *move;

    starttype = stoptype = MOVETYPE_DEFAULT;

    if(board->pieces[src] & PIECE_MASK_COLOR)
    {
        if(src / BOARD_LEN == 1)
        {
            starttype = MOVETYPE_PROMQ;
            stoptype = MOVETYPE_PROMN;
        }
        max = (src / BOARD_LEN == BOARD_LEN - 2) + 1;
        dir = DIR_S;
        atk[0] = DIR_SW;
        atk[1] = DIR_SE;
    }
    else
    {
        if(src / BOARD_LEN == BOARD_LEN-2)
        {
            starttype = MOVETYPE_PROMQ;
            stoptype = MOVETYPE_PROMN;
        }
        max = (src / BOARD_LEN == 1) + 1;
        dir = DIR_N;
        atk[0] = DIR_NW;
        atk[1] = DIR_NE;
    }

    for(type=starttype; type<=stoptype; type++)
        set = move_sweep(set, NULL, board, src, dir, max, true, type);

    for(i=0; i<2; i++)
    {
        if(!(src % BOARD_LEN) && !i)
            continue;
        if(src % BOARD_LEN == BOARD_LEN-1 && i)
            continue;

        dst = src + diroffs[atk[i]];

        // en passant requites board->enpas have rank of src and file of dst
        if(board->enpas / BOARD_LEN == src / BOARD_LEN
        && board->enpas % BOARD_LEN == dst % BOARD_LEN)
        {
            move = malloc(sizeof(moveset_t));
            move->move = 0;
            move->move |= src;
            move->move |= dst << MOVEBITS_DST_BITS;
            move->move |= ((uint16_t)MOVETYPE_ENPAS) << MOVEBITS_TYP_BITS;
            move->next = set;
            set = move;
            
            // no need to check since logically the square must be clear
            continue;
        }

        if(!(board->pieces[dst] & PIECE_MASK_TYPE)
        || (board->pieces[dst] & PIECE_MASK_COLOR) == (board->pieces[src] & PIECE_MASK_COLOR))
            continue;
        
        for(type=starttype; type<=stoptype; type++)
        {
            move = malloc(sizeof(moveset_t));
            move->move = 0;
            move->move |= src;
            move->move |= dst << MOVEBITS_DST_BITS;
            move->move |= ((uint16_t)type) << MOVEBITS_TYP_BITS;
            move->next = set;
            set = move;
        }
    }

    return set;
}

static moveset_t* move_knightmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    int dst;
    moveset_t *move;
    
    for(i=0; i<8; i++)
    {
        dst = move_knightoffs(i, src);
        if(dst < 0)
            continue;

        if((board->pieces[dst] & PIECE_MASK_TYPE) != PIECE_NONE
        && ((board->pieces[dst] & PIECE_MASK_COLOR) == (board->pieces[src] & PIECE_MASK_COLOR)))
            continue;

        move = malloc(sizeof(moveset_t));
        move->move = 0;
        move->move |= src;
        move->move |= dst << MOVEBITS_DST_BITS;
        move->next = set;
        set = move;
    }

    return set;
}

static moveset_t* move_bishopmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=DIR_NE; i<DIR_COUNT; i++)
        set = move_sweep(set, NULL, board, src, i, move_maxsweep(i, src), false, MOVETYPE_DEFAULT);

    return set;
}

static moveset_t* move_rookmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=0; i<DIR_NE; i++)
        set = move_sweep(set, NULL, board, src, i, move_maxsweep(i, src), false, MOVETYPE_DEFAULT);

    return set;
}

static moveset_t* move_kingmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    team_e team;
    moveset_t *move;

    team = TEAM_WHITE;
    if(board->pieces[src] & PIECE_MASK_COLOR)
        team = TEAM_BLACK;

    for(i=0; i<DIR_COUNT; i++)
        if(move_maxsweep(i, src))
            set = move_sweep(set, NULL, board, src, i, 1, false, MOVETYPE_DEFAULT);

    if(board->kcastle[team] 
    && !(board->pieces[src+1] & PIECE_MASK_TYPE)
    && !(board->pieces[src+2] & PIECE_MASK_TYPE))
    {
        move = malloc(sizeof(moveset_t));
        move->move = 0;
        move->move |= src;
        move->move |= ((uint16_t)src+2) << MOVEBITS_DST_BITS;
        move->move |= ((uint16_t)MOVETYPE_CASTLE) << MOVEBITS_TYP_BITS;
        move->next = set;
        set = move;
    }

    if(board->qcastle[team] 
    && !(board->pieces[src-1] & PIECE_MASK_TYPE)
    && !(board->pieces[src-2] & PIECE_MASK_TYPE)
    && !(board->pieces[src-3] & PIECE_MASK_TYPE))
    {
        move = malloc(sizeof(moveset_t));
        move->move = 0;
        move->move |= src;
        move->move |= ((uint16_t)src-2) << MOVEBITS_DST_BITS;
        move->move |= ((uint16_t)MOVETYPE_CASTLE) << MOVEBITS_TYP_BITS;
        move->next = set;
        set = move;
    }

    return set;
}

static moveset_t* move_queenmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=0; i<DIR_COUNT; i++)
        set = move_sweep(set, NULL, board, src, i, move_maxsweep(i, src), false, MOVETYPE_DEFAULT);

    return set;
}

moveset_t* move_legalmoves(board_t* board, uint8_t src)
{
    piece_e type;

    type = board->pieces[src] & PIECE_MASK_TYPE;
    switch(type)
    {
    case PIECE_KING:
        return move_kingmoves(NULL, board, src);
    case PIECE_QUEEN:
        return move_queenmoves(NULL, board, src);
    case PIECE_ROOK:
        return move_rookmoves(NULL, board, src);
    case PIECE_BISHOP:
        return move_bishopmoves(NULL, board, src);
    case PIECE_KNIGHT:
        return move_knightmoves(NULL, board, src);
    case PIECE_PAWN:
        return move_pawnmoves(NULL, board, src);
    default:
        return NULL;
    }
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