#include "transpose.h"

#include <stdlib.h>
#include <string.h>

void transpose_alloc(ttable_t* table, uint64_t sizekb)
{
    uint64_t nel;

    nel = sizekb * 1024 / sizeof(transpos_t);
    table->size = nel;
    table->data = malloc(nel * sizeof(transpos_t));
    memset(table->data, 0, nel * sizeof(transpos_t));
}

void transpose_free(ttable_t* table)
{
    free(table->data);

    table->size = 0;
    table->data = NULL;
}

void transpose_clear(ttable_t* table)
{
    memset(table->data, 0, table->size * sizeof(transpos_t));
}

transpos_t* transpose_find(ttable_t* table, uint64_t hash, uint8_t depth, int alpha, int beta, bool nostrict)
{
    uint64_t idx;

    if(!hash)
        return NULL;

    idx = hash % table->size;
    if(table->data[idx].hash != hash)
        return NULL;
    if(!nostrict && table->data[idx].depth < depth)
        return NULL;
    if(table->data[idx].type == TRANSPOS_LOWER && table->data[idx].eval < beta)
        return NULL;
    if(table->data[idx].type == TRANSPOS_UPPER && table->data[idx].eval >= alpha)
        return NULL;

    return &table->data[idx];
}

void transpose_store(ttable_t* table, uint64_t hash, uint8_t depth, score_t eval, move_t move, transpos_type_e type)
{
    uint64_t idx;

    if(!hash)
        return;

    idx = hash % table->size;
    table->data[idx].hash = hash;
    table->data[idx].depth = depth;
    table->data[idx].eval = eval;
    table->data[idx].type = type;
    table->data[idx].bestmove = move;
}

