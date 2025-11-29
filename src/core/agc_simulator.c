/*
  Original Copyright 2003-2006,2009 Ronald S. Burkey <info@sandroid.org>
  Modified Copyright 2008,2016 Onno Hommes <ohommes@alumni.cmu.edu>

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

  In addition, as a special exception, permission is given to
  link the code of this program with the Orbiter SDK library (or with
  modified versions of the Orbiter SDK library that use the same license as
  the Orbiter SDK library), and distribute linked combinations including
  the two. You must obey the GNU General Public License in all respects for
  all of the code used other than the Orbiter SDK library. If you modify
  this file, you may extend this exception to your version of the file,
  but you are not obligated to do so. If you do not wish to do so, delete
  this exception statement from your version.

  Filename:	agc_simulator.c
  Purpose:	This module implements the simulator behavior
  Contact:	Onno Hommes <ohommes@alumni.cmu.edu>
  Reference:	http://www.ibiblio.org/apollo
  Mods:         12/02/08 OH.	Began rework
                08/04/16 OH     Fixed the GPL statement and old user-id
                09/30/16 MAS    Added the InhibitAlarms option.
 */

#include <assert.h>
#include <core/agc_simulator.h>
#include <core/dsky.h>
#include <sys/time.h>

#include "agc_engine.h"
#include "us_time.h"

#ifndef PICO_BOARD
#include <stdio.h>
#include <unistd.h>
#endif


/**
This function executes one cycle of the AGC engine. This is
a wrapper function to eliminate showing the passing of the
current engine state. */
static void sim_exec_engine(sim_t* sim)
{
  agc_engine(&sim->state);
}

/**
Initialize the AGC Simulator; this means setting up the debugger, AGC
engine and initializing the simulator time parameters.
*/
int init_sim(sim_t* sim, opt_t* opt)
{
  int result = 0;

  /* Without Options we can't Run */
  if(!opt)
    return (6);

  /* Set the basic simulator variables */
  sim->dump_interval = opt->dump_time * sysconf(_SC_CLK_TCK);

  /* Set legacy Option variables */
  InhibitAlarms = opt->inhibit_alarms;
  ShowAlarms    = opt->show_alarms;
  /* If we are not in quiet mode display the version info */

  return (result | opt->version);
}


#ifdef PICO_BOARD
#include "pico/stdlib.h"
#endif

/**
This function puts the simulator in a sleep state to reduce
CPU usage on the PC.
*/
static void sim_sleep(void)
{
#ifdef WIN32
  Sleep(10);
#elif defined(PICO_BOARD)
  sleep_us(10000);
#else
  struct timespec req, rem;
  req.tv_sec  = 0;
  req.tv_nsec = 10000000;
  nanosleep(&req, &rem);
#endif
}

static void mul64x64(uint64_t lhs, uint64_t rhs, uint64_t* low, uint64_t* high)
{
  uint64_t a_lo = (uint32_t)lhs;
  uint64_t a_hi = lhs >> 32;
  uint64_t b_lo = (uint32_t)rhs;
  uint64_t b_hi = rhs >> 32;

  uint64_t p0 = a_lo * b_lo;
  uint64_t p1 = a_lo * b_hi;
  uint64_t p2 = a_hi * b_lo;
  uint64_t p3 = a_hi * b_hi;

  uint64_t carry = ((p0 >> 32) + (uint32_t)p1 + (uint32_t)p2) >> 32;

  *low = p0;
  *high = p3 + (p1 >> 32) + (p2 >> 32) + carry;
}

uint64_t mul_fixed_point(uint64_t lhs, uint64_t rhs, uint8_t shift)
{
  uint64_t low, high;
  mul64x64(lhs, rhs, &low, &high);
  return (high << (64 - shift)) | (low >> shift);
}

#define AGC_PER_US_I17F47 0xa6aaaaaaaaaaa800

void sim_exec(sim_t* sim)
{
  dsky_t dsky;
  dsky_init(&dsky);

  uint64_t start_us = time_us_64();

  bool mode = 0;

  while(1)
  {
    //sync cycles with the speed of the agc
    uint64_t current_us = time_us_64() - start_us;
    uint64_t desired_ucycles = mul_fixed_point(current_us, AGC_PER_US_I17F47, 47);
    uint64_t current_ucycles = sim->state.cycle_counter * 1000000;

    if(current_ucycles < desired_ucycles){
      sim_exec_engine(sim);
    }else{
      sim2agc_handle(&sim->state, &dsky);
      agc2dsky_handle(&sim->state, &dsky);
      dsky2agc_handle();
    }

    //handle_timer(&dsky);
  }
}
