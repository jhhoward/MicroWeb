/*

   mTCP Trace.H
   Copyright (C) 2006-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Data structures and functions for tracing support.

   Changes:

   2015-01-17: Split out from Utils.h
   2016-02-02: Expand Trace_Debugging to 16 bits; add an aggressive flush mode

*/


#ifndef _TRACE_H
#define _TRACE_H


#include <stdio.h>

#include "types.h"



//-----------------------------------------------------------------------------
//
// Tracing support
//
// Tracing is conditional on a bit within a global variable.  Each class
// of tracepoint owns one bit in the global variable, providing for
// eight classes.
//
//   0x01 WARNINGS - used all over
//   0x02 GENERAL  - used in the APP part of an application
//   0x04 ARP      - used by ARP
//   0x08 IP       - used by IP/ICMP
//   0x10 UDP      - used by UDP
//   0x20 TCP      - used by TCP
//   0x40 DNS      - used by DNS
//   0x80 DUMP     - packet dumping for seriously large traces
//
// WARNINGS is special - it is both a stand-alone class and it can be
// used as an attribute on the other classes.  This allows one to turn
// on warnings for the entire app with just one bit, while avoiding a
// ton of noise.
//
// A program enables tracing by setting bits in the global variable.
// A program should allow the user to set tracing on and off, and
// probably to provide some control over what gets traced.  I generally
// use an environment variable, although command line options or even
// interactive controls in a program can be used.
//
// By default trace points go to STDERR.  Provide a logfile name if
// necessary.  (This is a good idea for most applications.)
//
// Tracing support is normally compiled in, but it can be supressed by
// defining NOTRACE.



#ifndef NOTRACE


// TRACING is enabled


void Trace_beginTracing( void );
void Trace_endTracing( void );

void Trace_tprintf( char *fmt, ... );

extern FILE *Trace_Stream;
extern char  Trace_Severity;

extern uint16_t Trace_Debugging;
extern char    Trace_LogFile[];



#define TRACE_ON_WARN    (Trace_Debugging & 0x0001ul)
#define TRACE_ON_GENERAL (Trace_Debugging & 0x0002ul)
#define TRACE_ON_ARP     (Trace_Debugging & 0x0004ul)
#define TRACE_ON_IP      (Trace_Debugging & 0x0008ul)
#define TRACE_ON_UDP     (Trace_Debugging & 0x0010ul)
#define TRACE_ON_TCP     (Trace_Debugging & 0x0020ul)
#define TRACE_ON_DNS     (Trace_Debugging & 0x0040ul)
#define TRACE_ON_DUMP    (Trace_Debugging & 0x0080ul)

#define TRACE_ON_FLUSH   (Trace_Debugging & 0x8000ul)

#define TRACE_WARN( x )     { if ( TRACE_ON_WARN ) { Trace_Severity = 'W'; Trace_tprintf x; } }
#define TRACE( x )          { if ( TRACE_ON_GENERAL ) { Trace_tprintf x; } }
#define TRACE_ARP( x )      { if ( TRACE_ON_ARP ) { Trace_tprintf x; } }
#define TRACE_ARP_WARN( x ) { if ( TRACE_ON_ARP || TRACE_ON_WARN ) { Trace_Severity = 'W'; Trace_tprintf x; } }
#define TRACE_IP( x )       { if ( TRACE_ON_IP ) { Trace_tprintf x; }  }
#define TRACE_IP_WARN( x )  { if ( TRACE_ON_IP  || TRACE_ON_WARN ) { Trace_Severity = 'W'; Trace_tprintf x; } }
#define TRACE_UDP( x )      { if ( TRACE_ON_UDP ) { Trace_tprintf x; } }
#define TRACE_UDP_WARN( x ) { if ( TRACE_ON_UDP || TRACE_ON_WARN ) { Trace_Severity = 'W'; Trace_tprintf x; } }
#define TRACE_TCP( x )      { if ( TRACE_ON_TCP ) { Trace_tprintf x; } }
#define TRACE_TCP_WARN( x ) { if ( TRACE_ON_TCP || TRACE_ON_WARN ) { Trace_Severity = 'W'; Trace_tprintf x; } }
#define TRACE_DNS( x )      { if ( TRACE_ON_DNS ) { Trace_tprintf x; } }
#define TRACE_DNS_WARN( x ) { if ( TRACE_ON_DNS || TRACE_ON_WARN ) { Trace_Severity = 'W'; Trace_tprintf x; } }



#else

// NOTRACE is defined and tracing is disabled.  Just stub the macros out.

#define Trace_beginTracing( ) { }
#define Trace_endTracing( ) { }

#define TRACE_ON_WARN    (0)
#define TRACE_ON_GENERAL (0)
#define TRACE_ON_ARP     (0)
#define TRACE_ON_IP      (0)
#define TRACE_ON_UDP     (0)
#define TRACE_ON_TCP     (0)
#define TRACE_ON_DNS     (0)

#define TRACE_ON_FLUSH   (0)

#define TRACE_WARN( x )
#define TRACE( x )
#define TRACE_ARP( x )
#define TRACE_ARP_WARN( x )
#define TRACE_IP( x )
#define TRACE_IP_WARN( x )
#define TRACE_UDP( x )
#define TRACE_UDP_WARN( x )
#define TRACE_TCP( x )
#define TRACE_TCP_WARN( x )
#define TRACE_DNS( x )
#define TRACE_DNS_WARN( x )

#endif



#endif
