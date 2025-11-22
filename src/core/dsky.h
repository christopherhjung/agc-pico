#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  KEY_ONE     = 1,
  KEY_TWO     = 2,
  KEY_THREE   = 3,
  KEY_FOUR    = 4,
  KEY_FIVE    = 5,
  KEY_SIX     = 6,
  KEY_SEVEN   = 7,
  KEY_EIGHT   = 8,
  KEY_NINE    = 9,
  KEY_ZERO    = 16,
  KEY_VERB    = 17,
  KEY_RSET    = 18,
  KEY_KEY_REL = 25,
  KEY_PLUS    = 26,
  KEY_MINUS   = 27,
  KEY_ENTER   = 28,
  KEY_CLR     = 30,
  KEY_NOUN    = 31
} Key;

typedef struct
{
  unsigned int plus : 1;
  unsigned int minus : 1;
  unsigned int first : 4;
  unsigned int second : 4;
  unsigned int third : 4;
  unsigned int fourth : 4;
  unsigned int fifth : 4;
} dsky_row_t;

typedef struct
{
  uint8_t first;
  uint8_t second;
} dsky_two_t;

typedef struct
{
  unsigned int reserved3 : 6;
  unsigned int EL_OFF : 1;
  unsigned int reserved2 : 1;
  unsigned int stby : 1;
  unsigned int restart : 1;
  unsigned int opr_err : 1;
  unsigned int reserved1 : 1;
  unsigned int VN_FLASH : 1;
  unsigned int key_rel : 1;
  unsigned int temp : 1;
  unsigned int AGC_WARN : 1;
} dsky_flags_t;

typedef struct
{
  unsigned int vel : 1;
  unsigned int no_att : 1;
  unsigned int alt : 1;
  unsigned int gimbal_lock : 1;
  unsigned int restart : 1;
  unsigned int tracker : 1;
  unsigned int prog : 1;
  unsigned int comp_acty : 1;
  unsigned int uplink_acty : 1;
  unsigned int temp : 1;
  unsigned int key_rel : 1;
  unsigned int opr_err : 1;
  unsigned int stby : 1;
} dsky_indicator_t;

typedef struct
{
  dsky_indicator_t indicator;
  dsky_two_t   prog;
  dsky_two_t   verb;
  dsky_two_t   noun;
  dsky_row_t   rows[3];
  unsigned int blink_off : 1;
} dsky_t;

void agc2dsky_handle(dsky_t* dsky);
void dsky2agc_handle();

void dsky_press_key(Key key);
void dsky_press_pro(bool on);

void dsky_row_init(dsky_row_t* row);

void dsky_two_init(dsky_two_t* two);

void dsky_init(dsky_t* dsky);

int dsky_update_digit(dsky_t* dsky, uint16_t channel, uint16_t value);

void dsky_row_print(dsky_row_t* row);

void dsky_two_print(dsky_two_t* two);

void dsky_refresh(dsky_t* dsky);

int  dsky_channel_input(uint16_t* channel, uint16_t* value);
int  dsky_channel_output(uint16_t channel, uint16_t value);
