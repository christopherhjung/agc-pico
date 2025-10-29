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

#include "core/agc_simulator.h"

#include <assert.h>

#include "agc_engine.h"
#include "core/dsky.h"

#ifdef PLATFORMIO
#include <Arduino.h>
#else
#include <stdio.h>
#include <unistd.h>
#endif

/** Declare the singleton Simulator object instance */
sim_t Simulator;

static int sim_initialize_engine(void)
{
  int result = 0;

  /* Initialize the simulation */
  if(!Simulator.opt->debug_dsky)
  {
    if(Simulator.opt->resume == NULL)
    {
      if(Simulator.opt->cfg)
      {
        if(CmOrLm)
        {
          result = agc_engine_init(
            &Simulator.state, Simulator.opt->core, "CM.core", 0);
        }
        else
        {
          result = agc_engine_init(
            &Simulator.state, Simulator.opt->core, "LM.core", 0);
        }
      }
      else if(Simulator.opt->no_resume)
      {
        result = agc_engine_init(&Simulator.state, Simulator.opt->core, NULL, 0);
      }
      else
      {
        result = agc_engine_init(
          &Simulator.state, Simulator.opt->core, "core", 0);
      }
    }
    else
    {
      result = agc_engine_init(
        &Simulator.state, Simulator.opt->core, Simulator.opt->resume, 1);
    }

    /* Check AGC Engine Init Result and display proper message */
    switch(result)
    {
      case 0:
        break; /* All is OK */
      case 1:
        printf("Specified core-rope image file not found.\n");
        break;
      case 2:
        printf("Core-rope image larger than core memory.\n");
        break;
      case 3:
        printf("Core-rope image file must have even size.\n");
        break;
      case 5:
        printf("Core-rope image file read error.\n");
        break;
      default:
        printf("Initialization implementation error.\n");
        break;
    }
  }

  return (result);
}

/**
This function executes one cycle of the AGC engine. This is
a wrapper function to eliminate showing the passing of the
current engine state. */
static void sim_exec_engine()
{
  agc_engine(&Simulator.state);
}

/**
Initialize the AGC Simulator; this means setting up the debugger, AGC
engine and initializing the simulator time parameters.
*/
int sim_init(opt_t* Options)
{
  int result = 0;

  /* Without Options we can't Run */
  if(!Options)
    return (6);

  /* Set the basic simulator variables */
  Simulator.opt           = Options;
  Simulator.dump_interval = Options->dump_time * sysconf(_SC_CLK_TCK);

  /* Set legacy Option variables */
  DebugDsky     = Options->debug_dsky;
  InhibitAlarms = Options->inhibit_alarms;
  ShowAlarms    = Options->show_alarms;
  /* If we are not in quiet mode display the version info */

  /* Initialize the AGC Engine */
  result = sim_initialize_engine();

  /* Initialize realtime and cycle counters */
  Simulator.real_time_offset = times(&Simulator.dummy_time); // The starting time of the program.
  Simulator.next_core_dump = Simulator.real_time_offset + Simulator.dump_interval;
  sim_set_cycle_count(SIM_CYCLECOUNT_AGC); // Num. of AGC cycles so far.
  Simulator.real_time_offset -=
    (Simulator.cycle_dump + AGC_PER_SECOND / 2) / AGC_PER_SECOND;
  Simulator.last_real_time = ~((clock_t)0);

  return (result | Options->version);
}

/**
This function adjusts the Simulator Cycle Count. Either based on the number
of AGC Cycles or incremented during Sim cycles. This functions uses a
mode switch to determine how to set or adjust the Cycle Counter
*/
void sim_set_cycle_count(int Mode)
{
  switch(Mode)
  {
    case SIM_CYCLECOUNT_AGC:
      Simulator.cycle_dump = sysconf(_SC_CLK_TCK) * Simulator.state.cycle_counter;
      break;
    case SIM_CYCLECOUNT_INC:
      Simulator.cycle_dump += sysconf(_SC_CLK_TCK);
      break;
  }
}

/**
This function puts the simulator in a sleep state to reduce
CPU usage on the PC.
*/
static void sim_sleep(void)
{
#ifdef WIN32
  Sleep(10);
#elif defined(PLATFORMIO)
  delayMicroseconds(10000);
#else
  struct timespec req, rem;
  req.tv_sec  = 0;
  req.tv_nsec = 10000000;
  nanosleep(&req, &rem);
#endif
}

/**
 * This function is a helper to allow the debugger to update the realtime
 */
void sim_time_update(void)
{
  Simulator.real_time_offset +=
    ((Simulator.real_time = times(&Simulator.dummy_time)) - Simulator.last_real_time);
  Simulator.last_real_time = Simulator.real_time;
}

/**
This function manages the Simulator time to achieve the
average 11.7 microsecond per opcode execution
*/
static void sim_manage_time(void)
{
  Simulator.real_time = times(&Simulator.dummy_time);
  if(Simulator.real_time != Simulator.last_real_time)
  {
    // Need to recalculate the number of AGC cycles we're supposed to
    // have executed.  Notice the trick of multiplying both CycleCount
    // and DesiredCycles by CLK_TCK, to avoid a long long division by CLK_TCK.
    // This not only reduces overhead, but actually makes the calculation
    // more exact.  A bit tricky to understand at first glance, though.
    Simulator.last_real_time = Simulator.real_time;

    //DesiredCycles = ((RealTime - RealTimeOffset) * AGC_PER_SECOND) / CLK_TCK;
    //DesiredCycles = (RealTime - RealTimeOffset) * AGC_PER_SECOND;
    // The calculation is done in the following funky way because if done as in
    // the line above, the right-hand side will be done in 32-bit arithmetic
    // on a 32-bit CPU, while the left-hand side is 64-bit, and so the calculation
    // will overflow and fail after 4 minutes of operation.  Done the following
    // way, the calculation will always be 64-bit.  Making AGC_PER_SECOND a ULL
    // constant in agc_engine.h would fix it, but the Orbiter folk wouldn't
    // be able to compile it.
    Simulator.desired_cycles = Simulator.real_time;
    Simulator.desired_cycles -= Simulator.real_time_offset;
    Simulator.desired_cycles *= AGC_PER_SECOND;
  }
  else
    sim_sleep();
}

/**
Execute the simulated CPU.  Expecting to ACCURATELY cycle the simulation every
11.7 microseconds within Linux (or Win32) is a bit too much, I think.
(Not that it's really critical, as long as it looks right from the
user's standpoint.)  So I do a trick:  I just execute the simulation
often enough to keep up with real-time on the average.  AGC time is
measured as the number of machine cycles divided by AGC_PER_SECOND,
while real-time is measured using the times() function.  What this mains
is that AGC_PER_SECOND AGC cycles are executed every CLK_TCK clock ticks.
The timing is thus rather rough-and-ready (i.e., I'm sure it can be improved
a lot).  It's good enough for me, for NOW, but I'd be happy to take suggestions
for how to improve it in a reasonably portable way.*/
void sim_exec(void)
{
  dsky_t dsky;
  dsky_init(&dsky);

  agc_engine_init(&Simulator.state, "state/Core.bin", NULL, 0);
  while(1)
  {
    /* Manage the Simulated Time */
    sim_manage_time();

    while(Simulator.cycle_dump < Simulator.desired_cycles)
    {
      /* Execute a cycle of the AGC engine */
      sim_exec_engine();

      dsky_input_handle(&dsky);
      dsky_output_handle();

      /* Adjust the CycleCount */
      sim_set_cycle_count(SIM_CYCLECOUNT_INC);
    }
  }
}
