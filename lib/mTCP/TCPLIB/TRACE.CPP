/*

   mTCP Trace.cpp
   Copyright (C) 2005-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: mTCP tracing facility

   Changes:

   2015-01-17: Split from Utils.cpp
   2016-02-02: Expand Trace_Debugging to 16 bits; add an aggressive flush mode

*/


#include <dos.h>
#include <stdarg.h>

#include "trace.h"




FILE   *Trace_Stream = stderr;
char    Trace_Severity = ' ';
uint16_t Trace_Debugging;
char    Trace_LogFile[80];




// Trace_beginTracing
//
// Our default is to trace to stderr.  If somebody forgets to call this the
// default will save them.  If we can't open the file we'll also fall back
// stderr.

void Trace_beginTracing( void ) {

  // Protect us if the trace file is already open.
  if ( Trace_Stream != stderr ) return;

  if ( Trace_LogFile[0] ) {
    Trace_Stream = fopen( Trace_LogFile, "ac" );
    if ( Trace_Stream == NULL ) Trace_Stream = stderr;
  }

}


void Trace_endTracing( void ) {
  fclose( Trace_Stream );
}


void Trace_tprintf( char *fmt, ... ) {

  DosTime_t currentTime;
  gettime( &currentTime );

  DosDate_t currentDate;
  getdate( &currentDate );

  fprintf( Trace_Stream, "%04d-%02d-%02d %02d:%02d:%02d.%02d %c ",
           currentDate.year, currentDate.month, currentDate.day,
           currentTime.hour, currentTime.minute, currentTime.second,
           currentTime.hsecond, Trace_Severity );

  va_list ap;
  va_start( ap, fmt );
  vfprintf( Trace_Stream, fmt, ap );
  va_end( ap );

  if ( TRACE_ON_FLUSH ) flushall( );

  // reset trace severity before the next call
  Trace_Severity = ' ';
}

