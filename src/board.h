#ifndef _BOARD_H
#define _BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "transpose.h"
#include "zobrist.h"

#define BOARD_LEN 8
#define BOARD_AREA (BOARD_LEN * BOARD_LEN)
// this is overkill, but board_t is on the heap and only once or twice, so it's fine.
#define MAX_GAME_PLIES 8849

#define FEN_MAX 92

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
    PIECE_COUNT,
} piece_e;

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

// if you want to make the board not 8x8, this will need changing
typedef uint64_t bitboard_t;

#define SQUARE_BITS_TEAM 3 
#define SQUARE_MASK_TEAM (1 << SQUARE_BITS_TEAM)
#define SQUARE_MASK_TYPE 0x7
#define SQUARE_EMPTY 0

typedef uint8_t square_t;
typedef struct board_s
{
    // pboards[team][PIECE_NONE] is a bitwise or of all pieces for that team
    bitboard_t pboards[TEAM_COUNT][PIECE_COUNT];
    uint8_t npiece[TEAM_COUNT];
    uint8_t ptable[TEAM_COUNT][PIECE_MAX];
    square_t sqrs[BOARD_AREA];

    uint16_t nhistory;
    uint16_t lastperm; // last permenant history move, e.g. pawn push or capture
    uint64_t history[MAX_GAME_PLIES];
    uint64_t hash;

    bool stalemate;
    uint8_t fiftymove;

    bitboard_t attacks; // of !tomove
    bitboard_t pinmasks[BOARD_AREA];
    bool dblcheck;
    bool isthreat;
    bitboard_t threat;
    
    team_e tomove;
    bool check; // of tomove

    uint8_t enpas; // on the last move, did a pawn just move two squares? if so, the target. else 0xFF
    bool kcastle[TEAM_COUNT]; // starts at true, false if the team's kingside rook moves
    bool qcastle[TEAM_COUNT]; // starts at true, false if the team's queenside rook moves

    ttable_t ttable;
} board_t;

void board_findpieces(board_t* board);
void board_findcheck(board_t* board);
void board_checkstalemate(board_t* board);
void board_print(const board_t* board);
void board_printbits(const bitboard_t bits);
int board_loadfen(board_t* board, const char* fen);
void board_update(board_t* board);

#endif