/*

   mTCP IPv6.H
   Copyright (C) 2015-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: A minimal definition of an IPv6 header.  At some point
                IPv6 will be supported, so this will be filled out more
                later on.

   Changes:

   2015-03-31: New for use with pkttool.cpp

*/


#ifndef _IPV6_H
#define _IPV6_H

#include "types.h"


// Provide just a minimal IPv6 header so that we can parse the source and
// destination addresses.

class Ipv6Header {

  public:

    uint32_t   versClassFlow;
    uint16_t   payloadLen;
    uint8_t    nextHeader;
    uint8_t    hopLimit;
    Ipv6Addr_t src;
    Ipv6Addr_t dest;

};

#endif
