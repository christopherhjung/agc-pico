#include "core/dsky.h"
#include <stdio.h>
#include <stdlib.h>

char digit2char(unsigned int digit)
{
  if(digit >= 10)
    return ' ';
  return '0' + digit;
}

void dsky_row_print(dsky_row_t* row)
{
  printf(
    "%c%c%c%c%c%c\n",
    row->minus    ? '-'
      : row->plus ? '+'
                  : ' ',
    digit2char(row->first),
    digit2char(row->second),
    digit2char(row->third),
    digit2char(row->fourth),
    digit2char(row->fifth));
}

void dsky_two_print(dsky_two_t* two)
{
  if(two->on)
    printf("%c%c", digit2char(two->first), digit2char(two->second));
  else
    printf("  ");
}

void dsky_print(dsky_t* dsky)
{
  printf("%d\n", *((uint16_t*)&dsky->flags));
  printf("CA  PR\n");
  printf("%s", dsky->comp_acty ? "XX" : "  ");
  printf("  ");
  dsky_two_print(&dsky->prog);
  printf("\n");
  printf("VB  NO\n");
  dsky_two_print(&dsky->verb);
  printf("  ");
  dsky_two_print(&dsky->noun);
  printf("\n");
  dsky_row_print(&dsky->rows[0]);
  dsky_row_print(&dsky->rows[1]);
  dsky_row_print(&dsky->rows[2]);
}
