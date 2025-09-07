#ifndef _MOVE_H
#define _MOVE_H

#include "board.h"

static const int diroffs[DIR_COUNT] = 
{
    1,
    BOARD_LEN,
    -1,
    -BOARD_LEN,

     BOARD_LEN + 1,
     BOARD_LEN - 1,
    -BOARD_LEN - 1,
    -BOARD_LEN + 1,
};

typedef enum
{
    MOVETYPE_DEFAULT=0, // just moving a piece
    MOVETYPE_CASTLE,    // castle your king
    MOVETYPE_PROMQ,     // promote a pawn to a queen
    MOVETYPE_PROMR,     // promote a pawn to a rook
    MOVETYPE_PROMB,     // promote a pawn to a bishop
    MOVETYPE_PROMN,     // promote a pawn to a knight
    MOVETYPE_ENPAS,     // en passant
} movetype_e;
// TTTTDDDDDDSSSSSS
#define MOVEBITS_SRC_BITS ((uint16_t)0)
#define MOVEBITS_DST_BITS ((uint16_t)6)
#define MOVEBITS_TYP_BITS ((uint16_t)12)
#define MOVEBITS_SRC_MASK ((uint16_t)0x3F)
#define MOVEBITS_DST_MASK ((uint16_t)0xFC0)
#define MOVEBITS_TYP_MASK ((uint16_t)0xF000)
typedef uint16_t move_t;
typedef struct moveset_s
{
    move_t move;
    struct moveset_s *next;
} moveset_t;

void move_domove(board_t* board, move_t move);
void move_findattacks(board_t* board);
// doesn't clear existing pins
void move_findpins(board_t* board);
// moves can be NULL
moveset_t* move_legalmoves(board_t* board, moveset_t* moves, uint8_t src);
// every legal move for every piece of whoever's turn it is
moveset_t* move_alllegal(board_t* board);
// ignores input flags, fills output flags
move_t* move_findmove(moveset_t* set, move_t move);
void move_printset(moveset_t* set);
void move_freeset(moveset_t* set);

#endif