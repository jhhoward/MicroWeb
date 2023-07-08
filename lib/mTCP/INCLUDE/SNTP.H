/*

   mTCP SNTP.H
   Copyright (C) 2021-2023 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Data structions and defines for the SNTP client and server.

*/


#ifndef _sntp_h
#define _sntp_h

#include "types.h"


// NTP timestamps use 1900 as their base.  So to get to Coordinated
// Universal Time add this many seconds, which includes leap days.

#define NTP_OFFSET (2208988800ul)


// NTP packet format, used by the SNTP client and server.
// This does not include the optional authentication fields.

// On the bit fields ...  this compiler starts with the least significant
// bit and works toward the most significant bit.  The bit numbering is
// backwards from RFC 4330, where the most signifcant bit is bit 0 and the
// least significant bit is bit 7.  Mode doesn't actually show up in the
// packet first - leapIndicator does.

typedef struct {

  uint8_t  mode:3;
  uint8_t  version:3;
  uint8_t  leapIndicator:2;

  uint8_t  stratum;
  uint8_t  poll;
  int8_t   precision;

  // rootDelay and rootDispersion are in NTP short timestamp format which is
  // 16 bits for seconds and 16 bits for fractions of a second.
  uint16_t rootDelaySecs;
  uint16_t rootDelayFrac;

  uint16_t rootDispersionSecs;
  uint16_t rootDispersionFrac;

  uint8_t  refId[4];

  // The next four timestamps are in NTP timestamp format which is 32 bits
  // for the seconds (since 1900) and 32 bits for the fraction of a second.
  uint32_t refTimeSecs;
  uint32_t refTimeFrac;

  uint32_t origTimeSecs;
  uint32_t origTimeFrac;

  uint32_t recvTimeSecs;
  uint32_t recvTimeFrac;

  uint32_t transTimeSecs;
  uint32_t transTimeFrac;

} NTP_packet_t;


// Represent a 32 bit NTP fractional timestamp as two 16 bit halves so we
// can do 16 bit math on the part we care about.

typedef union {
  uint32_t big;
  struct {
    uint16_t lo;
    uint16_t hi;
  } parts;
} NTP_frac_time_t;



#endif
