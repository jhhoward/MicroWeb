
/*

   mTCP Timer.cpp
   Copyright (C) 2008-2020 Michael B. Brutman (mbbrutman@gmail.com)
   mTCP web page: http://www.brutman.com/mTCP


   This file is part of mTCP.

   mTCP is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   mTCP is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with mTCP.  If not, see <http://www.gnu.org/licenses/>.


   Description: Timer management code

   Changes:

   2011-05-27: Initial release as open source software

*/



// This code uses a lot of timers, and probably should use more.  I used
// to use C standard time structures, but computing elapsed time was killing
// performance.
//
// The standard BIOS time tick is 18 times a second, or 55ms.  That is not
// great resolution, but it works well enough for most of what we want to
// do.  One problem with the standard BIOS tick is that the counter rolls
// over at midnight.  Rather than dealing repeated with rollover, just
// define our own tick counter that does not roll over.  Hook the timer
// tick interrupt and maintain it ourselves.
//
// While this simplifies time management, it does introduce other problems.
// We've not hooked an interrupt hander, so we must always unhook before
// the program exits or the machine will crash.  We were already in this
// mode of thinking because we gave the packet driver callback addresses
// to our code, so this is not a major exposure.  If the TCP stack ends,
// make sure the packet driver doesn't want to call us anymore and unhook
// this timer interrupt.  (The Utils functions will handle this for us.)




#include <dos.h>

#if defined ( __WATCOMC__ ) || defined ( __WATCOM_CPLUSPLUS__ )
#include <i86.h>
#endif

#include "Timer.h"



extern volatile clockTicks_t Timer_CurrentTicks = 0;

static uint8_t Timer_hooked = 0;

#if defined ( __WATCOMC__ ) || defined ( __WATCOM_CPLUSPLUS__ )
void (__interrupt __far *Timer_old_tick_handler)( );

void __interrupt __far Timer_tick_handler( )
{
   Timer_CurrentTicks++;
   _chain_intr( Timer_old_tick_handler );
}
#else
void interrupt ( *Timer_old_tick_handler)( ... );

void interrupt Timer_tick_handler( ... )
{
   Timer_CurrentTicks++;
   Timer_old_tick_handler();
}
#endif

void Timer_start( void ) {
  disable_ints( );
  Timer_old_tick_handler = getvect( 0x1c );
  setvect( 0x1c, Timer_tick_handler );
  Timer_hooked = 1;
  enable_ints( );
}

void Timer_stop( void ) {
  disable_ints( );
  setvect( 0x1c, Timer_old_tick_handler );
  Timer_hooked = 0;
  enable_ints( );
}
