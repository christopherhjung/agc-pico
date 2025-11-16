/*
  Copyright 2003-2005,2007 Ronald S. Burkey <info@sandroid.org>
            2008-2009      Onno Hommes

  This file is part of yaAGC.

  yaAGC is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  yaAGC is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with yaAGC; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  In addition, as a special exception, Ronald S. Burkey gives permission to
  link the code of this program with the Orbiter SDK library (or with
  modified versions of the Orbiter SDK library that use the same license as
  the Orbiter SDK library), and distribute linked combinations including
  the two. You must obey the GNU General Public License in all respects for
  all of the code used other than the Orbiter SDK library. If you modify
  this file, you may extend this exception to your version of the file,
  but you are not obligated to do so. If you do not wish to do so, delete
  this exception statement from your version.

  Filename:	main.c
  Purpose:	A top-level program for running the AGC simulation\
  		in a PC environment.
  Compiler:	GNU gcc.
  Contact:	Onno Hommes <virtualagc@googlegroups.com>
  Reference:	http://www.ibiblio.org/apollo/index.html
  Mods:
		04/05/03 RSB	Began the AGC project
		02/03/08 OH	Start adding GDB/MI interface
		08/23/08 OH	Only support GDB/MI and not proprietary debugging
		03/12/09 OH	Complete re-write of the main function
		03/23/09 OH	Reduced main to bare minimum
		04/16/09 OH 	Merge April changes from RSB
		04/24/09 RSB	Added #include for pthreads.h.
*/

#include <core/agc_simulator.h>
#include <sys/fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "agc_cli.h"
#include "core/dsky.h"
#include "core/dsky_dump.h"
#include "core/profile.h"
#include "file.h"

// Set terminal to raw mode (no buffering, no enter needed)
void set_conio_terminal_mode()
{
  struct termios new_termios;

  tcgetattr(0, &new_termios); // get current terminal state
  new_termios.c_lflag &= ~(ICANON | ECHO); // disable canonical mode & echo
  tcsetattr(0, TCSANOW, &new_termios);
}

// Restore terminal to normal mode
void reset_terminal_mode()
{
  struct termios term;
  tcgetattr(0, &term);
  term.c_lflag |= (ICANON | ECHO);
  tcsetattr(0, TCSANOW, &term);
}

typedef struct {
  unsigned int parity : 1;
  unsigned int value : 15;
}intagc_t;


long long time_us_64(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000000LL + ts.tv_nsec / 1000);
}

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

uint8_t parse(dsky_two_t two)
{
  if(two.second == 15)
    return two.first;
  return two.first * 10 + two.second;
}

uint32_t parse_dsky_row(dsky_row_t row)
{
  uint8_t first = row.first == 15 ? 0 : row.first;
  uint8_t second = row.second == 15 ? 0 : row.second;
  uint8_t third = row.third == 15 ? 0 : row.third;
  uint8_t fourth = row.fourth == 15 ? 0 : row.fourth;
  uint8_t fifth = row.fifth == 15 ? 0 : row.fifth;

  return first * 3600 + second * 600 + third * 60 + fourth * 10 + fifth;
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

void handle_timer(dsky_t* dsky, char c)
{
  uint8_t timer = parse(dsky->noun);
  if(!(1 <= timer && timer <= 3))
    mode = IDLE_STATE;

  --timer;

  dsky_row_t* row = &dsky->rows[timer];

  if(last_mode != SET_TIMER_STATE)
  {
    row->first = 15;
    row->second = 15;
    row->third = 15;
    row->fourth = 15;
    row->fifth = 15;
  }

  if(c == '\n'){
    mode = IDLE_STATE;
    uint32_t seconds = parse_dsky_row(*row);
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
      handle_timer(dsky, c);
      break;
  }

  last_mode = prev_mode;
}

/**
The AGC main function from here the Command Line is parsed, the
Simulator is initialized and subsequently executed.
*/
int main(int argc, char* argv[])
{
  set_conio_terminal_mode();


  struct termios oldt, newt;
  int            ch;
  int            oldf;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  dsky_t dsky = {0};

  dsky_init(&dsky);

  while(true)
  {
    int c = getchar();
    if(c != -1)
    {
      handle(&dsky, c);
    }

    dsky_refresh(&dsky);
    usleep(100000);
  }


  const char *filename = "resources/profile.json";
  uint64_t len;
  char *file_contents = read_file(filename, &len);
  if (!file_contents) {
    return 1;
  }
  profile_load_file(file_contents, len);
  free((void*)file_contents);


  opt_t* opt = cli_parse_args(argc, argv);

  char *rom = read_file("bin/Colossus249.bin", &len);
  agc_load_rom(&Simulator.state, rom, len);
  free(rom);

  sim_init(opt);

  char *core = read_file("state/Core.bin", &len);
  agc_engine_init(&Simulator.state, core, len, 0);
  free(core);


  /* Declare Options and parse the command line */

  /* Initialize the Simulator and debugger if enabled
	 * if the initialization fails or Options is NULL then the simulator will
	 * return a non zero value and subsequently bail and exit the program */
  sim_exec();

  return (0);
}

void dsky_refresh(dsky_t *dsky)
{
  dsky_print(dsky);
}