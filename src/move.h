#ifndef _MOVE_H
#define _MOVE_H

#include "board.h"

#define PAWN_OFFS(team) ((team) == TEAM_WHITE ? diroffs[DIR_N] : diroffs[DIR_S])

#define MAX_LONGALG 6

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

extern int sweeptable[BOARD_AREA][DIR_COUNT];
extern bitboard_t kingatk[BOARD_AREA];
extern bitboard_t knightatk[BOARD_AREA];
extern bitboard_t pawnatk[TEAM_COUNT][BOARD_AREA];
extern bitboard_t pawnpush[TEAM_COUNT][BOARD_AREA];
extern bitboard_t pawndbl[TEAM_COUNT][BOARD_AREA];
extern bitboard_t emptysweeps[BOARD_AREA][DIR_COUNT];

typedef enum
{
    MOVETYPE_DEFAULT=0, // just moving a piece
    MOVETYPE_CASTLE,    // castle your king
    MOVETYPE_ENPAS,     // en passant
    MOVETYPE_PROMQ,     // promote a pawn to a queen
    MOVETYPE_PROMR,     // promote a pawn to a rook
    MOVETYPE_PROMB,     // promote a pawn to a bishop
    MOVETYPE_PROMN,     // promote a pawn to a knight
} movetype_e;
// TTTTDDDDDDSSSSSS
#define MOVEBITS_SRC_BITS ((uint16_t)0)
#define MOVEBITS_DST_BITS ((uint16_t)6)
#define MOVEBITS_TYP_BITS ((uint16_t)12)
#define MOVEBITS_SRC_MASK ((uint16_t)0x3F)
#define MOVEBITS_DST_MASK ((uint16_t)0xFC0)
#define MOVEBITS_TYP_MASK ((uint16_t)0xF000)
typedef uint16_t move_t;

#define MAX_MOVE 218
typedef struct moveset_s
{
    uint8_t count;
    move_t moves[MAX_MOVE];
} moveset_t;

typedef struct mademove_s
{
    move_t move;
    piece_e captured; // PIECE_NONE if nothing was captured

    uint8_t enpas;
    uint8_t castle; // KQkq
    uint8_t fiftymove;
    uint16_t lastperm;

    bitboard_t attacks;
    uint64_t oldhash;
} mademove_t;

void move_tolongalg(move_t move, char str[MAX_LONGALG]);
void move_make(board_t* restrict board, move_t move, mademove_t* restrict outmove);
void move_unmake(board_t* restrict board, const mademove_t* restrict move);
void move_makenull(board_t* restrict board, mademove_t* restrict outmove);
void move_unmakenull(board_t* restrict board, mademove_t* restrict outmove);
void move_makeinit(void);
void move_findattacks(board_t* restrict board);
// also finds threats
void move_findpins(board_t* restrict board);
// MUST be called before alllegal
void move_gensetup(board_t* restrict board);
// every legal move for every piece of whoever's turn it is
void move_alllegal(board_t* restrict board, moveset_t* restrict outmoves, bool caponly);
bool move_givescheck(board_t* restrict board, move_t move);
void move_init(void);

#endif