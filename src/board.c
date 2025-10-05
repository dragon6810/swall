#include "board.h"

#include <assert.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "move.h"

void board_findpieces(board_t* board)
{
    int sqr;
    
    team_e t;
    piece_e p;

    board->npiece[TEAM_WHITE] = board->npiece[TEAM_BLACK] = 0;

    for(sqr=0; sqr<BOARD_AREA; sqr++)
    {
        t = board->sqrs[sqr] >> SQUARE_BITS_TEAM;
        p = board->sqrs[sqr] & SQUARE_MASK_TYPE;

        if(!p)
            continue;
            
        if(board->npiece[t] >= PIECE_MAX)
        {
            printf("board_findpieces: max pieces reached for team %s!\n", t ? "black" : "white");
            board_print(board);
            exit(1);
        }

        board->ptable[t][board->npiece[t]++] = sqr;
    }
}

void board_findcheck(board_t* board)
{
    board->check = board->pboards[board->tomove][PIECE_KING] & board->attacks;
}

// #define PRINTUNICODE

#ifdef PRINTUNICODE
wint_t piecechars[TEAM_COUNT][PIECE_COUNT] = 
{
    {
        ' ',
        0x2654,
        0x2655,
        0x2656,
        0x2657,
        0x2658,
        0x2659,
    },
    { 
        ' ',
        0x265A,
        0x265B,
        0x265C,
        0x265D,
        0x265E,
        0x265F,
    },
};
#else
char piecechars[TEAM_COUNT][PIECE_COUNT] = 
{
    { 
        ' ',
        'K',
        'Q',
        'R',
        'B',
        'N',
        'P',
    },
    { 
        ' ',
        'k',
        'q',
        'r',
        'b',
        'n',
        'p',
    },
};
#endif

void board_print(const board_t* board)
{
    int i, r, f, t, p;
    char c;

    // shut up compiler
    c = 0;
    i = c;

    printf("\n ");

    for(i=0; i<BOARD_LEN; i++)
        printf("+---");
    printf("+\n");

    for(r=BOARD_LEN-1; r>=0; r--)
    {
        printf(" ");
        for(f=0; f<BOARD_LEN; f++)
        {
            i = r * BOARD_LEN + f;

            t = board->sqrs[i] >> SQUARE_BITS_TEAM;
            p = board->sqrs[i] & SQUARE_MASK_TYPE;

#ifdef PRINTUNICODE
            wprintf(L"| %lc ", piecechars[t][p]);
#else
            printf("| %c ", piecechars[t][p]);
#endif
        }

        printf("| %d\n ", r + 1);
        for(f=0; f<BOARD_LEN; f++)
            printf("+---");
        printf("+\n");
    }

    printf("  ");
    for(i=0; i<BOARD_LEN; i++)
        printf("  %c ", 'a' + i);
    printf("\n\n");

    printf("Key: 0x%016llX\n", board->hash);
}

void board_printbits(const bitboard_t bits)
{
    int r, f;

    int idx;

    for(r=BOARD_LEN-1; r>=0; r--)
    {
        for(f=0; f<BOARD_LEN; f++)
        {
            idx = r * BOARD_LEN + f;
            printf("%d ", (int) ((bits & ((uint64_t) 1 << idx)) >> idx));
        }
        printf("\n");
    }
}

int board_loadfen(board_t* board, const char* fen)
{
    int i, r, f;
    const char *c;

    bool black;
    char pc;

    if(board->ttable.data)
        transpose_free(&board->ttable);
    memset(board, 0, sizeof(board_t));

    r = BOARD_LEN-1;
    f = 0;
    c = fen;
    while(1)
    {
        if(r <= 0 && f == BOARD_LEN)
            break;

        switch(*c)
        {
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            f += *c - '0';
            if(f > BOARD_LEN)
                goto badfen;

            break;
        case 'K':
        case 'Q':
        case 'R':
        case 'B':
        case 'N':
        case 'P':
        case 'k':
        case 'q':
        case 'r':
        case 'b':
        case 'n':
        case 'p':
            if(f >= BOARD_LEN)
                goto badfen;

            pc = *c;
            black = false;
            if(pc >= 'a')
            {
                black = true;
                pc -= 'a' - 'A';
            }

            i = r * BOARD_LEN + f;
            switch(pc)
            {
            case 'K':
                board->sqrs[i] = black << SQUARE_BITS_TEAM | PIECE_KING;
                board->pboards[black][PIECE_KING] |= (uint64_t) 1 << i;
                board->pboards[black][PIECE_NONE] |= (uint64_t) 1 << i;
                break;
            case 'Q':
                board->sqrs[i] = black << SQUARE_BITS_TEAM | PIECE_QUEEN;
                board->pboards[black][PIECE_QUEEN] |= (uint64_t) 1 << i;
                board->pboards[black][PIECE_NONE] |= (uint64_t) 1 << i;
                break;
            case 'R':
                board->sqrs[i] = black << SQUARE_BITS_TEAM | PIECE_ROOK;
                board->pboards[black][PIECE_ROOK] |= (uint64_t) 1 << i;
                board->pboards[black][PIECE_NONE] |= (uint64_t) 1 << i;
                break;
            case 'B':
                board->sqrs[i] = black << SQUARE_BITS_TEAM | PIECE_BISHOP;
                board->pboards[black][PIECE_BISHOP] |= (uint64_t) 1 << i;
                board->pboards[black][PIECE_NONE] |= (uint64_t) 1 << i;
                break;
            case 'N':
                board->sqrs[i] = black << SQUARE_BITS_TEAM | PIECE_KNIGHT;
                board->pboards[black][PIECE_KNIGHT] |= (uint64_t) 1 << i;
                board->pboards[black][PIECE_NONE] |= (uint64_t) 1 << i;
                break;
            case 'P':
                board->sqrs[i] = black << SQUARE_BITS_TEAM | PIECE_PAWN;
                board->pboards[black][PIECE_PAWN] |= (uint64_t) 1 << i;
                board->pboards[black][PIECE_NONE] |= (uint64_t) 1 << i;
                break;
            default:
                goto badfen;
            }

            f++;

            break;
        case '/':
            if(r == 0 || f < BOARD_LEN)
                goto badfen;

            r--;
            f = 0;

            break;
        default:
            goto badfen;
        }
        
        c++;
    }

    if(*c++ != ' ')
        goto badfen;

    if(*c == 'w')
        board->tomove = TEAM_WHITE;
    else if(*c == 'b')
        board->tomove = TEAM_BLACK;
    else
        goto badfen;
    c++;

    if(*c++ != ' ')
        goto badfen;

    board->kcastle[TEAM_WHITE] = board->kcastle[TEAM_BLACK] = false;
    board->qcastle[TEAM_WHITE] = board->qcastle[TEAM_BLACK] = false;
    if(*c != '-')
    {
        if(*c != 'K' && *c != 'Q' && *c != 'k' && *c != 'q')
            goto badfen;
        if(*c == 'K')
        {
            board->kcastle[TEAM_WHITE] = true;
            c++;
        }
        if(*c != 'K' && *c != 'Q' && *c != 'k' && *c != 'q' && *c != ' ')
            goto badfen;
        if(*c == 'Q')
        {
            board->qcastle[TEAM_WHITE] = true;
            c++;
        }
        if(*c != 'K' && *c != 'Q' && *c != 'k' && *c != 'q' && *c != ' ')
            goto badfen;
        if(*c == 'k')
        {
            board->kcastle[TEAM_BLACK] = true;
            c++;
        }
        if(*c != 'K' && *c != 'Q' && *c != 'k' && *c != 'q' && *c != ' ')
            goto badfen;
        if(*c == 'q')
        {
            board->qcastle[TEAM_BLACK] = true;
            c++;
        }
    }
    else
        c++;

    if(*c++ != ' ')
        goto badfen;

    board->enpas = 0xFF;
    if(*c != '-')
    {
        if(*c < 'a' || *c > 'h')
            goto badfen;
        f = *c++ - 'a';
        if(*c < '1' || *c > '8')
            goto badfen;
        r = *c++ - '1';
        board->enpas = r * BOARD_LEN + f;
    }
    else
        c++;

    if(*c++ != ' ')
        goto badfen;

    while(*c >= '0' && *c <= '9')
        c++;

    if(*c++ != ' ')
        goto badfen;

    while(*c >= '0' && *c <= '9')
        c++;

    return c - fen;

badfen:
    printf("bad fen \"%s\".\n", fen);

    return -1;
}

void board_checkstalemate(board_t* board)
{
    int i;

    int repetitions;

    board->stalemate = false;

    if(board->fiftymove >= 100)
    {
        board->stalemate = true;
        return;
    }
    
    for(i=board->lastperm, repetitions=0; i<board->nhistory; i++)
    {
        if(board->history[i] != board->hash)
            continue;
        
        repetitions++;
        if(repetitions >= 3)
        {
            board->stalemate = true;
            return;
        }
    }
}

void board_update(board_t* board)
{
    board_findpieces(board);
    move_findattacks(board);
    board_findcheck(board);
    board->hash = zobrist_hash(board);
    board_checkstalemate(board);
}
