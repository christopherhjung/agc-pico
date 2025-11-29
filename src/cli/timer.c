
#include "timer.h"

#include <unistd.h>
#include <string.h>

#include "core/us_time.h"

#define IDLE_STATE 0
#define VERB_FIRST_STATE 1
#define VERB_SECOND_STATE 2
#define NOUN_FIRST_STATE 3
#define NOUN_SECOND_STATE 4
#define SET_TIMER_STATE 5

uint8_t last_mode = 0;
uint8_t mode = 0;
uint8_t idx = 0;
uint8_t timer_idx = 0;

typedef struct
{
  uint64_t target;
  bool running;
} timer_t;

timer_t timers[3];

uint8_t parse(dsky_two_t two)
{
  if(two.second == 15)
    return two.first;
  return two.first * 10 + two.second;
}

uint64_t dsky_row_decode_time(dsky_row_t row)
{
  uint8_t first = row.first == 15 ? 0 : row.first;
  uint8_t second = row.second == 15 ? 0 : row.second;
  uint8_t third = row.third == 15 ? 0 : row.third;
  uint8_t fourth = row.fourth == 15 ? 0 : row.fourth;
  uint8_t fifth = row.fifth == 15 ? 0 : row.fifth;

  return first * 3600 + second * 600 + third * 60 + fourth * 10 + fifth;
}

void dsky_row_encode_time(int64_t us, dsky_row_t* row)
{
  row->plus = false;
  row->minus = false;
  if(us > 0){
    row->plus = true;
  }else if(us < 0){
    row->minus = true;
    us *= -1;
  }

  us = (us + 500000) / 1000000;

  row->fifth = us % 10;
  us /= 10;
  row->fourth = us % 6;
  us /= 6;
  row->third = us % 10;
  us /= 10;
  row->second = us % 6;
  us /= 6;
  row->first = us % 10;

  row->first = row->first == 0 ? 15 : row->first;
  row->second = row->first == 15 && row->second == 0 ? 15 : row->second;
  row->third = row->second == 15 && row->third == 0 ? 15 : row->third;
  row->fourth = row->third == 15 && row->fourth == 0 ? 15 : row->fourth;
}

void handle_idle_state(dsky_t* dsky, char c)
{
  switch(c)
  {
    case 'V':
    case 'v':
      mode = VERB_FIRST_STATE;
      dsky->verb.first = 15;
      dsky->verb.second = 15;
      break;
    case 'N':
    case 'n':
      mode = NOUN_FIRST_STATE;
      dsky->noun.first = 15;
      dsky->noun.second = 15;
      break;
    case 'P':
    case 'p':
    {
      for(size_t idx = 0; idx < 3; ++idx)
      {
        if(timers[idx].target < time_us_64())
        {
          timers[idx].running = false;
          dsky_row_init(&dsky->rows[idx]);
        }
      }
      break;
    }
    case '\n':
    {
      uint8_t verb_nr = parse(dsky->verb);
      if(verb_nr == 1)
      {
        mode = SET_TIMER_STATE;
      }
      break;
    }
  }
}

void handle_first_verb_state(dsky_t* dsky, char c)
{
  if('0' <= c && c <= '9'){
    dsky->verb.first = c - '0';
    mode = VERB_SECOND_STATE;
  }else{
    handle_idle_state(dsky, c);
  }
}

void handle_second_verb_state(dsky_t* dsky, char c)
{
  if('0' <= c && c <= '9'){
    dsky->verb.second = c - '0';
    mode = IDLE_STATE;
  }else{
    handle_idle_state(dsky, c);
  }
}

void handle_first_noun_state(dsky_t* dsky, char c)
{
  if('0' <= c && c <= '9'){
    dsky->noun.first = c - '0';
    mode = NOUN_SECOND_STATE;
  }else{
    handle_idle_state(dsky, c);
  }
}

void handle_second_noun_state(dsky_t* dsky, char c)
{
  if('0' <= c && c <= '9'){
    dsky->noun.second = c - '0';
    mode = IDLE_STATE;
  }else{
    handle_idle_state(dsky, c);
  }
}

void handle_state_timer(dsky_t* dsky, char c)
{
  uint8_t timer_idx = parse(dsky->noun);
  if(!(1 <= timer_idx && timer_idx <= 3))
    mode = IDLE_STATE;

  --timer_idx;
  timer_t* timer = &timers[timer_idx];

  dsky_row_t* row = &dsky->rows[timer_idx];

  if(last_mode != SET_TIMER_STATE)
  {
    timer->running = false;
    row->minus = true;
    row->plus = false;
    row->first = 15;
    row->second = 15;
    row->third = 15;
    row->fourth = 15;
    row->fifth = 15;
  }

  if(c == '\n'){
    mode = IDLE_STATE;
    uint32_t seconds = dsky_row_decode_time(*row);
    timer->target = time_us_64() + seconds * 1000000;
    timer->running = true;
  }else if('0' <= c && c <= '9'){
    row->first = row->second;
    row->second = row->third;
    row->third = row->fourth;
    row->fourth = row->fifth;
    row->fifth = c - '0';
  }else{
    handle_idle_state(dsky, c);
  }
}


void handle(dsky_t* dsky, char c)
{
  uint8_t prev_mode = mode;
  switch(mode)
  {
    case IDLE_STATE:
      handle_idle_state(dsky, c);
      break;
    case VERB_FIRST_STATE:
      handle_first_verb_state(dsky, c);
      break;
    case VERB_SECOND_STATE:
      handle_second_verb_state(dsky, c);
      break;
    case NOUN_FIRST_STATE:
      handle_first_noun_state(dsky, c);
      break;
    case NOUN_SECOND_STATE:
      handle_second_noun_state(dsky, c);
      break;
    case SET_TIMER_STATE:
      handle_state_timer(dsky, c);
      break;
  }

  last_mode = prev_mode;
}

void handle_timer(dsky_t* dsky)
{

  while(true)
  {
    for(size_t idx = 0; idx < 3; ++idx)
    {
      timer_t* timer = &timers[idx];
      if(timer->running)
      {
        uint64_t now = time_us_64();
        int64_t remaining = (int64_t)now - timer->target;
        dsky_row_encode_time(remaining, &dsky->rows[idx]);
      }
    }

    int c = getchar();
    if(c != -1)
    {
      handle(dsky, c);
    }

    //if(memcmp(&old, dsky, sizeof(dsky_t)) == 0)
    dsky_refresh(dsky);
    usleep(100000);
  }
}