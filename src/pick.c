#include "pick.h"

#include "eval.h"

static inline bool pick_trycapture(board_t* restrict board, move_t move, picker_t* restrict picker)
{
    movetype_e type;
    int8_t src, dst;
    piece_e piece, capture;
    score_t capscore;

    src = move & MOVEBITS_SRC_MASK;
    dst = (move & MOVEBITS_DST_MASK) >> MOVEBITS_DST_BITS;
    type = (move & MOVEBITS_TYP_MASK) >> MOVEBITS_TYP_BITS;

    capture = board->sqrs[dst] & SQUARE_MASK_TYPE;
    if(type == MOVETYPE_ENPAS)
        capture = PIECE_PAWN;

    if(!capture)
        return false;

    piece = board->sqrs[src] & SQUARE_MASK_TYPE;

    capscore = eval_pscore[capture];
    if(board->attacks & (bitboard_t) 1 << src)
        capscore -= eval_pscore[piece];

    if(capscore >= 0)
        picker->goodcap.moves[picker->goodcap.count++] = move;
    else
        picker->badcap.moves[picker->badcap.count++] = move;

    return true;
}

void pick_sort(board_t* restrict board, moveset_t* restrict moves, 
uint8_t depth, score_t alpha, score_t beta, picker_t* restrict picker)
{
    int i;

    transpos_t *transpos;
    move_t tt;

    picker->tt = 0;
    picker->checks.count = 0;
    picker->goodcap.count = 0;
    picker->quiet.count = 0;
    picker->badcap.count = 0;

    picker->state = PICK_CHECKS;
    picker->idx = 0;

    tt = 0;
    transpos = transpose_find(&board->ttable, board->hash, depth, alpha, beta, true);
    if(transpos)
        tt = transpos->bestmove;

    for(i=0; i<moves->count; i++)
    {
        if(moves->moves[i] == tt)
        {
            picker->tt = tt;
            picker->state = PICK_TT;
            continue;
        }

        if(pick_trycapture(board, moves->moves[i], picker))
            continue;

        if(move_givescheck(board, moves->moves[i]))
        {
            picker->checks.moves[picker->checks.count++] = moves->moves[i];
            continue;
        }

        picker->quiet.moves[picker->quiet.count++] = moves->moves[i];
    }
}

move_t pick(picker_t* restrict picker)
{
    move_t move;

    if(picker->state == PICK_TT && picker->idx)
    {
        picker->state++;
        picker->idx = 0;
    }
    if(picker->state == PICK_CHECKS && picker->idx >= picker->checks.count)
    {
        picker->state++;
        picker->idx = 0;
    }
    if(picker->state == PICK_GOODCAP && picker->idx >= picker->goodcap.count)
    {
        picker->state++;
        picker->idx = 0;
    }
    if(picker->state == PICK_QUIET && picker->idx >= picker->quiet.count)
    {
        picker->state++;
        picker->idx = 0;
    }
    if(picker->state == PICK_BADCAP && picker->idx >= picker->badcap.count)
    {
        picker->state++;
        picker->idx = 0;
    }

    switch(picker->state)
    {
    case PICK_TT:
        move = picker->tt;
        break;
    case PICK_CHECKS:
        move = picker->checks.moves[picker->idx];
        break;
    case PICK_GOODCAP:
        move = picker->goodcap.moves[picker->idx];
        break;
    case PICK_QUIET:
        move = picker->quiet.moves[picker->idx];
        break;
    case PICK_BADCAP:
        move = picker->badcap.moves[picker->idx];
        break;
    default:
        move = 0;
        break;
    }

    picker->idx++;
    return move;
}
