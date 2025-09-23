#include "move.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAWN_OFFS(team) (team == TEAM_WHITE ? diroffs[DIR_N] : diroffs[DIR_S])

int16_t* move_threefold(board_t* board)
{
    int16_t *pval;

    board->stalemate = false;

    pval = zobrist_find(&board->threefold, board->hash);
    if(pval)
        return pval;

    zobrist_set(&board->threefold, board->hash, 0);
    return zobrist_find(&board->threefold, board->hash);
}

static void move_docapture(board_t* board, move_t move, mademove_t* made)
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

static void move_docastle(board_t* board, move_t move)
{
    int src, dst;
    team_e team;
    int rooksrc, rookdst;
    bitboard_t rookmask;

    if((move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS != MOVETYPE_CASTLE)
        return;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    team = board->tomove;

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

    board->sqrs[rooksrc] = SQUARE_EMPTY;
    board->sqrs[rookdst] = team << SQUARE_BITS_TEAM | PIECE_ROOK;
    board->pboards[team][PIECE_NONE] ^= rookmask;
    board->pboards[team][PIECE_ROOK] ^= rookmask;
}

static void move_updatecastlerights(board_t* board, move_t move)
{
    int src, dst;
    team_e team;
    piece_e piece, capture;
    int enemyrank;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    team = board->tomove;
    piece = board->sqrs[src] & SQUARE_MASK_TYPE;
    capture = board->sqrs[dst] & SQUARE_MASK_TYPE;

    if(piece == PIECE_KING)
        board->kcastle[team] = board->qcastle[team] = false;

    else if(piece == PIECE_ROOK && src == 0)
        board->qcastle[team] = false;
    else if(piece == PIECE_ROOK && src == BOARD_LEN - 1)
        board->kcastle[team] = false;

    enemyrank = team == TEAM_WHITE ? BOARD_LEN - 1 : 0;
    if(capture != PIECE_ROOK || dst / BOARD_LEN != enemyrank)
        return;

    if(dst % BOARD_LEN == 0)
        board->qcastle[!team] = false;
    else if(dst % BOARD_LEN == BOARD_LEN - 1)
        board->kcastle[!team] = false;
}

static bool move_capenpas(board_t* board, move_t move)
{
    int tile;
    team_e team;
    bitboard_t tilemask;

    if((move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS != MOVETYPE_ENPAS)
        return false;

    team = board->tomove;
    tile = board->enpas - PAWN_OFFS(team);
    tilemask = (uint64_t) 1 << tile;

    board->sqrs[tile] = SQUARE_EMPTY;
    board->pboards[!team][PIECE_NONE] ^= tilemask;
    board->pboards[!team][PIECE_PAWN] ^= tilemask;
    
    return true;
}

static void move_updateenpas(board_t* board, move_t move, mademove_t* made)
{
    int src, dst;
    team_e team;
    piece_e piece;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    team = board->tomove;
    piece = board->sqrs[src] & SQUARE_MASK_TYPE;

    if(piece != PIECE_PAWN)
        return;

    if(move_capenpas(board, move))
        made->captured = PIECE_PAWN;
    
    board->enpas = 0xFF;
    if(abs(dst - src) == BOARD_LEN * 2)
        board->enpas = src + PAWN_OFFS(team);
}

static void move_copytomade(board_t* board, move_t move, mademove_t* made)
{
    int dst;

    made->move = move;
    made->captured = PIECE_NONE;
    made->enpas = board->enpas;
    made->kcastle[0] = board->kcastle[0];
    made->kcastle[1] = board->kcastle[1];
    made->qcastle[0] = board->qcastle[0];
    made->qcastle[1] = board->qcastle[1];

    memcpy(made->npiece, board->npiece, sizeof(board->npiece));
    memcpy(made->ptable, board->ptable, sizeof(board->ptable));
    memcpy(made->attacks, board->attacks, sizeof(board->attacks));
    memcpy(made->npins, board->npins, sizeof(board->npins));
    memcpy(made->pins, board->pins, sizeof(board->pins));
    memcpy(made->check, board->check, sizeof(board->check));

    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    if(board->sqrs[dst])
        made->captured = board->sqrs[dst] & SQUARE_MASK_TYPE;
    else
        made->captured = 0;
}

void move_make(board_t* board, move_t move, mademove_t* outmove)
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
    move_updateenpas(board, move, outmove);
    move_updatecastlerights(board, move);
    move_docastle(board, move);
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
    board->pboards[team][ptype] ^= srcmask;
    board->pboards[team][PIECE_NONE] ^= srcmask;
    board->pboards[team][newtype] ^= dstmask;
    board->pboards[team][PIECE_NONE] ^= dstmask;

    board->tomove = !board->tomove;
    
    board_findpieces(board);
    move_findattacks(board);
    move_findpins(board);
    board_findcheck(board);
    board->hash = zobrist_hash(board);
    (*move_threefold(board))++;

    if(board->check[!board->tomove])
    {
        printf("legality pruning failed!\n");
        printf("%s moved %c%c -> %c%c into check:\n", !board->tomove ? "black" : "white",
            'a' + src % BOARD_LEN, '1' + src / BOARD_LEN, 'a' + dst % BOARD_LEN, '1' + dst / BOARD_LEN);
        board_print(board);
        printf("%s attack:\n", board->tomove ? "black" : "white");
        board_printbits(board->attacks[board->tomove][PIECE_NONE]);
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

    (*move_threefold(board))--;

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

    board->hash = zobrist_hash(board);
}
