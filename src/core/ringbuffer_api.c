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

#include "core/yaAGC.h"
#include "agc_engine.h"
#include "ringbuffer.h"

int LastRhcPitch, LastRhcYaw, LastRhcRoll;

static int CurrentChannelValues[256] = { 0 };
static int ChannelMasks[256] = { 077777 };

static int ChannelIsSetUp = 0;

static void
ChannelSetup (agc_t* State)
{
  ChannelIsSetUp = 1;

  ringbuffer_init (&ringbuffer_out);
  ringbuffer_init (&ringbuffer_in);

  for (int i = 0; i < 256; i++)
    ChannelMasks[i] = 077777;
}

/* The simulated AGC CPU calls this function when it wants to output data.  It
 * stores the data in ringbuffer_out so as to not have to call
 * a foreign and possibly slow function.  The data is supposed to be read from
 * the ring buffer asynchronously. See also NullAPI.c
 *
 * The bulk of the code was copied from SocketAPI.c
 */
void
agc_channel_output (agc_t* State, int channel, int value)
{
  if (!ChannelIsSetUp)
    ChannelSetup (State);

  // Some output channels have purposes within the CPU, so we have to
  // account for those separately.
  if (channel == 7)
    {
      State->InputChannel[7] = State->OutputChannel7 = (value & 0160);
      return;
    }

  // Stick data into the RHCCTR registers, if bits 8,9 of channel 013 are set.
  if (channel == 013 && 0600 == (0600 & value) && !CmOrLm)
    {
      State->Erasable[0][042] = LastRhcPitch;
      State->Erasable[0][043] = LastRhcYaw;
      State->Erasable[0][044] = LastRhcRoll;
    }

  packet_t packet = {
    .channel = channel,
    .value = value
  };

  ringbuffer_put (&ringbuffer_out, (unsigned char*)&packet);
}


/* The simulated AGC CPU calls this function when it wants to input data.
 * The data is read from the ring buffer ringbuffer_in.
 * See also NullAPI.c
 */
int
agc_channel_input (agc_t* State)
{
  if (!ChannelIsSetUp)
    ChannelSetup (State);

  packet_t packet;
  while (ringbuffer_get(&ringbuffer_in, (unsigned char*)&packet))
    {
      // This body of the while loop follows the work done in SocketAPI.c.
      // I removed socket client related code, and refactored and reformatted
      // the code.

      int uBit = 0;
      packet.value &= 077777; // Convert to AGC format (only keep upper 15 bits).

      if (uBit)
        {
          // This is not an actual input to the CPU. It only sets a bit mask
          // for masking of future inputs.
          ChannelMasks[packet.channel] = packet.value;
          continue; // Proceed with the next packet in the ring buffer.
        }

      if (packet.channel & 0x80)
        {
          // This is a counter increment. According to NullAPI.c we need to
          // immediately return a value of 1.
          UnprogrammedIncrement (State, packet.channel, packet.value);
          return 1;
        }

      // Mask out irrelevant bits, set current bits, and write to CPU.
      packet.value &= ChannelMasks[packet.channel];
      packet.value |= ReadIO (State, packet.channel) & ~ChannelMasks[packet.channel];
      WriteIO (State, packet.channel, packet.value);

      // If this is a keystroke from the DSKY, generate an interrupt req.
      if (packet.channel == 015){
        State->InterruptRequests[5] = 1;
      }
      // If this is on fictitious input channel 0173, then the data
      // should be placed in the INLINK counter register, and an
      // UPRUPT interrupt request should be set.
      else if (packet.channel == 0173)
        {
          State->Erasable[0][RegINLINK] = (packet.value & 077777);
          State->InterruptRequests[7] = 1;
        }
      // Fictitious registers for rotational hand controller (RHC).
      // Note that the RHC angles are not immediately used, but
      // merely squirreled away for later.  They won't actually
      // go into the counter registers until the RHC counters are
      // enabled and the data requested (bits 8,9 of channel 13).
      else if (packet.channel == 0166)
        {
          LastRhcPitch = packet.value;
          agc_channel_output (State, packet.channel, packet.value);  // echo
        }
      else if (packet.channel == 0167)
        {
          LastRhcYaw = packet.value;
          agc_channel_output (State, packet.channel, packet.value);  // echo
        }
      else if (packet.channel == 0170)
        {
          LastRhcRoll = packet.value;
          agc_channel_output (State, packet.channel, packet.value);  // echo
        }
    } // while

  return 0;
}


void
ChannelRoutine (agc_t* State)
{
}

void
ShiftToDeda (agc_t* State, int Data)
{
}

void
RequestRadarData (agc_t *State)
{
}
