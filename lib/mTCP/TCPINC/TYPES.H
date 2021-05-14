/*

   mTCP Types.H
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


   Description: Data type definitions and some #defines to help
     with porting from Turbo C++ to Open Watcom.

   Changes:

   2011-05-27: Initial release as open source software
   2015-01-17: Move the static_assert macro here since it is almost
               always included early.

*/



#ifndef _TYPES_H
#define _TYPES_H


#ifdef __TURBOC__

// Typedefs for common data types.  These make it very easy to see
// if a datatype is signed or unsigned and how big it is at a glance.
// Very useful when moving between platforms where int might not be
// 32 bits.

typedef unsigned char uint8_t;     //  8 bit int, range 0 to 255
typedef unsigned int  uint16_t;    // 16 bit int, range 0 to 64K
typedef unsigned long uint32_t;    // 32 bit int, range 0 to 4GB

typedef signed char   int8_t;      //  8 bit int, range -128 to +127
typedef signed int    int16_t;     // 16 bit int, range -32K to +32K
typedef signed long   int32_t;     // 32 bit int, range -2GB to +2GB

#else

#include <sys/types.h>

#endif



// This union lets us express a 4 byte quantity in 2 different ways:
//
// - A single 32 bit int
// - An array of four 8 bit ints

typedef union {
  uint32_t l;
  uint8_t  c[4];
} uint32_union_t;



// This union lets us express a 2 byte quantity in 2 different ways:
//
// - A single 16 bit int
// - An array of two 8 bit ints

typedef union {
  uint16_t s;
  uint8_t  c[2];
} uint16_union_t;




// Common types that are going to use constantly.

typedef uint8_t  EthAddr_t[6];  // An Ethernet address is 6 bytes
typedef uint16_t EtherType;     // 16 bits representing an Ethernet frame type
typedef uint8_t  IpAddr_t[4];   // An IPv4 address is 4 bytes
typedef uint8_t  Ipv6Addr_t[8]; // An IPv6 address is 8 bytes

typedef uint32_t clockTicks_t; // 32 bit counter for elapsed clock ticks




// Useful macros


//-----------------------------------------------------------------------------
//

// Static assert macro; used for compile-time parameter checks.  Defines an
// empty function that should result in no code or blows up with a compile-time
// error.

#ifndef static_assert
#define static_assert(expr) static void __static_assert( int bogusParm[(expr)?1:-1] )
#endif




#define QUOTEME_(x) #x
#define QUOTEME(x) QUOTEME_(x)

#ifdef __TURBOC__
#define COMPILER_NAME Turbo C++
#else
#define COMPILER_NAME Watcom
#endif



#ifdef __TURBOC__

typedef struct time DosTime_t;
typedef struct date DosDate_t;

#define disable_ints( ) asm cli;
#define enable_ints( ) asm sti;


#else


typedef struct dostime_t DosTime_t;
typedef struct dosdate_t DosDate_t;

#define disable_ints( ) _asm {\
     cli \
     };

#define enable_ints( ) _asm {\
     sti \
     };

// Macros to make Watcom look more like TurboC

#define gettime( x ) _dos_gettime( x )
#define getdate( x ) _dos_getdate( x )
#define getvect( x ) _dos_getvect( x )
#define setvect( x, y) _dos_setvect( x, y )
#define bioskey( x ) _bios_keybrd( x )
#define findnext( x ) _dos_findnext( x )
#define outportb( x, y ) outp( x, y )

#define MAXDRIVE  (3)
#define MAXDIR   (66)
#define MAXPATH  (80)
#define MAXFILE   (9)
#define MAXEXT    (5)


#endif


#endif
