#include "board.h"

#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

void board_findpieces(board_t* board)
{
    int i;

    team_e team;

    board->npieces[TEAM_WHITE] = board->npieces[TEAM_BLACK] = 0;

    for(i=0; i<BOARD_AREA; i++)
    {
        if(!(board->pieces[i] & PIECE_MASK_TYPE))
            continue;

        team = TEAM_WHITE;
        if(board->pieces[i] & PIECE_MASK_COLOR)
            team = TEAM_BLACK;

        if(board->npieces[team] >= PIECE_MAX)
        {
            printf("board_findpieces: max pieces reached! max is %d.\n", PIECE_MAX);
            exit(1);
        }

        board->quickp[team][board->npieces[team]++] = i;
    }
}

void board_findcheck(board_t* board)
{
    int i, j;

    for(i=0; i<TEAM_COUNT; i++)
    {
        board->check[i] = false;
        for(j=0; j<board->npieces[i]; j++)
        {
            if((board->pieces[board->quickp[i][j]] & PIECE_MASK_TYPE) == PIECE_KING)
                break;
        }

        if(j >= board->npieces[i])
            continue;

        board->check[i] = (((uint64_t) 1 << board->quickp[i][j]) & board->attacks[!i]) != 0;
        if(board->check[i])
        {
            printf("check found:\n");
            printf("king:\n");
            board_printbits((uint64_t) 1 << board->quickp[i][j]);
            printf("atk:\n");
            board_printbits(board->attacks[!i]);
        }
    }
}

#define PRINTUNICODE

void board_print(const board_t* board)
{
    int i, r, f;
    char c;

    // shut up compiler
    c = 0;
    i = c;

    printf("  ");
    for(i=0; i<BOARD_LEN; i++)
        printf("  %c ", 'a' + i);
    printf("\n  ");

    for(i=0; i<BOARD_LEN; i++)
        printf("+---");
    printf("+\n");

    for(r=BOARD_LEN-1; r>=0; r--)
    {
        printf("%d ", r + 1);
        for(f=0; f<BOARD_LEN; f++)
        {
            i = r * BOARD_LEN + f;

#ifdef PRINTUNICODE
            switch (board->pieces[i] & PIECE_MASK_TYPE)
            {
            case PIECE_KING:
                if(board->pieces[i] & PIECE_MASK_COLOR)
                    wprintf(L"| %lc ", (wint_t) 0x265A);
                else
                    wprintf(L"| %lc ", (wint_t) 0x2654);
                break;
            case PIECE_QUEEN:
                if(board->pieces[i] & PIECE_MASK_COLOR)
                    wprintf(L"| %lc ", (wint_t) 0x265B);
                else
                    wprintf(L"| %lc ", (wint_t) 0x2655);
                break;
            case PIECE_ROOK:
                if(board->pieces[i] & PIECE_MASK_COLOR)
                    wprintf(L"| %lc ", (wint_t) 0x265C);
                else
                    wprintf(L"| %lc ", (wint_t) 0x2656);
                break;
            case PIECE_BISHOP:
                if(board->pieces[i] & PIECE_MASK_COLOR)
                    wprintf(L"| %lc ", (wint_t) 0x265D);
                else
                    wprintf(L"| %lc ", (wint_t) 0x2657);
                break;
            case PIECE_KNIGHT:
                if(board->pieces[i] & PIECE_MASK_COLOR)
                    wprintf(L"| %lc ", (wint_t) 0x265E);
                else
                    wprintf(L"| %lc ", (wint_t) 0x2658);
                break;
            case PIECE_PAWN:
                if(board->pieces[i] & PIECE_MASK_COLOR)
                    wprintf(L"| %lc ", (wint_t) 0x265F);
                else
                    wprintf(L"| %lc ", (wint_t) 0x2659);
                break;
            default:
                printf("|   ");
                break;
            }
#else
            switch(board->pieces[i] & PIECE_MASK_TYPE)
            {
            case PIECE_KING:
                c = 'K';
                break;
            case PIECE_QUEEN:
                c = 'Q';
                break;
            case PIECE_ROOK:
                c = 'R';
                break;
            case PIECE_BISHOP:
                c = 'B';
                break;
            case PIECE_KNIGHT:
                c = 'N';
                break;
            case PIECE_PAWN:
                c = 'P';
                break;
            default:
                c = ' ';
            }

            if(c != ' ' && board->pieces[i] & PIECE_MASK_COLOR)
                c += 'a' - 'A';

            printf("| %c ", c);
            #endif
        }

        printf("|\n  ");
        for(f=0; f<BOARD_LEN; f++)
            printf("+---");
        printf("+\n");
    }
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

void board_loadfen(board_t* board, const char* fen)
{
    int i, r, f;
    const char *c;

    bool black;
    char pc;

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

            board->pieces[i] = 0;
            switch(pc)
            {
            case 'K':
                board->pieces[i] |= PIECE_KING;
                break;
            case 'Q':
                board->pieces[i] |= PIECE_QUEEN;
                break;
            case 'R':
                board->pieces[i] |= PIECE_ROOK;
                break;
            case 'B':
                board->pieces[i] |= PIECE_BISHOP;
                break;
            case 'N':
                board->pieces[i] |= PIECE_KNIGHT;
                break;
            case 'P':
                board->pieces[i] |= PIECE_PAWN;
                break;
            default:
                goto badfen;
            }

            if(black)
                board->pieces[i] |= PIECE_MASK_COLOR;

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
        if(*c != 'K' && *c != 'Q' && *c != 'k' && *c != 'q')
            goto badfen;
        if(*c == 'Q')
        {
            board->qcastle[TEAM_WHITE] = true;
            c++;
        }
        if(*c != 'K' && *c != 'Q' && *c != 'k' && *c != 'q')
            goto badfen;
        if(*c == 'k')
        {
            board->kcastle[TEAM_BLACK] = true;
            c++;
        }
        if(*c != 'K' && *c != 'Q' && *c != 'k' && *c != 'q')
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

    // TODO: halfmove and fullmove

    return;

badfen:
    printf("bad fen \"%s\".\n", fen);
    exit(1);
}
