#ifndef _BOARD_H
#define _BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_LEN 8
#define BOARD_AREA (BOARD_LEN * BOARD_LEN)

#define FEN_MAX 92

#define PIECE_MASK_COLOR 0x8
#define PIECE_MASK_TYPE  0x7

typedef enum
{
    PIECE_NONE=0,
    PIECE_KING,
    PIECE_QUEEN,
    PIECE_ROOK,
    PIECE_BISHOP,
    PIECE_KNIGHT,
    PIECE_PAWN,
} piece_e;

// 0000 (0 = white, 1 = black) type
typedef uint8_t piece_t;

// src, dst
typedef uint8_t move_t[2];

typedef struct board_s
{
    // i = (rank - 1) * BOARD_LEN + (file - 1)
    piece_t pieces[BOARD_AREA];
    
    bool blackmove; // if false, white to move.

    // bool kcastle[2];
    // bool qcastle[2];
} board_t;

void board_print(const board_t* board);
void board_loadfen(board_t* board, const char* fen);
bool board_movelegal(const board_t* board, const move_t move);

#endif