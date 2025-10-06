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
    {
        picker->goodscores[picker->goodcap.count] = capscore;
        picker->goodcap.moves[picker->goodcap.count++] = move;
    }
    else
    {
        picker->badscores[picker->badcap.count] = capscore;
        picker->badcap.moves[picker->badcap.count++] = move;
    }

    return true;
}

static inline bool pick_trykiller(move_t move, int plies, picker_t* restrict picker)
{
    int i;
    
    for(i=0; i<MAX_KILLER; i++)
    {
        if(move != search_killers[plies][i])
            continue;

        picker->killers[picker->nkillers++] = move;
        return true;
    }

    return false;
}

static inline move_t pick_nextfromset(moveset_t* restrict set, score_t* restrict scores, int idx)
{
    int i;

    int bestidx;
    score_t bestscore;

    bestidx = idx;
    bestscore = scores[idx];
    for(i=idx+1; i<set->count; i++)
    {
        if(scores[i] > bestscore)
        {
            bestidx = i;
            bestscore = scores[i];
        }
    }

    if(bestidx == idx)
        return set->moves[bestidx];

    set->moves[idx] ^= set->moves[bestidx];
    set->moves[bestidx] ^= set->moves[idx];
    set->moves[idx] ^= set->moves[bestidx];

    scores[idx] ^= scores[bestidx];
    scores[bestidx] ^= scores[idx];
    scores[idx] ^= scores[bestidx];

    return set->moves[idx];
}

void pick_sort(board_t* restrict board, moveset_t* restrict moves, 
int plies, uint8_t depth, score_t alpha, score_t beta, picker_t* restrict picker)
{
    int i;

    transpos_t *transpos;
    move_t tt;

    picker->tt = 0;
    picker->checks.count = 0;
    picker->goodcap.count = 0;
    picker->nkillers = 0;
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

        if(pick_trykiller(moves->moves[i], plies, picker))
            continue;

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
    if(picker->state == PICK_KILLERS && picker->idx >= picker->nkillers)
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
        move = pick_nextfromset(&picker->goodcap, picker->goodscores, picker->idx);
        break;
    case PICK_KILLERS:
        move = picker->killers[picker->idx];
        break;
    case PICK_QUIET:
        move = picker->quiet.moves[picker->idx];
        break;
    case PICK_BADCAP:
        move = pick_nextfromset(&picker->badcap, picker->badscores, picker->idx);
        break;
    default:
        move = 0;
        break;
    }

    picker->idx++;
    return move;
}
