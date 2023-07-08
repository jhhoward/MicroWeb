/*

   mTCP Timer.H
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


   Description: Timer management data structures and functions

   Changes:

   2011-05-27: Initial release as open source software

*/



#ifndef TIMER_H
#define TIMER_H

#include "types.h"


// Low resolution timer support
//
// Using the standard C library to implement a spin loop for a short duration
// is too heavy.  Use a shadow copy of the BIOS timer tick counter instead.
// That only has a resolution of 55ms, but that's good enough for most of our
// purposes.
//
// The shadow copy also has another advantage - it doesn't get reset at
// midnight.  It uses a 32 bit counter so it will be good for comparing
// timestamps for 7.5 years before you have to worry about a rollover.

#define TIMER_TICKS_PER_SEC         (18ul)
#define TIMER_TICKS_PER_MINUTE    (1092ul)
#define TIMER_TICKS_PER_DAY    (1573042ul)
#define TIMER_TICK_LEN              (55ul)

#define TIMER_GET_CURRENT( )   ( Timer_CurrentTicks )

#define TIMER_MS_TO_TICKS( a )   ( (a) / TIMER_TICK_LEN )
#define TIMER_SECS_TO_TICKS( a ) (a * 18ul)
#define TIMER_MINS_TO_TICKS( a ) (a * 1092ul)

#define Timer_diff( start, end ) ( (end) - (start) )


extern volatile clockTicks_t Timer_CurrentTicks;

extern void Timer_start( void );
extern void Timer_stop( void );



// Short duration countdown timer support
//
// The existing timer support above uses a shadow BIOS ticks counter which
// is a full 32 bit counter.  This is sufficient for up to 7.5 years, which
// is a silly amount of time.
//
// Most tasks don't need that kind of precision.  And that kind of precision
// is expensive because you have to do 32 bit math, which is usually just
// subtraction but it's still ugly.
//
// Instead, define a countdown timer mechanism.  The user sets the timer to
// a value, and the timer interrupt slowly decrements that value until it
// hits zero.  If the user detects zero in the value, it's time to do something
// including possibly reloading the timer.
//
// This is more expensive during the timer tick interrupt because now each
// managed timer has to be decremented.  But it saves time overall in the
// main loop of a program because now you don't have to do 32 bit loads,
// subtracts and compares to know when it's time to do something.
//
// Note: These timers are good to 60 minutes.  For anything longer than that
// you are on your own.

#define Timer_SetCountdownTimerMs(a, b)   { a = ( b / TIMER_TICK_LEN ); }
#define Timer_SetCountdownTimerSecs(a, b) { a = ( b * TIMER_TICKS_PER_SEC ); }
#define Timer_SetCountdownTimerMins(a, b) { a = ( b * TIMER_TICKS_PER_MINUTE ); }

#define Timer_isExpired( a ) ( ((a) == 0) )


// Call this to add your uint16_t timer to the list of timers to manage during
// the BIOS timer interrupt.  To be safe only use global variables as timers.
// If you use a stack variable and the function ends all sorts of fun things
// can happen to you because the timer will stil be decrementing.
//
// If you insist on using a timer that is a stack variable, call the
// Timer_stopManagingTimer function before the variable goes out of scope.

void Timer_manageTimer( uint16_t *p );
void Timer_stopManagingTimer( uint16_t *p );

#endif
