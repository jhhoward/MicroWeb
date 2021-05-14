/*

   mTCP Udp.H
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


   Description: UDP data structures and functions

   Changes:

   2011-05-27: Initial release as open source software
   2014-05-26: Add some static checks to the configuration options

*/


#ifndef _UDP_H
#define _UDP_H


// Load and check the configuration options

#include CFG_H
#include "types.h"

static_assert( UDP_MAX_CALLBACKS >=  1 );
static_assert( UDP_MAX_CALLBACKS <= 16 );



// Continue with other includes

#include "eth.h"
#include "ip.h"




class UdpHeader {

  public:

    // All of these need to be in network byte order.
    uint16_t src;
    uint16_t dst;
    uint16_t len;
    uint16_t chksum;

};



// Only enough for headers and no payload.

typedef struct {
  EthHeader eh;
  IpHeader  ip;
  UdpHeader udp;
} UdpPacket_t;



class Udp {

  public:

    static int8_t sendUdp( IpAddr_t host, uint16_t srcPort, uint16_t dstPort,
			   uint16_t payloadLen, unsigned char *data, uint8_t preAlloc );

    static int8_t sendUdpFragments( IpAddr_t host, uint16_t srcPort, uint16_t dstPort,
			   uint16_t payLoadLen, unsigned char *data, uint8_t preAlloc );

    // Use only if sendUdp returned a 1 and you had a preAllocated packet
    static int8_t resend( UdpPacket_t *packetPtr, uint16_t len );


    static void process( const unsigned char *packet, IpHeader *ip );

    static int8_t registerCallback( uint16_t srcPort,
				 void (*f)(const unsigned char *packet,
					   const UdpHeader *udp) );

    static int8_t unregisterCallback( uint16_t port );


    static void dumpStats( FILE *stream );

    static uint32_t Packets_Sent;
    static uint32_t Fragments_Sent;
    static uint32_t Packets_Received;
    static uint32_t NoHandler;
    static uint32_t ChecksumErrors;


  private:

    static uint16_t  callbackList[ UDP_MAX_CALLBACKS ];
    static uint8_t   callbackPorts;
    static void    (*callbackFunctions[ UDP_MAX_CALLBACKS ] )
				   (const unsigned char *packet,
				   const UdpHeader *udp);

};


#endif
