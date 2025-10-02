#ifndef _MAGIC_H
#define _MAGIC_H

#include <stdint.h>

#include "board.h"

typedef enum
{
    MAGIC_ROOK=0,
    MAGIC_BISHOP,
    MAGIC_COUNT,
} magicpiece_e;

void magic_init(void);
void magic_findmagic(void);
bitboard_t magic_lookup(magicpiece_e p, uint8_t pos, bitboard_t blockers);

#endif