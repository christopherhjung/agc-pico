
#include <core/dsky.h>
#include <stdio.h>
#include "core/profile.h"
#include "core/dsky_dump.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"

#include "dsky_output_handler.h"
#include "spi.h"

#define KY_CS_PIN 20
#define KY_SH_PIN 21

typedef struct __attribute__((packed))
{
  uint zero : 1;
  uint noun : 1;
  uint verb : 1;
  uint minus : 1;
  uint four : 1;
  uint five : 1;
  uint _reserved_0 : 1;
  uint one : 1;
  uint _reserved_1 : 1;
  uint plus : 1;
  uint seven : 1;
  uint eight : 1;
  uint nine : 1;
  uint clr : 1;
  uint _reserved_2 : 2;
  uint three : 1;
  uint two : 1;
  uint _reserved_3 : 1;
  uint six : 1;
  uint pro : 1;
  uint entr : 1;
  uint rset : 1;
  uint key_rel : 1;
} keyboard_t;

static keyboard_t read_keyboard()
{
  gpio_put(KY_SH_PIN, 1);

  keyboard_t result;
  cs_select(KY_CS_PIN);
  spi_read_blocking(spi_default, 0, (uint8_t*)&result, 3);
  cs_deselect(KY_CS_PIN);

  gpio_put(KY_SH_PIN, 0);
  return result;
}

void init_keyboard()
{
  gpio_init(KY_CS_PIN);
  gpio_set_dir(KY_CS_PIN, GPIO_OUT);
  gpio_put(KY_CS_PIN, 0);

  gpio_init(KY_SH_PIN);
  gpio_set_dir(KY_SH_PIN, GPIO_OUT);
  gpio_put(KY_SH_PIN, 0);
}

keyboard_t to_keys_down(keyboard_t last_keyboard, keyboard_t current_keyboard)
{
  typedef union {
    keyboard_t bits;
    uint32_t   raw;
  } keyboard_union_t;

  keyboard_union_t last_keyboard_union = {.bits = last_keyboard};
  keyboard_union_t current_keyboard_union = {.bits = current_keyboard};

  current_keyboard_union.raw = (~last_keyboard_union.raw) & current_keyboard_union.raw;
  return current_keyboard_union.bits;
}

static keyboard_t last_keyboard = {0};
static uint64_t next_time = 0;

void dsky_output_handle()
{

  int c = getchar_timeout_us(0);
  switch(c)
  {
    case '0':
      dsky_press_key(KEY_ZERO);
      break;
    case 'V':
    case 'v':
      dsky_press_key(KEY_VERB);
      break;
    case 'N':
    case 'n':
      dsky_press_key(KEY_NOUN);
      break;
    case 'R':
    case 'r':
      dsky_press_key(KEY_RSET);
      break;
    case 'K':
    case 'k':
      dsky_press_key(KEY_KEY_REL);
      break;
    case 'P':
    case 'p':
      dsky_press_pro(0);
      break;
    case 'O':
    case 'o':
      dsky_press_pro(1);
      break;
    case 'E':
    case 'e':
    case '\n':
      dsky_press_key(KEY_ENTER);
      break;
    case -2:
      break;
    default:
      if('1' <= c && c <= '9')
        dsky_press_key(c - '1' + KEY_ONE);
  }


  uint64_t current_time = time_us_64();
  if(next_time > current_time) return;

  keyboard_t current_keyboard = read_keyboard();
  keyboard_t keys_down = to_keys_down(last_keyboard, current_keyboard);

  if(keys_down.entr)
    dsky_press_key(KEY_ENTER);
  else if(keys_down.verb)
    dsky_press_key(KEY_VERB);
  else if(keys_down.noun)
    dsky_press_key(KEY_NOUN);
  else if(keys_down.zero)
    dsky_press_key(KEY_ZERO);
  else if(keys_down.one)
    dsky_press_key(KEY_ONE);
  else if(keys_down.two)
    dsky_press_key(KEY_TWO);
  else if(keys_down.three)
    dsky_press_key(KEY_THREE);
  else if(keys_down.four)
    dsky_press_key(KEY_FOUR);
  else if(keys_down.five)
    dsky_press_key(KEY_FIVE);
  else if(keys_down.six)
    dsky_press_key(KEY_SIX);
  else if(keys_down.seven)
    dsky_press_key(KEY_SEVEN);
  else if(keys_down.eight)
    dsky_press_key(KEY_EIGHT);
  else if(keys_down.nine)
    dsky_press_key(KEY_NINE);
  else if(keys_down.rset)
    dsky_press_key(KEY_RSET);
  else if(keys_down.key_rel)
    dsky_press_key(KEY_KEY_REL);
  else if(keys_down.pro)
    dsky_press_pro(0);
  else if(keys_down.clr)
    dsky_press_key(KEY_CLR);
  else if(keys_down.plus)
    dsky_press_key(KEY_PLUS);
  else if(keys_down.minus)
    dsky_press_key(KEY_MINUS);

  if(last_keyboard.pro && !current_keyboard.pro)
    dsky_press_pro(1);

  last_keyboard = current_keyboard;
  next_time = current_time + 10000;
}
