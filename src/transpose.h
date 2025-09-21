#ifndef _TRANSPOSE_H
#define _TRANSPOSE_H

#include <stdint.h>

typedef struct transpos_s
{
    uint64_t hash; // 0 is special null value, zero hashes might not work
    int16_t eval;
} transpos_t;

typedef struct ttable_s
{
    uint64_t size;
    transpos_t *data;
} ttable_t;

void transpose_alloc(ttable_t* table, uint64_t sizekb);
void transpose_free(ttable_t* table);
void transpose_clear(ttable_t* table);
transpos_t* transpose_find(ttable_t* table, uint64_t hash);
void transpose_store(ttable_t* table, uint64_t hash, int16_t eval);

#endif