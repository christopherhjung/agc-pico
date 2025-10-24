/*
  Copyright 2003-2005,2009 Ronald S. Burkey <info@sandroid.org>

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

  Filename:	yaAGC.h
  Purpose:	Header file for common yaAGC-project functionality.
  Contact:	Ron Burkey <info@sandroid.org>
  Compiler:	GNU gcc
  Reference:	http://www.ibiblio.org/apollo/index.html
  Mods:		04/21/03 RSB.	Began.
  		08/20/03 RSB.	Added uBit to ParseIoPacket.
		10/20/03 RSB.	Defined MSG_NOSIGNAL for Win32.
		11/24/03 RSB.	Began trying to adjust for Mac OS X.
		07/12/04 RSB	Q is now 16 bits
		08/18/04 RSB	Added the __embedded__ type system.
		02/27/05 RSB	Added the license exception, as required by
				the GPL, for linking to Orbiter SDK libraries.
		05/29/05 RSB	Added AGS equivalents for a couple of
				AGC packet functions.
		08/13/05 RSB	Added the extern "C" stuff.
		02/28/09 RSB	Added FORMAT_64U, FORMAT_64O for bypassing
				some compiler warnings on 64-bit machines.
		05/13/21 MKF	Disabled headers which are not implemented
				in wasi-libc. Disabled functions which cannot
				be ported to wasi-libc.
*/

#pragma once

#define FORMAT_64U "%llu"
#define FORMAT_64O "%llo"


