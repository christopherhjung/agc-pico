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

  Filename:	agc_simulator.h
  Purpose:	This header contains the simulator interface definitions
  Contact:	Onno Hommes <ohommes@alumni.cmu.edu>
  Reference:	http://www.ibiblio.org/apollo
  Mods:         12/02/08 OH.	Began rework
                08/04/16 OH     Fixed the GPL statement and old user-id
 */
#pragma once

#include "agc.h"
#include "agc_engine.h"

#ifdef PLATFORMIO
struct tms
{
  clock_t tms_utime;  /* user time */
  clock_t tms_stime;  /* system time */
  clock_t tms_cutime; /* user time of children */
  clock_t tms_cstime; /* system time of children */
};

#define _SC_CLK_TCK (1000)
#define sysconf(x) (x)
#define times(p) (clock_t)0

#else

#include <sys/times.h>
#include <time.h>

#endif

#define SIM_E_OK 0
#define SIM_E_VERSION 6

#define SIM_CYCLECOUNT_INC 1
#define SIM_CYCLECOUNT_AGC 2

typedef struct
{
  char* core;
  char* resume;
  char* cdu_log;
  char* symtab;
  char* directory;
  char* cd;
  char* cfg;
  char* fromfile;
  int   port;
  int   dump_time;
  int   debug_dsky;
  int   debug_deda;
  int   deda_quiet;
  int   inhibit_alarms;
  int   show_alarms;
  int   quiet;
  int   fullname;
  int   debug;
  int   interlace;
  int   resumed;
  int   version;
  int   initializeSunburst37;
  int   no_resume;
} opt_t;

typedef struct
{
  opt_t*      opt;
  clock_t     dump_interval;
  clock_t     real_time_offset;
  clock_t     real_time;
  clock_t     last_real_time;
  clock_t     next_core_dump;
  uint64_t    desired_cycles;
  uint64_t    cycle_dump;
  struct tms  dummy_time;
  agc_state_t state;
} sim_t;

extern sim_t Simulator;

extern void sim_set_cycle_count(int Mode);
extern int  sim_init(opt_t* opt);
extern void sim_exec(void);
extern void sim_time_update(void);
