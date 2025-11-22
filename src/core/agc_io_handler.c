/*
  Copyright 2020 Michael Karl Franzl <public.michael@franzl.name>

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

  Filename:  ringbuffer_api.c
  Purpose:   Allows yaAGC to read/write i/o packets from/to simple ringbuffers
             in memory.  This file was based on NullAPI.c. Some code was copied
             from SocketAPI.c (where noted).
             Ring buffers were chosen so as to not slow down the AGC execution
             from possibly slow foreign function calls. The ringbuffers
             obviously have to be filled and emptied from other places where
             exact timing is not so relevant.
  Contact:   Michael Karl Franzl <public.michael@franzl.name>
  Reference: http://www.ibiblio.org/apollo/index.html
  Mods:      2020-06-07 MKF  Created.
             2024-01-29 MAS  Added stub RequestRadarData function for use
                             by integrators of yaAGC into spacecraft
                             simulations.
*/

#include "agc_engine.h"
#include <core/agc.h>
#include "ringbuffer.h"

int16_t last_rhc_pitch = 0;
int16_t last_rhc_yaw   = 0;
int16_t last_rhc_roll  = 0;

static int channel_is_set_up = 0;

static void init_dsky_channel(agc_state_t* state)
{
  channel_is_set_up = 1;

  ringbuffer_init(&ringbuffer_out);
  ringbuffer_init(&ringbuffer_in);
}

/* The simulated AGC CPU calls this function when it wants to output data.  It
 * stores the data in ringbuffer_out so as to not have to call
 * a foreign and possibly slow function.  The data is supposed to be read from
 * the ring buffer asynchronously. See also NullAPI.c
 *
 * The bulk of the code was copied from SocketAPI.c
 */
void agc_channel_output(agc_state_t* state, int channel, int value)
{
  if(!channel_is_set_up)
    init_dsky_channel(state);

  // Some output channels have purposes within the CPU, so we have to
  // account for those separately.
  if(channel == 7)
  {
    input(7) = state->output_channel_7 = (value & 0160);
    return;
  }

  // Stick data into the RHCCTR registers, if bits 8,9 of channel 013 are set.
  if(channel == 013 && 0600 == (0600 & value) && !CmOrLm)
  {
    mem0(042) = last_rhc_pitch;
    mem0(043) = last_rhc_yaw;
    mem0(044) = last_rhc_roll;
  }

  packet_t packet = {.channel = channel, .value = value};
  ringbuffer_put(&ringbuffer_out, (unsigned char*)&packet);
}

/* The simulated AGC CPU calls this function when it wants to input data.
 * The data is read from the ring buffer ringbuffer_in.
 * See also NullAPI.c
 */
int agc_channel_input(agc_state_t* state)
{
  if(!channel_is_set_up)
    init_dsky_channel(state);

  packet_t packet;
  while(ringbuffer_get(&ringbuffer_in, (unsigned char*)&packet))
  {
    // This body of the while loop follows the work done in SocketAPI.c.
    // I removed socket client related code, and refactored and reformatted
    // the code.

    packet.value &= 077777; // Convert to AGC format (only keep upper 15 bits).
    if(packet.channel & 0x80)
    {
      // This is a counter increment. According to NullAPI.c we need to
      // immediately return a value of 1.
      unprogrammed_increment(state, packet.channel, packet.value);
      return 1;
    }

    uint16_t mask = 077777;
    if(packet.channel == 24)
    {
      mask = 0x0010;
    }

    // Mask out irrelevant bits, set current bits, and write to CPU.
    packet.value &= mask;
    packet.value |= read_io(state, packet.channel) & ~mask;
    write_io(state, packet.channel, packet.value);

    // If this is a keystroke from the DSKY, generate an interrupt req.
    if(packet.channel == 015)
    {
      state->interrupt_requests[5] = 1;
    }
    // If this is on fictitious input channel 0173, then the data
    // should be placed in the INLINK counter register, and an
    // UPRUPT interrupt request should be set.
    else if(packet.channel == 0173)
    {
      mem0(RegINLINK)              = (packet.value & 077777);
      state->interrupt_requests[7] = 1;
    }
    // Fictitious registers for rotational hand controller (RHC).
    // Note that the RHC angles are not immediately used, but
    // merely squirreled away for later.  They won't actually
    // go into the counter registers until the RHC counters are
    // enabled and the data requested (bits 8,9 of channel 13).
    else if(packet.channel == 0166)
    {
      last_rhc_pitch = packet.value;
      agc_channel_output(state, packet.channel, packet.value); // echo
    }
    else if(packet.channel == 0167)
    {
      last_rhc_yaw = packet.value;
      agc_channel_output(state, packet.channel, packet.value); // echo
    }
    else if(packet.channel == 0170)
    {
      last_rhc_roll = packet.value;
      agc_channel_output(state, packet.channel, packet.value); // echo
    }
  } // while

  return 0;
}

void channel_routine(agc_state_t* state)
{
}

void request_radar_data(agc_state_t* state)
{
}
