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


#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "yaAGC.h"
#include "agc_cli.h"
#include "agc_engine.h"

#include "agc_simulator.h"

#include <assert.h>

/** Declare the singleton Simulator object instance */
static Simulator_t Simulator;

static int SimInitializeEngine(void)
{
	int result = 0;

	/* Initialize the simulation */
	if (!Simulator.Options->debug_dsky)
	{
		if (Simulator.Options->resume == NULL)
		{
			if (Simulator.Options->cfg)
			{
				if (CmOrLm)
				{
					result = agc_engine_init (&Simulator.State,
							Simulator.Options->core, "CM.core", 0);
				}
				else
				{
					result = agc_engine_init (&Simulator.State,
							Simulator.Options->core, "LM.core", 0);
				}
			}
			else if (Simulator.Options->no_resume)
			{
				result = agc_engine_init (&Simulator.State,
						Simulator.Options->core, NULL, 0);
			}
			else
			{
				result = agc_engine_init (&Simulator.State,
						Simulator.Options->core, "core", 0);
			}
		}
		else
		{
			result = agc_engine_init (&Simulator.State,
					Simulator.Options->core, Simulator.Options->resume, 1);
		}

		/* Check AGC Engine Init Result and display proper message */
		switch (result)
		{
		    case 0:
		    	break; /* All is OK */
			case 1:
			  printf ("Specified core-rope image file not found.\n");
			  break;
			case 2:
			  printf ("Core-rope image larger than core memory.\n");
			  break;
			case 3:
			  printf ("Core-rope image file must have even size.\n");
			  break;
			case 5:
			  printf ("Core-rope image file read error.\n");
			  break;
			default:
			  printf ("Initialization implementation error.\n");
			  break;
		}
	}

	return (result);
}

/**
This function executes one cycle of the AGC engine. This is
a wrapper function to eliminate showing the passing of the
current engine state. */
static void SimExecuteEngine()
{
	agc_engine (&Simulator.State);
}


/**
Initialize the AGC Simulator; this means setting up the debugger, AGC
engine and initializing the simulator time parameters.
*/
int SimInitialize(Options_t* Options)
{
	int result = 0;

	/* Without Options we can't Run */
	if (!Options) return(6);

	/* Set the basic simulator variables */
	Simulator.Options = Options;
	Simulator.DebugRules = DebugRules;
	Simulator.DumpInterval = Options->dump_time * sysconf (_SC_CLK_TCK);

	/* Set legacy Option variables */
	DebugDsky = Options->debug_dsky;
	DebugDeda = Options->debug_deda;
	DedaQuiet = Options->deda_quiet;
	InhibitAlarms = Options->inhibit_alarms;
	ShowAlarms = Options->show_alarms;
	initializeSunburst37 = Options->initializeSunburst37;

	//Simulator.DumpInterval = Simulator.DumpInterval;
	SocketInterlaceReload = Options->interlace;

	/* If we are not in quiet mode display the version info */

	/* Initialize the AGC Engine */
	result = SimInitializeEngine();

	/* Initialize the Debugger if running with debug mode */

//	if (Options->cdu_log)
//	{
//	  extern FILE *CduLog;
//	  CduLog = fopen (Options->cdu_log, "w");
//	}

	/* Initialize realtime and cycle counters */
	Simulator.RealTimeOffset = times (&Simulator.DummyTime);	// The starting time of the program.
	Simulator.NextCoreDump = Simulator.RealTimeOffset + Simulator.DumpInterval;
	SimSetCycleCount(SIM_CYCLECOUNT_AGC); // Num. of AGC cycles so far.
	Simulator.RealTimeOffset -= (Simulator.CycleCount + AGC_PER_SECOND / 2) / AGC_PER_SECOND;
	Simulator.LastRealTime = ~((clock_t) 0);

	return (result | Options->version);
}

/**
This function adjusts the Simulator Cycle Count. Either based on the number
of AGC Cycles or incremented during Sim cycles. This functions uses a
mode switch to determine how to set or adjust the Cycle Counter
*/
void SimSetCycleCount(int Mode)
{
	switch(Mode)
	{
		case SIM_CYCLECOUNT_AGC:
			Simulator.CycleCount = sysconf (_SC_CLK_TCK) * Simulator.State.CycleCounter;
			break;
		case SIM_CYCLECOUNT_INC:
			Simulator.CycleCount += sysconf (_SC_CLK_TCK);
			break;
	}
}

/**
This function puts the simulator in a sleep state to reduce
CPU usage on the PC.
*/
static void SimSleep(void)
{
#ifdef WIN32
	Sleep (10);
#else
	struct timespec req, rem;
	req.tv_sec = 0;
	req.tv_nsec = 10000000;
	nanosleep (&req, &rem);
#endif
}


/**
 * This function is a helper to allow the debugger to update the realtime
 */
void SimUpdateTime(void)
{
	Simulator.RealTimeOffset +=
		((Simulator.RealTime = times (&Simulator.DummyTime)) - Simulator.LastRealTime);
	Simulator.LastRealTime = Simulator.RealTime;
}

/**
This function manages the Simulator time to achieve the
average 11.7 microsecond per opcode execution
*/
static void SimManageTime(void)
{
	Simulator.RealTime = times (&Simulator.DummyTime);
	if (Simulator.RealTime != Simulator.LastRealTime)
	{
		// Need to recalculate the number of AGC cycles we're supposed to
		// have executed.  Notice the trick of multiplying both CycleCount
		// and DesiredCycles by CLK_TCK, to avoid a long long division by CLK_TCK.
		// This not only reduces overhead, but actually makes the calculation
		// more exact.  A bit tricky to understand at first glance, though.
		Simulator.LastRealTime = Simulator.RealTime;

		//DesiredCycles = ((RealTime - RealTimeOffset) * AGC_PER_SECOND) / CLK_TCK;
		//DesiredCycles = (RealTime - RealTimeOffset) * AGC_PER_SECOND;
		// The calculation is done in the following funky way because if done as in
		// the line above, the right-hand side will be done in 32-bit arithmetic
		// on a 32-bit CPU, while the left-hand side is 64-bit, and so the calculation
		// will overflow and fail after 4 minutes of operation.  Done the following
		// way, the calculation will always be 64-bit.  Making AGC_PER_SECOND a ULL
		// constant in agc_engine.h would fix it, but the Orbiter folk wouldn't
		// be able to compile it.
		Simulator.DesiredCycles = Simulator.RealTime;
		Simulator.DesiredCycles -= Simulator.RealTimeOffset;
		Simulator.DesiredCycles *= AGC_PER_SECOND;
	}
	else SimSleep();
}

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

void
OutputKeycode (Keyboard Keycode)
{
  dsky_channel_output(015, Keycode);
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
int idx = 0;
Keyboard keys[10] = {
  KEY_VERB,
  KEY_THREE,
  KEY_FIVE,
  KEY_ENTER
};

int map[] = {
 10,
 10, 10,
 1,
 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
 4,
 10, 10, 10,
 7,
 10,
 0,
 10, 10, 10,
 2,
 10,
 3,
 6,
 8,
 5,
 9
};

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
  int prog;
  int verb;
  int noun;
  dsky_row_t rows[3];
} dsky_t;

int update_dsky(dsky_t *dsky, uint16_t value)
{
  int loc = (value >> 11) & 0x0F;
  int sign = (value >> 10) & 1;
  int left = map[(value >> 5) & 0x1F];
  int right = map[value & 0x1F];

  switch (loc)
  {
  case 1:
    dsky->rows[2].fifth = right;
    dsky->rows[2].fourth = left;
    dsky->rows[2].minus |= sign;
    break;
  case 2:
    dsky->rows[2].third = right;
    dsky->rows[2].second = left;
    dsky->rows[2].plus = sign;
    break;
  case 3:
    dsky->rows[2].first = left;
    dsky->rows[1].fifth = right;
    break;
  case 4:
    dsky->rows[1].fourth = right;
    dsky->rows[1].third = left;
    dsky->rows[1].minus = sign;
    break;
  case 5:
    dsky->rows[1].second = right;
    dsky->rows[1].first = left;
    dsky->rows[1].plus = sign;
    break;
  case 6:
    dsky->rows[0].fifth = right;
    dsky->rows[0].fourth = left;
    dsky->rows[0].minus = sign;
    break;
  case 7:
    dsky->rows[0].third = right;
    dsky->rows[0].second = left;
    dsky->rows[0].plus = sign;
    break;
  case 8:
    dsky->rows[0].first = right;
    break;
  case 9:
    dsky->noun = left << 4 | right;
    break;
  case 10:
    dsky->verb = left << 4 | right;
    break;
  case 11:
    dsky->prog = left << 4 | right;
    break;
  default:
    return 0;
  }

  return 1;
}

char digit2char(unsigned int digit)
{
  if (digit >= 10) return ' ';
  return '0' + digit;
}

void print_dsky_row(dsky_row_t *row)
{
  printf("%c%c%c%c%c%c\n", row->minus ? '-' : row->plus ? '+' : ' ', digit2char(row->first), digit2char(row->second), digit2char(row->third), digit2char(row->fourth), digit2char(row->fifth));
}

void print_dsky(dsky_t *dsky)
{
  printf("PR VB NO\n");
  printf("%c%c %c%c %c%c\n", digit2char(dsky->prog >> 4), digit2char(dsky->prog & 0x7), digit2char(dsky->verb >> 4), digit2char(dsky->verb & 0x7d), digit2char(dsky->noun >> 4), digit2char(dsky->noun & 0x7));
  print_dsky_row(&dsky->rows[0]);
  print_dsky_row(&dsky->rows[1]);
  print_dsky_row(&dsky->rows[2]);
}

void SimExecute(void)
{
  dsky_t dsky = {0};
	while(1)
	{
		/* Manage the Simulated Time */
		SimManageTime();

		while (Simulator.CycleCount < Simulator.DesiredCycles)
		{
			/* Execute a cycle of the AGC engine */
			SimExecuteEngine();

		  int channel;
		  int value;
		  while (dsky_channel_input(&channel, &value))
		  {
		    if (channel == 010)
		    {
          if (update_dsky(&dsky, value))
          {
		        print_dsky(&dsky);
          }
		    }
		  }

		  if (Simulator.CycleCount % 10000000 == 0)
		  {
        if (idx < 4)
		      OutputKeycode(keys[idx++]);
		  }

		  /* Adjust the CycleCount */
		  SimSetCycleCount(SIM_CYCLECOUNT_INC);
		}
	}
}



