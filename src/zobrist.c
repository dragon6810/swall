#include "zobrist.h"

#include <stdlib.h>
#include <string.h>

#include "board.h"

uint64_t randa, randb, randc, randd;
uint64_t sqrhashes[PIECE_COUNT][BOARD_AREA], tomovehash, castlehash[16], enpashash[BOARD_LEN];

#define rot(x,k) (((x)<<(k))|((x)>>(64-(k))))
static uint64_t zobrist_rand(void)
{
    uint64_t e;

    e = randa - rot(randb, 7);
    randa = randb ^ rot(randc, 13);
    randb = randc + rot(randd, 37);
    randc = randd + e;
    randd = e + randa;
    return randd;
}
#undef rot

static void zobrist_initrand(void)
{
    const uint64_t seed = 5113671424;
    int i;

    randa = 0xF1EA5EED;
    randb = randc = randd = seed;
    for(i=0; i<20; i++)
        zobrist_rand();
}

void zobrist_init(void)
{
    int i, j;

    zobrist_initrand();

    for(i=0; i<PIECE_COUNT; i++)
        for(j=0; j<BOARD_AREA; j++)
            sqrhashes[i][j] = zobrist_rand();
    tomovehash = zobrist_rand();
    for(i=0; i<16; i++)
        castlehash[i] = zobrist_rand();
    for(i=0; i<BOARD_LEN; i++)
        enpashash[i] = zobrist_rand();
}

uint64_t zobrist_hash(board_t* board)
{
    int t, p;

    uint64_t hash;
    piece_e type;

    hash = 0;
    for(t=0; t<TEAM_COUNT; t++)
    {
        for(p=0; p<board->npiece[t]; p++)
        {
            type = board->sqrs[board->ptable[t][p]] & SQUARE_MASK_TYPE;
            hash ^= sqrhashes[type][board->ptable[t][p]];
        }
    }

    return hash;
}

void zobrist_alloctable(zobristdict_t* table, uint64_t buckets)
{
    table->size = buckets;
    table->data = malloc(buckets * sizeof(zobristentry_t*));
    memset(table->data, 0, buckets * sizeof(zobristentry_t*));
}

int16_t* zobrist_find(zobristdict_t* table, uint64_t hash)
{
    zobristentry_t *cur;

    uint64_t idx;

    idx = hash % table->size;
    
    for(cur=table->data[idx]; cur; cur=cur->next)
        if(cur->key == hash)
            return &cur->val;
    return NULL;
}

void zobrist_set(zobristdict_t* table, uint64_t hash, int16_t val)
{
    zobristentry_t *cur;

    uint64_t idx;
    zobristentry_t *newentry;

    idx = hash % table->size;
    
    for(cur=table->data[idx]; cur; cur=cur->next)
    {
        if(cur->key != hash)
            continue;

        cur->val = val;
        return;
    }

    newentry = malloc(sizeof(zobristentry_t));
    newentry->key = hash;
    newentry->val = val;
    newentry->next = table->data[idx];
    table->data[idx] = newentry;
}

void zobrist_freetable(zobristdict_t* table)
{
    int i;
    zobristentry_t *cur, *next;

    for(i=0; i<table->size; i++)
    {
        cur = table->data[i];
        while (cur)
        {
            next = cur->next;
            free(cur);
            cur = next;
        }
    }

    if(table->data)
        free(table->data);

    table->size = 0;
    table->data = NULL;
}
