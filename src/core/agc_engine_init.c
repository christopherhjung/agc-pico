/*
 * Copyright 2003-2006,2009,2017 Ronald S. Burkey <info@sandroid.org>
 *
 * This file is part of yaAGC.
 *
 * yaAGC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaAGC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with yaAGC; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * In addition, as a special exception, Ronald S. Burkey gives permission to
 * link the code of this program with the Orbiter SDK library (or with
 * modified versions of the Orbiter SDK library that use the same license as
 * the Orbiter SDK library), and distribute linked combinations including
 * the two. You must obey the GNU General Public License in all respects for
 * all of the code used other than the Orbiter SDK library. If you modify
 * this file, you may extend this exception to your version of the file,
 * but you are not obligated to do so. If you do not wish to do so, delete
 * this exception statement from your version.
 *
 * Filename:	agc_engine_init.c
 * Purpose:	This is the function which initializes the AGC simulation,
 * 		from a file representing the binary image of core memory.
 * Compiler:	GNU gcc.
 * Contact:	Ron Burkey <info@sandroid.org>
 * Reference:	http://www.ibiblio.org/apollo/index.html
 * Mods:	04/05/03 RSB.	Began.
 *  		09/07/03 RSB.	Fixed data ordering in the core-rope image
 * 				file (to both endian CPU types to work).
 * 		11/26/03 RSB.	Up to now, a pseudo-linear space was used to
 * 				model internal AGC memory.  This was simply too
 * 				tricky to work with, because it was too hard to
 * 				understand the address conversions that were
 * 				taking place.  I now use a banked model much
 * 				closer to the true AGC memory map.
 * 		11/29/03 RSB.	Added the core-dump save/load.
 * 		05/06/04 RSB	Now use rfopen in looking for the binary.
 * 		07/12/04 RSB	Q is now 16 bits.
 * 		07/15/04 RSB	AGC data now aligned at bit 0 rather than 1.
 * 		07/17/04 RSB	I/O channels 030-033 now default to 077777
 * 				instead of 00000, since the signals are
 * 				supposed to be inverted.
 * 		02/27/05 RSB	Added the license exception, as required by
 * 				the GPL, for linking to Orbiter SDK libraries.
 * 		05/14/05 RSB	Corrected website references.
 * 	 	07/05/05 RSB	Added AllOrErasable.
 * 		07/07/05 RSB	On a resume, now restores 010 on up (rather
 * 				than 020 on up), on Hugh's advice.
 * 		02/26/06 RSB	Various changes requested by Mark Grant
 * 				to make it easier to integrate with Orbiter.
 * 				The main change is the addition of an
 * 				agc_load_binfile function.  Shouldn't affect
 * 				non-orbiter builds.
 * 		02/28/09 RSB	Fixed some compiler warnings for 64-bit machines.
 * 		03/18/09 RSB	Eliminated periodic messages about
 * 				core-dump creation when the DebugMode
 * 				flag is set.
 * 		03/27/09 RSB	I've noticed that about half the time, using
 * 				--resume causes the DSKY to become non-responsive.
 * 				I wonder if somehow not all the state variables
 * 				are being saved, and in particular not the
 * 				state related to interrupt.  (I haven't checked
 * 				this!)  Anyhow, there are extra state variables
 * 				in the agc_t structure which aren't being
 * 				saved or restored, so I'm adding all of these.
 * 		03/30/09 RSB	Added the Downlink variable to the core dumps.
 * 		08/14/16 OH	Issue #29 fix return value of agc_engine_init.
 * 		09/30/16 MAS    Added initialization of NightWatchman.
 * 		01/04/17 MAS    Added initialization of ParityFail.
 * 		01/30/17 MAS    Added support for heuristic loading of ROM files
 *                 		produced with --hardware, by looking for any set
 *                              parity bits. If such a file is detected, parity
 *                              bit checking is enabled.
 * 		03/09/17 MAS    Added initialization of SbyStillPressed.
 * 		03/26/17 MAS    Added initialization of previously-static things
 *                              from agc_engine.c that are now in agc_t.
 * 		03/27/17 MAS    Fixed a parity-related program loading bug and
 *                              added initialization of a new night watchman bit.
 *  		04/02/17 MAS	Added initialization of a couple of flags used
 *  				for simulation of the TC Trap hardware bug.
 * 		04/16/17 MAS    Added initialization of warning filter variables.
 * 		05/16/17 MAS    Enabled interrupts at startup.
 * 		05/31/17 RSB	Added --initialize-sunburst-37.
 * 		07/13/17 MAS	Added initialization of the three HANDRUPT traps.
 * 		05/13/21 MKF	Disabled UnblockSocket for the WASI target
 *  				(there are no sockets in wasi-libc)
 * 		01/29/24 MAS	Added initialization of RadarGateCounter.
 */

#include <stdio.h>
#include <string.h>

#include "agc.h"
#include "agc_engine.h"

int initializeSunburst37 = 0;

//---------------------------------------------------------------------------
// Returns:
//      0 -- success.
//      1 -- ROM image file not found.
//      2 -- ROM image file larger than core memory.
//      3 -- ROM image file size is odd.
//      4 -- agc_t structure not allocated.
//      5 -- File-read error.
//      6 -- Core-dump file not found.
// Normally, on input the CoreDump filename is NULL, in which case all of the
// i/o channels, erasable memory, etc., are cleared to their reset values.
// When the CoreDump is loaded instead, it allows execution to continue precisely
// from the point at which the CoreDump was created, if AllOrErasable != 0.
// If AllOrErasable == 0, then only the erasable memory is initialized from the
// core-dump file.

int agc_load_rom(agc_state_t* state, const uint8_t* image, uint64_t image_size)
{
  // The following sequence of steps loads the ROM image into the simulated
  // core memory, in what I think is a pretty obvious way.

  // Must be an integral number of words.
  if(0 != (image_size & 1))
    return 3;

  image_size /= 2; // Convert byte-count to word-count.
  if(image_size > 36 * 02000)
    return 2;

  if(state == NULL)
    return 4;

  state->check_parity = 0;
  memset(&state->parities, 0, sizeof(state->parities));

  const uint16_t* image2 = (const uint16_t*)image;
  for(int bank = 2, j = 0, i = 0; i < image_size; i++)
  {
    // Within the input file, the fixed-memory banks are arranged in the order
    // 2, 3, 0, 1, 4, 5, 6, 7, ..., 35.  Therefore, we have to take a little care
    // reordering the banks.
    if(bank > 35) return 2;
    uint16_t raw_value = image2[i] >> 8 | ((image2[i] & 0xff) << 8);
    uint8_t parity    = raw_value & 1;

    state->fixed[bank][j] = raw_value >> 1;
    state->parities[(bank * 02000 + j) / 32] |= parity << (j % 32);
    j++;

    // If any of the parity bits are actually set, this must be a ROM built with
    // --hardware. Enable parity checking.
    if(parity)
      state->check_parity = 1;

    if(j == 02000)
    {
      j = 0;
      // Bank filled.  Advance to next fixed-memory bank.
      if(bank == 2)
        bank = 3;
      else if(bank == 3)
        bank = 0;
      else if(bank == 0)
        bank = 1;
      else if(bank == 1)
        bank = 4;
      else
        bank++;
    }
  }

  return 0;
}

int agc_engine_init(agc_state_t* state, const uint8_t* core_image, uint64_t core_size, int all_or_erasable)
{
  uint64_t lli;
  int      ret = 0, i, j, Bank;

  // Clear i/o channels.
  for(i = 0; i < NUM_CHANNELS; i++)
    input(i) = 0;
  input(030) = 037777;
  input(031) = 077777;
  input(032) = 077777;
  input(033) = 077777;

  // Clear erasable memory.
  for(Bank = 0; Bank < 8; Bank++)
    for(j = 0; j < 0400; j++)
      state->erasable[Bank][j] = 0;
  mem0(RegZ) = 04000; // Initial program counter.

  // Set up the CPU state variables that aren't part of normal memory.
  state->cycle_counter   = 0;
  state->extra_code      = 0;
  state->allow_interrupt = 1; // The GOJAM sequence enables interrupts
  state->interrupt_requests[8] = 1; // DOWNRUPT.
  //State->RegA16 = 0;
  state->pend_flag   = 0;
  state->pend_delay  = 0;
  state->extra_delay = 0;
  //State->RegQ16 = 0;

  state->output_channel_7 = 0;
  for(j = 0; j < 16; j++)
    state->output_channel_10[j] = 0;
  state->index_value = 0;
  for(j = 0; j < 1 + NUM_INTERRUPT_TYPES; j++)
    state->interrupt_requests[j] = 0;
  state->in_isr                 = 0;
  state->substitute_instruction = 0;
  state->downrupt_time_valid    = 1;
  state->downrupt_time          = 0;
  state->downlink               = 0;

  state->night_watchman         = 0;
  state->night_watchman_tripped = 0;
  state->rupt_lock              = 0;
  state->no_rupt                = 0;
  state->tc_trap                = 0;
  state->no_tc                  = 0;
  state->parity_fail            = 0;

  state->warning_filter    = 0;
  state->generated_warning = 0;

  state->restart_light     = 0;
  state->standby           = 0;
  state->sby_pressed       = 0;
  state->sby_still_pressed = 0;

  state->next_z                = 0;
  state->scale_counter         = 0;
  state->channel_routine_count = 0;

  state->dsky_timer       = 0;
  state->dsky_flash       = 0;
  state->dsky_channel_163 = 0;

  state->took_bzf  = 0;
  state->took_bzmf = 0;

  state->trap_31a = 0;
  state->trap_31b = 0;
  state->trap_32  = 0;

  state->radar_gate_counter = 0;

  if(initializeSunburst37)
  {
    mem0(0067) = 077777;
    mem0(0157) = 077777;
    mem0(0375) = 005605;
    mem0(0376) = 004003;
  }

  if(core_image == NULL)
    goto Done;

  ret = 5;

  // Load up the i/o channels.
  for(i = 0; i < NUM_CHANNELS; i++)
  {
    //if(1 != scanf("%o", &j))
      goto Done;
    if(all_or_erasable)
      input(i) = j;
  }

  // Load up erasable memory.
  for(Bank = 0; Bank < 8; Bank++)
    for(j = 0; j < 0400; j++)
    {
      if(1 != scanf("%o", &i))
        goto Done;
      if(all_or_erasable || Bank > 0 || j >= 010)
        state->erasable[Bank][j] = i;
    }

  if(all_or_erasable)
  {
    // Set up the CPU state variables that aren't part of normal memory.
    core_image += scanf("%o", &state->cycle_counter);
    core_image += scanf("%o", &i);
    state->extra_code = i;
    core_image += scanf("%o", &i);
    state->allow_interrupt = i;
    core_image += scanf("%o", &i);
    state->pend_flag = i;
    core_image += scanf("%o", &i);
    state->pend_delay = i;
    core_image += scanf("%o", &i);
    state->extra_delay = i;
    core_image += scanf("%o", &state->output_channel_7);

    for(j = 0; j < 16; j++)
    {
      core_image += scanf("%o", &state->output_channel_10[j]);
    }
    core_image += scanf("%o", &i);
    state->index_value = i;
    for(j = 0; j < 1 + NUM_INTERRUPT_TYPES; j++)
    {
      core_image += scanf("%o", &i);
      state->interrupt_requests[j] = i;
    }
    // Override the above and make DOWNRUPT always enabled at start.
    state->interrupt_requests[8] = 1;
    core_image += scanf("%o", &i);
    state->in_isr = i;
    core_image += scanf("%o", &i);
    state->substitute_instruction = i;
    core_image += scanf("%o", &i);
    state->downrupt_time_valid = i;
    core_image += scanf("%o", &i);
    state->downrupt_time = lli;
    core_image += scanf("%o", &i);
    state->downlink = i;
  }

  ret = 0;

Done:
  return (ret);
}

//-------------------------------------------------------------------------------
// A function for creating a core-dump which can be read by agc_engine_init.
