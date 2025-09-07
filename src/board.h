#ifndef _BOARD_H
#define _BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_LEN 8
#define BOARD_AREA (BOARD_LEN * BOARD_LEN)

#define FEN_MAX 92

#define PIECE_MASK_COLOR 0x8
#define PIECE_MASK_TYPE  0x7

#define PIECE_MAX 16

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

typedef enum
{
    TEAM_WHITE=0,
    TEAM_BLACK,
    TEAM_COUNT,
} team_e;

typedef enum
{
    DIR_E=0,
    DIR_N,
    DIR_W,
    DIR_S,
    DIR_NE,
    DIR_NW,
    DIR_SW,
    DIR_SE,
    DIR_COUNT,
} dir_e;

typedef struct pline_s
{
    uint8_t start, end;
    dir_e dir;
} pinline_t;

// if you want to make the board not 8x8, this will need changing
typedef uint8_t bitboard_t[BOARD_LEN];

typedef struct board_s
{
    // row-major, could bit pack in the future since each peaces is one nibble.
    piece_t pieces[BOARD_AREA];

    bitboard_t attacks[TEAM_COUNT];
    pinline_t *pins[TEAM_COUNT];

    uint8_t npieces[TEAM_COUNT];
    uint8_t quickp[TEAM_COUNT][PIECE_MAX];
    
    team_e tomove;

    uint8_t enpas; // on the last move, did a pawn just move two squares? if so, the target. else 0xFF
    bool kcastle[TEAM_COUNT]; // starts at true, false if the team's kingside rook moves
    bool qcastle[TEAM_COUNT]; // starts at true, false if the team's queenside rook moves
} board_t;

void board_findpieces(board_t* board);
void board_print(const board_t* board);
void board_printbits(const bitboard_t bits);
void board_loadfen(board_t* board, const char* fen);

#endif