#include "move.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bitboard_t nocastlefrom[TEAM_COUNT][2];
bitboard_t nocastlecap[TEAM_COUNT][2];

static inline void move_docapture(board_t* restrict board, move_t move, mademove_t* restrict made)
{
    int dst;
    team_e team;
    piece_e capture;
    bitboard_t dstmask;

    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    team = board->tomove;
    capture = board->sqrs[dst] & SQUARE_MASK_TYPE;

    if(!capture)
        return;

    dstmask = (uint64_t) 1 << dst;

    made->captured = capture;
    board->pboards[!team][PIECE_NONE] ^= dstmask;
    board->pboards[!team][capture] ^= dstmask;
}

// reversible
static inline void move_docastle(board_t* restrict board, move_t move, team_e team)
{
    int src, dst;
    int rooksrc, rookdst;
    bitboard_t rookmask;
    square_t temp;

    if((move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS != MOVETYPE_CASTLE)
        return;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;

    // queenside
    if(dst < src)
    {
        rooksrc = dst - 2;
        rookdst = dst + 1;
    }
    // kingside
    else
    {
        rooksrc = dst + 1;
        rookdst = dst - 1;
    }

    rookmask = ((uint64_t) 1 << rooksrc) | ((uint64_t) 1 << rookdst);

    temp = board->sqrs[rooksrc];
    board->sqrs[rooksrc] = board->sqrs[rookdst];
    board->sqrs[rookdst] = temp;
    board->pboards[team][PIECE_NONE] ^= rookmask;
    board->pboards[team][PIECE_ROOK] ^= rookmask;
}

static inline void move_updatecastlerights(board_t* restrict board, move_t move)
{
    int src, dst;
    team_e team;
    bitboard_t srcmask, dstmask;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    team = board->tomove;

    srcmask = (bitboard_t) 1 << src;
    dstmask = (bitboard_t) 1 << dst;

    if(board->kcastle[team] && srcmask & nocastlefrom[team][0])
    {
        board->hash ^= zobrist_hashes[768 + team * 2];
        board->kcastle[team] = false;
    }
    if(board->qcastle[team] && srcmask & nocastlefrom[team][1])
    {
        board->hash ^= zobrist_hashes[769 + team * 2];
        board->qcastle[team] = false;
    }

    if(board->kcastle[!team] && dstmask & nocastlecap[!team][0])
    {
        board->hash ^= zobrist_hashes[768 + !team * 2];
        board->kcastle[!team] = false;
    }
    if(board->qcastle[!team] && dstmask & nocastlecap[!team][1])
    {
        board->hash ^= zobrist_hashes[769 + !team * 2];
        board->qcastle[!team] = false;
    }
}

static inline void move_updateenpas(board_t* restrict board, move_t move, mademove_t* made)
{
    int src, dst;
    team_e team;
    piece_e piece;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    team = board->tomove;
    piece = board->sqrs[src] & SQUARE_MASK_TYPE;

    board->enpas = 0xFF;

    if(piece != PIECE_PAWN)
        return;

    if(abs(dst - src) == BOARD_LEN * 2)
        board->enpas = src + PAWN_OFFS(team);
}

static inline void move_doenpas(board_t* restrict board, move_t move)
{
    int type;
    team_e team;
    int tile;
    bitboard_t mask;
    
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;
    team = board->tomove;

    if(type != MOVETYPE_ENPAS)
        return;

    tile = board->enpas + PAWN_OFFS(!team);
    mask = (uint64_t) 1 << tile;

    board->sqrs[tile] = SQUARE_EMPTY;
    board->pboards[!team][PIECE_NONE] &= ~mask;
    board->pboards[!team][PIECE_PAWN] &= ~mask;
}

static inline void move_updatefiftymove(board_t* restrict board, move_t move)
{
    int src, dst;
    piece_e piece;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;

    piece = board->sqrs[src] & SQUARE_MASK_TYPE;

    if(piece == PIECE_PAWN)
        goto clear;
    if(board->sqrs[dst] & SQUARE_MASK_TYPE)
        goto clear;

    return;

clear:
    board->fiftymove = 0;
}

static inline void move_makehash(board_t* restrict board, move_t move)
{
    team_e team;
    movetype_e type;
    int8_t src, dst, rooka, rookb;
    piece_e piece, newtype, capture;

    team = board->tomove;
    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    piece = board->sqrs[src] & SQUARE_MASK_TYPE;
    capture = board->sqrs[dst] & SQUARE_MASK_TYPE;
    newtype = piece;
    if(type >= MOVETYPE_PROMQ && type <= MOVETYPE_PROMN)
        newtype = PIECE_QUEEN + type - MOVETYPE_PROMQ;

    board->hash ^= zobrist_hashes[BOARD_AREA * zobrist_piecetohash[team][piece] + src];
    if(capture)
        board->hash ^= zobrist_hashes[BOARD_AREA * zobrist_piecetohash[!team][capture] + dst];

    board->hash ^= zobrist_hashes[BOARD_AREA * zobrist_piecetohash[team][newtype] + dst];

    if(type == MOVETYPE_ENPAS)
        board->hash ^= zobrist_hashes[BOARD_AREA * zobrist_piecetohash[!team][PIECE_PAWN] + board->enpas + PAWN_OFFS(!team)];
    
    if(type == MOVETYPE_CASTLE)
    {
        // queenside
        if(dst < src)
        {
            rooka = dst - 2;
            rookb = dst + 1;
        }
        else
        {
            rooka = dst - 1;
            rookb = dst + 1;
        }

        board->hash ^= zobrist_hashes[BOARD_AREA * zobrist_piecetohash[team][PIECE_ROOK] + rooka];
        board->hash ^= zobrist_hashes[BOARD_AREA * zobrist_piecetohash[team][PIECE_ROOK] + rookb];
    }

    if(board->enpas != 0xFF && (board->pboards[board->tomove][PIECE_PAWN] & pawnatk[!board->tomove][board->enpas]))
        board->hash ^= zobrist_hashes[772 + board->enpas % BOARD_LEN];

    if(piece == PIECE_PAWN && abs(dst - src) == BOARD_LEN * 2 
    && board->pboards[!team][PIECE_PAWN] & pawnatk[team][src + PAWN_OFFS(team)])
        board->hash ^= zobrist_hashes[772 + dst % BOARD_LEN];

    board->hash ^= zobrist_hashes[780];
}

static inline void move_copytomade(board_t* restrict board, move_t move, mademove_t* restrict made)
{
    int dst;

    made->move = move;
    made->captured = PIECE_NONE;
    made->enpas = board->enpas;
    made->castle = board->kcastle[TEAM_WHITE] << 3 
                 | board->qcastle[TEAM_WHITE] << 2 
                 | board->kcastle[TEAM_BLACK] << 1 
                 | board->qcastle[TEAM_BLACK];
    made->fiftymove = board->fiftymove;
    made->lastperm = board->lastperm;

    made->attacks = board->attacks;
    made->oldhash = board->hash;

    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    if(board->sqrs[dst])
        made->captured = board->sqrs[dst] & SQUARE_MASK_TYPE;
    else
        made->captured = 0;
}

static inline void move_copyfrommade(board_t* restrict board, const mademove_t* restrict made)
{
    board->enpas = made->enpas;
    board->kcastle[TEAM_WHITE] = made->castle >> 3 & 1;
    board->qcastle[TEAM_WHITE] = made->castle >> 2 & 1;
    board->kcastle[TEAM_BLACK] = made->castle >> 1 & 1;
    board->qcastle[TEAM_BLACK] = made->castle & 1;
    board->fiftymove = made->fiftymove;
    board->lastperm = made->lastperm;
    board->attacks = made->attacks;
    board->hash = made->oldhash;
}

static inline void move_updatelastperm(board_t* restrict board, move_t move)
{
    int src, dst;
    piece_e piece;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;

    piece = board->sqrs[src] & SQUARE_MASK_TYPE;

    if(piece == PIECE_PAWN || board->sqrs[dst] & SQUARE_MASK_TYPE)
        board->lastperm = board->nhistory;
}

void move_make(board_t* restrict board, move_t move, mademove_t* restrict outmove)
{
    int type, src, dst;
    team_e team;
    piece_e ptype, newtype;
    bitboard_t srcmask, dstmask;
    
    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    srcmask = (uint64_t) 1 << src;
    dstmask = (uint64_t) 1 << dst;

    team = board->tomove;
    ptype = board->sqrs[src] & SQUARE_MASK_TYPE;

    move_copytomade(board, move, outmove);
    move_makehash(board, move);
    move_updatelastperm(board, move);
    move_updatefiftymove(board, move);
    move_doenpas(board, move);
    move_updateenpas(board, move, outmove);
    move_updatecastlerights(board, move);
    move_docastle(board, move, team);
    move_docapture(board, move, outmove);

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
    board->pboards[team][PIECE_NONE] &= ~srcmask;
    board->pboards[team][ptype] &= ~srcmask;
    board->pboards[team][PIECE_NONE] |= dstmask;
    board->pboards[team][newtype] |= dstmask;

    board->tomove = !board->tomove;
    board_findpieces(board);
    board_checkstalemate(board);
    board->history[board->nhistory++] = board->hash;
}

// there's a lot of repeated code from move_domove.
// maybe figure out how to generalize this.
void move_unmake(board_t* restrict board, const mademove_t* restrict move)
{
    int type, src, dst;
    team_e team;
    piece_e ptype, oldtype;
    bitboard_t srcmask, dstmask;
    
    src = move->move & MOVEBITS_SRC_MASK;
    dst = (move->move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move->move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    srcmask = (uint64_t) 1 << src;
    dstmask = (uint64_t) 1 << dst;

    team = !board->tomove;
    ptype = board->sqrs[dst] & SQUARE_MASK_TYPE;

    move_docastle(board, move->move, team);
    move_copyfrommade(board, move);

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

    board->sqrs[src] = team << SQUARE_BITS_TEAM | oldtype;
    board->pboards[team][PIECE_NONE] ^= dstmask;
    board->pboards[team][ptype] ^= dstmask;
    board->pboards[team][PIECE_NONE] ^= srcmask;
    board->pboards[team][oldtype] ^= srcmask;

    if(type == MOVETYPE_ENPAS)
    {
        board->sqrs[board->enpas + PAWN_OFFS(!team)] = !team << SQUARE_BITS_TEAM | PIECE_PAWN;
        board->pboards[!team][PIECE_NONE] ^= (uint64_t) 1 << (board->enpas + PAWN_OFFS(!team));
        board->pboards[!team][PIECE_PAWN] ^= (uint64_t) 1 << (board->enpas + PAWN_OFFS(!team));
    }
    
    board->sqrs[dst] = SQUARE_EMPTY;
    if(move->captured)
    {
        board->sqrs[dst] = !team << SQUARE_BITS_TEAM | move->captured;
        board->pboards[!team][PIECE_NONE] ^= dstmask;
        board->pboards[!team][move->captured] ^= dstmask;
    }
    
    board->tomove = team;
    board->nhistory--;

    board_findpieces(board);
    board->stalemate = false;
}

void move_makeinit(void)
{
    const bitboard_t kinghome = 0x10;
    const bitboard_t krook = 0x80;
    const bitboard_t qrook = 0x01;

    team_e t;
    int homerank;

    for(t=0; t<TEAM_COUNT; t++)
    {
        homerank = t == TEAM_WHITE ? 0 : BOARD_LEN - 1;

        nocastlefrom[t][0] = (kinghome | krook) << homerank * BOARD_LEN;
        nocastlefrom[t][1] = (kinghome | qrook) << homerank * BOARD_LEN;

        nocastlecap[t][0] = krook << homerank * BOARD_LEN;
        nocastlecap[t][1] = qrook << homerank * BOARD_LEN;
    }
}
