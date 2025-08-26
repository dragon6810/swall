#include "board.h"

#include <stdio.h>

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