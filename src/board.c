#include "board.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void board_print(const board_t* board)
{
    int i, r, f;
    char c;

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

    for(r=BOARD_LEN-1; r>=0; r--)
    {
        for(f=0; f<BOARD_LEN; f++)
            printf("%c ", '0' + ((bits[r] & (1 << f)) >> f));
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
        board->blackmove = false;
    else if(*c == 'b')
        board->blackmove = true;
    else
        goto badfen;
    c++;

    if(*c++ != ' ')
        goto badfen;

    // return here, for now.

    return;

badfen:
    printf("bad fen \"%s\".\n", fen);
    exit(1);
}

static void board_knightmoves(board_t* board, bitboard_t bits, uint8_t src)
{
    int i, j;

    piece_t p;
    int r, f;
    int x, y;

    p = board->pieces[src];
    r = src / BOARD_LEN;
    f = src % BOARD_LEN;

    for(i=DIR_E; i<=DIR_S; i++)
    {
        for(j=0; j<2; j++)
        {
            x = f;
            y = r;

            if(i == DIR_E)
                x += 2;
            if(i == DIR_N)
                y += 2;
            if(i == DIR_W)
                x -= 2;
            if(i == DIR_S)
                y -= 2;

            if(i == DIR_E || i == DIR_W)
                y += j * 2 - 1;
            if(i == DIR_N || i == DIR_S)
                x += j * 2 - 1;

            if(x < 0 || x >= BOARD_LEN)
                continue;
            if(y < 0 || y >= BOARD_LEN)
                continue;

            if((board->pieces[y * BOARD_LEN + x] & PIECE_MASK_TYPE)
            && (board->pieces[y * BOARD_LEN + x] & PIECE_MASK_COLOR) == (p & PIECE_MASK_COLOR))
                continue;

            bits[y] |= (1 << x);
        }
    }
}

// does not include src in its sweep
// if maxdst is 0, it will go until it hits the end of the board (or a piece).
static void board_sweepdir(board_t* board, bitboard_t bits, uint8_t src, dir_e dir, int maxdst)
{
    int i, isqr;

    int r, f;
    uint8_t fmask;
    piece_t p;
    int dr, df;
    
    p = board->pieces[src];

    r = src / BOARD_LEN;
    f = src % BOARD_LEN;

    dr = df = 0;
    if(dir == DIR_E || dir == DIR_NE || dir == DIR_SE)
        df = 1;
    if(dir == DIR_N || dir == DIR_NE || dir == DIR_NW)
        dr = 1;
    if(dir == DIR_W || dir == DIR_NW || dir == DIR_SW)
        df = -1;
    if(dir == DIR_S || dir == DIR_SW || dir == DIR_SE)
        dr = -1;

    r += dr;
    f += df;
    for(i=0; !maxdst||i<maxdst; i++, r+=dr, f+=df)
    {
        printf("dr, df = %d, %d\n", dr, df);
        if(r < 0 || r >= BOARD_LEN)
            break;
        if(f < 0 || f >= BOARD_LEN)
            break;

        isqr = r * BOARD_LEN + f;

        if((board->pieces[isqr] & PIECE_MASK_TYPE) 
        && (board->pieces[isqr] & PIECE_MASK_COLOR) == (p & PIECE_MASK_COLOR))
            break;

        fmask = 1 << f;
        bits[r] |= fmask;

        if(board->pieces[isqr] & PIECE_MASK_TYPE)
            break;
    }
}

void board_getlegal(board_t* board, uint8_t src, bitboard_t outbits)
{
    int i;

    piece_t p;
    piece_e type;
    dir_e dir;
    int dst;

    memset(outbits, 0, BOARD_LEN);

    p = board->pieces[src];
    type = p & PIECE_MASK_TYPE;

    switch(type)
    {
    case PIECE_KING:
        for(i=0; i<=DIR_SE; i++)
            board_sweepdir(board, outbits, src, i, 1);
        break;
    case PIECE_QUEEN:
        for(i=0; i<=DIR_SE; i++)
            board_sweepdir(board, outbits, src, i, 0);
        break;
    case PIECE_ROOK:
        for(i=0; i<=DIR_S; i++)
            board_sweepdir(board, outbits, src, i, 0);
        break;
    case PIECE_BISHOP:
        for(i=DIR_NE; i<=DIR_SE; i++)
            board_sweepdir(board, outbits, src, i, 0);
        break;
    case PIECE_KNIGHT:
        board_knightmoves(board, outbits, src);
        break;
    case PIECE_PAWN:
        dst = 1;
        if(p & PIECE_MASK_COLOR)
        {
            printf("black %d\n", src);
            dir = DIR_S;
            if(src / BOARD_LEN == BOARD_LEN-2)
                dst = 2;
        }
        else
        {
            dir = DIR_N;
            if(src / BOARD_LEN == 1)
                dst = 2;
        }

        board_sweepdir(board, outbits, src, dir, dst);
        break;
    default:
        break;
    }
}