#include "board.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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

            if(c != ' ' && ~board->pieces[i] & PIECE_MASK_COLOR)
                c += 'a' - 'A';

            printf("| %c ", c);
        }

        printf("|\n  ");
        for(f=0; f<BOARD_LEN; f++)
            printf("+---");
        printf("+\n");
    }
}

void board_loadfen(board_t* board, const char* fen)
{
    int i, r, f;
    const char *c;

    bool black;
    char pc;

    r = 0;
    f = 0;
    c = fen;
    while(1)
    {
        if(r == BOARD_LEN-1 && f == BOARD_LEN)
            break;
        printf("r, f, c: %d, %d, %c\n", r, f, *c);
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
            if(r == BOARD_LEN-1 || f < BOARD_LEN)
                goto badfen;

            r++;
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

bool board_movelegal_king(const board_t* board, const move_t move)
{
    int i;

    int mr[2], mf[2];

    for(i=0; i<2; i++)
    {
        mr[i] = move[i] / BOARD_LEN;
        mf[i] = move[i] - mr[i] * BOARD_LEN;
        printf("r, f: %d, %d\n", mr[i], mf[i]);
    }

    printf("%d -> %d\n", move[0], move[1]);
    printf("%c%c -> %c%c\n", mf[0] + 'a', mr[0] + '1', mf[1] + 'a', mr[1] + '1');
    return true;
}

bool board_movelegal(const board_t* board, const move_t move)
{
    piece_t p[2];

    p[0] = board->pieces[move[0]];
    p[1] = board->pieces[move[1]];
    if((p[1] & PIECE_MASK_TYPE) != PIECE_NONE && (p[0] & PIECE_MASK_COLOR) == (p[1] & PIECE_MASK_COLOR))
        return false;

    switch(board->pieces[move[0]] & PIECE_MASK_TYPE)
    {
    case PIECE_KING:
        return board_movelegal_king(board, move);
    default:
        return false;
    }
}
