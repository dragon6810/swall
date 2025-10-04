#include "move.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int16_t* move_threefold(board_t* board)
{
    int16_t *pval;

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

// reversible
static void move_docastle(board_t* board, move_t move, team_e team)
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

static void move_updatecastlerights(board_t* board, move_t move)
{
    int src, dst;
    team_e team;
    piece_e piece, capture;
    int ourrank, enemyrank;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    team = board->tomove;
    piece = board->sqrs[src] & SQUARE_MASK_TYPE;
    capture = board->sqrs[dst] & SQUARE_MASK_TYPE;
    
    enemyrank = team == TEAM_WHITE ? BOARD_LEN - 1 : 0;
    ourrank = team == TEAM_BLACK ? BOARD_LEN - 1 : 0;

    if(piece == PIECE_KING)
        board->kcastle[team] = board->qcastle[team] = false;

    else if(piece == PIECE_ROOK && src % BOARD_LEN == 0 && src / BOARD_LEN == ourrank)
        board->qcastle[team] = false;
    else if(piece == PIECE_ROOK && src % BOARD_LEN == BOARD_LEN - 1 && src / BOARD_LEN == ourrank)
        board->kcastle[team] = false;

    if(capture != PIECE_ROOK || dst / BOARD_LEN != enemyrank)
        return;

    if(dst % BOARD_LEN == 0)
        board->qcastle[!team] = false;
    else if(dst % BOARD_LEN == BOARD_LEN - 1)
        board->kcastle[!team] = false;
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

    board->enpas = 0xFF;

    if(piece != PIECE_PAWN)
        return;

    if(abs(dst - src) == BOARD_LEN * 2)
        board->enpas = src + PAWN_OFFS(team);
}

static void move_doenpas(board_t* board, move_t move)
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

static void move_updatefiftymove(board_t* board, move_t move)
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
    made->npins = board->npins;
    made->dblcheck = board->dblcheck;
    made->isthreat = board->isthreat;
    made->threat = board->threat;
    made->isenpaspin = board->isenpaspin;
    made->fiftymove = board->fiftymove;

    memcpy(made->npiece, board->npiece, sizeof(board->npiece));
    memcpy(made->ptable, board->ptable, sizeof(board->ptable));
    memcpy(made->attacks, board->attacks, sizeof(board->attacks));
    memcpy(made->pins, board->pins, sizeof(board->pins));
    memcpy(made->check, board->check, sizeof(board->check));

    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    if(board->sqrs[dst])
        made->captured = board->sqrs[dst] & SQUARE_MASK_TYPE;
    else
        made->captured = 0;
}

static void move_copyfrommade(board_t* board, const mademove_t* made)
{
    board->enpas = made->enpas;
    board->kcastle[0] = made->kcastle[0];
    board->kcastle[1] = made->kcastle[1];
    board->qcastle[0] = made->qcastle[0];
    board->qcastle[1] = made->qcastle[1];
    board->npins = made->npins;
    board->dblcheck = made->dblcheck;
    board->isthreat = made->isthreat;
    board->threat = made->threat;
    board->isenpaspin = made->isenpaspin;
    board->fiftymove = made->fiftymove;

    memcpy(board->npiece, made->npiece, sizeof(made->npiece));
    memcpy(board->ptable, made->ptable, sizeof(made->ptable));
    memcpy(board->attacks, made->attacks, sizeof(made->attacks));
    memcpy(board->pins, made->pins, sizeof(made->pins));
    memcpy(board->check, made->check, sizeof(made->check));
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
    board_update(board);
    (*move_threefold(board))++;

#ifdef PARANOID
    if(board->check[!board->tomove])
    {
        char str[MAX_LONGALG];

        move_tolongalg(move, str);

        printf("moved into check (%s):\n", str);
        board_print(board);

        printf("before move:\n");
        move_unmake(board, outmove);
        board_print(board);

        abort();
    }
#endif
}

// there's a lot of repeated code from move_domove.
// maybe figure out how to generalize this.
void move_unmake(board_t* board, const mademove_t* move)
{
    int i;

    int type, src, dst;
    team_e team;
    piece_e ptype, oldtype;
    bitboard_t srcmask, dstmask;

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

    move_docastle(board, move->move, team);

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

    move_copyfrommade(board, move);

    board->hash = zobrist_hash(board);
}
