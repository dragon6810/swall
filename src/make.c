#include "move.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void move_make(board_t* board, move_t move, mademove_t* outmove)
{
    int type, src, dst;
    team_e team;
    piece_e ptype, newtype, captype;
    bitboard_t srcmask, dstmask, cstlsrc, cstldst;
    int cstlsrcsqr, cstldstsqr;
    int enpasoffs;

    outmove->move = move;
    outmove->captured = PIECE_NONE;
    outmove->enpas = board->enpas;
    outmove->kcastle[0] = board->kcastle[0];
    outmove->kcastle[1] = board->kcastle[1];
    outmove->qcastle[0] = board->qcastle[0];
    outmove->qcastle[1] = board->qcastle[1];

    memcpy(outmove->npiece, board->npiece, sizeof(board->npiece));
    memcpy(outmove->ptable, board->ptable, sizeof(board->ptable));
    memcpy(outmove->attacks, board->attacks, sizeof(board->attacks));
    memcpy(outmove->npins, board->npins, sizeof(board->npins));
    memcpy(outmove->pins, board->pins, sizeof(board->pins));
    memcpy(outmove->check, board->check, sizeof(board->check));
    
    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    srcmask = (uint64_t) 1 << src;
    dstmask = (uint64_t) 1 << dst;

    team = board->tomove;
    ptype = board->sqrs[src] & SQUARE_MASK_TYPE;

    enpasoffs = board->tomove == TEAM_WHITE ? diroffs[DIR_S] : diroffs[DIR_N];
    if(type == MOVETYPE_ENPAS)
    {
        outmove->captured = PIECE_PAWN;
        board->sqrs[board->enpas + enpasoffs] = SQUARE_EMPTY;
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
            cstlsrcsqr = dst + 1;
            cstldstsqr = dst - 1;
        }
        // queenside
        else
        {
            cstlsrcsqr = dst - 2;
            cstldstsqr = dst + 1;
        }

        cstlsrc = (uint64_t) 1 << cstlsrcsqr;
        cstldst = (uint64_t) 1 << cstldstsqr;

        board->sqrs[cstldstsqr] = board->sqrs[cstlsrcsqr];
        board->sqrs[cstlsrcsqr] = SQUARE_EMPTY;
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

    board->sqrs[dst] = team << SQUARE_BITS_TEAM | newtype;
    board->sqrs[src] = SQUARE_EMPTY;
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
            
            if(captype == PIECE_ROOK)
            {
                if((team == TEAM_BLACK && dst / BOARD_LEN == 0)
                || (team == TEAM_WHITE && dst / BOARD_LEN == BOARD_LEN - 1))
                {
                    if(dst % BOARD_LEN == BOARD_LEN - 1)
                        board->kcastle[!team] = false;
                    else if(!(dst % BOARD_LEN))
                        board->qcastle[!team] = false;
                }
            }

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
}

// there's a lot of repeated code from move_domove.
// maybe figure out how to generalize this.
void move_unmake(board_t* board, const mademove_t* move)
{
    int i;

    int type, src, dst;
    team_e team;
    piece_e ptype, oldtype;
    bitboard_t srcmask, dstmask, cstlsrc, cstldst;
    int cstlsrcsqr, cstldstsqr;
    int enpasoffs;

    board->enpas = move->enpas;
    for(i=0; i<TEAM_COUNT; i++)
    {
        board->kcastle[i] = move->kcastle[i];
        board->qcastle[i] = move->qcastle[i];
    }
    
    src = move->move & MOVEBITS_SRC_MASK;
    dst = (move->move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    srcmask = (uint64_t) 1 << src;
    dstmask = (uint64_t) 1 << dst;

    team = !board->tomove;
    ptype = board->sqrs[dst] & SQUARE_MASK_TYPE;

    if(type == MOVETYPE_CASTLE)
    {
        // kingside
        if(dst > src)
        {
            cstlsrcsqr = dst + 1;
            cstldstsqr = dst - 1;
        }
        // queenside
        else
        {
            cstlsrcsqr = dst - 2;
            cstldstsqr = dst + 1;
        }

        cstlsrc = (uint64_t) 1 << cstlsrcsqr;
        cstldst = (uint64_t) 1 << cstldstsqr;

        board->sqrs[cstlsrcsqr] = board->sqrs[cstldstsqr];
        board->sqrs[cstldstsqr] = SQUARE_EMPTY;
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

    board->sqrs[dst] = SQUARE_EMPTY;
    board->sqrs[src] = team << SQUARE_BITS_TEAM | oldtype;
    board->pboards[team][PIECE_NONE] ^= dstmask;
    board->pboards[team][ptype] ^= dstmask;
    board->pboards[team][PIECE_NONE] ^= srcmask;
    board->pboards[team][oldtype] ^= srcmask;

    if(type == MOVETYPE_ENPAS)
    {
        enpasoffs = team == TEAM_WHITE ? diroffs[DIR_S] : diroffs[DIR_N];
        board->sqrs[board->enpas + enpasoffs] = !team << SQUARE_BITS_TEAM | PIECE_PAWN;
        board->pboards[!team][PIECE_NONE] ^= (uint64_t) 1 << (board->enpas + enpasoffs);
        board->pboards[!team][PIECE_PAWN] ^= (uint64_t) 1 << (board->enpas + enpasoffs);
    }
    else if(move->captured)
    {
        board->sqrs[dst] = !team << SQUARE_BITS_TEAM | move->captured;
        board->pboards[!team][PIECE_NONE] ^= dstmask;
        board->pboards[!team][move->captured] ^= dstmask;
    }
    
    board->tomove = team;

    memcpy(board->npiece, move->npiece, sizeof(board->npiece));
    memcpy(board->ptable, move->ptable, sizeof(board->ptable));
    memcpy(board->attacks, move->attacks, sizeof(board->attacks));
    memcpy(board->npins, move->npins, sizeof(board->npins));
    memcpy(board->pins, move->pins, sizeof(board->pins));
    memcpy(board->check, move->check, sizeof(board->check));
}
