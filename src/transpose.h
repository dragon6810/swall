#ifndef _TRANSPOSE_H
#define _TRANSPOSE_H

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t move_t;

typedef enum
{
    TRANSPOS_PV=0,
    TRANSPOS_LOWER,
    TRANSPOS_UPPER,
} transpos_type_e;

typedef struct transpos_s
{
    uint64_t hash; // 0 is special null value, zero hashes might not work
    uint8_t depth; // how many plys to leaves? 0 for leaves.
    int16_t eval;
    transpos_type_e type;
    move_t bestmove;
} transpos_t;

typedef struct ttable_s
{
    uint64_t size;
    transpos_t *data;
} ttable_t;

void transpose_alloc(ttable_t* table, uint64_t sizekb);
void transpose_free(ttable_t* table);
void transpose_clear(ttable_t* table);
// if nostrict is set, the result will often be incorrect, but good first guess for move ordering
transpos_t* transpose_find(ttable_t* table, uint64_t hash, uint8_t depth, int alpha, int beta, bool nostrict);
void transpose_store(ttable_t* table, uint64_t hash, uint8_t depth, int16_t eval, move_t move, transpos_type_e type);

#endif