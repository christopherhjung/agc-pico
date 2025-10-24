/*
 Copyright 2003-2005,2009,2016,2019 Ronald S. Burkey <info@sandroid.org>

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
 * In addition, as a special exception, permission is granted to
 * link the code of this program with the Orbiter SDK library (or with
 * modified versions of the Orbiter SDK library that use the same license as
 * the Orbiter SDK library), and distribute linked combinations including
 * the two. You must obey the GNU General Public License in all respects for
 * all of the code used other than the Orbiter SDK library. If you modify
 * this file, you may extend this exception to your version of the file,
 * but you are not obligated to do so. If you do not wish to do so, delete
 * this exception statement from your version.
 *
 * Filename:	agc_engine.c
 * Purpose:	This is the main engine for binary simulation of the Apollo AGC
 *  		computer.  It is separate from the Display/Keyboard (DSKY)
 *  		simulation and Apollo hardware simulation, though compatible
 *  		with them.  The executable binary may be created using the
 *  		yayul (Yet Another YUL) assembler.
 * Compiler:	GNU gcc.
 * Contact:	Ron Burkey <info@sandroid.org>
 * Reference:	http://www.ibiblio.org/apollo/index.html
 * Mods:	04/05/03 RSB.	Began.
 *   		08/20/03 RSB.	Now bitmasks are handled on input channels.
 *   		11/26/03 RSB.	Up to now, a pseudo-linear space was used to
 *   				model internal AGC memory.  This was simply too
 *   				tricky to work with, because it was too hard to
 *   				understand the address conversions that were
 *   				taking place.  I now use a banked model much
 *   				closer to the true AGC memory map.
 *   		11/28/03 RSB.	Added code for a bunch of instruction types,
 *   				and fixed a bunch of bugs.
 *   		11/29/03 RSB.	Finished out all of the instruction types.
 *   				Still a lot of uncertainty if Overflow
 *   				and/or editing has been handled properly.
 *   				Undoubtedly many bugs.
 *   		05/01/04 RSB	Now makes sure that in --debug-dsky mode
 *   				doesn't execute any AGC code (since there
 *   				isn't any loaded anyhow).
 *   		05/03/04 RSB	Added a workaround for "TC Q".  It's not
 *   				right, but it has to be more right than
 *   				what was there before.
 *   		05/04/04 RSB	Fixed a bug in CS, where the unused bit
 *   				could get set and therefore mess up
 *   				later comparisons.  Fixed an addressing bug
 *   				(was 10-bit but should have been 12-bit) in
 *   				the AD instruction.  DCA was completely
 *   				messed up (still don't know about overflow).
 *   		05/05/04 RSB	INDEX'ing was messed up because the pending
 *				index was zeroed before being completely
 *				used up.  Fixed the "CCS A" instruction.
 *				Fixed CCS in the case of negative compare-
 *				values.
 *		05/06/04 RSB	Added rfopen.  The operation of "DXCH L"
 *				(which is ambiguous in the docs but actually
 *				used --- at Luminary131 address 33,03514) ---
 *				has been redefined in accordance with the
 *				Luminary program's comments.  Adjusted
 *				"CS A" and "CA A", though I don't actually
 *				think they work differently.  Fixed a
 *				potential divide-by-0 in DV.
 *		05/10/04 RSB	Fixed up i/o channel operations so that they
 *				properly use AGC-formatted integers rather
 *				than the simulator's native-format integers.
 *		05/12/04 RSB	Added the data collection for backtraces.
 *		05/13/04 RSB	Corrected calculation of the superbank address.
 *		05/14/04 RSB	Added interrupt service and hopefully fixed the
 *				RESUME instruction.  Fixed a bunch of instructions
 *				(but not all, since I couldn't figure out how to
 *				consistently do it) that modify the Z register.
 *		05/15/04 RSB	Repaired the interrupt vector and the RESUME
 *				instruction, so that they do not automatically
 *				save/restore A, L, Q, and BB to/from
 *				ARUPUT, LRUPT, QRUPT, and BBRUPT.  The ISR
 *				is supposed to do that itself, if it wants
 *				it done. And (sigh!) the RESUME instruction
 *				wasn't working, but was treated as INDEX 017.
 *		05/17/04 RSB	Added MasterInterruptEnable.  Added updates of
 *				timer-registers TIME1 and TIME2, of TIME3 and
 *				TIME4, and of SCALER1 and SCALER2.
 *		05/18/04 RSB	The mask used for writing to channel 7 have
 *				changed from 0100 to 0160, because the
 *				Luminary131 source (p.59) claims that bits
 *				5-7 are used.  I don't know what bits 5-6
 *				are for, though.
 *		05/19/04 RSB	I'm beginning to grasp now what to do for
 *				overflow.  The AD instruction (in which
 *				overflow was messed up) and the TS instruction
 *				(which was completely bollixed) have hopefully
 *				been fixed now.
 *		05/30/04 RSB	Now have a spec to work from (my assembly-
 *				language manual).  Working to bring this code
 *				up to v0.50 of the spec.
 *		05/31/04 RSB	The instruction set has basically been completely
 *				rewritten.
 *		06/01/04 RSB	Corrected the indexing of instructions
 *				for negative indices.  Oops!  The instruction
 *				executed on RESUME was taken from BBRUPT
 *				instead of BRUPT.
 *		06/02/04 RSB	Found that I was using an unsigned datatype
 *				for EB, FB, and BB, thus causing comparisons of
 *				them to registers to fail.  Now autozero the
 *				unused bits of EB, FB, and BB.
 *		06/04/04 RSB	Separated ServerStuff function from agc_engine
 *				function.
 *		06/05/04 RSB	Fixed the TCAA, ZQ, and ZL instructions.
 *		06/11/04 RSB	Added a definition for uint16_t in Win32.
 *		06/30/04 RSB	Made SignExtend, AddSP16, and
 *				OverflowCorrected non-static.
 *		07/02/04 RSB	Fixed major bug in SU instruction, in which it
 *				not only used the wrong value, but overwrote
 *				the wrong location.
 *		07/04/04 RSB	Fixed bug (I hope) in converting "decent"
 *				to "DP".  The DAS instruction did not leave the
 *				proper values in the A,L registers.
 *		07/05/04 RSB	Changed DXCH to do overflow-correction on the
 *				accumulator.  Also, the special cases "DXCH A"
 *				and "DXCH L" were being checked improperly
 *				before, and therefore were not treated
 *				properly.
 *		07/07/04 RSB	Some cases of DP arithmetic with the MS word
 *				or LS word being -0 were fixed.
 *		07/08/04 RSB	CA and CS fixed to re-edit after doing their work.
 *				Instead of using the overflow-corrected
 *				accumulator, BZF and BZMF now use the complete
 *				accumulator.  Either positive or negative
 *				overflow blocks BZF, while positive overflow
 *				blocks BZMF.
 *		07/09/04 RSB	The DAS instruction has been completely rewritten
 *				to alter the relative signs of the output.
 *				Previously they were normalized to be identical,
 *				and this is wrong.  In the DV instruction, the
 *				case of remainder==0 needed to be fixed up to
 *				distinguish between +0 and -0.
 *		07/10/04 RSB	Completely replaced MSU.  And ... whoops! ...
 *				forgot to inhibit interrupts while the
 *				accumulator contains overflow.  The special
 *				cases "DCA L" and "DCS L" have been addressed.
 *				"CCS A" has been changed similarly to BZF and
 *				BZMF w.r.t. overflow.
 *		07/12/04 RSB	Q is now 16 bits.
 *		07/15/04 RSB	Pretty massive rewrites:  Data alignment changed
 *				to bit 0 rather than 1.  All registers at
 *				addresses less than REG16 are now 16 bits,
 *				rather than just A and Q.
 *		07/17/04 RSB	The final contents of L with DXCH, DCA, and
 *				DCS are now overflow-corrected.
 *		07/19/04 RSB	Added SocketInterlace/Reload.
 *		08/12/04 RSB	Now account for output ports that are latched
 *				externally, and for updating newly-attached
 *				peripherals with current i/o-port values.
 *		08/13/04 RSB	The Win32 version of yaAGC now recognizes when
 *				socket-disconnects have occurred, and allows
 *				the port to be reused.
 *		08/18/04 RSB	Split off all socket-related stuff into
 *				SocketAPI.c, so that a cleaner API could be
 *				available for integrators.
 *		02/27/05 RSB	Added the license exception, as required by
 *				the GPL, for linking to Orbiter SDK libraries.
 *		05/14/05 RSB	Corrected website references.
 *		05/15/05 RSB	Oops!  The unprogrammed counter increments were
 *				hooked up to i/o space rather than to
 *				erasable.  So incoming counter commands were
 *				ignored.
 *		06/11/05 RSB	Implemented the fictitious output channel 0177
 *				used to make it easier to implement an
 *				emulation of the gyros.
 *		06/12/05 RSB	Fixed the CounterDINC function to emit a
 *				POUT, MOUT, or ZOUT, as it is supposed to.
 *		06/16/05 RSB	Implemented IMU CDU drive output pulses.
 *		06/18/05 RSB	Fixed PINC/MINC at +0 to -1 and -0 to +1
 *				transitions.
 *		06/25/05 RSB	Fixed the DOWNRUPT interrupt requests.
 *		06/30/05 RSB	Hopefully fixed fine-alignment, by making
 *				the gyro torquing depend on the GYROCTR
 *				register as well as elapsed time.
 *		07/01/05 RSB	Replaced the gyro-torquing code, to
 *				avoid simulating the timing of the
 *				3200 pps. pulses, which was conflicting
 *				somehow with the timing Luminary wanted
 *				to impose.
 *		07/02/05 RSB	OPTXCMD & OPTYCMD.
 *		07/04/05 RSB	Fix for writes to channel 033.
 *		08/17/05 RSB	Fixed an embarrassing bug in SpToDecent,
 *				thanks to Christian Bucher.
 *		08/20/05 RSB	I no longer allow interrupts when the
 *				program counter is in the range 0-060.
 *				I do this principally to guard against the
 *				case Z=0,1,2, since I'm not sure that all of
 *				the AGC code saves registers properly in this
 *				case.  Now I inhibit interrupts prior to
 *				INHINT, RELINT, and EXTEND (as we're supposed
 *				to), as well as RESUME (as we're not supposed
 *				to).  The intent of the latter is to take
 *				care of problems that occur when EDRUPT is used.
 *				(Specifically, an interrupt can occur between
 *				RELINT and RESUME after an EDRUPT, and this
 *				messes up the return address in ZRUPT used by
 *				the RESUME.)
 *		08/21/05 RSB	Removed the interrupt inhibition from the
 *				address range 0-060, because it prevented
 *				recovery from certain conditions.
 *		08/28/05 RSB	Oops!  Had been using PINC sequences on
 *				TIME6 rather than DINC sequences.
 *		10/05/05 RSB	FIFOs were introduced for PCDU or MCDU
 *				commands on the registers CDUX, CDUY, CDUZ.
 *				The purpose of these FIFOs is to make sure
 *				that CDUX, CDUY, CDUZ are updated at no
 *				more than an 800 cps rate, in order to
 *				avoid problems with the Kalman filter in the
 *				DAP, which is otherwise likely to reject
 *				counts that change too quickly.
 *		10/07/05 RSB	FIFOs changed from 800 cps to either 400 cps
 *				("low rate") or 6400 cps ("high rate"),
 *				depending on the variable CduHighRate.
 *				At the moment, CduHighRate is stuck at 0,
 *				because we've worked out no way to plausibly
 *				change it.
 *		11/13/05 RSB	Took care of auto-adjust buffer timing for
 *				high-rate and low-rate CDU counter updates.
 *				PCDU/MCDU commands 1/3 are slow mode, and
 *				PCDU/MCDU commands 021/023 are fast mode.
 *		02/26/06 RSB	Oops!  This wouldn't build under Win32 because
 *				of the lack of an int32_t datatype.  Fixed.
 *		03/30/09 RSB	Moved Downlink local static variable from
 *				CpuWriteIO() to agc_t, trying to overcome the
 *				lack of resumption of telemetry after an AGC
 *				resumption in Windows.
 *		07/17/16 RSB	Commented out a variable that wasn't being
 *				used but was generating compiler warnings.
 *		08/31/16 MAS	Corrected implementation of DINC and TIME6.
 *				DINC now uses AddSP16() to do its math. T6RUPT
 *				can only be triggered by a ZOUT from DINC. TIME6
 *				only counts when enabled, and is disabled upon
 *				triggering T6RUPT.
 *		09/08/16 MAS	Added a special case for DV -- when the dividend
 *				is 0 and the divisor is not, the quotient and
 *				remainder are both 0 with the sign matching the
 *				dividend.
 *		09/11/16 MAS	Applied Gergo's fix for multi-MCT instructions
 *				taking a cycle longer than they should.
 *		09/30/16 MAS	Added emulation of the Night Watchman, TC Trap,
 *		                and Rupt Lock hardware alarms, alarm-generated
 *		                resets, and the CH77 restart monitor module.
 *		10/01/16 RSB	Moved location of "int ExecutedTC = 0" to the
 *				top of the function, since it triggered compiler
 *				warnings (and hence errors) for me.  (The
 *				"goto AllDone" jumped over it, leaving
 *				ExecutedTC potentially uninitialized.)
 *		10/01/16 MAS	Added a corner case to one of DV's corner cases
 *				(0 / 0), made CCS consider overflow before the
 *				diminished absolute value calculation, changed
 *				how interrupt priorities are handled, and
 *				corrected ZRUPT to be return addr+1.
 *				Aurora 12 now passes all of SELFCHK in yaAGC.
 *		10/04/16 MAS	Added support for standby mode, added the
 *				standby light to the light test, and fixed
 *				the speed of scaler counting and phasing of
 *				TIME6.
 *		11/12/16 MAS	Stopped preventing interrupts on Q and L
 *				overflow (only A overflow should do so). This
 *				was causing the O-UFLOW check in Validation
 *				to never allow interrupts, triggering a rupt
 *				lock alarm.
 *		11/12/16 MAS	Apparently CH11 bit 10 only turns off RESTART
 *				*on write*, with the actual value of the
 *				channel being otherwise  meaningless.
 *		12/19/16 MAS	Corrected one more bug in the DV instruction;
 *				the case of a number being divided by itself
 *				was not sign-extending the result in the L
 *				register. The overflow correction of the L
 *				register was then destroying the calculated
 *				sign. This was caught by Retread; apparently
 *				Aurora doesn't test for it.
 *		12/22/16 MAS	Fixed the No TC hardware alarm, discovered
 *				to be erroneously counting EXTENDS by
 *				BOREALIS.
 *		01/04/17 MAS	Added parity fail alarms caused by accessing
 *				nonexistent superbanks. There's still no way
 *				to get an erasable parity fail, because I
 *				haven't come up with a program that can cause
 *				one to happen.
 *		01/08/17 MAS	Corrected behavior of the EDRUPT instruction
 *				(it really is just a generic interrupt request).
 *				Along the way, re-enabled BRUPT substitution by
 *				default and allowed interrupts to happen after
 *				INDEXes.
 *		01/29/17 MAS	Hard-wired the DKSY's RSET button to turning
 *				off the RESTART light (the button had its
 *				own discrete to reset the RESTART flip flop
 *				in the real AGC/DSKY).
 *		01/30/17 MAS	Added parity bit checking for fixed memory
 *				when running with a ROM that contains
 *				parity bits.
 *		03/09/17 MAS	Prevented yaAGC from exiting standby if PRO is
 *				still held down from entry to standby. Also
 *				corrected turning off of RESTART, and in the
 *				process improved DSKY light latency. Last,
 *				added a new channel 163 bit that indicates
 *				power for the DSKY EL panel is switched off.
 *		03/11/17 MAS	Further improved DSKY light responsiveness,
 *				and split the logic out to its own function
 *				as a bit of housekeeping.
 *		03/26/17 MAS	Made several previously-static things a part
 *				of the agc_t state structure, which should
 *				make integration easier for simulator
 *				integrators.
 *		03/27/17 MAS	Fixed parity checking for superbanks, and added
 *				simulation of the Night Watchman's assertion of
 *				its channel 77 bit for 1.28 seconds after each
 *				triggering.
 *		04/02/17 MAS	Added simulation of a hardware bug in the
 *				design of the "No TCs" part of the TC Trap.
 *				Most causes of transients that can reset
 *				the alarm are now accounted for. Also
 *				corrected the phasing of the standby circuit
 *				and TIME1-TIME5 relative the to the scaler.
 *				With the newly corrected timer phasings,
 *				Aurora/Sunburst's self-tests cannot pass
 *				without simulation of the TC Trap bug.
 *		04/16/17 MAS	Added a simple linear model of the AGC warning
 *				filter, and added the AGC (CMC/LGC) warning
 *				light status to DSKY channel 163. Also added
 *				proper handling of the channel 33 inbit that
 *				indicates such a warning occurred.
 *		05/11/17 MAS	Moved special cases for writing to I/O channels
 *				from WriteIO into CpuWriteIO, allowing those
 *				channels to be safely written to via WriteIO
 *				by external callers (e.g. SocketAPI and NASSP).
 *		05/16/17 MAS	Made alarm restarts enable interrupts.
 *		07/13/17 MAS	Added simulation of HANDRUPT, as generated by
 *				any of three traps, which monitor various bits
 *				in channels 31 and 32.
 *		08/19/17 MAS	Made GOJAMs clear all pending interrupt requests,
 *				as well as a bunch of output channels and one
 *				input bit. Not doing so was preventing the DSKY
 *				RESTART light from working in Colossus 249 and
 *				all later CMC versions.
 *		09/03/17 MAS	Slightly tweaked handling of Z, so (as before)
 *				higher order bits typically disappear, but
 *				(newly) are visible if you're clever with
 *				use of ZRUPT. Also, made GOJAMs transfer the
 *				Z register value to Q on restart, which makes
 *				the RSBBQ erasables in Colossus/Luminary work.
 *		09/04/17 MAS	Extended emulation of DV to properly handle all
 *				off-nominal situations, including overflow in
 *				the divisor as well as all situations that create
 *				"total nonsense" as described by Savage&Drake
 *				(we previously simply returned random values).
 *		09/27/17 MAS	Fixed standby, which was broken by the GOJAM
 *				update. All I/O channels are held in their reset
 *				state via an ever-present GOJAM during standby,
 *				but the standby enabled bit (CH13 bit 11) is not
 *				required to be set to exit standby, only to enter.
 *		01/06/18 MAS	Added a new channel 163 bit for the TEMP light,
 *				which is the logical OR of channel 11 bit 4 and
 *				channel 30 bit 15. The AGC did this internally
 *				so the light would still work in standby.
 *		04/29/19 RSB	It seems that agc_symtab.h is no longer being used
 *				here, and that its presence isn't too desirable for
 *				those who are porting minimized implementations,
 *				so I've commented it out.
 *		01/03/24 MAS	Changed GOJAM simulation to clear out a lot more
 *				yaAGC-internal state that doesn't necessarily
 *				exactly match hardware GOJAM actions, but needs
 *				to be cleared out nevertheless due to how yaAGC
 *				is implemented. This is necessary to fix a
 *				longstanding bug where the AGC would fail to
 *				correctly exit standby, due to corruption of the
 *				first instruction at address 4000 (usually by a
 *				non-zero IndexValue).
 *		01/29/24 MAS	Added simulation of RADARUPT. Unlike other
 *				interrupts, RADARUPT is automatically generated
 *				internally by the AGC itself once a full radar
 *				cycle has had time to complete. This is important
 *				when running ropes sensitive to this timing,
 *				like Artemis and Skylark.
 *		07/16/24 MAS	Changed TC trap implementation to differentiate
 *				between real TC/TCFs and transients for the purpose
 *				of resetting the "TCs only" side of the alarm.
 *				This alarm was improperly considering all instructions
 *				that trigger transients to be real TCs/TCFs. This
 *				resulted in erroneous triggerings for instruction
 *				sequences consisting of only TCs and transient-
 *				generating instructions (e.g. a CS A; TC -1 loop).
 *
 * The technical documentation for the Apollo Guidance & Navigation (G&N) system,
 * or more particularly for the Apollo Guidance Computer (AGC) may be found at
 * http://hrst.mit.edu/hrs/apollo/public.  That is, to the extent that the
 * documentation exists online it may be found there.  I'm sure -- or rather
 * HOPE -- that there's more documentation at NASA and MIT than has been made
 * available yet.  I personally had no knowledge of the AGC, other than what
 * I had seen in the movie "Apollo 13" and the HBO series "From the Earth to
 * the Moon", before I conceived this project last night at midnight and
 * started doing web searches.  So, bear with me; it's a learning experience!
 *
 * Also at hrst.mit.edu are the actual programs for the Command Module (CM) and
 * Lunar Module (LM) AGCs.  Or rather, what's there are scans of 1700-page
 * printouts of assembly-language listings of SOME versions of those programs.
 * (Respectively, called "Colossus" and "Luminary".)  I'll worry about how to
 * get those into a usable version only after I get the CPU simulator working!
 *
 * What THIS file contains is basically a pure simulation of the CPU, without any
 * input and output as such.  (I/O, to the DSKY or to CM or LM hardware
 * simulations occurs through the mechanism of sockets, and hence the DSKY
 * front-end and hardware back-end simulations may be implemented as complete
 * stand-alone programs and replaced at will.)  There is a single globally
 * interesting function, called agc_engine, which is intended to be called once
 * per AGC instruction cycle -- i.e., every 11.7 microseconds.  (Yes, that's
 * right, the CPU clock speed was a little over 85 KILOhertz.  That's a factor
 * that obviously makes the simulation much easier!)  The function may be called
 * more or less often than this, to speed up or slow down the apparent passage
 * of time.
 *
 * This function is intended to be completely portable, so that it may be run in
 * a PC environment (Microsoft Windows) or in any *NIX environment, or indeed in
 * an embedded target if anybody should wish to create an actual physical
 * replacement for an AGC.  Also, multiple copies of the simulation may be run
 * on the same PC -- for example to simulation a CM and LM simultaneously.
 */

//#include <errno.h>
//#include <stdlib.h>
#include <stdio.h>
#include "agc.h"
#include "core/agc_engine.h"

// If COARSE_SMOOTH is 1, then the timing of coarse-alignment (in terms of
// bursting and separation of bursts) is according to the Delco manual.
// However, since the simulated IMU has no physical inertia, it adjusts
// instantly (and therefore jerkily).  The COARSE_SMOOTH constant creates
// smaller bursts, and therefore smoother FDAI motion.  Normally, there are
// 192 pulses in a burst.  In the simulation, there are 192/COARSE_SMOOTH
// pulses in a burst.  COARSE_SMOOTH should be in integral divisor of both
// 192 and of 50*1024.  This constrains it to be any power of 2, up to 64.
#define COARSE_SMOOTH 8

// Some helpful macros for manipulating registers.
#define c(Reg) state->erasable[0][Reg]
#define IsA(Address) ((Address) == RegA)
#define IsL(Address) ((Address) == RegL)
#define IsQ(Address) ((Address) == RegQ)
#define IsEB(Address) ((Address) == RegEB)
#define IsZ(Address) ((Address) == RegZ)
#define IsReg(Address, Reg) ((Address) == (Reg))

// Some helpful constants in parsing the "address" field from an instruction
// or from the Z register.
#define SIGNAL_00 000000
#define SIGNAL_01 002000
#define SIGNAL_10 004000
#define SIGNAL_11 006000
#define SIGNAL_0011 001400
#define MASK9 000777
#define MASK10 001777
#define MASK12 007777

// Some numerical constant, in AGC format.
#define AGC_P0 ((int16_t)0)
#define AGC_M0 ((int16_t)077777)
#define AGC_P1 ((int16_t)1)
#define AGC_M1 ((int16_t)077776)

// Here are arrays which tell (for each instruction, as determined by the
// uppermost 5 bits of the instruction) how many extra machine cycles are
// needed to execute the instruction.  (In other words, the total number of
// machine cycles for the instruction, minus 1.) The opcode and quartercode
// are taken into account.  There are two arrays -- one for normal
// instructions and one for "extracode" instructions.
static const int InstructionTiming[32] = {
  0, 0, 0, 0, // Opcode = 00.
  1, 0, 0, 0, // Opcode = 01.
  2, 1, 1, 1, // Opcode = 02.
  1, 1, 1, 1, // Opcode = 03.
  1, 1, 1, 1, // Opcode = 04.
  1, 2, 1, 1, // Opcode = 05.
  1, 1, 1, 1, // Opcode = 06.
  1, 1, 1, 1  // Opcode = 07.
};

// Note that the following table does not properly handle the EDRUPT or
// BZF/BZMF instructions, and extra delay may need to be added specially for
// those cases.  The table figures 2 MCT for EDRUPT and 1 MCT for BZF/BZMF.
static const int ExtracodeTiming[32] = {
  1, 1, 1, 1, // Opcode = 010.
  5, 0, 0, 0, // Opcode = 011.
  1, 1, 1, 1, // Opcode = 012.
  2, 2, 2, 2, // Opcode = 013.
  2, 2, 2, 2, // Opcode = 014.
  1, 1, 1, 1, // Opcode = 015.
  1, 0, 0, 0, // Opcode = 016.
  2, 2, 2, 2  // Opcode = 017.
};

// A way, for debugging, to disable interrupts. The 0th entry disables
// everything if 0.  Entries 1-10 disable individual interrupts.
int DebuggerInterruptMasks[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

//-----------------------------------------------------------------------------
// Stuff for doing structural coverage analysis.  Yes, I know it could be done
// much more cleverly.

int      COVERAGE_COUNTS = 0; // Increment coverage counts is != 0.
unsigned ErasableReadCounts[8][0400];
unsigned ErasableWriteCounts[8][0400];
unsigned ErasableInstructionCounts[8][0400];
unsigned FixedAccessCounts[40][02000];
unsigned IoReadCounts[01000];
unsigned IoWriteCounts[01000];

// For debugging the CDUX,Y,Z inputs.
FILE* CduLog = NULL;

//-----------------------------------------------------------------------------
// DSKY handling constants and variables.
#define DSKY_OVERFLOW 81920
#define DSKY_FLASH_PERIOD 4

#define WARNING_FILTER_INCREMENT 15000
#define WARNING_FILTER_DECREMENT 15
#define WARNING_FILTER_MAX 140000
#define WARNING_FILTER_THRESHOLD 20000

int DebugDsky     = 0;
int InhibitAlarms = 0;
int ShowAlarms    = 0;
int NumDebugRules = 0;

//-----------------------------------------------------------------------------
// Functions for reading or writing from/to i/o channels.  The reason we have
// to provide a function for this rather than accessing the i/o-channel buffer
// directly is that the L and Q registers appear in both memory and i/o space,
// at the same addresses.

int read_io(agc_state_t* state, int Address)
{
  if(Address < 0 || Address > 0777)
    return (0);
  if(COVERAGE_COUNTS)
    IoReadCounts[Address]++;
  if(Address == RegL || Address == RegQ)
    return (state->erasable[0][Address]);
  return (state->inputChannel[Address]);
}

void write_io(agc_state_t* state, int Address, int Value)
{
  // The value should be in AGC format.
  Value &= 077777;
  if(Address < 0 || Address > 0777)
    return;
  if(COVERAGE_COUNTS)
    IoWriteCounts[Address]++;
  if(Address == RegL || Address == RegQ)
    state->erasable[0][Address] = Value;

  if(Address == 010)
  {
    // Channel 10 is converted externally to the CPU into up to 16 ports,
    // by means of latching relays.  We need to capture this data.
    state->output_channel_10[(Value >> 11) & 017] = Value;
  }
  else if((Address == 015 || Address == 016) && Value == 022)
  {
    // RSET being pressed on either DSKY clears the RESTART light
    // flip-flop directly, without software intervention
    state->restart_light = 0;
  }
  else if(Address == 033)
  {
    // Channel 33 bits 11-15 are controlled internally, so don't let
    // anybody write to them
    Value = (state->inputChannel[Address] & 076000) | (Value & 001777);
  }

  state->inputChannel[Address] = Value;
}

void cpu_write_io(agc_state_t* state, int Address, int Value)
{
  //static int Downlink = 0;

  if(Address == 013)
  {
    // Enable the appropriate traps for HANDRUPT. Note that the trap
    // settings cannot be read back out, so after setting the traps the
    // enable bits are masked out.
    if(Value & 004000)
      state->trap_31a = 1;
    if(Value & 010000)
      state->trap_31b = 1;
    if(Value & 020000)
      state->trap_32 = 1;

    Value &= 043777;
  }
  if(Address == 033)
  {
    // 2005-07-04 RSB.  The necessity for this was pointed out by Mark
    // Grant via Markus Joachim.  Although channel 033 is an input channel,
    // the CPU writes to it from time to time, to "reset" bits 11-15 to 1.
    // Apparently, these are latched inputs, and this resets the latches.
    state->inputChannel[Address] |= 076000;

    // Don't allow the AGC warning input to be reset if the light
    // is still on
    if(state->warning_filter > WARNING_FILTER_THRESHOLD)
      state->inputChannel[Address] &= 057777;

    // The actual value that was written now doesn't matter, so make sure
    // no changes occur.
    Value = state->inputChannel[Address];
  }
  else if(Address == 077)
  {
    // Similarly, the CH77 Restart Monitor Alarm Box has latches for
    // alarm codes that are reset when CH77 is written to.
    Value = 0;

    // If the Night Watchman was recently tripped, its CH77 bit
    // is forcibly asserted (unlike all the others) for 1.28s
    if(state->night_watchman_tripped)
      Value |= CH77_NIGHT_WATCHMAN;
  }
  else if(Address == 011 && (Value & 01000))
  {
    // The DSKY RESTART light is reset whenever CH11 bit 10 is written
    // with a 1. The controlling flip-flop in the AGC also has a hard
    // line to the DSKY's RSET button, so on depression of RSET the
    // light is turned off without need for software intervention.
    state->restart_light = 0;
  }

  write_io(state, Address, Value);
  agc_channel_output(state, Address, Value & 077777);

  // 2005-06-25 RSB.  DOWNRUPT stuff.  I assume that the 20 ms. between
  // downlink transmissions is due to the time needed for transmitting,
  // so I don't interrupt at a regular rate,  Instead, I make sure that
  // there are 20 ms. between transmissions
  if(Address == 034)
    state->downlink |= 1;
  else if(Address == 035)
    state->downlink |= 2;
  if(state->downlink == 3)
  {
    //State->InterruptRequests[8] = 1;	// DOWNRUPT.
    state->downrupt_time_valid = 1;
    state->downrupt_time = state->cycle_counter + (AGC_PER_SECOND / 50);
    state->downlink      = 0;
  }
}

//-----------------------------------------------------------------------------
// This function does all of the processing associated with converting a
// 12-bit "address" as used within instructions or in the Z register, to a
// pointer to the actual word in the simulated memory.  In other words, here
// we take memory bank-selection into account.

static int16_t* FindMemoryWord(agc_state_t* state, int addr_12)
{
  //int PseudoAddress;
  int      adj_eb, adj_fb;
  int16_t* addr;

  // Get rid of the parity bit.
  //Address12 = Address12;

  // Make sure the darn thing really is 12 bits.
  addr_12 &= 07777;

  // Check to see if NEWJOB (67) has been accessed for Night Watchman
  if(addr_12 == 067)
  {
    // Address 67 has been accessed in some way. Clear the Night Watchman.
    state->night_watchman = 0;
  }

  // It should be noted as far as unswitched-erasable and common-fixed memory
  // is concerned, that the following rules actually do result in continuous
  // block of memory that don't have problems in crossing bank boundaries.
  if(addr_12 < 00400) // Unswitched-erasable.
    return (&state->erasable[0][addr_12 & 00377]);
  else if(addr_12 < 01000) // Unswitched-erasable (continued).
    return (&state->erasable[1][addr_12 & 00377]);
  else if(addr_12 < 01400) // Unswitched-erasable (continued).
    return (&state->erasable[2][addr_12 & 00377]);
  else if(addr_12 < 02000) // Switched-erasable.
  {
    // Recall that the parity bit is accounted for in the shift below.
    adj_eb = (7 & (c(RegEB) >> 8));
    return (&state->erasable[adj_eb][addr_12 & 00377]);
  }
  else if(addr_12 < 04000) // Fixed-switchable.
  {
    adj_fb = (037 & (c(RegFB) >> 10));
    // Account for the superbank bit.
    if(030 == (adj_fb & 030) && (state->output_channel_7 & 0100) != 0)
      adj_fb += 010;
  }
  else if(addr_12 < 06000) // Fixed-fixed.
    adj_fb = 2;
  else // Fixed-fixed (continued).
    adj_fb = 3;

  addr = (&state->fixed[adj_fb][addr_12 & 01777]);

  if(state->check_parity)
  {
    // Check parity for fixed memory if such checking is enabled
    uint16_t LinearAddr = adj_fb * 02000 + (addr_12 & 01777);
    int16_t  ExpectedParity =
      (state->parities[LinearAddr / 32] >> (LinearAddr % 32)) & 1;
    int16_t Word = ((*addr) << 1) | ExpectedParity;
    Word ^= (Word >> 8);
    Word ^= (Word >> 4);
    Word ^= (Word >> 2);
    Word ^= (Word >> 1);
    Word &= 1;
    if(Word != 1)
    {
      // The program is trying to access unused fixed memory, which
      // will trigger a parity alarm.
      state->parity_fail = 1;
      state->inputChannel[077] |= CH77_PARITY_FAIL;
    }
  }
  return addr;
}

// Same thing, basically, but for collecting coverage data.
#if 0
static void
CollectCoverage (agc_state_t * State, int Address12, int Read, int Write, int Instruction)
{
  int AdjustmentEB, AdjustmentFB;

  if (!COVERAGE_COUNTS)
  return;

  // Get rid of the parity bit.
  Address12 = Address12;

  // Make sure the darn thing really is 12 bits.
  Address12 &= 07777;

  if (Address12 < 00400)// Unswitched-erasable.
    {
      AdjustmentEB = 0;
      goto Erasable;
    }
  else if (Address12 < 01000)	// Unswitched-erasable (continued).
    {
      AdjustmentEB = 1;
      goto Erasable;
    }
  else if (Address12 < 01400)	// Unswitched-erasable (continued).
    {
      AdjustmentEB = 2;
      goto Erasable;
    }
  else if (Address12 < 02000)	// Switched-erasable.
    {
      // Recall that the parity bit is accounted for in the shift below.
      AdjustmentEB = (7 & (c (RegEB) >> 8));
      Erasable:
      Address12 &= 00377;
      if (Read)
      ErasableReadCounts[AdjustmentEB][Address12]++;
      if (Write)
      ErasableWriteCounts[AdjustmentEB][Address12]++;
      if (Instruction)
      ErasableInstructionCounts[AdjustmentEB][Address12]++;
    }
  else if (Address12 < 04000)	// Fixed-switchable.
    {
      AdjustmentFB = (037 & (c (RegFB) >> 10));
      // Account for the superbank bit.
      if (030 == (AdjustmentFB & 030) && (State->output_channel_7 & 0100) != 0)
      AdjustmentFB += 010;
      Fixed:
      FixedAccessCounts[AdjustmentFB][Address12 & 01777]++;
    }
  else if (Address12 < 06000)	// Fixed-fixed.
    {
      AdjustmentFB = 2;
      goto Fixed;
    }
  else				// Fixed-fixed (continued).
    {
      AdjustmentFB = 3;
      goto Fixed;
    }
  return;
}
#endif //0

//-----------------------------------------------------------------------------
// Assign a new value to "erasable" memory, performing editing as necessary
// if the destination address is one of the 4 editing registers.  The value to
// be written is a properly formatted AGC value in D1-15.  The difference between
// Assign and AssignFromPointer is simply that Assign needs a memory bank number
// and an offset into that bank, while AssignFromPointer simply uses a pointer
// directly to the simulated memory location.

static void assign(agc_state_t* state, int bank, int offset, int value)
{
  if(bank < 0 || bank >= 8)
    return; // Non-erasable memory.
  if(offset < 0 || offset >= 0400)
    return;
  if(COVERAGE_COUNTS)
    ErasableWriteCounts[bank][offset]++;
  if(bank == 0)
  {
    switch(offset)
    {
      case RegZ:
        state->next_z = value;
        break;
      case RegCYR:
        value &= 077777;
        if(0 != (value & 1))
          value = (value >> 1) | 040000;
        else
          value = (value >> 1);
        break;
      case RegSR:
        value &= 077777;
        if(0 != (value & 040000))
          value = (value >> 1) | 040000;
        else
          value = (value >> 1);
        break;
      case RegCYL:
        value &= 077777;
        if(0 != (value & 040000))
          value = (value << 1) + 1;
        else
          value = (value << 1);
        break;
      case RegEDOP:
        value &= 077777;
        value = ((value >> 7) & 0177);
        break;
      case RegZERO:
        value = AGC_P0;
        break;
      default:
        // No editing of the Value is needed in this case.
        break;
    }
    if(offset >= REG16 || (offset >= 020 && offset <= 023))
      state->erasable[0][offset] = value & 077777;
    else
      state->erasable[0][offset] = value & 0177777;
  }
  else
    state->erasable[bank][offset] = value & 077777;
}

static void assign_from_pointer(agc_state_t* state, int16_t* pointer, int value)
{
  int Address;
  Address = pointer - state->erasable[0];
  if(Address >= 0 && Address < 04000)
  {
    assign(state, Address / 0400, Address & 0377, value);
    return;
  }
}

//-----------------------------------------------------------------------------
// Compute the "diminished absolute value".  The input data and output data
// are both in AGC 1's-complement format.

static int16_t dabs(int16_t Input)
{
  if(0 != (040000 & Input))
    Input = 037777 & ~Input; // Input was negative, but now is positive.
  if(Input > 1)              // "diminish" it if >1.
    Input--;
  else
    Input = AGC_P0;
  return (Input);
}

// Same, but for 16-bit registers.
static int odabs(int Input)
{
  if(0 != (0100000 & Input))
    Input = (0177777 & ~Input); // Input was negative, but now is positive.
  if(Input > 1)                 // "diminish" it if >1.
    Input--;
  else
    Input = AGC_P0;
  return (Input);
}

//-----------------------------------------------------------------------------
// Convert an AGC-formatted word to CPU-native format.

static int agc2cpu(int Input)
{
  if(0 != (040000 & Input))
    return (-(037777 & ~Input));
  else
    return (037777 & Input);
}

//-----------------------------------------------------------------------------
// Convert a native CPU-formatted word to AGC format. If the input value is
// out of range, it is truncated by discarding high-order bits.

static int cpu2agc(int Input)
{
  if(Input < 0)
    return (077777 & ~(-Input));
  else
    return (077777 & Input);
}

//-----------------------------------------------------------------------------
// Double-length versions of the same.

static int agc2cpu2(int Input)
{
  if(0 != (02000000000 & Input))
    return (-(01777777777 & ~Input));
  else
    return (01777777777 & Input);
}

static int cpu2agc2(int Input)
{
  if(Input < 0)
    return (03777777777 & ~(01777777777 & (-Input)));
  else
    return (01777777777 & Input);
}

//----------------------------------------------------------------------------
// Here is a small suite of functions for converting back and forth between
// 15-bit SP values and 16-bit accumulator values.

#if 0

// Gets the full 16-bit value of the accumulator (plus parity bit).

static int
GetAccumulator (agc_state_t * State)
  {
    int Value;
    Value = State->erasable[0][RegA];
    Value &= 0177777;
    return (Value);
  }

// Gets the full 16-bit value of Q (plus parity bit).

static int
GetQ (agc_state_t * State)
  {
    int Value;
    Value = State->erasable[0][RegQ];
    Value &= 0177777;
    return (Value);
  }

// Store a 16-bit value (plus parity) into the accumulator.

static void
PutAccumulator (agc_state_t * State, int Value)
  {
    c (RegA) = (Value & 0177777);
  }

// Store a 16-bit value (plus parity) into Q.

static void
PutQ (agc_state_t * State, int Value)
  {
    c (RegQ) = (Value & 0177777);
  }

#endif // 0

// Returns +1, -1, or +0 (in SP) format, on the basis of whether an
// accumulator-style "16-bit" value (really 17 bits including parity)
// contains overflow or not.  To do this for the accumulator itself,
// use ValueOverflowed(GetAccumulator(State)).

static int16_t value_ovf(int Value)
{
  switch(Value & 0140000)
  {
    case 0040000:
      return (AGC_P1);
    case 0100000:
      return (AGC_M1);
    default:
      return (AGC_P0);
  }
}

// Return an overflow-corrected value from a 16-bit (plus parity ) SP word.
// This involves just moving bit 16 down to bit 15.

int16_t overflow_corrected(int Value)
{
  return ((Value & 037777) | ((Value >> 1) & 040000));
}

// Sign-extend a 15-bit SP value so that it can go into the 16-bit (plus parity)
// accumulator.

int sign_extend(int16_t Word)
{
  return ((Word & 077777) | ((Word << 1) & 0100000));
}

//-----------------------------------------------------------------------------
// Here are functions to convert a DP into a more-decent 1's-
// complement format in which there's not an extra sign-bit to contend with.
// (In other words, a 29-bit format in which there's a single sign bit, rather
// than a 30-bit format in which there are two sign bits.)  And vice-versa.
// The DP value consists of two adjacent SP values, MSW first and LSW second,
// and we're given a pointer to the second word.  The main difficulty here
// is dealing with the case when the two SP words don't have the same sign,
// and making sure all of the signs are okay when one or more words are zero.
// A sign-extension is added a la the normal accumulator.

static int sp_to_decent(int16_t* LsbSP)
{
  int16_t Msb, Lsb;
  int     Value, Complement;
  Msb = LsbSP[-1];
  Lsb = *LsbSP;
  if(Msb == AGC_P0 || Msb == AGC_M0) // Msb is zero.
  {
    // As far as the case of the sign of +0-0 or -0+0 is concerned,
    // we follow the convention of the DV instruction, in which the
    // overall sign is the sign of the less-significant word.
    Value = sign_extend(Lsb);
    if(Value & 0100000)
      Value |= ~0177777;
    return (07777777777 & Value); // Eliminate extra sign-ext. bits.
  }
  // If signs of Msb and Lsb words don't match, then make them match.
  if((040000 & Lsb) != (040000 & Msb))
  {
    if(Lsb == AGC_P0 || Lsb == AGC_M0) // Lsb is zero.
    {
      // Adjust sign of Lsb to match Msb.
      if(0 == (040000 & Msb))
        Lsb = AGC_P0;
      else
        Lsb = AGC_M0; // 2005-08-17 RSB.  Was "Msb".  Oops!
    }
    else // Lsb is not zero.
    {
      // The logic will be easier if the Msb is positive.
      Complement = (040000 & Msb);
      if(Complement)
      {
        Msb = (077777 & ~Msb);
        Lsb = (077777 & ~Lsb);
      }
      // We now have Msb positive non-zero and Lsb negative non-zero.
      // Subtracting 1 from Msb is equivalent to adding 2**14 (i.e.,
      // 0100000, accounting for the parity) to Lsb.  An additional 1
      // must be added to account for the negative overflow.
      Msb--;
      Lsb = ((Lsb + 040000 + AGC_P1) & 077777);
      // Restore the signs, if necessary.
      if(Complement)
      {
        Msb = (077777 & ~Msb);
        Lsb = (077777 & ~Lsb);
      }
    }
  }
  // We now have an Msb and Lsb of the same sign; therefore,
  // we can simply juxtapose them, discarding the sign bit from the
  // Lsb.  (And recall that the 0-position is still the parity.)
  Value = (03777740000 & (Msb << 14)) | (037777 & Lsb);
  // Also, sign-extend for further arithmetic.
  if(02000000000 & Value)
    Value |= 04000000000;
  return (Value);
}

static void decent_to_sp(int Decent, int16_t* LsbSP)
{
  int Sign;
  Sign   = (Decent & 04000000000);
  *LsbSP = (037777 & Decent);
  if(Sign)
    *LsbSP |= 040000;
  LsbSP[-1] = overflow_corrected(0177777 & (Decent >> 14)); // Was 13.
}

// Adds two sign-extended SP values.  The result may contain overflow.
int add_sp_16(int Addend1, int Addend2)
{
  int Sum;
  Sum = Addend1 + Addend2;
  if(Sum & 0200000)
  {
    Sum += AGC_P1;
    Sum &= 0177777;
  }
  return (Sum);
}

// Absolute value of an SP value.

static int16_t abs_sp(int16_t Value)
{
  if(040000 & Value)
    return (077777 & ~Value);
  return (Value);
}

// Check if an SP value is negative.

//static int
//IsNegativeSP (int16_t Value)
//{
//  return (0 != (0100000 & Value));
//}

// Negate an SP value.

static int16_t neg_sp(int16_t Value)
{
  return (077777 & ~Value);
}

//-----------------------------------------------------------------------------
// The following are various operations performed on counters, as defined
// in Savage & Drake (E-2052) 1.4.8.  The functions all return 0 normally,
// and return 1 on overflow.

#include <stdio.h>
static int TrapPIPA = 0;

// 1's-complement increment
int counter_pinc(int16_t* Counter)
{
  int16_t i;
  int     Overflow = 0;
  i                = *Counter;
  if(i == 037777)
  {
    Overflow = 1;
    i        = AGC_P0;
  }
  else
  {
    Overflow = 0;
    if(TrapPIPA)
      printf("PINC: %o", i);
    i = ((i + 1) & 077777);
    if(TrapPIPA)
      printf(" %o", i);
    if(i == AGC_P0) // Account for -0 to +1 transition.
      i++;
    if(TrapPIPA)
      printf(" %o\n", i);
  }
  *Counter = i;
  return (Overflow);
}

// 1's-complement decrement, but only of negative integers.
int counter_minc(int16_t* Counter)
{
  int16_t i;
  int     ovf = 0;
  i                = *Counter;
  if(i == (int16_t)040000)
  {
    ovf = 1;
    i        = AGC_M0;
  }
  else
  {
    ovf = 0;
    if(TrapPIPA)
      printf("MINC: %o", i);
    i = ((i - 1) & 077777);
    if(TrapPIPA)
      printf(" %o", i);
    if(i == AGC_M0) // Account for +0 to -1 transition.
      i--;
    if(TrapPIPA)
      printf(" %o\n", i);
  }
  *Counter = i;
  return (ovf);
}

// 2's-complement increment.
int counter_pcdu(int16_t* Counter)
{
  int16_t i;
  int     Overflow = 0;
  i                = *Counter;
  if(i == (int16_t)077777)
    Overflow = 1;
  i++;
  i &= 077777;
  *Counter = i;
  return (Overflow);
}

// 2's-complement decrement.
int counter_mcdu(int16_t* Counter)
{
  int16_t i;
  int     ovf = 0;
  i                = *Counter;
  if(i == 0)
    ovf = 1;
  i--;
  i &= 077777;
  *Counter = i;
  return (ovf);
}

// Diminish increment.
int counter_dinc(agc_state_t* State, int CounterNum, int16_t* Counter)
{
  int     RetVal = 0;
  int16_t i;
  i = *Counter;
  if(i == AGC_P0 || i == AGC_M0) // Zero?
  {
    // Emit a ZOUT.
    if(CounterNum != 0)
      agc_channel_output(State, 0x80 | CounterNum, 017);

    RetVal = 1;
  }
  else if(040000 & i) // Negative?
  {
    i = add_sp_16(sign_extend(i), sign_extend(AGC_P1)) & 077777;

    // Emit a MOUT.
    if(CounterNum != 0)
      agc_channel_output(State, 0x80 | CounterNum, 016);
  }
  else // Positive?
  {
    i = add_sp_16(sign_extend(i), sign_extend(AGC_M1)) & 077777;

    // Emit a POUT.
    if(CounterNum != 0)
      agc_channel_output(State, 0x80 | CounterNum, 015);
  }

  *Counter = i;

  return (RetVal);
}

// Left-shift increment.
int counter_shinc(int16_t* Counter)
{
  int16_t i;
  int     Overflow = 0;
  i                = *Counter;
  if(020000 & i)
    Overflow = 1;
  i        = (i << 1) & 037777;
  *Counter = i;
  return (Overflow);
}

// Left-shift and add increment.
int counter_shanc(int16_t* Counter)
{
  int16_t i;
  int     Overflow = 0;
  i                = *Counter;
  if(020000 & i)
    Overflow = 1;
  i        = ((i << 1) + 1) & 037777;
  *Counter = i;
  return (Overflow);
}

// Pinch hits for the above in setting interrupt requests with INCR,
// AUG, and DIM instructins.  The docs aren't very forthcoming as to
// which counter registers are affected by this ... but still.

static void interrupt_requests(agc_state_t* state, int16_t Address10, int Sum)
{
  if(value_ovf(Sum) == AGC_P0)
    return;
  if(IsReg(Address10, RegTIME1))
    counter_pinc(&c(RegTIME2));
  else if(IsReg(Address10, RegTIME5))
    state->interrupt_requests[2] = 1;
  else if(IsReg(Address10, RegTIME3))
    state->interrupt_requests[3] = 1;
  else if(IsReg(Address10, RegTIME4))
    state->interrupt_requests[4] = 1;
  // TIME6 requires a ZOUT to happen during a DINC sequence for its
  // interrupt to fire
}

//-------------------------------------------------------------------------------
// The case of PCDU or MCDU triggers being applied to the CDUX,Y,Z counters
// presents a special problem.  The AGC expects these triggers to be
// applied at a certain fixed rate.  The DAP portion of Luminary or Colossus
// applies a digital filter to the counts, in order to eliminate electrical
// noise, as well as noise caused by vibration of the spacecraft.  Therefore,
// if the simulated IMU applies PCDU/MCDU triggers too fast, the digital
// filter in the DAP will simply reject the count, and therefore the spacecraft's
// orientation cannot be measured by the DAP.  Consequently, we have to
// fake up a kind of FIFO on the triggers to the CDUX,Y,Z counters so that
// we can increment or decrement the counters at no more than the fixed rate.
// (Conversely, of course, the simulated IMU has to be able to supply the
// triggers *at least* as fast as the fixed rate.)
//
// Actually, there are two different fixed rates for PCDU/MCDU:  400 counts
// per second in "slow mode", and 6400 counts per second in "fast mode".
//
// *** FIXME! All of the following junk will need to move to agc_t, and will
//     somehow have to be made compatible with backtraces. ***
// The way the FIFO works is that it can hold an ordered set of + counts and
// - counts.  For example, if it held 7,-5,10, it would mean to apply 7 PCDUs,
// followed by 5 MCDUs, followed by 10 PCDUs.  If there are too many sign-changes
// buffered, triggers will be transparently dropped.
#define MAX_CDU_FIFO_ENTRIES 128
#define NUM_CDU_FIFOS 3 // Increase to 5 to include OPTX, OPTY.
#define FIRST_CDU 032

typedef struct
{
  int      Ptr;          // Index of next entry being pulled.
  int      Size;         // Number of entries.
  int      IntervalType; // 0,1,2,0,1,2,...
  uint64_t NextUpdate; // Cycle count at which next counter update occurs.
  int32_t Counts[MAX_CDU_FIFO_ENTRIES];
} cdu_fifo_t;

static cdu_fifo_t CduFifos[NUM_CDU_FIFOS]; // For registers 032, 033, and 034.
static int CduChecker = 0; // 0, 1, ..., NUM_CDU_FIFOS-1, 0, 1, ...

// Here's an auxiliary function to add a count to a CDU FIFO.  The only allowed
// increment types are:
//	001	PCDU "slow mode"
//	003	MCDU "slow mode"
//	021	PCDU "fast mode"
//	023	MCDU "fast mode"
// Within the FIFO, we distinguish these cases as follows:
//	001	Upper bits = 00
//	003	Upper bits = 01
//	021	Upper bits = 10
//	023	Upper bits = 11
// The least-significant 30 bits are simply the absolute value of the count.
static void push_cdu_fifo(agc_state_t* State, int Counter, int IncType)
{
  cdu_fifo_t* CduFifo;
  int        Next, Interval;
  int32_t    Base;
  if(Counter < FIRST_CDU || Counter >= FIRST_CDU + NUM_CDU_FIFOS)
    return;
  switch(IncType)
  {
    case 1:
      Interval = 213;
      Base     = 0x00000000;
      break;
    case 3:
      Interval = 213;
      Base     = 0x40000000;
      break;
    case 021:
      Interval = 13;
      Base     = 0x80000000;
      break;
    case 023:
      Interval = 13;
      Base     = 0xC0000000;
      break;
    default:
      return;
  }
  if(CduLog != NULL)
    fprintf(CduLog, "< " FORMAT_64U " %o %02o\n", State->cycle_counter, Counter, IncType);
  CduFifo = &CduFifos[Counter - FIRST_CDU];
  // It's a little easier if the FIFO is completely empty.
  if(CduFifo->Size == 0)
  {
    CduFifo->Ptr          = 0;
    CduFifo->Size         = 1;
    CduFifo->Counts[0]    = Base + 1;
    CduFifo->NextUpdate   = State->cycle_counter + Interval;
    CduFifo->IntervalType = 1;
    return;
  }
  // Not empty, so find the last entry in the FIFO.
  Next = CduFifo->Ptr + CduFifo->Size - 1;
  if(Next >= MAX_CDU_FIFO_ENTRIES)
    Next -= MAX_CDU_FIFO_ENTRIES;
  // Last entry has different sign from the new data?
  if((CduFifo->Counts[Next] & 0xC0000000) != (unsigned)Base)
  {
    // The sign is different, so we have to add a new entry to the
    // FIFO.
    if(CduFifo->Size >= MAX_CDU_FIFO_ENTRIES)
    {
      // No place to put it, so drop the data.
      return;
    }
    CduFifo->Size++;
    Next++;
    if(Next >= MAX_CDU_FIFO_ENTRIES)
      Next -= MAX_CDU_FIFO_ENTRIES;
    CduFifo->Counts[Next] = Base + 1;
    return;
  }
  // Okay, add in the new data to the last FIFO entry.  The sign is assured
  // to be compatible.  The size of the FIFO doesn't increase. We also don't
  // bother to check for arithmetic overflow, since only the wildest IMU
  // failure could cause it.
  CduFifo->Counts[Next]++;
}

// Here's an auxiliary function to perform the next available PCDU or MCDU
// from a CDU FIFO, if it is time to do so.  We only check one of the CDUs
// each time around (in order to preserve proper cycle counts), so this function
// must be called at at least an 6400*NUM_CDU_FIFO cps rate.  Returns 0 if no
// counter was updated, non-zero if a counter was updated.
static int sdu_fifo(agc_state_t* State)
{
  int        Count, RetVal = 0, HighRate, DownCount;
  cdu_fifo_t* CduFifo;
  int16_t*   Ch;
  // See if there are any pending PCDU or MCDU counts we need to apply.  We only
  // check one of the CDUs, and the CDU to check is indicated by CduChecker.
  CduFifo = &CduFifos[CduChecker];

  if(CduFifo->Size > 0 && State->cycle_counter >= CduFifo->NextUpdate)
  {
    // Update the counter.
    Ch        = &State->erasable[0][CduChecker + FIRST_CDU];
    Count     = CduFifo->Counts[CduFifo->Ptr];
    HighRate  = (Count & 0x80000000);
    DownCount = (Count & 0x40000000);
    if(DownCount)
    {
      counter_mcdu(Ch);
      if(CduLog != NULL)
        fprintf(CduLog, ">\t\t" FORMAT_64U " %o 03\n", State->cycle_counter, CduChecker + FIRST_CDU);
    }
    else
    {
      counter_pcdu(Ch);
      if(CduLog != NULL)
        fprintf(CduLog, ">\t\t" FORMAT_64U " %o 01\n", State->cycle_counter, CduChecker + FIRST_CDU);
    }
    Count--;
    // Update the FIFO.
    if(0 != (Count & ~0xC0000000))
      CduFifo->Counts[CduFifo->Ptr] = Count;
    else
    {
      // That FIFO entry is exhausted.  Remove it from the FIFO.
      CduFifo->Size--;
      CduFifo->Ptr++;
      if(CduFifo->Ptr >= MAX_CDU_FIFO_ENTRIES)
        CduFifo->Ptr = 0;
    }
    // And set next update time.
    // Set up for next update time.  The intervals is are of the form
    // x, x, y, depending on whether CduIntervalType is 0, 1, or 2.
    // This is done because with a cycle type of 1024000/12 cycles per
    // second, the exact CDU update times don't fit on exact cycle
    // boundaries, but every 3rd CDU update does hit a cycle boundary.
    if(CduFifo->NextUpdate == 0)
      CduFifo->NextUpdate = State->cycle_counter;
    if(CduFifo->IntervalType < 2)
    {
      if(HighRate)
        CduFifo->NextUpdate += 13;
      else
        CduFifo->NextUpdate += 213;
      CduFifo->IntervalType++;
    }
    else
    {
      if(HighRate)
        CduFifo->NextUpdate += 14;
      else
        CduFifo->NextUpdate += 214;
      CduFifo->IntervalType = 0;
    }
    // Return an indication that a counter was updated.
    RetVal = 1;
  }

  CduChecker++;
  if(CduChecker >= NUM_CDU_FIFOS)
    CduChecker = 0;

  return (RetVal);
}

//----------------------------------------------------------------------------
// This function is used to update the counter registers on the basis of
// commands received from the outside world.

void unprogrammed_increment(agc_state_t* State, int Counter, int IncType)
{
  int16_t* Ch;
  int      Overflow = 0;
  Counter &= 0x7f;
  Ch = &State->erasable[0][Counter];
  if(COVERAGE_COUNTS)
    ErasableWriteCounts[0][Counter]++;
  switch(IncType)
  {
    case 0:
      //TrapPIPA = (Counter >= 037 && Counter <= 041);
      Overflow = counter_pinc(Ch);
      break;
    case 1:
    case 021:
      // For the CDUX,Y,Z counters, push the command into a FIFO.
      if(Counter >= FIRST_CDU && Counter < FIRST_CDU + NUM_CDU_FIFOS)
        push_cdu_fifo(State, Counter, IncType);
      else
        Overflow = counter_pcdu(Ch);
      break;
    case 2:
      //TrapPIPA = (Counter >= 037 && Counter <= 041);
      Overflow = counter_minc(Ch);
      break;
    case 3:
    case 023:
      // For the CDUX,Y,Z counters, push the command into a FIFO.
      if(Counter >= FIRST_CDU && Counter < FIRST_CDU + NUM_CDU_FIFOS)
        push_cdu_fifo(State, Counter, IncType);
      else
        Overflow = counter_mcdu(Ch);
      break;
    case 4:
      Overflow = counter_dinc(State, Counter, Ch);
      break;
    case 5:
      Overflow = counter_shinc(Ch);
      break;
    case 6:
      Overflow = counter_shanc(Ch);
      break;
    default:
      break;
  }
  if(Overflow)
  {
    // On some counters, overflow is supposed to cause
    // an interrupt.  Take care of setting the interrupt request here.
  }
  TrapPIPA = 0;
}

//----------------------------------------------------------------------------
// Function handles the coarse-alignment output pulses for one IMU CDU drive axis.
// It returns non-0 if a non-zero count remains on the axis, 0 otherwise.

static int BurstOutput(agc_state_t* State, int DriveBitMask, int CounterRegister, int Channel)
{
  static int CountCDUX = 0, CountCDUY = 0, CountCDUZ = 0; // In target CPU format.
  int DriveCount = 0, DriveBit, Direction = 0, Delta, DriveCountSaved;
  if(CounterRegister == RegCDUXCMD)
    DriveCountSaved = CountCDUX;
  else if(CounterRegister == RegCDUYCMD)
    DriveCountSaved = CountCDUY;
  else if(CounterRegister == RegCDUZCMD)
    DriveCountSaved = CountCDUZ;
  else
    return (0);
  // Driving this axis?
  DriveBit = (State->inputChannel[014] & DriveBitMask);
  // If so, we must retrieve the count from the counter register.
  if(DriveBit)
  {
    DriveCount = State->erasable[0][CounterRegister];
    State->erasable[0][CounterRegister] = 0;
  }
  // The count may be negative.  If so, normalize to be positive and set the
  // direction flag.
  Direction = (040000 & DriveCount);
  if(Direction)
  {
    DriveCount ^= 077777;
    DriveCountSaved -= DriveCount;
  }
  else
    DriveCountSaved += DriveCount;
  if(DriveCountSaved < 0)
  {
    DriveCountSaved = -DriveCountSaved;
    Direction       = 040000;
  }
  else
    Direction = 0;
  // Determine how many pulses to output.  The max is 192 per burst.
  Delta = DriveCountSaved;
  if(Delta >= 192 / COARSE_SMOOTH)
    Delta = 192 / COARSE_SMOOTH;
  // If the count is non-zero, pulse it.
  if(Delta > 0)
  {
    agc_channel_output(State, Channel, Direction | Delta);
    DriveCountSaved -= Delta;
  }
  if(Direction)
    DriveCountSaved = -DriveCountSaved;
  if(CounterRegister == RegCDUXCMD)
    CountCDUX = DriveCountSaved;
  else if(CounterRegister == RegCDUYCMD)
    CountCDUY = DriveCountSaved;
  else if(CounterRegister == RegCDUZCMD)
    CountCDUZ = DriveCountSaved;
  return (DriveCountSaved);
}

static void update_dsky(agc_state_t* State)
{
  unsigned LastChannel163 = State->dsky_channel_163;

  State->dsky_channel_163 &=
    ~(DSKY_KEY_REL | DSKY_VN_FLASH | DSKY_OPER_ERR | DSKY_RESTART
      | DSKY_STBY | DSKY_AGC_WARN | DSKY_TEMP);

  if(State->inputChannel[013] & 01000)
    // The light test is active. Light RESTART and STBY.
    State->dsky_channel_163 |= DSKY_RESTART | DSKY_STBY; //

  // If we're in standby, light the standby light
  if(State->standby)
    State->dsky_channel_163 |= DSKY_STBY;

  // Make the RESTART light mirror State->RestartLight.
  if(State->restart_light)
    State->dsky_channel_163 |= DSKY_RESTART;

  // Light TEMP if channel 11 bit 4 is set, or channel 30 bit 15 is set
  if((State->inputChannel[011] & 010) || (State->inputChannel[030] & 040000))
    State->dsky_channel_163 |= DSKY_TEMP;

  // Set KEY REL and OPER ERR according to channel 11
  if(State->inputChannel[011] & DSKY_KEY_REL)
    State->dsky_channel_163 |= DSKY_KEY_REL;
  if(State->inputChannel[011] & DSKY_OPER_ERR)
    State->dsky_channel_163 |= DSKY_OPER_ERR;

  // Turn on the AGC warning light if the warning filter is above its threshold
  if(State->warning_filter > WARNING_FILTER_THRESHOLD)
  {
    State->dsky_channel_163 |= DSKY_AGC_WARN;

    // Set the AGC Warning input bit in channel 33
    State->inputChannel[033] &= 057777;
  }

  // Update the DSKY flash counter based on the DSKY timer
  while(State->dsky_timer >= DSKY_OVERFLOW)
  {
    State->dsky_timer -= DSKY_OVERFLOW;
    State->dsky_flash = (State->dsky_flash + 1) % DSKY_FLASH_PERIOD;
  }

  // Flashing lights on the DSKY have a period of 1.28s, and a 75% duty cycle
  if(!State->standby && State->dsky_flash == 0)
  {
    // If V/N FLASH is high, then the lights are turned off
    if(State->inputChannel[011] & DSKY_VN_FLASH)
      State->dsky_channel_163 |= DSKY_VN_FLASH;

    // Flash off the KEY REL and OPER ERR lamps
    State->dsky_channel_163 &= ~DSKY_KEY_REL;
    State->dsky_channel_163 &= ~DSKY_OPER_ERR;
  }

  // Send out updated display information, if something on the DSKY changed
  if(State->dsky_channel_163 != LastChannel163)
    agc_channel_output(State, 0163, State->dsky_channel_163);
}

//----------------------------------------------------------------------------
// This function implements a model of what happens in the actual AGC hardware
// during a divide -- but made a bit more readable / software-centric than the
// actual register transfer level stuff. It should nevertheless give accurate
// results in all cases, including those that result in "total nonsense".
// If A, L, or Z are the divisor, it assumes that the unexpected transformations
// have already been applied to the "divisor" argument.
static void SimulateDV(agc_state_t* state, uint16_t divisor)
{
  uint16_t dividend_sign = 0;
  uint16_t divisor_sign  = 0;
  uint16_t remainder;
  uint16_t remainder_sign = 0;
  uint16_t quotient_sign  = 0;
  uint16_t quotient       = 0;
  uint16_t sum            = 0;
  uint16_t a              = c(RegA);
  uint16_t l              = c(RegL);
  int      i;

  // Assume A contains the sign of the dividend
  dividend_sign = a & 0100000;

  // Negate A if it was positive
  if(!dividend_sign)
    a = ~a;
  // If A is now -0, take the dividend sign from L
  if(a == 0177777)
    dividend_sign = l & 0100000;
  // Negate L if the dividend is negative.
  if(dividend_sign)
    l = ~l;

  // Add 40000 to L
  l = add_sp_16(l, 040000);
  // If this did not cause positive overflow, add one to A
  if(value_ovf(l) != AGC_P1)
    a = add_sp_16(a, 1);
  // Initialize the remainder with the current value of A
  remainder = a;

  // Record the sign of the divisor, and then take its absolute value
  divisor_sign = divisor & 0100000;
  if(divisor_sign)
    divisor = ~divisor;
  // Initialize the quotient via a WYD on L (L's sign is placed in bits
  // 16 and 1, and L bits 14-1 are placed in bits 15-2).
  quotient_sign = l & 0100000;
  quotient = quotient_sign | ((l & 037777) << 1) | (quotient_sign >> 15);

  for(i = 0; i < 14; i++)
  {
    // Shift up the quotient
    quotient <<= 1;
    // Perform a WYD on the remainder
    remainder_sign = remainder & 0100000;
    remainder      = remainder_sign | ((remainder & 037777) << 1);
    // The sign is only placed in bit 1 if the quotient's new bit 16 is 1
    if((quotient & 0100000) == 0)
      remainder |= (remainder_sign >> 15);
    // Add the divisor to the remainder
    sum = add_sp_16(remainder, divisor);
    if(sum & 0100000)
    {
      // If the resulting sum has its bit 16 set, OR a 1 onto the
      // quotient and take the sum as the new remainder
      quotient |= 1;
      remainder = sum;
    }
  }
  // Restore the proper quotient sign
  a = quotient_sign | (quotient & 077777);

  // The final value for A is negated if the dividend sign and the
  // divisor sign did not match
  c(RegA) = (dividend_sign != divisor_sign) ? ~a : a;
  // The final value for L is negated if the dividend was negative
  c(RegL) = (dividend_sign) ? remainder : ~remainder;
}

//-----------------------------------------------------------------------------
// Execute one machine-cycle of the simulation.  Use agc_engine_init prior to
// the first call of agc_engine, to initialize State, and then call agc_engine
// thereafter every (simulated) 11.7 microseconds.
//
// Returns:
//      0 -- success
// I'm not sure if there are any circumstances under which this can fail ...

// Note on addressing of bits within words:  The MIT docs refer to bits
// 1 through 15, with 1 being the least-significant, and 15 the most
// significant.  A 16th bit, the (odd) parity bit, would be bit 0 in this
// scheme.  Now, we're probably not going to use the parity bit in our
// simulation -- I haven't fully decided this at the time I'm writing
// this note -- so we have a choice of whether to map the 15 bits that ARE
// used to D0-14 or to D1-15.  I'm going to choose the latter, even though
// it requires slightly more processing, in order to conform as obviously
// as possible to the MIT docs.

#define SCALER_OVERFLOW 80
#define SCALER_DIVIDER 3

// Fine-alignment.
// The gyro needs 3200 pulses per second, and therefore counts twice as
// fast as the regular 1600 pps counters.
#define GYRO_OVERFLOW 160
#define GYRO_DIVIDER (2 * 3)
static unsigned GyroCount    = 0;
static unsigned OldChannel14 = 0, GyroTimer = 0;

// Coarse-alignment.
// The IMU CDU drive emits bursts every 600 ms.  Each cycle is
// 12/1024000 seconds long.  This happens to mean that a burst is
// emitted every 51200 CPU cycles, but we multiply it out below
// to make it look pretty
#define IMUCDU_BURST_CYCLES \
  ((600 * 1024000) / (1000 * 12 * COARSE_SMOOTH))
static uint64_t ImuCduCount  = 0;
static unsigned ImuChannel14 = 0;

int agc_engine(agc_state_t* state)
{
  int i, j;
  uint16_t pc, inst, /*OpCode,*/ quarter_code, s_extra_code;
  int16_t* where_word;
  uint16_t address_12, address_10, address_9;
  int      ValueK, KeepExtraCode = 0;
  //int Operand;
  int16_t  pperand_16;
  int16_t  eb, fb, bb;
  uint16_t ext_ppcode;
  int      ovf, acc;
  //int OverflowQ, Qumulator;
  // Keep track of TC executions for the TC Trap alarm
  int executed_tc   = 0;
  int tc_transient  = 0;
  int just_took_bzf  = 0;
  int just_took_bzmf = 0;

  s_extra_code = 0;

  // For DOWNRUPT
  if(state->downrupt_time_valid && state->cycle_counter >= state->downrupt_time)
  {
    state->interrupt_requests[8] = 1; // Request DOWNRUPT
    state->downrupt_time_valid   = 0;
  }

  // The first time through the loop, light up the DSKY RESTART light
  if(state->cycle_counter == 0)
  {
    state->restart_light = 1;
  }

  state->cycle_counter++;

  //----------------------------------------------------------------------
  // Update the thingy that determines when 1/1600 second has passed.
  // 1/1600 is the basic timing used to drive timer registers.  1/1600
  // second happens to be 160/3 machine cycles.

  state->scale_counter += SCALER_DIVIDER;
  state->dsky_timer += SCALER_DIVIDER;

  //-------------------------------------------------------------------------

  // Handle server stuff for socket connections used for i/o channel
  // communications.  Stuff like listening for clients we only do
  // every once and a while---nominally, every 100 ms.  Actually
  // processing input data is done every cycle.
  if(state->channel_routine_count == 0)
    channel_routine(state);
  state->channel_routine_count = ((state->channel_routine_count + 1) & 017777);

  // Update the various hardware-driven DSKY lights
  update_dsky(state);

  // Get data from input channels.  Return immediately if a unprogrammed
  // counter-increment was performed.
  if(agc_channel_input(state))
    return (0);

  // If in --debug-dsky mode, don't want to take the chance of executing
  // any AGC code, since there isn't any loaded anyway.
  if(DebugDsky)
    return (0);

  //----------------------------------------------------------------------
  // This stuff takes care of extra CPU cycles used by some instructions.

  // A little extra delay, needed sometimes after branch instructions that
  // don't always take the same amount of time.
  if(state->extra_delay)
  {
    state->extra_delay--;
    return (0);
  }

  // If an instruction that takes more than one clock-cycle is in progress,
  // we simply return.  We don't do any of the actual computations for such
  // an instruction until the last clock cycle for it is reached.
  // (Except for a few weird cases dealt with by ExtraDelay as above.)
  if(state->pend_flag && state->pend_delay > 0)
  {
    state->pend_delay--;
    return (0);
  }

  //----------------------------------------------------------------------
  // Take care of any PCDU or MCDU operations that are lingering in CDU
  // FIFOs.
  if(sdu_fifo(state))
  {
    // A CDU counter was serviced, so a cycle was used up, and we must
    // return.
    return (0);
  }

  if(state->inputChannel[032] & 020000)
  {
    state->sby_pressed       = 0;
    state->sby_still_pressed = 0;
  }

  //----------------------------------------------------------------------
  // Here we take care of counter-timers.  There is a basic 1/3200 second
  // clock that is used to drive the timers.  1/3200 second happens to
  // be SCALER_OVERFLOW/SCALER_DIVIDER machine cycles, and the variable
  // ScalerCounter has already been updated the correct number of
  // multiples of SCALER_DIVIDER.  Note that incrementing a timer register
  // takes 1 machine cycle.

  // This can only iterate once, but I use 'while' just in case.
  while(state->scale_counter >= SCALER_OVERFLOW)
  {
    int TriggeredAlarm = 0;

    // First, update SCALER1 and SCALER2. These are direct views into
    // the clock dividers in the Scaler module, and so don't take CPU
    // time to 'increment'
    state->scale_counter -= SCALER_OVERFLOW;
    state->inputChannel[ChanSCALER1]++;
    if(state->inputChannel[ChanSCALER1] == 040000)
    {
      state->inputChannel[ChanSCALER1] = 0;
      state->inputChannel[ChanSCALER2] =
        (state->inputChannel[ChanSCALER2] + 1) & 037777;
    }

    // Check alarms first, since there's a chance we might go to standby
    if(04000 == (07777 & state->inputChannel[ChanSCALER1]))
    {
      // The Night Watchman begins looking once every 1.28s
      if(!state->standby)
        state->night_watchman = 1;

      // The standby circuit finishes checking to see if we're going to standby now
      // (it has the same period as but is 180 degrees out of phase with the Night Watchman)
      if(
        state->sby_pressed
        && ((state->inputChannel[013] & 002000) || state->standby))
      {
        if(!state->standby)
        {
          // Standby is enabled, and PRO has been held down for the required amount of time.
          state->standby           = 1;
          state->sby_still_pressed = 1;

          // While this isn't technically an alarm, it causes GOJAM just like all the rest
          if(ShowAlarms)
            printf("Alarm: Standby\n");
          TriggeredAlarm = 1;

          // Turn on the STBY light, and switch off the EL segments
          state->dsky_channel_163 |= DSKY_STBY | DSKY_EL_OFF;
          agc_channel_output(state, 0163, state->dsky_channel_163);
        }
        else if(!state->sby_still_pressed)
        {
          // PRO was pressed for long enough to turn us back on. Let's get going!
          state->standby = 0;

          // Turn off the STBY light
          state->dsky_channel_163 &= ~(DSKY_STBY | DSKY_EL_OFF);
          agc_channel_output(state, 0163, state->dsky_channel_163);
        }
      }
    }
    else if(00000 == (07777 & state->inputChannel[ChanSCALER1]))
    {
      // The standby circuit checks the SBY/PRO button state every 1.28s
      if(0 == (state->inputChannel[032] & 020000))
        state->sby_pressed = 1;

      // The Night Watchman finishes looking now
      if(!state->standby && state->night_watchman)
      {
        // NEWJOB wasn't checked before 0.64s elapsed. Sound the alarm!
        if(ShowAlarms)
          printf("Alarm: NightWatchman\n");
        TriggeredAlarm = 1;

        // Set the NIGHT WATCHMAN bit in channel 77. Don't go through CpuWriteIO() because
        // instructions writing to CH77 clear it. We'll broadcast changes to it in the
        // generic alarm handler a bit further down.
        state->inputChannel[077] |= CH77_NIGHT_WATCHMAN;
        state->night_watchman_tripped = 1;
      }
      else
        // If it's been 1.28s since a Night Watchman alarm happened, stop asserting its
        // channel 77 bit
        state->night_watchman_tripped = 0;
    }
    else if(00 == (07 & state->inputChannel[ChanSCALER1]))
    {
      // Update the warning filter. Once every 160ms, if an input to the filter has been
      // generated (or if the light test is active), the filter is charged. Otherwise,
      // it slowly discharges. This is being modeled as a simple linear function right now,
      // and should be updated when we learn its real implementation details.
      if(
        (0400 == (0777 & state->inputChannel[ChanSCALER1]))
        && (state->generated_warning || (state->inputChannel[013] & 01000)))
      {
        state->generated_warning = 0;
        state->warning_filter += WARNING_FILTER_INCREMENT;
        if(state->warning_filter > WARNING_FILTER_MAX)
          state->warning_filter = WARNING_FILTER_MAX;
      }
      else
      {
        if(state->warning_filter >= WARNING_FILTER_DECREMENT)
          state->warning_filter -= WARNING_FILTER_DECREMENT;
        else
          state->warning_filter = 0;
      }
    }

    // All the rest of this is switched off during standby.
    if(!state->standby)
    {
      if(0400 == (0777 & state->inputChannel[ChanSCALER1]))
      {
        // The Rupt Lock alarm watches ISR state starting every 160ms
        state->rupt_lock = 1;
        state->no_rupt   = 1;
      }
      else if(
        (state->rupt_lock || state->no_rupt)
        && 0300 == (0777 & state->inputChannel[ChanSCALER1]))
      {
        // We've either had no interrupts, or stuck in one, for 140ms. Sound the alarm!
        if(ShowAlarms && state->rupt_lock)
          printf("Alarm: RuptLock\n");
        if(ShowAlarms && state->no_rupt)
          printf("Alarm: NoRupt\n");
        TriggeredAlarm = 1;

        // Set the RUPT LOCK bit in channel 77.
        state->inputChannel[077] |= CH77_RUPT_LOCK;
      }

      if(020 == (037 & state->inputChannel[ChanSCALER1]))
      {
        // The TC Trap alarm watches executing instructions every 5ms
        state->tc_trap = 1;
        state->no_tc   = 1;
      }
      else if((state->tc_trap || state->no_tc) && 000 == (037 & state->inputChannel[ChanSCALER1]))
      {
        // We've either executed no TC at all, or only TCs, for the past 5ms. Sound the alarm!
        if(ShowAlarms && state->tc_trap)
          printf("Alarm: TCTrap\n");
        if(ShowAlarms && state->no_tc)
          printf("Alarm: NoTC\n");
        TriggeredAlarm = 1;

        // Set the TC TRAP bit in channel 77.
        state->inputChannel[077] |= CH77_TC_TRAP;
      }

      // Now that that's taken care of...
      // Update the 10 ms. timers TIME1 and TIME3.
      // Recall that the registers are in AGC integer format,
      // and therefore are actually shifted left one space.
      // When taking a reset, the real AGC would skip unprogrammed
      // sequences and go straight to GOJAM. The requests, however,
      // would be saved and the counts would happen immediately
      // after the first instruction at 4000, so doing them now
      // is not too inaccurate.
      if(020 == (037 & state->inputChannel[ChanSCALER1]))
      {
        state->extra_delay++;
        if(counter_pinc(&c(RegTIME1)))
        {
          state->extra_delay++;
          counter_pinc(&c(RegTIME2));
        }
        state->extra_delay++;
        if(counter_pinc(&c(RegTIME3)))
          state->interrupt_requests[3] = 1;
      }
      // TIME5 is the same as TIME3, but 5 ms. out of phase.
      if(000 == (037 & state->inputChannel[ChanSCALER1]))
      {
        state->extra_delay++;
        if(counter_pinc(&c(RegTIME5)))
          state->interrupt_requests[2] = 1;

        // Synchronously with TIME5, if radar activity is enabled,
        // increment the radar gate counter.
        if(state->inputChannel[013] & 010)
          state->radar_gate_counter++;
      }
      // TIME4 is the same as TIME3, but 7.5ms out of phase
      if(010 == (037 & state->inputChannel[ChanSCALER1]))
      {
        state->extra_delay++;
        if(counter_pinc(&c(RegTIME4)))
          state->interrupt_requests[4] = 1;
      }
      // TIME6 only increments when it has been enabled via CH13 bit 15.
      // It increments 0.3125ms after TIME1/TIME3
      if(040000 & state->inputChannel[013] && (state->inputChannel[ChanSCALER1] & 01) == 01)
      {
        state->extra_delay++;
        if(counter_dinc(state, 0, &c(RegTIME6)))
        {
          state->interrupt_requests[1] = 1;
          // Triggering a T6RUPT disables T6 by clearing the CH13 bit
          cpu_write_io(state, 013, state->inputChannel[013] & 037777);
        }
      }

      // Check for HANDRUPT conditions (the actually timing is very slightly off
      // from this, but not enough to matter). The traps are reset upon triggering.
      if(state->trap_31a && ((state->inputChannel[031] & 000077) != 000077))
      {
        state->trap_31a               = 0;
        state->interrupt_requests[10] = 1;
      }

      if(state->trap_31b && ((state->inputChannel[031] & 007700) != 007700))
      {
        state->trap_31b               = 0;
        state->interrupt_requests[10] = 1;
      }

      if(state->trap_32 && ((state->inputChannel[032] & 001777) != 001777))
      {
        state->trap_32                = 0;
        state->interrupt_requests[10] = 1;
      }

      // Similarly, check for radar cycle completion. As with HANDRUPT, the
      // timing here is slightly off (early by ~13 MCT), but this should
      // not be enough to matter.
      if((state->radar_gate_counter == 9) && (036 == (037 & state->inputChannel[ChanSCALER1])))
      {
        // Completion of a radar cycle triggers the following actions:
        // 1. The radar gate counter is set back to 0.
        // 2. The radar activity bit (bit 4 of channel 13) is reset.
        // 3. Data from the radar is shifted into the RNRAD counter
        //    (this is expected to be performed by RequestRadarData().
        // 4. RADARUPT is set pending.
        state->radar_gate_counter = 0;
        state->inputChannel[013] &= ~010;
        request_radar_data(state);
        state->interrupt_requests[9] = 1; // RADARUPT
      }
    }

    // If we triggered any alarms, simulate a GOJAM
    if(TriggeredAlarm || state->parity_fail)
    {
      if(!InhibitAlarms) // ...but only if doing so isn't inhibited
      {
        int i;

        // Two single-MCT instruction sequences, GOJAM and TC 4000, are about to happen
        state->extra_delay += 2;

        // The net result of those two is Z = 4000. Interrupt state is cleared, and
        // interrupts are enabled. The TC 4000 has the beneficial side-effect of
        // storing the current Z in Q, where it can helpfully be recovered.
        c(RegQ)                = c(RegZ);
        c(RegZ)                = 04000;
        state->in_isr          = 0;
        state->allow_interrupt = 1;
        state->parity_fail     = 0;

        // HANDRUPT traps are all disabled.
        state->trap_31a = 0;
        state->trap_31b = 0;
        state->trap_32  = 0;

        // All interrupt requests are cleared.
        for(i = 1; i <= NUM_INTERRUPT_TYPES; i++)
          state->interrupt_requests[i] = 0;

        // Clear channels 5, 6, 10, 11, 12, 13, and 14
        cpu_write_io(state, 005, 0);
        cpu_write_io(state, 006, 0);
        cpu_write_io(state, 010, 0);
        cpu_write_io(state, 011, 0);
        cpu_write_io(state, 012, 0);
        cpu_write_io(state, 013, 0);
        cpu_write_io(state, 014, 0);

        // Clear the UPLINK TOO FAST bit (11) in channel 33
        state->inputChannel[033] |= 002000;

        // Clear channels 34 and 35, and don't let doing so generate a downrupt
        cpu_write_io(state, 034, 0);
        cpu_write_io(state, 035, 0);
        state->downrupt_time_valid = 0;

        // Clear other yaAGC-internal state
        state->index_value            = AGC_P0;
        state->extra_code             = 0;
        state->substitute_instruction = 0;
        state->pend_flag              = 0;
        state->pend_delay             = 0;
        state->took_bzf               = 0;
        state->took_bzmf              = 0;

        // Light the RESTART light on the DSKY, if we're not going into standby
        if(!state->standby)
        {
          state->restart_light     = 1;
          state->generated_warning = 1;
        }
      }

      // Push the CH77 updates to the outside world
      agc_channel_output(state, 077, state->inputChannel[077]);
    }

    if(state->extra_delay)
    {
      // Return, so as to account for the time occupied by updating the
      // counters and/or GOJAM.
      state->extra_delay--;
      return (0);
    }
  }

  // If we're in standby mode, this is all we can accomplish --
  // everything else is switched off.
  if(state->standby)
    return (0);

    //----------------------------------------------------------------------
    // Same principle as for the counter-timers (above), but for handling
    // the 3200 pulse-per-second fictitious register 0177 I use to support
    // driving the gyro.

#ifdef GYRO_TIMING_SIMULATED
  // Update the 3200 pps gyro pulse counter.
  GyroTimer += GYRO_DIVIDER;
  while(GyroTimer >= GYRO_OVERFLOW)
  {
    GyroTimer -= GYRO_OVERFLOW;
    // We get to this point 3200 times per second.  We increment the
    // pulse count only if the GYRO ACTIVITY bit in channel 014 is set.
    if(0 != (state->inputChannel[014] & 01000) && state->erasable[0][RegGYROCTR] > 0)
    {
      GyroCount++;
      state->erasable[0][RegGYROCTR]--;
      if(state->erasable[0][RegGYROCTR] == 0)
        state->inputChannel[014] &= ~01000;
    }
  }

  // If 1/4 second (nominal gyro pulse count of 800 decimal) or the gyro
  // bits in channel 014 have changed, output to channel 0177.
  i = (state->inputChannel[014] & 01740); // Pick off the gyro bits.
  if(i != OldChannel14 || GyroCount >= 800)
  {
    j            = ((OldChannel14 & 0740) << 6) | GyroCount;
    OldChannel14 = i;
    GyroCount    = 0;
    agc_channel_output(state, 0177, j);
  }
#else // GYRO_TIMING_SIMULATED
#define GYRO_BURST 800
#define GYRO_BURST2 1024
  if(0 != (state->inputChannel[014] & 01000))
    if(0 != state->erasable[0][RegGYROCTR])
    {
      // If any torquing is still pending, do it all at once before
      // setting up a new torque counter.
      while(GyroCount)
      {
        j = GyroCount;
        if(j > 03777)
          j = 03777;
        agc_channel_output(state, 0177, OldChannel14 | j);
        GyroCount -= j;
      }
      // Set up new torque counter.
      GyroCount                      = state->erasable[0][RegGYROCTR];
      state->erasable[0][RegGYROCTR] = 0;
      OldChannel14 = ((state->inputChannel[014] & 0740) << 6);
      GyroTimer    = GYRO_OVERFLOW * GYRO_BURST - GYRO_DIVIDER;
    }
  // Update the 3200 pps gyro pulse counter.
  GyroTimer += GYRO_DIVIDER;
  while(GyroTimer >= GYRO_BURST * GYRO_OVERFLOW)
  {
    GyroTimer -= GYRO_BURST * GYRO_OVERFLOW;
    if(GyroCount)
    {
      j = GyroCount;
      if(j > GYRO_BURST2)
        j = GYRO_BURST2;
      agc_channel_output(state, 0177, OldChannel14 | j);
      GyroCount -= j;
    }
  }
#endif // GYRO_TIMING_SIMULATED

  //----------------------------------------------------------------------
  // ... and somewhat similar principles for the IMU CDU drive for
  // coarse alignment.

#if 0
  i = (state->inputChannel[014] & 070000);	// Check IMU CDU drive bits.
  if (ImuChannel14 == 0 && i != 0)// If suddenly active, start drive.
  ImuCduCount = IMUCDU_BURST_CYCLES;
  if (i != 0 && ImuCduCount >= IMUCDU_BURST_CYCLES)// Time for next burst.
    {
      // Adjust the cycle counter.
      ImuCduCount -= IMUCDU_BURST_CYCLES;
      // Determine how many pulses are wanted on each axis this burst.
      ImuChannel14 = BurstOutput (state, 040000, RegCDUXCMD, 0174);
      ImuChannel14 |= BurstOutput (state, 020000, RegCDUYCMD, 0175);
      ImuChannel14 |= BurstOutput (state, 010000, RegCDUZCMD, 0176);
    }
  else
  ImuCduCount++;
#else  // 0
  i = (state->inputChannel[014] & 070000); // Check IMU CDU drive bits.
  if(ImuChannel14 == 0 && i != 0) // If suddenly active, start drive.
    ImuCduCount = state->cycle_counter - IMUCDU_BURST_CYCLES;
  if(i != 0 && (state->cycle_counter - ImuCduCount) >= IMUCDU_BURST_CYCLES) // Time for next burst.
  {
    // Adjust the cycle counter.
    ImuCduCount += IMUCDU_BURST_CYCLES;
    // Determine how many pulses are wanted on each axis this burst.
    ImuChannel14 = BurstOutput(state, 040000, RegCDUXCMD, 0174);
    ImuChannel14 |= BurstOutput(state, 020000, RegCDUYCMD, 0175);
    ImuChannel14 |= BurstOutput(state, 010000, RegCDUZCMD, 0176);
  }
#endif // 0

  //----------------------------------------------------------------------
  // Finally, stuff for driving the optics shaft & trunnion CDUs.  Nothing
  // fancy like the fine-alignment and coarse-alignment stuff above.
  // Just grab the data from the counter and dump it out the appropriate
  // fictitious port as a giant lump.

  if(state->erasable[0][RegOPTX] && 0 != (state->inputChannel[014] & 02000))
  {
    agc_channel_output(state, 0172, state->erasable[0][RegOPTX]);
    state->erasable[0][RegOPTX] = 0;
  }
  if(state->erasable[0][RegOPTY] && 0 != (state->inputChannel[014] & 04000))
  {
    agc_channel_output(state, 0171, state->erasable[0][RegOPTY]);
    state->erasable[0][RegOPTY] = 0;
  }

  //----------------------------------------------------------------------
  // Okay, here's the stuff that actually has to do with decoding instructions.

  // Store the current value of several registers.
  eb = c(RegEB);
  fb = c(RegFB);
  bb = c(RegBB);
  // Reform 16-bit accumulator and test for overflow in accumulator.
  acc = c(RegA) & 0177777;
  ovf    = (value_ovf(acc) != AGC_P0);
  //Qumulator = GetQ (State);
  //OverflowQ = (ValueOverflowed (Qumulator) != AGC_P0);

  // After each instruction is executed, the AGC's Z register is updated to
  // indicate the next instruction to be executed. The Z register is 16
  // bits long, but its value is transferred to the 12-bit S regsiter for
  // addressing, so the upper bits are lost.
  pc = c(RegZ) & 07777;
  where_word      = FindMemoryWord(state, pc);

  // Fetch the instruction itself.
  //Instruction = *WhereWord;
  if(state->substitute_instruction)
    inst = c(RegBRUPT);
  else
  {
    // The index is sometimes positive and sometimes negative.  What to
    // do if the result has overflow, I can't say.  I arbitrarily
    // overflow-correct it.
    inst = overflow_corrected(add_sp_16(
      sign_extend(state->index_value), sign_extend(*where_word)));
  }
  inst &= 077777;

  s_extra_code = state->extra_code;

  ext_ppcode = inst >> 9; //2;
  if(s_extra_code)
    ext_ppcode |= 0100;

  quarter_code = inst & ~MASK10;
  address_12   = inst & MASK12;
  address_10   = inst & MASK10;
  address_9    = inst & MASK9;

  // Handle interrupts.
  if(
    (DebuggerInterruptMasks[0] && !state->in_isr && state->allow_interrupt
     && !state->extra_code && !state->pend_flag && !ovf
     && inst != 3 && inst != 4 && inst != 6)
    || ext_ppcode == 0107) // Always check if the instruction is EDRUPT.
  {
    int i;
    int InterruptRequested = 0;
    // Interrupt vectors are ordered by their priority, with the lowest
    // address corresponding to the highest priority interrupt. Thus,
    // we can simply search through them in order for the next pending
    // request. There's two extra MCTs associated with taking an
    // interrupt -- one each for filling ZRUPT and BRUPT.
    // Search for the next interrupt request.
    for(i = 1; i <= NUM_INTERRUPT_TYPES; i++)
    {
      if(state->interrupt_requests[i] && DebuggerInterruptMasks[i])
      {
        // Clear the interrupt request.
        state->interrupt_requests[i] = 0;
        state->interrupt_requests[0] = i;

        state->next_z = 04000 + 4 * i;

        InterruptRequested = 1;
        break;
      }
    }

    // If no pending interrupts and we're dealing with EDRUPT, fall
    // back to address 0 (A) as the interrupt vector
    if(!InterruptRequested && ext_ppcode == 0107)
    {
      state->next_z      = 0;
      InterruptRequested = 1;
    }

    if(InterruptRequested)
    {
      // Set up the return stuff.
      c(RegZRUPT) = pc + 1;
      c(RegBRUPT) = inst;
      // Clear various metadata. Extracode is cleared (this can only
      // really happen with EDRUPT), and the index value and substituted
      // instruction were both applied earlier and their effects were
      // saved in BRUPT.
      state->extra_code             = 0;
      state->index_value            = AGC_P0;
      state->substitute_instruction = 0;
      // Vector to the interrupt.
      state->in_isr = 1;
      state->extra_delay++;
      goto AllDone;
    }
  }

  // Add delay for multi-MCT instructions.  Works for all instructions
  // except EDRUPT, BZF, and BZMF.  For BZF and BZMF, an extra cycle is added
  // AFTER executing the instruction -- not because it's more logically
  // correct, just because it's easier. EDRUPT's timing is handled with
  // the interrupt logic.
  if(!state->pend_flag)
  {
    int i;
    i = quarter_code >> 10;
    if(state->extra_code)
      i = ExtracodeTiming[i];
    else
      i = InstructionTiming[i];
    if(i)
    {
      state->pend_flag  = 1;
      state->pend_delay = i - 1;
      return (0);
    }
  }
  else
    state->pend_flag = 0;

  // Now that the index value has been used, get rid of it.
  state->index_value = AGC_P0;
  // And similarly for the substitute instruction from a RESUME.
  state->substitute_instruction = 0;

  // Compute the next value of the instruction pointer. The Z register is
  // 16 bits long, even though in almost all cases only the lower 12 bits
  // are used. When the Z register is incremented between each instruction,
  // only the lower 12 bits are read into the adder, so if something sets
  // any of the 4 most significant bits of Z, they will be lost before
  // the next instruction sees them.
  state->next_z = 1 + c(RegZ);
  // The contents of the Z register are updated before an instruction is
  // executed (really, it happens at the end of the previous instruction).
  c(RegZ) = state->next_z;

  // A BZF followed by an instruction other than EXTEND causes a TCF0 transient
  if(state->took_bzf && !((ext_ppcode == 000) && (address_12 == 6)))
    tc_transient = 1;

  // Parse the instruction.  Refer to p.34 of 1689.pdf for an easy
  // picture of what follows.
  switch(ext_ppcode)
  {
    case 000: // TC.
    case 001:
    case 002:
    case 003:
    case 004:
    case 005:
    case 006:
    case 007:
      // TC instruction (1 MCT).
      ValueK = address_12; // Convert AGC numerical format to native CPU format.
      if(ValueK == 3) // RELINT instruction.
      {
        state->allow_interrupt = 1;

        if(state->took_bzf || state->took_bzmf)
          // RELINT after a single-cycle instruction causes a TC0 transient
          tc_transient = 1;
      }
      else if(ValueK == 4) // INHINT instruction.
      {
        state->allow_interrupt = 0;

        if(state->took_bzf || state->took_bzmf)
          // INHINT after a single-cycle instruction causes a TC0 transient
          tc_transient = 1;
      }
      else if(ValueK == 6) // EXTEND instruction.
      {
        state->extra_code = 1;
        // Normally, ExtraCode will be reset when agc_engine is finished.
        // We inhibit that behavior with this flag.
        KeepExtraCode = 1;
      }
      else
      {
        if(ValueK != RegQ) // If not a RETURN instruction ...
          c(RegQ) = 0177777 & state->next_z;
        state->next_z = address_12;
        executed_tc    = 1;
      }

      break;
    case 010: // CCS.
    case 011:
      // CCS instruction (2 MCT).
      // Figure out where the data is stored, and fetch it.
      if(address_10 < REG16)
      {
        ValueK    = 0177777 & c(address_10);
        pperand_16 = overflow_corrected(ValueK);
        c(RegA)   = odabs(ValueK);
      }
      else // K!=accumulator.
      {
        where_word = FindMemoryWord(state, address_10);
        pperand_16 = *where_word & 077777;
        // Compute the "diminished absolute value", and save in accumulator.
        c(RegA) = dabs(pperand_16);
        // Assign back the read data in case editing is needed
        assign_from_pointer(state, where_word, pperand_16);
      }
      // Now perform the actual comparison and jump on the basis
      // of it.  There's no explanation I can find as to what
      // happens if we're already at the end of the memory bank,
      // so I'll just pretend that that can't happen.  Note,
      // by the way, that if the Operand is > +0, then NextZ
      // is already correct, and in the other cases we need to
      // increment it by 2 less because NextZ has already been
      // incremented.
      if(address_10 < REG16 && value_ovf(ValueK) == AGC_P1)
        state->next_z += 0;
      else if(address_10 < REG16 && value_ovf(ValueK) == AGC_M1)
        state->next_z += 2;
      else if(pperand_16 == AGC_P0)
        state->next_z += 1;
      else if(pperand_16 == AGC_M0)
        state->next_z += 3;
      else if(0 != (pperand_16 & 040000))
        state->next_z += 2;
      break;
    case 012: // TCF.
    case 013:
    case 014:
    case 015:
    case 016:
    case 017:
      // TCF instruction (1 MCT).
      state->next_z = address_12;
      // THAT was easy ... too easy ...
      executed_tc = 1;
      break;
    case 020: // DAS.
    case 021:
      //DasInstruction:
      // DAS instruction (3 MCT).
      {
        // We add the less-significant words (as SP values), and thus
        // the sign of the lower word of the output does not necessarily
        // match the sign of the upper word.
        int Msw, Lsw;
        if(IsL(address_10)) // DDOUBL
        {
          Lsw = add_sp_16(0177777 & c(RegL), 0177777 & c(RegL));
          Msw = add_sp_16(acc, acc);
          if((0140000 & Lsw) == 0040000)
            Msw = add_sp_16(Msw, AGC_P1);
          else if((0140000 & Lsw) == 0100000)
            Msw = add_sp_16(Msw, sign_extend(AGC_M1));
          Lsw     = overflow_corrected(Lsw);
          c(RegA) = 0177777 & Msw;
          c(RegL) = 0177777 & sign_extend(Lsw);
          break;
        }
        where_word = FindMemoryWord(state, address_10);
        if(address_10 < REG16)
          Lsw = add_sp_16(0177777 & c(RegL), 0177777 & c(address_10));
        else
          Lsw = add_sp_16(0177777 & c(RegL), sign_extend(*where_word));
        if(address_10 < REG16 + 1)
          Msw = add_sp_16(acc, 0177777 & c(address_10 - 1));
        else
          Msw = add_sp_16(acc, sign_extend(where_word[-1]));

        if((0140000 & Lsw) == 0040000)
          Msw = add_sp_16(Msw, AGC_P1);
        else if((0140000 & Lsw) == 0100000)
          Msw = add_sp_16(Msw, sign_extend(AGC_M1));
        Lsw = overflow_corrected(Lsw);

        if((0140000 & Msw) == 0100000)
          c(RegA) = sign_extend(AGC_M1);
        else if((0140000 & Msw) == 0040000)
          c(RegA) = AGC_P1;
        else
          c(RegA) = AGC_P0;
        c(RegL) = AGC_P0;
        // Save the results.
        if(address_10 < REG16)
          c(address_10) = sign_extend(Lsw);
        else
          assign_from_pointer(state, where_word, Lsw);
        if(address_10 < REG16 + 1)
          c(address_10 - 1) = Msw;
        else
          assign_from_pointer(state, where_word - 1, overflow_corrected(Msw));
      }
      break;
    case 022: // LXCH.
    case 023:
      // "LXCH K" instruction (2 MCT).
      if(IsL(address_10))
        break;
      if(IsReg(address_10, RegZERO)) // ZL
        c(RegL) = AGC_P0;
      else if(address_10 < REG16)
      {
        pperand_16 = c(RegL);
        c(RegL)   = c(address_10);
        if(address_10 >= 020 && address_10 <= 023)
          assign_from_pointer(
            state, where_word, overflow_corrected(0177777 & pperand_16));
        else
          c(address_10) = pperand_16;
        if(address_10 == RegZ)
          state->next_z = c(RegZ);
      }
      else
      {
        where_word = FindMemoryWord(state, address_10);
        pperand_16 = *where_word;
        assign_from_pointer(
          state, where_word, overflow_corrected(0177777 & c(RegL)));
        c(RegL) = sign_extend(pperand_16);
      }
      break;
    case 024: // INCR.
    case 025:
      // INCR instruction (2 MCT).
      {
        int Sum;
        where_word = FindMemoryWord(state, address_10);
        if(address_10 < REG16)
          c(address_10) = add_sp_16(AGC_P1, 0177777 & c(address_10));
        else
        {
          Sum = add_sp_16(AGC_P1, sign_extend(*where_word));
          assign_from_pointer(state, where_word, overflow_corrected(Sum));
          interrupt_requests(state, address_10, Sum);
        }
      }
      break;
    case 026: // ADS.  Reviewed against Blair-Smith.
    case 027:
      // ADS instruction (2 MCT).
      {
        where_word = FindMemoryWord(state, address_10);
        if(IsA(address_10))
          acc = add_sp_16(acc, acc);
        else if(address_10 < REG16)
          acc = add_sp_16(acc, 0177777 & c(address_10));
        else
          acc = add_sp_16(acc, sign_extend(*where_word));
        c(RegA) = acc;
        if(IsA(address_10))
        {
        }
        else if(address_10 < REG16)
          c(address_10) = acc;
        else
          assign_from_pointer(state, where_word, overflow_corrected(acc));
      }
      break;
    case 030: // CA
    case 031:
    case 032:
    case 033:
    case 034:
    case 035:
    case 036:
    case 037:
      if(IsA(address_12)) // NOOP
        break;
      if(address_12 < REG16)
      {
        c(RegA) = c(address_12);
        ;
        break;
      }
      where_word = FindMemoryWord(state, address_12);
      c(RegA)   = sign_extend(*where_word);
      assign_from_pointer(state, where_word, *where_word);
      break;
    case 040: // CS
    case 041:
    case 042:
    case 043:
    case 044:
    case 045:
    case 046:
    case 047:
      tc_transient = 1; // CS causes transients on the TC0 line

      if(IsA(address_12)) // COM
      {
        c(RegA) = ~acc;
        ;
        break;
      }
      if(address_12 < REG16)
      {
        c(RegA) = ~c(address_12);
        break;
      }
      where_word = FindMemoryWord(state, address_12);
      c(RegA)   = sign_extend(neg_sp(*where_word));
      assign_from_pointer(state, where_word, *where_word);
      break;
    case 050: // INDEX
    case 051:
      if(address_10 == 017)
        goto Resume;
      if(address_10 < REG16)
        state->index_value = overflow_corrected(c(address_10));
      else
      {
        where_word          = FindMemoryWord(state, address_10);
        state->index_value = *where_word;
      }
      break;
    case 0150: // INDEX (continued)
    case 0151:
    case 0152:
    case 0153:
    case 0154:
    case 0155:
    case 0156:
    case 0157:
      if(address_12 == 017 << 1)
      {
      Resume:
        state->next_z                 = c(RegZRUPT) - 1;
        state->in_isr                 = 0;
        state->substitute_instruction = 1;
      }
      else
      {
        if(address_12 < REG16)
          state->index_value = overflow_corrected(c(address_12));
        else
        {
          where_word          = FindMemoryWord(state, address_12);
          state->index_value = *where_word;
        }
        KeepExtraCode = 1;
      }
      break;
    case 052: // DXCH
    case 053:
      tc_transient = 1; // DXCH causes transients on the TCF0 line

      // Remember, in the following comparisons, that the address is pre-incremented.
      if(IsL(address_10))
      {
        c(RegL) = sign_extend(overflow_corrected(c(RegL)));
        break;
      }
      where_word = FindMemoryWord(state, address_10);
      // Topmost word.
      if(address_10 < REG16)
      {
        pperand_16    = c(address_10);
        c(address_10) = c(RegL);
        c(RegL)      = pperand_16;
        if(address_10 == RegZ)
          state->next_z = c(RegZ);
      }
      else
      {
        pperand_16 = sign_extend(*where_word);
        assign_from_pointer(state, where_word, overflow_corrected(c(RegL)));
        c(RegL) = pperand_16;
      }
      c(RegL) = sign_extend(overflow_corrected(c(RegL)));
      // Bottom word.
      if(address_10 < REG16 + 1)
      {
        pperand_16        = c(address_10 - 1);
        c(address_10 - 1) = c(RegA);
        c(RegA)          = pperand_16;
        if(address_10 == RegZ + 1)
          state->next_z = c(RegZ);
      }
      else
      {
        pperand_16 = sign_extend(where_word[-1]);
        assign_from_pointer(state, where_word - 1, overflow_corrected(c(RegA)));
        c(RegA) = pperand_16;
      }
      break;
    case 054: // TS
    case 055:
      tc_transient = 1;   // TS causes transients on the TCF0 line
      if(IsA(address_10)) // OVSK
      {
        if(ovf)
          state->next_z += AGC_P1;
      }
      else if(IsZ(address_10)) // TCAA
      {
        state->next_z = (077777 & acc);
        if(ovf)
          c(RegA) = sign_extend(value_ovf(acc));
      }
      else // Not OVSK or TCAA.
      {
        where_word = FindMemoryWord(state, address_10);
        if(address_10 < REG16)
          c(address_10) = acc;
        else
          assign_from_pointer(state, where_word, overflow_corrected(acc));
        if(ovf)
        {
          c(RegA) = sign_extend(value_ovf(acc));
          state->next_z += AGC_P1;
        }
      }
      break;
    case 056: // XCH
    case 057:
      tc_transient = 1; // XCH causes transients on the TCF0 line
      if(IsA(address_10))
        break;
      if(address_10 < REG16)
      {
        c(RegA)      = c(address_10);
        c(address_10) = acc;
        if(address_10 == RegZ)
          state->next_z = c(RegZ);
        break;
      }
      where_word = FindMemoryWord(state, address_10);
      c(RegA)   = sign_extend(*where_word);
      assign_from_pointer(state, where_word, overflow_corrected(acc));
      break;
    case 060: // AD
    case 061:
    case 062:
    case 063:
    case 064:
    case 065:
    case 066:
    case 067:
      if(IsA(address_12)) // DOUBLE
        acc = add_sp_16(acc, acc);
      else if(address_12 < REG16)
        acc = add_sp_16(acc, 0177777 & c(address_12));
      else
      {
        where_word   = FindMemoryWord(state, address_12);
        acc = add_sp_16(acc, sign_extend(*where_word));
        assign_from_pointer(state, where_word, *where_word);
      }
      c(RegA) = acc;
      break;
    case 070: // MASK
    case 071:
    case 072:
    case 073:
    case 074:
    case 075:
    case 076:
    case 077:
      if(address_12 < REG16)
        c(RegA) = acc & c(address_12);
      else
      {
        c(RegA)   = overflow_corrected(acc);
        where_word = FindMemoryWord(state, address_12);
        c(RegA)   = sign_extend(c(RegA) & *where_word);
      }
      break;
    case 0100: // READ
      if(IsL(address_9) || IsQ(address_9))
        c(RegA) = c(address_9);
      else
        c(RegA) = sign_extend(read_io(state, address_9));
      break;
    case 0101: // WRITE
      if(IsL(address_9) || IsQ(address_9))
        c(address_9) = acc;
      else
        cpu_write_io(state, address_9, overflow_corrected(acc));
      break;
    case 0102: // RAND
      if(IsL(address_9) || IsQ(address_9))
        c(RegA) = (acc & c(address_9));
      else
      {
        pperand_16 = overflow_corrected(acc);
        pperand_16 &= read_io(state, address_9);
        c(RegA) = sign_extend(pperand_16);
      }
      break;
    case 0103: // WAND
      if(IsL(address_9) || IsQ(address_9))
        c(RegA) = c(address_9) = (acc & c(address_9));
      else
      {
        pperand_16 = overflow_corrected(acc);
        pperand_16 &= read_io(state, address_9);
        cpu_write_io(state, address_9, pperand_16);
        c(RegA) = sign_extend(pperand_16);
      }
      break;
    case 0104: // ROR
      if(IsL(address_9) || IsQ(address_9))
        c(RegA) = (acc | c(address_9));
      else
      {
        pperand_16 = overflow_corrected(acc);
        pperand_16 |= read_io(state, address_9);
        c(RegA) = sign_extend(pperand_16);
      }
      break;
    case 0105: // WOR
      if(IsL(address_9) || IsQ(address_9))
        c(RegA) = c(address_9) = (acc | c(address_9));
      else
      {
        pperand_16 = overflow_corrected(acc);
        pperand_16 |= read_io(state, address_9);
        cpu_write_io(state, address_9, pperand_16);
        c(RegA) = sign_extend(pperand_16);
      }
      break;
    case 0106: // RXOR
      if(IsL(address_9) || IsQ(address_9))
        c(RegA) = (acc ^ c(address_9));
      else
      {
        pperand_16 = overflow_corrected(acc);
        pperand_16 ^= read_io(state, address_9);
        c(RegA) = sign_extend(pperand_16);
      }
      break;
    case 0107: // EDRUPT
      // It shouldn't be possible to get here, since EDRUPT is treated
      // as an interrupt above.
      break;
    case 0110: // DV
    case 0111:
    {
      int16_t AccPair[2], AbsA, AbsL, AbsK, Div16;
      int     Dividend, Divisor, Quotient, Remainder;

      AccPair[0] = overflow_corrected(acc);
      AccPair[1] = c(RegL);
      Dividend   = sp_to_decent(&AccPair[1]);
      decent_to_sp(Dividend, &AccPair[1]);
      // Check boundary conditions.
      AbsA = abs_sp(AccPair[0]);
      AbsL = abs_sp(AccPair[1]);

      if(IsA(address_10))
      {
        // DV modifies A before reading the divisor, so in this
        // case the divisor is -|A|.
        Div16 = c(RegA);
        if((c(RegA) & 0100000) == 0)
          Div16 = 0177777 & ~Div16;
      }
      else if(IsL(address_10))
      {
        // DV modifies L before reading the divisor. L is first
        // negated if the quotient A,L is negative according to
        // DV sign rules. Then, 40000 is added to it.
        Div16 = c(RegL);
        if(((AbsA == 0) && (0100000 & c(RegL))) || ((AbsA != 0) && (0100000 & c(RegA))))
          Div16 = 0177777 & ~Div16;
        // Make sure to account for L's built-in overflow correction
        Div16 = sign_extend(overflow_corrected(add_sp_16((uint16_t)Div16, 040000)));
      }
      else if(IsZ(address_10))
      {
        // DV modifies Z before reading the divisor. If the
        // quotient A,L is negative according to DV sign rules,
        // Z16 is set.
        Div16 = c(RegZ);
        if(((AbsA == 0) && (0100000 & c(RegL))) || ((AbsA != 0) && (0100000 & c(RegA))))
          Div16 |= 0100000;
      }
      else if(address_10 < REG16)
        Div16 = c(address_10);
      else
        Div16 = sign_extend(*FindMemoryWord(state, address_10));

      // Fetch the values;
      AbsK = abs_sp(overflow_corrected(Div16));
      if(AbsA > AbsK || (AbsA == AbsK && AbsL != AGC_P0) || value_ovf(Div16) != AGC_P0)
      {
        // The divisor is smaller than the dividend, or the divisor has
        // overflow. In both cases, we fall back on a slower simulation
        // of the hardware registers, which will produce "total nonsense"
        // (that nonetheless will match what the actual AGC would have gotten).
        SimulateDV(state, Div16);
      }
      else if(AbsA == 0 && AbsL == 0)
      {
        // The dividend is 0 but the divisor is not. The standard DV sign
        // convention applies to A, and L remains unchanged.
        if((040000 & c(RegL)) == (040000 & overflow_corrected(Div16)))
        {
          if(AbsK == 0)
            pperand_16 = 037777; // Max positive value.
          else
            pperand_16 = AGC_P0;
        }
        else
        {
          if(AbsK == 0)
            pperand_16 = (077777 & ~037777); // Max negative value.
          else
            pperand_16 = AGC_M0;
        }

        c(RegA) = sign_extend(pperand_16);
      }
      else if(AbsA == AbsK && AbsL == AGC_P0)
      {
        // The divisor is equal to the dividend.
        if(AccPair[0] == overflow_corrected(Div16)) // Signs agree?
        {
          pperand_16 = 037777; // Max positive value.
        }
        else
        {
          pperand_16 = (077777 & ~037777); // Max negative value.
        }
        c(RegL) = sign_extend(AccPair[0]);
        c(RegA) = sign_extend(pperand_16);
      }
      else
      {
        // The divisor is larger than the dividend.  Okay to actually divide!
        // Fortunately, the sign conventions agree with those of the normal
        // C operators / and %, so all we need to do is to convert the
        // 1's-complement values to native CPU format to do the division,
        // and then convert back afterward.  Incidentally, we know we
        // aren't dividing by zero, since we know that the divisor is
        // greater (in magnitude) than the dividend.
        Dividend  = agc2cpu2(Dividend);
        Divisor   = agc2cpu(overflow_corrected(Div16));
        Quotient  = Dividend / Divisor;
        Remainder = Dividend % Divisor;
        c(RegA)   = sign_extend(cpu2agc(Quotient));
        if(Remainder == 0)
        {
          // In this case, we need to make an extra effort, because we
          // might need -0 rather than +0.
          if(Dividend >= 0)
            c(RegL) = AGC_P0;
          else
            c(RegL) = sign_extend(AGC_M0);
        }
        else
          c(RegL) = sign_extend(cpu2agc(Remainder));
      }
    }
    break;
    case 0112: // BZF
    case 0113:
    case 0114:
    case 0115:
    case 0116:
    case 0117:
      //Operand16 = OverflowCorrected (Accumulator);
      //if (Operand16 == AGC_P0 || Operand16 == AGC_M0)
      if(acc == 0 || acc == 0177777)
      {
        state->next_z = address_12;
        just_took_bzf   = 1;
      }
      break;
    case 0120: // MSU
    case 0121:
    {
      unsigned ui, uj;
      int      diff;
      where_word = FindMemoryWord(state, address_10);
      if(address_10 < REG16)
      {
        ui = 0177777 & acc;
        uj = 0177777 & ~c(address_10);
      }
      else
      {
        ui = (077777 & overflow_corrected(acc));
        uj = (077777 & ~*where_word);
      }
      diff = ui + uj + 1; // Two's complement subtraction -- add the complement plus one
      // The AGC sign-extends the result from A15 to A16, then checks A16 to see if
      // one needs to be subtracted. We'll go in the opposite order, which also works
      if(diff & 040000)
      {
        diff |= 0100000; // Sign-extend A15 into A16
        diff--;          // Subtract one from the result
      }
      if(IsQ(address_10))
        c(RegA) = 0177777 & diff;
      else
      {
        pperand_16 = (077777 & diff);
        c(RegA)   = sign_extend(pperand_16);
      }
      if(address_10 >= 020 && address_10 <= 023)
        assign_from_pointer(state, where_word, *where_word);
    }
    break;
    case 0122: // QXCH
    case 0123:
      if(IsQ(address_10))
        break;
      if(IsReg(address_10, RegZERO)) // ZQ
        c(RegQ) = AGC_P0;
      else if(address_10 < REG16)
      {
        pperand_16    = c(RegQ);
        c(RegQ)      = c(address_10);
        c(address_10) = pperand_16;
        if(address_10 == RegZ)
          state->next_z = c(RegZ);
      }
      else
      {
        where_word = FindMemoryWord(state, address_10);
        pperand_16 = overflow_corrected(c(RegQ));
        c(RegQ)   = sign_extend(*where_word);
        assign_from_pointer(state, where_word, pperand_16);
      }
      break;
    case 0124: // AUG
    case 0125:
    {
      int Sum;
      int Operand16, Increment;
      where_word = FindMemoryWord(state, address_10);
      if(address_10 < REG16)
        Operand16 = c(address_10);
      else
        Operand16 = sign_extend(*where_word);
      Operand16 &= 0177777;
      if(0 == (0100000 & Operand16))
        Increment = AGC_P1;
      else
        Increment = sign_extend(AGC_M1);
      Sum = add_sp_16(0177777 & Increment, 0177777 & Operand16);
      if(address_10 < REG16)
        c(address_10) = Sum;
      else
      {
        assign_from_pointer(state, where_word, overflow_corrected(Sum));
        interrupt_requests(state, address_10, Sum);
      }
    }
    break;
    case 0126: // DIM
    case 0127:
    {
      int Sum;
      int Operand16, Increment;
      where_word = FindMemoryWord(state, address_10);
      if(address_10 < REG16)
        Operand16 = c(address_10);
      else
        Operand16 = sign_extend(*where_word);
      Operand16 &= 0177777;
      if(Operand16 == AGC_P0 || Operand16 == sign_extend(AGC_M0))
        break;
      if(0 == (0100000 & Operand16))
        Increment = sign_extend(AGC_M1);
      else
        Increment = AGC_P1;
      Sum = add_sp_16(0177777 & Increment, 0177777 & Operand16);
      if(address_10 < REG16)
        c(address_10) = Sum;
      else
        assign_from_pointer(state, where_word, overflow_corrected(Sum));
    }
    break;
    case 0130: // DCA
    case 0131:
    case 0132:
    case 0133:
    case 0134:
    case 0135:
    case 0136:
    case 0137:
      if(IsL(address_12))
      {
        c(RegL) = sign_extend(overflow_corrected(c(RegL)));
        break;
      }
      where_word = FindMemoryWord(state, address_12);
      // Do topmost word first.
      if(address_12 < REG16)
        c(RegL) = c(address_12);
      else
        c(RegL) = sign_extend(*where_word);
      c(RegL) = sign_extend(overflow_corrected(c(RegL)));
      // Now do bottom word.
      if(address_12 < REG16 + 1)
        c(RegA) = c(address_12 - 1);
      else
        c(RegA) = sign_extend(where_word[-1]);
      if(address_12 >= 020 && address_12 <= 023)
        assign_from_pointer(state, where_word, where_word[0]);
      if(address_12 >= 020 + 1 && address_12 <= 023 + 1)
        assign_from_pointer(state, where_word - 1, where_word[-1]);
      break;
    case 0140: // DCS
    case 0141:
    case 0142:
    case 0143:
    case 0144:
    case 0145:
    case 0146:
    case 0147:
      if(IsL(address_12)) // DCOM
      {
        c(RegA) = ~acc;
        c(RegL) = ~c(RegL);
        c(RegL) = sign_extend(overflow_corrected(c(RegL)));
        break;
      }
      where_word = FindMemoryWord(state, address_12);
      // Do topmost word first.
      if(address_12 < REG16)
        c(RegL) = ~c(address_12);
      else
        c(RegL) = ~sign_extend(*where_word);
      c(RegL) = sign_extend(overflow_corrected(c(RegL)));
      // Now do bottom word.
      if(address_12 < REG16 + 1)
        c(RegA) = ~c(address_12 - 1);
      else
        c(RegA) = ~sign_extend(where_word[-1]);
      if(address_12 >= 020 && address_12 <= 023)
        assign_from_pointer(state, where_word, where_word[0]);
      if(address_12 >= 020 + 1 && address_12 <= 023 + 1)
        assign_from_pointer(state, where_word - 1, where_word[-1]);
      break;
    // For 0150..0157 see the INDEX instruction above.
    case 0160: // SU
    case 0161:
      if(IsA(address_10))
        acc = sign_extend(AGC_M0);
      else if(address_10 < REG16)
        acc = add_sp_16(acc, 0177777 & ~c(address_10));
      else
      {
        where_word = FindMemoryWord(state, address_10);
        acc =
          add_sp_16(acc, sign_extend(neg_sp(*where_word)));
        assign_from_pointer(state, where_word, *where_word);
      }
      c(RegA) = acc;
      break;
    case 0162: // BZMF
    case 0163:
    case 0164:
    case 0165:
    case 0166:
    case 0167:
      //Operand16 = OverflowCorrected (Accumulator);
      //if (Operand16 == AGC_P0 || IsNegativeSP (Operand16))
      if(acc == 0 || 0 != (acc & 0100000))
      {
        state->next_z = address_12;
        just_took_bzmf  = 1;
      }
      break;
    case 0170: // MP
    case 0171:
    case 0172:
    case 0173:
    case 0174:
    case 0175:
    case 0176:
    case 0177:
    {
      // For MP A (i.e., SQUARE) the accumulator is NOT supposed to
      // be overflow-corrected.  I do it anyway, since I don't know
      // what it would mean to carry out the operation otherwise.
      // Fix later if it causes a problem.
      // FIX ME: Accumulator is overflow-corrected before SQUARE.
      int16_t MsWord, LsWord, OtherOperand16;
      int     Product;
      where_word = FindMemoryWord(state, address_12);
      pperand_16 = overflow_corrected(acc);
      if(address_12 < REG16)
        OtherOperand16 = overflow_corrected(c(address_12));
      else
        OtherOperand16 = *where_word;
      if(OtherOperand16 == AGC_P0 || OtherOperand16 == AGC_M0)
        MsWord = LsWord = AGC_P0;
      else if(pperand_16 == AGC_P0 || pperand_16 == AGC_M0)
      {
        if((pperand_16 == AGC_P0 && 0 != (040000 & OtherOperand16)) || (pperand_16 == AGC_M0 && 0 == (040000 & OtherOperand16)))
          MsWord = LsWord = AGC_M0;
        else
          MsWord = LsWord = AGC_P0;
      }
      else
      {
        int16_t WordPair[2];
        Product = agc2cpu(sign_extend(pperand_16))
          * agc2cpu(sign_extend(OtherOperand16));
        Product = cpu2agc2(Product);
        // Sign-extend, because it's needed for DecentToSp.
        if(02000000000 & Product)
          Product |= 004000000000;
        // Convert back to DP.
        decent_to_sp(Product, &WordPair[1]);
        MsWord = WordPair[0];
        LsWord = WordPair[1];
      }
      c(RegA) = sign_extend(MsWord);
      c(RegL) = sign_extend(LsWord);
    }
    break;
    default:
      // Isn't possible to get here, but still ...
      //printf ("Unrecognized instruction %06o.\n", Instruction);
      break;
  }

AllDone:
  // All done!
  if(!state->pend_flag)
  {
    c(RegZERO)             = AGC_P0;
    state->inputChannel[7] = state->output_channel_7 &= 0160;
    c(RegZ)                = state->next_z;
    // In all cases except for RESUME, Z will be truncated to
    // 12 bits between instructions
    if(!state->substitute_instruction)
      c(RegZ) = c(RegZ) & 07777;
    if(!KeepExtraCode)
      state->extra_code = 0;
    // Values written to EB and FB are automatically mirrored to BB,
    // and vice versa.
    if(bb != c(RegBB))
    {
      c(RegFB) = (c(RegBB) & 076000);
      c(RegEB) = (c(RegBB) & 07) << 8;
    }
    else if(eb != c(RegEB) || fb != c(RegFB))
      c(RegBB) = (c(RegFB) & 076000) | ((c(RegEB) & 03400) >> 8);
    c(RegEB) &= 03400;
    c(RegFB) &= 076000;
    c(RegBB) &= 076007;
    // Correct overflow in the L register (this is done on read in the original,
    // but is much easier here)
    c(RegL) = sign_extend(overflow_corrected(c(RegL)));

    // Check ISR status, and clear the Rupt Lock flags accordingly
    if(state->in_isr)
      state->no_rupt = 0;
    else
      state->rupt_lock = 0;

    // Update TC Trap flags according to the instruction we just executed
    if(executed_tc || tc_transient)
      state->no_tc = 0;
    if(!executed_tc)
      state->tc_trap = 0;

    state->took_bzf  = just_took_bzf;
    state->took_bzmf = just_took_bzmf;
  }
  return (0);
}
