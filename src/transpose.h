#ifndef _TRANSPOSE_H
#define _TRANSPOSE_H

#include <stdint.h>

typedef uint16_t move_t;

typedef struct transpos_s
{
    uint64_t hash; // 0 is special null value, zero hashes might not work
    uint8_t depth; // how many plys to leaves? 0 for leaves.
    int16_t eval;
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
// if depth is 255, ignore depth
transpos_t* transpose_find(ttable_t* table, uint64_t hash, uint8_t depth);
void transpose_store(ttable_t* table, uint64_t hash, uint8_t depth, int16_t eval, move_t move);

#endif