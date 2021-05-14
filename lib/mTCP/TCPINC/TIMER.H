/*

   mTCP Timer.H
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


   Description: Timer management data structures and functions

   Changes:

   2011-05-27: Initial release as open source software

*/



#ifndef TIMER_H
#define TIMER_H

#include "types.h"


// Low resolution timer support

#define TIMER_TICKS_PER_SEC         (18ul)
#define TIMER_TICKS_PER_DAY    (1570909ul)
#define TIMER_TICK_LEN             (55ul)

#define TIMER_GET_CURRENT( )   ( Timer_CurrentTicks )
#define TIMER_MS_TO_TICKS( a ) ( (a) / TIMER_TICK_LEN )

#define Timer_diff( start, end ) ( (end) - (start) )


extern volatile clockTicks_t Timer_CurrentTicks;

extern void Timer_start( void );
extern void Timer_stop( void );




#endif
