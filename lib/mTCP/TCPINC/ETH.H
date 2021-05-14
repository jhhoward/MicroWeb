/*

   mTCP Eth.H
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


   Description: Ethernet data structures and functions

   Changes:

   2011-05-27: Initial release as open source software

*/



#ifndef _ETH_HEADER_H
#define _ETH_HEADER_H

#include "types.h"
#include "inlines.h"


#define ETH_MTU_MIN    46
#define ETH_MTU_MAX  1500
#define ETH_MTU_SAFE  576


extern EthAddr_t MyEthAddr;
extern uint16_t  MyMTU;

class EthHeader {

  public:

    EthAddr_t dest;
    EthAddr_t src;
    uint16_t  type;

    inline void setDest( const EthAddr_t p_dest ) {

      ((uint16_t *)dest)[0] = ((uint16_t *)p_dest)[0];
      ((uint16_t *)dest)[1] = ((uint16_t *)p_dest)[1];
      ((uint16_t *)dest)[2] = ((uint16_t *)p_dest)[2];

      // dest[0] = p_dest[0]; dest[1] = p_dest[1];
      // dest[2] = p_dest[2]; dest[3] = p_dest[3];
      // dest[4] = p_dest[4]; dest[5] = p_dest[5];
    }

    inline void setSrc( const EthAddr_t p_src ) {

      ((uint16_t *)src)[0] = ((uint16_t *)p_src)[0];
      ((uint16_t *)src)[1] = ((uint16_t *)p_src)[1];
      ((uint16_t *)src)[2] = ((uint16_t *)p_src)[2];

      // src[0] = p_src[0]; src[1] = p_src[1];
      // src[2] = p_src[2]; src[3] = p_src[3];
      // src[4] = p_src[4]; src[5] = p_src[5];
    }

    inline void setType( uint16_t p_type ) {
      type = htons( p_type );
    }


};


class Eth {

  public:

    static const EthAddr_t Eth_Broadcast;

    static inline int isSame( const EthAddr_t a, const EthAddr_t b ) {

      // Should be slightly faster to evaluate 16 bits at a time

      return ( ( ((uint16_t *)a)[0] == ((uint16_t *)b)[0] ) &&
	       ( ((uint16_t *)a)[1] == ((uint16_t *)b)[1] ) &&
	       ( ((uint16_t *)a)[2] == ((uint16_t *)b)[2] ) );

      // return ( (a[0] == b[0]) && (a[1] == b[1]) && (a[2] == b[2]) &&
      //          (a[3] == b[3]) && (a[4] == b[4]) && (a[5] == b[5]) );
    }


    static inline void copy( EthAddr_t target, const EthAddr_t source ) {

      ((uint16_t *)target)[0] = ((uint16_t *)source)[0];
      ((uint16_t *)target)[1] = ((uint16_t *)source)[1];
      ((uint16_t *)target)[2] = ((uint16_t *)source)[2];

      // target[0] = source[0]; target[1] = source[1];
      // target[2] = source[2]; target[3] = source[3];
      // target[4] = source[4]; target[5] = source[5];
    };


};

#endif
