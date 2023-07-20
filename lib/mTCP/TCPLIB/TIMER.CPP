/*

   mTCP Timer.cpp
   Copyright (C) 2008-2023 Michael B. Brutman (mbbrutman@gmail.com)
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


#include <dos.h>
#include <i86.h>

#include "Timer.h"


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



// Shadow count of BIOS tick counter.  This one does not reset
// at midnight, so it will be good for about 7.5 years of
// continuous runtime.  This counter is public

extern volatile clockTicks_t Timer_CurrentTicks = 0;



// Locals - do not use outside of this code.

// Did we hook the timer interrupt?
static uint8_t timer_hooked = 0;

// Short duration countdown timer data
static uint16_t *countdownTimers[10];  // List of timers
static uint16_t  activeTimers = 0;     // Number of active timers

// Pointer to the timer tick handler that we are chaining to.
// This is the one we will restore when we are done.
void (__interrupt __far *Timer_old_tick_handler)( );



void __interrupt __far Timer_tick_handler( ) {

   Timer_CurrentTicks++;

   for ( int i=0; i < activeTimers; i++ ) {
     if ( *countdownTimers[i] > 0 ) {
       (*countdownTimers[i])--;
     }
   }
     
   _chain_intr( Timer_old_tick_handler );
}


void Timer_start( void ) {
  disable_ints( );
  Timer_old_tick_handler = getvect( 0x1c );
  setvect( 0x1c, Timer_tick_handler );
  timer_hooked = 1;
  enable_ints( );
}


void Timer_stop( void ) {
  if ( timer_hooked ) {
    disable_ints( );
    setvect( 0x1c, Timer_old_tick_handler );
    timer_hooked = 0;
    enable_ints( );
  }
}




// Short duration countdown timer support
//
// It's safe to add a timer without disabling interrupts, as the new
// timer is just appended to the end of the list and at worst case
// it will be missed once.  But when removing a timer disable
// interrupts.

void Timer_manageTimer( uint16_t *p ) {
  if (activeTimers < 10) {
    countdownTimers[activeTimers] = p;
    activeTimers++;
  }
}

void Timer_stopManagingTimer( uint16_t *p ) {
  disable_ints( );
  for ( int i=0; i < activeTimers++; i++ ) {
    if ( countdownTimers[i] == p ) {
      // Take the last timer and move it into this slot.
      countdownTimers[i] = countdownTimers[activeTimers-1];
      activeTimers--;
    }
  }
  enable_ints( );
}


