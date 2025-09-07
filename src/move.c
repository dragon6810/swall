#include "move.h"

#include <stdio.h>
#include <stdlib.h>

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

static moveset_t* move_sweep(moveset_t* set, board_t* board, uint8_t src, dir_e dir, int max)
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
        && ((board->pieces[idx] & PIECE_MASK_COLOR) == (piece & PIECE_MASK_COLOR)))
            break;

        newmove = malloc(sizeof(moveset_t));
        newmove->move = 0;
        newmove->move |= src;
        newmove->move |= idx << MOVEBITS_DST_BITS;
        newmove->next = newset;
        newset = newmove;

        // enemy, but we captured.
        if((board->pieces[idx] & PIECE_MASK_TYPE) != PIECE_NONE)
            break;
    }

    return newset;
}

static moveset_t* move_pawnmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int max;
    dir_e dir;

    if(board->pieces[src] & PIECE_MASK_COLOR)
    {
        max = (src / BOARD_LEN == BOARD_LEN - 2) + 1;
        dir = DIR_S;
    }
    else
    {
        max = (src / BOARD_LEN == 1) + 1;
        dir = DIR_N;
    }

    set = move_sweep(set, board, src, dir, max);

    return set;
}

static moveset_t* move_bishopmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=DIR_NE; i<DIR_COUNT; i++)
        set = move_sweep(set, board, src, i, move_maxsweep(i, src));

    return set;
}

static moveset_t* move_rookmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=0; i<DIR_NE; i++)
        set = move_sweep(set, board, src, i, move_maxsweep(i, src));

    return set;
}

static moveset_t* move_kingmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=0; i<DIR_COUNT; i++)
        if(move_maxsweep(i, src))
            set = move_sweep(set, board, src, i, 1);

    return set;
}

static moveset_t* move_queenmoves(moveset_t* set, board_t* board, uint8_t src)
{
    int i;

    for(i=0; i<DIR_COUNT; i++)
        set = move_sweep(set, board, src, i, move_maxsweep(i, src));

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
        return NULL;
    case PIECE_PAWN:
        return move_pawnmoves(NULL, board, src);
    default:
        return NULL;
    }
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