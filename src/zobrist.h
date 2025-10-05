#ifndef _ZOBRIST_H
#define _ZOBRIST_H

#include <stdint.h>

typedef struct board_s board_t;

typedef struct zobristentry_s
{
    uint64_t key;
    int16_t val;

    struct zobristentry_s *next;
} zobristentry_t;

typedef struct zobristdict_s
{
    uint64_t size;
    zobristentry_t **data;
} zobristdict_t;

extern const uint64_t zobrist_hashes[781];
extern const int zobrist_piecetohash[2][7];

uint64_t zobrist_hash(board_t* board);
void zobrist_alloctable(zobristdict_t* table, uint64_t buckets);
int16_t* zobrist_find(zobristdict_t* table, uint64_t hash);
void zobrist_set(zobristdict_t* table, uint64_t hash, int16_t val);
void zobrist_freetable(zobristdict_t* table);

#endif