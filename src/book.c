#include "book.h"

#include <stdio.h>
#include <stdlib.h>

#define SWAP64(x) (((x & 0x00000000000000FF) << 56) | \
                   ((x & 0x000000000000FF00) << 40) | \
                   ((x & 0x0000000000FF0000) << 24) | \
                   ((x & 0x00000000FF000000) << 8)  | \
                   ((x & 0x000000FF00000000) >> 8)  | \
                   ((x & 0x0000FF0000000000) >> 24) | \
                   ((x & 0x00FF000000000000) >> 40) | \
                   ((x & 0xFF00000000000000) >> 56))

#define SWAP32(x) (((x & 0x000000FF) << 24) | \
                   ((x & 0x0000FF00) << 8)  | \
                   ((x & 0x00FF0000) >> 8)  | \
                   ((x & 0xFF000000) >> 24))

#define SWAP16(x) (((x & 0x00FF) << 8) | \
                   ((x & 0xFF00) >> 8))            

#pragma pack(push, 1)
typedef struct bookentry_s
{
    uint64_t key;
    uint16_t move;
    uint16_t weight;
    uint32_t learn;
} bookentry_t;
#pragma pack(pop)

uint64_t nentrites = 0;
bookentry_t *entries = NULL;

void book_load(const char* path)
{
    int i;

    FILE *ptr;
    uint64_t fsize;

    ptr = fopen(path, "rb");
    if(!ptr)
        return;

    fseek(ptr, 0, SEEK_END);
    fsize = ftell(ptr);
    fseek(ptr, 0, SEEK_SET);

    if(fsize % sizeof(bookentry_t))
    {
        fclose(ptr);
        return;
    }

    nentrites = fsize / sizeof(bookentry_t);
    entries = malloc(fsize);
    
    for(i=0; i<nentrites; i++)
    {
        fread(&entries[i], sizeof(entries[i]), 1, ptr);
        entries[i].key = SWAP64(entries[i].key);
        entries[i].move = SWAP16(entries[i].move);
        entries[i].weight = SWAP16(entries[i].weight);
        entries[i].learn = SWAP32(entries[i].learn);
    }

    fclose(ptr);
}

bool book_findmove(board_t* board, move_t* outmove)
{
    int i;
    
    int weightsum;
    float accumweight, scaledweight;
    float random;
    int srcf, srcr, dstf, dstr, promp;

    for(i=weightsum=0; i<nentrites; i++)
    {
        if(entries[i].key != board->hash)
            continue;

        weightsum += entries[i].weight;
    }

    if(!weightsum)
        return false;

    random = (double) rand() / (double) RAND_MAX;

    for(i=0, accumweight=0; i<nentrites; i++)
    {
        if(entries[i].key != board->hash)
            continue;

        scaledweight = (double) entries[i].weight / (double) weightsum;
        accumweight += scaledweight;

        if(random < accumweight - scaledweight || random > accumweight)
            continue;

        dstf = entries[i].move & 0x7;
        dstr = (entries[i].move >> 3) & 0x7;
        srcf = (entries[i].move >> 6) & 0x7;
        srcr = (entries[i].move >> 9) & 0x7;
        promp = (entries[i].move >> 12) & 0x7;

        *outmove = srcr * BOARD_LEN + srcf;
        *outmove |= (dstr * BOARD_LEN + dstf) << MOVEBITS_DST_BITS;
        switch(promp)
        {
        case 1:
            *outmove |= ((uint16_t) MOVETYPE_PROMN) << MOVEBITS_TYP_BITS;
            break;
        case 2:
            *outmove |= ((uint16_t) MOVETYPE_PROMB) << MOVEBITS_TYP_BITS;
            break;
        case 3:
            *outmove |= ((uint16_t) MOVETYPE_PROMR) << MOVEBITS_TYP_BITS;
            break;
        case 4:
            *outmove |= ((uint16_t) MOVETYPE_PROMQ) << MOVEBITS_TYP_BITS;
            break;
        default:
            break;
        }

        return true;
    }

    return false;
}

void book_free(void)
{
    if(!entries)
        return;

    free(entries);

    nentrites = 0;
    entries = NULL;
}