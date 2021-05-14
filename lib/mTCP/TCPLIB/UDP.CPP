/*

   mTCP Udp.cpp
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


   Description: UDP protocol handling code

   Changes:

   2011-05-27: Initial release as open source software

*/


// This contains the Udp handler (process) and some management functions.
//
// To use UDP you create a function that matches our protoype and you
// register that handler with a specific port number.  If we see a packet
// that matches, we call your function.
//
// When the user function gets the packet, it is responsible for it.  It
// is getting the actual buffer used to receive data, so if that buffer
// does not get returned using our methods you will have a very very short
// run.  If you can process quickly, do so then free the buffer.  Otherwise
// copy what you need to your own buffer, release our buffer, and do your
// processing later.
//
// Sending a packet is pretty easy.  You can preallocate a full packet
// suitable for transmitting, or you can just point at your data.  If you
// preallocate you need to allocate room for Ethernet and IP headers because
// we're going to use that buffer for transmission.  If you just give us
// data we'll malloc storage, but that might turn into a performance problem
// if you do it alot.
//
// If you send more data than will fit in a packet (MTU - headers) you need
// to have the fragment support compiled in.  Your data will automatically
// get chunked up and sent out.  There is a malloc required though, so it's
// not a fast path.



#include <dos.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udp.h"
#include "trace.h"
#include "packet.h"
#include "eth.h"
#include "ip.h"



uint8_t   Udp::callbackPorts = 0;
uint16_t  Udp::callbackList[ UDP_MAX_CALLBACKS ];
void    (*Udp::callbackFunctions[ UDP_MAX_CALLBACKS ])
				(const unsigned char *packet,
				 const UdpHeader *udp);


uint32_t Udp::Packets_Sent = 0;
uint32_t Udp::Fragments_Sent = 0;
uint32_t Udp::Packets_Received = 0;
uint32_t Udp::NoHandler = 0;
uint32_t Udp::ChecksumErrors = 0;



void Udp::dumpStats( FILE *stream ) {
  fprintf( stream, "Udp: Sent %lu Rcvd %lu NoHandler %lu Checksum errs %lu Fragments sent %lu\n",
           Packets_Sent, Packets_Received, NoHandler, ChecksumErrors, Fragments_Sent );
}


// Udp::registerCallback
//
// 

// Return 0 if successful, -1 if not.

int8_t Udp::registerCallback( uint16_t port,
			      void (*f)(const unsigned char *packet,
			      const UdpHeader *udp) ) {

  if ( callbackPorts == UDP_MAX_CALLBACKS ) {
    return -1;
  }

  for (uint8_t i=0; i < callbackPorts; i++ ) {
    if ( port == callbackList[ i ] ) {
      return -1;
    }
  }

  callbackList[ callbackPorts ] = port;
  callbackFunctions[ callbackPorts ] = f;
  callbackPorts++;

  return 0;
}


int8_t Udp::unregisterCallback( uint16_t port ) {


  for (uint8_t i=0; i < callbackPorts; i++ ) {
    if ( port == callbackList[ i ] ) {

      // Take the last callback port and move it into this slot.
      // If this is the only one in the list it won't hurt.

      callbackPorts--;
      callbackList[ i ] = callbackList[ callbackPorts ];
      callbackFunctions[ i ] = callbackFunctions[ callbackPorts ];

      return 0;
    }
  }

  return -1;
}




// Udp::sendUdp
//
//   IpAddr_t host - target host
//   uint16_t srcPort - local port number
//   uint16_t dstPort - destination port number
//   uint16_t payloadLen - user level payload (not including headers) length
//   uint8_t *data - user level payload or full packet (see below)
//   uint8    preAlloc - see below
//
// If preAlloc is true the user allocated the space for outgoing UDP packet
// including the Ethernet, IP, and UDP headers.  The Ethernet header is always
// 14 bytes and the IP header is assumed to be 20 bytes - no IP header
// options are legal.  PreAlloc is preferred for performance reasons and
// for avoiding memory fragmentation.  The data pointer passed in points
// to the first byte of the Ethernet header; we can find the user level
// payload from there.
//
// If preAlloc is false the user is passing in data pointer to the user
// level payload.  We have to allocate memory for the Ethernet, IP and
// UDP headers.  Then we have to copy the user data over.  At the end
// of the routine the allocated memory gets freed whether the packet
// was sent or not.  While simplistic, it avoids any memory fragmentation
// problem and removes responsibility from the user for managing the
// allocated memory.
//
// Return codes:
//  -1 is bad - this is a hard error
//   0 is good - packet was sent
//   1 is pending ARP resolution; try again later.
//
// Historically you had to pass data that was padded to a 16 bit boundary.
// This was because our checksum code would set the last byte to zero if
// you passed in an odd data length.  The current code doesn't have this
// restriction.

int8_t Udp::sendUdp( IpAddr_t host, uint16_t srcPort, uint16_t dstPort,
	      uint16_t payloadLen, uint8_t *data, uint8_t preAlloc ) {

  TRACE_UDP(( "Udp: Send: Ip: %d.%d.%d.%d SrcPort: %u DstPort: %u PayloadLen: %u PreAlloc: %d\n",
	      host[0], host[1], host[2], host[3],
              srcPort, dstPort, payloadLen, preAlloc ));


  // If the user payload won't fit in a packet after space for the IP header
  // and UDP header, then we need to send fragments.

  if ( payloadLen > (MyMTU - sizeof(IpHeader) - sizeof(UdpHeader)) ) {
    #ifdef IP_SEND_UDP_FRAGS
      return sendUdpFragments( host, srcPort, dstPort,
                               payloadLen, data, preAlloc );
    #else
      TRACE_UDP_WARN(( "Udp: Packet too big and cant fragment!\n" ));
      return -1;
    #endif
  }


  // The full packet length is the payloadLen + the required headers.
  uint16_t packetLen = sizeof(UdpPacket_t) + payloadLen;

  UdpPacket_t* packetPtr;

  if ( preAlloc == 0 ) {

    // Malloc space for headers and user data and then copy the user data in.
    packetPtr = (UdpPacket_t*)malloc( packetLen );
    if ( packetPtr == NULL ) {
      TRACE_UDP_WARN(( "Udp: malloc error sending data\n" ));
      return -1;
    }

    memcpy( ((uint8_t *)packetPtr) + sizeof(UdpPacket_t), data, payloadLen );
  }
  else {
    // No malloc needed.  Just set the packetPtr;
    packetPtr = (UdpPacket_t*)(data);
  }

  uint16_t udpLen = sizeof(UdpHeader) + payloadLen;

  // Fill in the UDP header 
  packetPtr->udp.src = htons( srcPort );
  packetPtr->udp.dst = htons( dstPort );
  packetPtr->udp.len = htons( udpLen );
  packetPtr->udp.chksum = 0;

  // packetPtr->udp.chksum = Ip::pseudoChecksum( MyIpAddr, host,
  //                           ((uint16_t *)&(packetPtr->udp)), 17, udpLen );

  packetPtr->udp.chksum = ip_p_chksum( MyIpAddr, host,
                                       ((uint16_t *)&(packetPtr->udp)),
                                       IP_PROTOCOL_UDP, udpLen );


  // Fill in the IP header
  packetPtr->ip.set( IP_PROTOCOL_UDP, host, udpLen, 0, 0 );


  // Fill in the Eth header
  packetPtr->eh.setSrc( MyEthAddr );
  packetPtr->eh.setType( 0x0800 );


  // Returns 0 if we resolved, 1 if we are pending ARP resolution.
  // If pending ARP resolution the user must do the retry.
  uint8_t rc = packetPtr->ip.setDestEth( packetPtr->eh.dest );
  if ( rc == 0 ) {
    Packet_send_pkt( packetPtr, packetLen );
    Packets_Sent++;
  }


  if ( preAlloc == 0 ) {
    free( packetPtr );
  }

  return rc;
}




#ifdef IP_SEND_UDP_FRAGS

int8_t Udp::sendUdpFragments( IpAddr_t host, uint16_t srcPort,
                              uint16_t dstPort, uint16_t payloadLen,
                              uint8_t *data, uint8_t preAlloc ) {

  TRACE_UDP(( "Udp: Sending Fragments!\n" ));

  // Ip header offset; must always be a multiple of 8.
  uint16_t ipOffset = 0;


  // Allocate memory for a packet that we will use for the fragments.
  // MyMTU does not include the Ethernet Header so add it in.
  //
  // One of our incoming packet buffers would be ideal for this, except
  // we only want the packet driver using the free list.  You could get
  // one here but you'd have to protect it from interrupts.  Would make
  // sense if you had an app that constantly sent large UDP packets.

  uint16_t packetLen = MyMTU + sizeof(EthHeader);
  UdpPacket_t* packetPtr = (UdpPacket_t *)malloc( packetLen );
  if ( packetPtr == NULL ) {
      TRACE_UDP_WARN(( "Udp: malloc error sending fragments"\n ));
      return -1;
  }


  // Prealloc = 1 doesn't help if we are fragmenting.  But we still
  // have to set the pointers correctly.

  uint8_t *payload = data;  // Probably faster than doing an if-then-else
  if ( preAlloc ) {
    // If they preAlloced then skip past their headers; we are not using them
    payload = payload + sizeof(UdpPacket_t);
  }



  // Figure out how much of the user data we are going to send in the first
  // packet.  The first packet must include the IpHeader and the UdpHeader.
  // The Ip offset must land on an eight byte boundary.

  uint16_t copyLen = (MyMTU - sizeof(IpHeader) - sizeof(UdpHeader)) & 0xfff8;
  uint16_t firstPacketLen = copyLen + sizeof(IpHeader) + sizeof(UdpHeader);


  // First fragment gets the UDP header.  This is the same as the
  // non-fragmented path.
  uint16_t udpLen = sizeof(UdpHeader) + payloadLen;


  // Fill in the UDP header 
  packetPtr->udp.src = htons( srcPort );
  packetPtr->udp.dst = htons( dstPort );
  packetPtr->udp.len = htons( udpLen );
  packetPtr->udp.chksum = 0;
  
  packetPtr->udp.chksum = ip_p_chksum2( MyIpAddr, host,
                                        ((uint16_t *)&(packetPtr->udp)),
                                        IP_PROTOCOL_UDP,
                                        sizeof(UdpHeader),
                                        (uint16_t *)payload, payloadLen );


  // Copy first load of data to the outgoing frame
  memcpy( ((uint8_t *)packetPtr) + sizeof(UdpPacket_t), payload, copyLen );

  // Fill in the IP header with more fragments=on and offset=0
  packetPtr->ip.set( IP_PROTOCOL_UDP, host,
                     copyLen + sizeof(UdpHeader), 1, ipOffset );

  // Setup for next time
  payload += copyLen;
  payloadLen -= copyLen;
  ipOffset += copyLen + sizeof( UdpHeader );


  // Fill in the Eth header
  packetPtr->eh.setSrc( MyEthAddr );
  packetPtr->eh.setType( 0x0800 );


  // Returns 0 if we resolved, 1 if we are pending ARP resolution.
  // If pending ARP resolution the user must do the retry.
  uint8_t rc = packetPtr->ip.setDestEth( packetPtr->eh.dest );


  if ( rc ) {
    // Dang - ARP.  Free memory and bail out; the user has to retry.
    free( packetPtr );
    return 1;
  }

  // Send out first fragment.  This one is full sized
  Packet_send_pkt( packetPtr, firstPacketLen + sizeof(EthHeader) );



  while ( payloadLen ) {

    uint8_t moreFragments = 1;

    uint16_t copyLen = MyMTU - sizeof(IpHeader);
    if ( copyLen > payloadLen ) {
      // Last packet
      copyLen = payloadLen;
      moreFragments = 0;
    }
    else {
      // Middle packet - offset has to work out to a multiple of 8.
      copyLen = (MyMTU - sizeof(IpHeader)) & 0xfff8;
    }

    memcpy( ((uint8_t *)packetPtr) + sizeof(EthHeader) + sizeof(IpHeader),
            payload, copyLen );

    payload += copyLen;
    payloadLen -= copyLen;


    // Fill in the IP header.
    packetPtr->ip.set( IP_PROTOCOL_UDP, host, copyLen,
                       moreFragments, ipOffset );

    ipOffset += copyLen;

    // Eth header is already set, including the destination.  We made it
    // through ARP already so that can't fail.

    Packet_send_pkt( packetPtr,
                     copyLen + sizeof( EthHeader ) + sizeof( IpHeader ) );
    Packets_Sent++;
    Fragments_Sent++;

  }

  return 0;
}

#endif





void Udp::process( const unsigned char *packet, IpHeader *ip ) {

  Packets_Received++;

  UdpHeader *udp = (UdpHeader *)(ip->payloadPtr( ));

  uint16_t udpLen  = ntohs( udp->len );
  uint16_t srcPort = ntohs( udp->src );
  uint16_t dstPort = ntohs( udp->dst );

  TRACE_UDP(( "Udp: Process: SrcPort: %u  DstPort: %u   Len: %u\n",
	      srcPort, dstPort, udpLen ));


  // Check the incoming chksum.
  //
  // During the DHCP process we may not know what our IP address is
  // yet, so skip the incoming checksum check.

  #ifndef DHCP_CLIENT

  // uint16_t myChksum = Ip::pseudoChecksum( ip->ip_src, MyIpAddr,
  //                       ((uint16_t *)udp), 17, udpLen );

  uint16_t myChksum = ip_p_chksum( ip->ip_src, ip->ip_dest,
                                   ((uint16_t *)udp),
                                   IP_PROTOCOL_UDP, udpLen );

  if ( myChksum ) {
    TRACE_UDP_WARN(( "Udp: Bad chksum from %d.%d.%d.%d:%u to port %u len: %u\n",
                     ip->ip_src[0], ip->ip_src[1], ip->ip_src[2], ip->ip_src[3],
                     srcPort, dstPort, udpLen ));
    ChecksumErrors++;
    Buffer_free( packet );
    return;
  }

  #endif


  // Find the registered function to call for htis port.
  uint8_t i;
  for ( i=0; i < callbackPorts; i++ ) {
    if ( callbackList[i] == dstPort ) {
      // The user is responsible for freeing the packet.
      callbackFunctions[i]( packet, udp );
      break;
    }
  }

  if ( i == callbackPorts ) {
    // There was no handler so we have to throw the packet away.
    NoHandler++;
    Buffer_free( packet );
  }


}
