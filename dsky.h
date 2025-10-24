#pragma once

#include <stdint.h>

typedef enum
{
  KEY_ZERO = 0,
  KEY_ONE = 1,
  KEY_TWO = 2,
  KEY_THREE = 3,
  KEY_FOUR = 4,
  KEY_FIVE = 5,
  KEY_SIX = 6,
  KEY_SEVEN = 7,
  KEY_EIGHT = 8,
  KEY_NINE = 9,
  KEY_VERB = 17,
  KEY_KEY_REL = 25,
  KEY_CLR = 30,
  KEY_NOUN = 31,
  KEY_ENTER = 28,
} Keyboard;

void dsky_keyboard_press (Keyboard Keycode);

typedef struct
{
  unsigned int plus:1;
  unsigned int minus:1;
  unsigned int first:4;
  unsigned int second:4;
  unsigned int third:4;
  unsigned int fourth:4;
  unsigned int fifth:4;
} dsky_row_t;

typedef struct
{
  unsigned int on:1;
  unsigned int first :4;
  unsigned int second: 4;
} dsky_two_t;

typedef struct
{
  dsky_two_t prog;
  dsky_two_t verb;
  dsky_two_t noun;
  dsky_row_t rows[3];
  unsigned int comp_acty:1;
} dsky_t;

void dsky_row_init(dsky_row_t* row);

void dsky_two_init(dsky_two_t* two);

void dsky_init(dsky_t* dsky);

int dsky_update_digit(dsky_t *dsky, uint16_t value);

void dsky_row_print(dsky_row_t *row);

void dsky_two_print(dsky_two_t *two);

void dsky_print(dsky_t *dsky);