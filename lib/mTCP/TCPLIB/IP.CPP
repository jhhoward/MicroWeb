/*

   mTCP Ip.cpp
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


   Description: IP protocol code

   Changes:

   2011-05-27: Initial release as open source software
   2013-03-23: Get rid of some duplicate strings

*/




#include <malloc.h>
#include <mem.h>
#include <stdio.h>


#include "eth.h"
#include "ip.h"
#include "packet.h"
#include "timer.h"
#include "trace.h"

#ifdef COMPILE_ARP
#include "arp.h"
#endif

#ifdef COMPILE_TCP
#include "tcp.h"
#endif

#ifdef COMPILE_UDP
#include "udp.h"
#endif



// Initial values are chosen so that they are either easily spotted
// as not set or are safe to use.

char     MyHostname[20] = "DOSRULES";
IpAddr_t MyIpAddr  = { 255, 255, 255, 255 };  // Bad except for DHCP
IpAddr_t Netmask   = { 255, 255, 255, 255 };  // Bad value - must be set
IpAddr_t Gateway   = { 0,     0,   0,   0 };  // Safe default

uint32_t MyIpAddr_u = 0xfffffffful;
uint32_t Netmask_u = 0xfffffffful;


// Constants
IpAddr_t IpBroadcast   = { 255, 255, 255, 255 };
IpAddr_t IpThisMachine = { 0, 0, 0, 0 }; // This network and This host



// Statistics

uint32_t Ip::icmpRecvPackets = 0;
uint32_t Ip::ptrWrapCorrected = 0;
uint32_t Ip::badChecksum = 0;
uint32_t Ip::unhandledProtocol = 0;
uint32_t Ip::fragsReceived;

#ifdef IP_FRAGMENTS_ON
uint32_t Ip::goodReassemblies;
uint32_t Ip::timeoutReassemblies;
uint32_t Ip::notEnoughSlots;
uint32_t Ip::tooManyInFlight;
uint32_t Ip::payloadTooBig;
#endif



// Global counter which gives us a unique identifier for each outgoing packet.
uint16_t IpHeader::IpIdent = 0;


#ifdef IP_FRAGMENTS_ON
uint8_t *Ip::ipReassemblyMemoryStart = NULL;
uint8_t *Ip::ipReassemblyMemoryEnd = NULL;
uint8_t  Ip::fragsInReassembly = 0;
#endif




void Ip::dumpStats( FILE *stream ) {

  // Used to have Ip::ptrWrapCorrected, ran out of room

  // Standard stats we show everybody
  fprintf( stream, "Ip:  Icmp Rcvd %lu Frags Rcvd %lu Checksum errs %lu No Handler %lu\n",
           Ip::icmpRecvPackets, Ip::fragsReceived, Ip::badChecksum, Ip::unhandledProtocol );


  #ifdef IP_FRAGMENTS_ON

  // These stats only appear if debugging is turned on.

  #ifndef NOTRACE
  if ( Trace_Debugging ) {
    fprintf( stream, "     Frags: Good %lu Timeout %lu NoSlots %lu TooMany %lu SizeOvr %lu\n",
             Ip::goodReassemblies, Ip::timeoutReassemblies, Ip::notEnoughSlots,
             Ip::tooManyInFlight, Ip::payloadTooBig );
  }
  #endif

  #endif
}



/* Standard algorithm for IP style checksum

   This code hasn't been used in ages.  mTCP is currently using the assembler
   routines.


uint16_t simpleChecksum( uint16_t *data_p, uint16_t len ) {

  uint32_t sum = 0;

  while ( len > 1 ) {

    sum += *data_p;
    data_p++;

    if ( sum & 0x80000000) sum = (sum & 0xffff) + (sum>>16);

    len = len -2;
  }

  if ( len ) sum += (unsigned short)*(unsigned char *)data_p;

  while ( sum >> 16 ) {
    sum = (sum & 0xffff) + (sum>>16);
  }

  return ~sum;
}

uint16_t Ip::genericChecksum( uint16_t *data_p, uint16_t len ) {
  return simpleChecksum( data_p, len );
}


uint16_t Ip::pseudoChecksum( const IpAddr_t src, const IpAddr_t target,
         uint16_t *data_p, uint8_t protocol, uint16_t len ) {

  struct {
    IpAddr_t src;
    IpAddr_t target;
    uint8_t zero;
    uint8_t protocol;
    uint16_t len;
  } psheader;

  Ip::copy( psheader.src, src );
  Ip::copy( psheader.target, target );
  psheader.zero = 0;
  psheader.protocol = protocol;
  psheader.len = htons(len);



  // Checksum the pseudo header

  uint32_t sum = 0;

  uint16_t *tmpData = (uint16_t *)&psheader;
  uint16_t tmpLen = sizeof( psheader );

  while ( tmpLen > 1 ) {

    sum += *tmpData;
    tmpData++;

    if ( sum & 0x80000000) sum = (sum & 0xffff) + (sum>>16);

    tmpLen = tmpLen -2;
  }

  while ( sum >> 16 ) {
    sum = (sum & 0xffff) + (sum>>16);
  }


  // Now do the main part

  while ( len > 1 ) {

    sum += *data_p;
    data_p++;

    if ( sum & 0x80000000) sum = (sum & 0xffff) + (sum>>16);

    len = len -2;
  }

  if ( len ) sum += (unsigned short)*(unsigned char *)data_p;

  while ( sum >> 16 ) {
    sum = (sum & 0xffff) + (sum>>16);
  }

  return (~sum) & 0xFFFF;
}

*/



#if !defined ( __WATCOMC__ ) && !defined ( __WATCOM_CPLUSPLUS__ )
// Ip::genericChecksum
//
// Use this to compute a standard IP checksum on a block of data.
//
// We used to require that the buffer end on a word boundary even if the
// incoming length was odd.  This allowed us to set the last byte to a
// zero if necessary, making the code simpler.  This version of the code
// doesn't require that anymore.

extern "C" uint16_t ipchksum( uint16_t far *data_p, uint16_t len ) {

  // Create our own FAR version of the data pointer so that we can
  // normalize it and use it no matter what the memory model is.
  //
  // Don't normalize if we are not near the end of the segment because
  // the division part is expensive.

  uint16_t far *data = data_p;

  if ( FP_OFF(data) > 0xFA00 ) {

    uint16_t seg = FP_SEG( data );
    uint16_t off = FP_OFF( data );
    seg = seg + (off/16);
    off = off & 0x000F;
    data = (uint16_t far *)MK_FP( seg, off );

    // Are we ever hitting this?
    // ptrWrapCorrected++;
  }


  asm mov dx, len;  // Save original len here
  asm mov cx, dx;   // And here.
  asm shr cx, 1;    // Convert to words; we'll handle odd lengths later.
  asm xor bx, bx;   // Zero checksum register

  // Save some state and set the direction flag.
  asm push ds;
  asm push si;
  asm cld;

  asm lds si, data; // Setup DS and SI to point at the buffer
  asm clc;          // Ensure the carry bit is clear

  top:
    asm lodsw;
    asm adc bx, ax; // Add to checksum reg including last carry bit
    asm loop top;

  asm adc bx, 0;    // Get the last carry bit back into the checksum

  asm and dx, 1;    // Was the original length odd?
  asm jz notodd;    // If even, skip this next code


  // Add the odd byte to the checksum
  asm lodsw;
  asm xor ah, ah;   // Zero the high byte of the 16 bit reg we just loaded
  asm add bx, ax;   // Add to checksum.  Note the add, not adc ...
  asm adc bx, 0;    // Get the last carry


  notodd:

  asm not bx;       // Complement the checksum

  // Restore prior state
  asm pop si;
  asm pop ds;


  return _BX;

}



// Ip::pseudoChecksum
//
// Compute a standard checksum on a block of data.  Include the
// pseudo-header used by UDP and TCP.
//
// We know that UDP packets have an 8 byte header, and that TCP packets
// have a minimum of a 20 byte header.  So unroll the main loop four
// times for performance reasons.

extern "C" uint16_t ip_p_chksum( IpAddr_t src, IpAddr_t target,
         uint16_t *data_p, uint8_t protocol, uint16_t len ) {

  // Create our own FAR version of the data pointer so that we can
  // normalize it and use it no matter what the memory model is.
  //
  // Don't normalize if we are not near the end of the segment because
  // the division part is expensive.

  uint16_t far *data = data_p;

  if ( FP_OFF(data) > 0xFA00 ) {

    uint16_t seg = FP_SEG( data );
    uint16_t off = FP_OFF( data );
    seg = seg + (off/16);
    off = off & 0x000F;
    data = (uint16_t far *)MK_FP( seg, off );

    // Are we ever hitting this?
    // ptrWrapCorrected++;
  }


  asm mov dx, len;  // Save original len here
  asm xor bx, bx;   // Zero checksum register

  asm push ds;      // Save some state
  asm push si;      // Save some state
  asm cld;          // Clear direction flag for LODSW
  asm clc;          // Clear the carry bit

  // Setup addressing: src IP addr
  #if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)
  asm mov si, src;
  #else
  asm lds si, src;
  #endif

  asm lodsw;        // First word of source IP address
  asm add bx, ax;   // No carry to worry about
  asm lodsw;        // Get the next 16 bit word
  asm adc bx, ax;   // Add with carry


  // Setup addressing: dest IP addr
  #if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)
  asm mov si, target;
  #else
  asm lds si, target;
  #endif

  asm lodsw;        // First word of dest IP address
  asm adc bx, ax;   // Add with carry
  asm lodsw;        // Get the next 16 bit word
  asm adc bx, ax;   // Add with carry

  asm adc bx, 0;    // Add in any extra carry.


  // Add in protocol and length
  //
  // The algorithm works the same on both little endian and big endian
  // machines.  Data loaded from memory is one way, while the values we
  // have in variables are the other.  Therefore, swap before adding.

  asm xor cl, cl;         // Zero this out because protocol is 8 bits
  asm mov ch, protocol;   // Set protocol
  asm add bx, cx;         // Add only - carry was done already above.

  asm xchg dl, dh;        // Swap len to add it
  asm adc bx, dx;         // Add with carry
  asm adc bx, 0;          // Add in any extra carry.
  asm xchg dl, dh;        // Restore len back to correct byte order


  // We know that we are always called with a minimum of 8 bytes, which is
  // four words.  (UDP has a header size of 8 bytes, and TCP has a minimum
  // header size of 16 bytes.)  The loop is unrolled to do four words per
  // iteration, which will always be safe.  Odd numbers of words at the end
  // are handled by a smaller loop.

  asm mov cx, dx;
  asm shr cx, 1;    // Number of words
  asm shr cx, 1;    // Divide by 2
  asm shr cx, 1;    // Divide by 2 again ..  loop is unrolled 4x.
  asm clc;          // Clear the carry bit in case shr set it.


  // Setup addressing: data from user
  asm lds si, data;

  top:
    asm lodsw;
    asm adc bx, ax; // Add with carry
    asm lodsw;
    asm adc bx, ax; // Add with carry
    asm lodsw;
    asm adc bx, ax; // Add with carry
    asm lodsw;
    asm adc bx, ax; // Add with carry

    asm loop top;

  asm adc bx, 0;    // Add any extra carry bit


  // Are there words left over?

  asm mov cx, dx;   // DX has the original length
  asm shr cx, 1;    // Get to number of words
  asm and cx, 3;    // Figure out how many words are left
  asm jz  endwords; // If zero, skip ahead.

  asm clc;          // Clear carry bit from shr above

  top2:
    asm lodsw;
    asm adc bx, ax; // Add with carry
    asm loop top2;

    asm adc bx, 0;  // Add any extra carry bit


  endwords:


  // Is there a last byte?

  asm and dx, 1;    // Was the original length odd?
  asm jz notodd;

  asm lodsw;
  asm xor ah, ah;   // Zero the high byte of the 16 bit reg we just loaded
  asm add bx, ax;   // Add to checksum
  asm adc bx, 0;    // Get the last carry


  notodd:

  asm not bx;       // Ones complement


  // Restore prior state
  asm pop si;
  asm pop ds;


  return _BX;

}
#endif




#ifdef IP_FRAGMENTS_ON

// Fragmentation strategy
//
// If we get fragments, hold onto the original incoming packets that contain
// the fragments.  If we figure out that we received all of the fragments for
// an IP packet, then create a 'BigPacket' that looks like like a
// received packet, except it is large enough to hold the reassembled packet.
// Pass that to the upper layers just like any other received packet.
// On return we'll spot it and recycle it instead of putting it on the normal
// free list where dequeued packets go.
//
// A packet fails reassembly if it is going to be too big for our BigPackets,
// if there are too many fragments, if the fragments overlap, or if we don't
// receive all of the fragments before a timer expires.  In that case pretend
// nothing ever happened, which will make the other side retransmit.
//
// Fragmentation is complex; don't worry too much about performance, just keep
// the code readable.




// BigPacket_t
//
// Normally packets come from the pre-allocated packets created by the
// packet interface layer.  If we reassemble a bunch of fragments we'll
// use this jumbo sized packet to hold the contents, and then pretend
// to the layers upstream that this is the received packet.
//
// When we call the packet interface layer functions to recycle a packet
// onto the free list to be used for a new incoming packet we have
// to make sure that one of these doesn't get put on the free list like
// a normal packet.  There will be a quick check based on address ranges
// that will prevent that from happening.
//
// The number of BigPackets is independent of the number of fragment
// control structures.  It is possible (and maybe even desirable) to have
// fewer BigPackets than IpFragControls.  If the upper layer does what it
// needs to quickly then there won't be a problem.

typedef struct {
  EthHeader eh;
  IpHeader ip;
  uint8_t data[IP_BIGPACKET_SIZE];
} BigPacket_t;

static BigPacket_t *BigPacketFreeList[IP_MAX_FRAG_PACKETS];
static uint16_t BigPacketFreeIndex;


static inline BigPacket_t *getBigPacket( void ) {
  if ( BigPacketFreeIndex ) {
    BigPacketFreeIndex--;
    return BigPacketFreeList[BigPacketFreeIndex];
  }
  return NULL;
}

static inline void freeBigPacket( BigPacket_t *bp ) {
  BigPacketFreeList[BigPacketFreeIndex] = bp;
  BigPacketFreeIndex++;
}






// Fragment control structure.  This does our housekeeping while we are
// assembling fragments.

typedef struct {

  uint8_t       inUse;          // Is this structure in use
  uint8_t       fragsRcvd;      // How many fragments have we received so far
                                // 0 if reassembly is complete and we pushed
                                // this upstream.
  uint8_t       lastFragRcvd;   // Has the last fragment been seen
  uint8_t       padding;        // not used
  IpAddr_t      srcAddr;        // Part of key: IP address of the sender
  uint16_t      ident;          // Part of key: Packet ident
  clockTicks_t  startTime;      // Reassembly timer in timer ticks

  uint16_t  offsets[IP_MAX_FRAGS_PER_PACKET];    // Offset of each fragment
  uint16_t  lengths[IP_MAX_FRAGS_PER_PACKET];    // Length of each fragment
  uint8_t  *packets[IP_MAX_FRAGS_PER_PACKET];    // Pointer to each received fragment

} IpFragControl_t;


// Table of fragment control structures
//
// This is very simple code.  If we need to find something we just scan
// through the table.  In use structures have a flag set.

static IpFragControl_t fragControl[IP_MAX_FRAG_PACKETS];





// Local utility functions

// Given a source IP address and a packet Ident, tell us if we are already
// reassembling packets.  Returns a IpFragControl_t if we are, NULL if not.

static IpFragControl_t * findFragControl( IpAddr_t src, uint16_t ident ) {
  for ( uint8_t i=0; i < IP_MAX_FRAG_PACKETS; i++ ) {
    if ( fragControl[i].inUse && Ip::isSame( fragControl[i].srcAddr, src ) && (fragControl[i].ident == ident) ) {
      return &fragControl[i];
    }
  }
  return NULL;
}


// Find an open spot in the table.  Returns NULL if nothing available.

static IpFragControl_t * findOpenFragControl( void ) {
  for ( uint8_t i=0; i < IP_MAX_FRAG_PACKETS; i++ ) {
    if ( fragControl[i].inUse == 0 ) {
      return &fragControl[i];
    }
  }
  return NULL;
}



// Resets an IpFragControl_t.  Should only call this if it is in use.  Otherwise
// the Ip::fragsInReassembly counter will get goofed up.

static void killFragmentControl( IpFragControl_t *fc ) {

  // Recycle the fragments and mark this as dead.

  for ( uint16_t j=0; j < fc->fragsRcvd; j++ ) {
    Buffer_free( fc->packets[j] );
  }
  fc->fragsRcvd = 0;
  fc->inUse = 0;

  Ip::fragsInReassembly--;
}
  





// Public functions

int Ip::initForReassembly( void ) {

  // Allocate memory for BigPackets.  Be sure not to bust 64KB.
  // The assumption is that all allocated memory is in the same
  // segment.  If not, our code spot these packets when we return
  // a packet to the free list is broken.

  uint16_t tmpSize = sizeof( BigPacket_t ) * IP_MAX_FRAG_PACKETS;

  uint8_t *tmp = (uint8_t *)malloc( tmpSize );
  if ( tmp == NULL ) {
    return -1;
  }

  ipReassemblyMemoryStart = tmp;
  ipReassemblyMemoryEnd = tmp+tmpSize;


  // Initialize IpFragControl_t structs and BigPacketFreeList

  for ( uint16_t i=0; i < IP_MAX_FRAG_PACKETS; i++ ) {

    fragControl[i].inUse = 0;

    BigPacketFreeList[i] = (BigPacket_t *)tmp;
    tmp = tmp + sizeof( BigPacket_t );
  }

  BigPacketFreeIndex = IP_MAX_FRAG_PACKETS;

  return 0;
}



void Ip::reassemblyStop( void ) {

  // Return any packets that we were holding onto.

  for ( uint16_t i=0; i < IP_MAX_FRAG_PACKETS; i++ ) {
    if ( fragControl[i].inUse ) killFragmentControl( &fragControl[i] );
  }

  // Return the memory.  By this point nothing should be in use.
  // (Would make a greak consistency check.)
  free( ipReassemblyMemoryStart );
}



// Return a BigPacket to the free list.  Used by the Buffer_free
// routine.  Buffer_free already figured out this was a big
// packet, so just stick it on the free list.

void Ip::returnBigPacket( uint8_t *bp ) {
  freeBigPacket( (BigPacket_t *)bp );
}
  




// Check the reassembly timers for any packets that are being reassembled.
// This gets called if there are any packets in the process of being
// reassembled.

void Ip::purgeOverdue( void ) {

  for ( uint16_t i=0; i < IP_MAX_FRAG_PACKETS; i++ ) {

    if ( fragControl[i].inUse ) {

      clockTicks_t elapsedTicks = Timer_diff( fragControl[i].startTime, TIMER_GET_CURRENT( ) );

      if ( elapsedTicks > TIMER_MS_TO_TICKS(IP_FRAG_REASSEMBLY_TIMEOUT) ) {

        TRACE_IP_WARN(( "Ip: Reassembly timeout: src: %d.%d.%d.%d  ident: %u\n",
                        fragControl[i].srcAddr[0], fragControl[i].srcAddr[1],
                        fragControl[i].srcAddr[2], fragControl[i].srcAddr[3],
                        ntohs( fragControl[i].ident ) ));

        killFragmentControl( &fragControl[i] );
        timeoutReassemblies++;
      }

    }

  }

}




// Create a BigPacket from smaller fragments.  By the time we get here we have
// all of the fragments in order.

static uint8_t * makeBigPacket( IpFragControl_t *fc ) {

  // Should never fail, but add some consistency checking code for testing.
  BigPacket_t *bp = getBigPacket( );

  if ( bp == NULL ) {
    // Crap - reassembled a fragment but have no packet to put it in.
    // Have to throw it all away.
    TRACE_IP_WARN(( "Ip: No BigPackets avail\n" ));
    killFragmentControl( fc );
    return NULL;
  }


  // In theory all we need is a packet with the IP header, and the IP header
  // doesn't even need to be perfect.  (We could leave a bad checksum value
  // in it with no ill effects.)  It doesn't cost much to make a correct
  // packet so copy the Ethernet header, IP header, and data and fix up the
  // miscellaneous fields of the IP header.

  // Copy the Ethernet header
  memcpy( &(bp->eh), fc->packets[0], sizeof( EthHeader ) );


  // Copy the IP header.  We're going to do something sleazy here and
  // purposefully not copy the IP header options to the BigPacket.
  // If we wanted to do this correctly it's possible, but more work.
  // Fudge the header length to be exactly sizeof(IpHeader) too.

  IpHeader *firstIpHeader = (IpHeader *)(fc->packets[0] + sizeof(EthHeader));
  memcpy( &bp->ip, firstIpHeader, sizeof(IpHeader) );
  bp->ip.setIpHlen( sizeof(IpHeader) );



  // Copy data from fragments
  //
  // The new BigPacket is supposed to include 20 bytes of the IP header which
  // is counted as part of the offset field.  We've already copied the IP
  // header from the first packet, so skip those 20 bytes.

  uint16_t startOffset = 0;

  for ( int i=0; i < fc->fragsRcvd; i++ ) {
    IpHeader *hdr = (IpHeader *)(fc->packets[i] + sizeof( EthHeader ));
    memcpy( bp->data+startOffset, hdr->payloadPtr( ), fc->lengths[i] );
    startOffset += fc->lengths[i];
  }

  // Need to fudge total length, flags, fragment offset, and header checksum
  bp->ip.total_length = htons( startOffset + sizeof(IpHeader) );
  bp->ip.flags = 0;

  // Checksum at the IP header level is meaningless - upper layers will not use it.
  // bp->ip.chksum = ipchksum( (uint16_t *)&bp->ip, bp->ip.getIpHlen( ) );
  bp->ip.chksum = 0;


  // We are done with this IpFragControl_t.
  killFragmentControl( fc );

  Ip::goodReassemblies++;

  // Utils::dumpBytes( (unsigned char *)fc->bigPacket, sizeof(EthHeader) + sizeof( IpHeader ) + startOffset );

  return (uint8_t *)bp;
}





static char Err_PacketTooBig[] = "Ip: Packet too big to reassemble\n";


// processFragment
//
// Returns
//
//   Null: if the packet is being held for reassembly
//   *:    if a bigPacket has been substituted


static uint8_t * processFragment( IpHeader *ip, uint8_t *packet ) {

  // We use these often
  uint16_t fragmentOffset = ip->fragmentOffset( );
  uint16_t fragmentLength = ntohs(ip->total_length) - ip->getIpHlen( );
  uint8_t  isLastFragment = ip->isLastFragment( );

  TRACE_IP(( "Ip: Frag off: %u  Frag len: %u  Islast: %d  Packet: %p\n",
             fragmentOffset, fragmentLength, isLastFragment, packet ));


  // Have we seen a fragment from this packet yet?
  IpFragControl_t *fc = findFragControl( ip->ip_src, ip->ident ); 

  if ( fc == NULL ) {

    // New packet to reassemble

    // Room to start a new reassembly?
    fc = findOpenFragControl( );
    if ( fc == NULL ) {
      TRACE_IP_WARN(( "Ip: No room for reassembly\n" ));
      Ip::tooManyInFlight++;
      Buffer_free( packet );
      return NULL;
    }

    // We have room to start reassembling this packet.  The only thing that
    // can go wrong is that this fragment is bigger than what our BigPacket
    // can accomodate.  If that happens, abort.

    if ( fragmentOffset + fragmentLength > IP_BIGPACKET_SIZE ) {
      TRACE_IP_WARN(( Err_PacketTooBig ));
      Ip::payloadTooBig++;
      Buffer_free( packet );
      return NULL;
    }

    TRACE_IP(( "Ip: Start reassembly\n" ));

    // This counter tells us how many packets are in reassembly, which is
    // the same as how many IpFragControl_t structs are in use.
    Ip::fragsInReassembly++;

    // Initialize all fields

    fc->inUse = 1;
    fc->fragsRcvd = 1;
    fc->lastFragRcvd = isLastFragment;
    Ip::copy( fc->srcAddr, ip->ip_src );
    fc->ident = ip->ident;
    fc->startTime = TIMER_GET_CURRENT( );

    fc->offsets[0] = fragmentOffset;
    fc->lengths[0] = fragmentLength;
    fc->packets[0] = packet;

    // We are done for the moment.  Fall through to return NULL.

  }
  else {

    // Already started collecting fragments for reassembly.  Add this
    // fragment to the list.


    // Is this fragment out of bounds?
    if ( fragmentOffset + fragmentLength > IP_BIGPACKET_SIZE ) {
      TRACE_IP_WARN(( Err_PacketTooBig ));
      Ip::payloadTooBig++;
      Buffer_free( packet );
      // No point in continuing with the rest of the reassembly.
      killFragmentControl( fc );
      return NULL;
    }


    if ( isLastFragment ) {
      fc->lastFragRcvd = 1;
    }


    // Insert fragment in list by finding the first fragment that is at
    // the same offset or greater than the incoming fragment offset.
    // When the search stops insertPos will be pointing at that packet,
    // or past the end of the list where it will get added.

    // For style points rewrite this to start at the last packet and
    // go backwards through the list.  Most packets come in order so
    // we should be adding onto the back of the list.

    int insertPos;
    for ( insertPos=0; insertPos < fc->fragsRcvd; insertPos++ ) {
      if ( fragmentOffset <= fc->offsets[insertPos] ) break;
    }

    // Are we inserting into to the middle of the list?
    if ( insertPos < fc->fragsRcvd ) {

      if ( fragmentOffset == fc->offsets[insertPos] && fragmentLength == fc->lengths[insertPos] ) {
        // Duplicate fragment; silently throw it away
        Buffer_free( packet );
        return NULL;
      }

      if ( fragmentOffset+fragmentLength > fc->offsets[insertPos] ) {
        // Overlapping fragment.  Not good.  Abort the whole mission.
        Buffer_free( packet );
        killFragmentControl( fc );
        return NULL;
      }

      // Make room by sliding others down.
      for ( int j=fc->fragsRcvd; j>insertPos; j-- ) {
        fc->offsets[j] = fc->offsets[j-1];
        fc->lengths[j] = fc->lengths[j-1];
        fc->packets[j] = fc->packets[j-1];
      }

    }

    fc->offsets[insertPos] = fragmentOffset;
    fc->lengths[insertPos] = fragmentLength;
    fc->packets[insertPos] = packet;

    fc->fragsRcvd++;


    // Dump list
    /*
    for ( int j=0; j < fc->fragsRcvd; j++ ) {
      TRACE_IP(( "Ip: Slot: %d  Offset: %u   Len: %u  %p\n", j,
                 fc->offsets[j], fc->lengths[j], fc->packets[j] ));
    }
    */


    // Is the fragment list complete?

    if ( fc->lastFragRcvd ) {

      uint16_t startOffset = 0;
      uint16_t complete = 1;

      for ( uint16_t j=0; j < fc->fragsRcvd; j++ ) {
        if ( startOffset != fc->offsets[j] ) {
          complete = 0;
          break;
        }
        startOffset += fc->lengths[j];
      }

      if ( complete ) {
        TRACE_IP(( "Reassembly complete\n" ));
        uint8_t *newPacket = makeBigPacket( fc );
        return newPacket;
      }

    } // end if last frag received


    // If we are not complete and we just filled our table, then we are done.
    if ( fc->fragsRcvd == IP_MAX_FRAGS_PER_PACKET ) {
      killFragmentControl( fc );
      Ip::notEnoughSlots++;
    }

    // Fall through to return NULL.
  }

  return NULL;
}

#endif




void Ip::process( uint8_t *packet, uint16_t packetLen ) {

  IpHeader *ip = (IpHeader *)(packet + sizeof(EthHeader) );

  uint16_t ipHdrLen = ip->getIpHlen( );

  TRACE_IP(( "Ip: Process Src: %d.%d.%d.%d  Hlen: %d  Len: %d  Prot: %d  Ident: %u\n",
             ip->ip_src[0], ip->ip_src[1], ip->ip_src[2], ip->ip_src[3],
             ipHdrLen, ntohs(ip->total_length), ip->protocol, ntohs(ip->ident) ));

  // Check the incoming chksum.

  // uint16_t myChksum = genericChecksum( (uint16_t *)ip, ipHdrLen );

  uint16_t myChksum = ipchksum( (uint16_t *)ip, ipHdrLen );

  if ( myChksum ) {
    badChecksum++;
    TRACE_IP_WARN(( "Ip: Bad checksum: %04x, should be %04x Src: %d.%d.%d.%d\n", ip->chksum, myChksum,
                    ip->ip_src[0], ip->ip_src[1], ip->ip_src[2], ip->ip_src[3] ));
    Buffer_free( packet );
    return;
  }



  if ( ip->isFragment( ) ) {

    fragsReceived++;

    #ifdef IP_FRAGMENTS_ON

      // If processFragment returns null, it's time to leave.  Assume that it
      // returned the packet or held it as it needed.  If it returns a pointer
      // then it has swapped in a large packet as a substitute.  Treat that
      // as our packet from this point forward.

      packet = processFragment( ip, packet );
      if ( packet == NULL ) return;
      ip = (IpHeader *)(packet + sizeof(EthHeader) );

    #else

      // No fragment support
      Buffer_free( packet );
      return;

    #endif

  }


  // From this point forward you have a packet, but it's not necessarily
  // the same packet you started with.  Don't worry, the code that return
  // the packets to the free list know how to deal with anything we did.


  // Route to the correct protocol

  #ifdef COMPILE_TCP
  if ( ip->protocol == IP_PROTOCOL_TCP ) {
    Tcp::process( packet, ip );
  }
  else
  #endif

  #ifdef COMPILE_UDP
  if ( ip->protocol == IP_PROTOCOL_UDP ) {
    Udp::process( packet, ip );
  } else
  #endif

  #ifdef COMPILE_ICMP
  if ( ip->protocol == IP_PROTOCOL_ICMP ) {
    icmpRecvPackets++;
    Icmp::process( packet, ip );
  }
  else
  #endif

  {
    // Unhandled
    Buffer_free( packet );
  }

}


void IpHeader::set( uint8_t protocol_p, const IpAddr_t dstHost,
                    uint16_t payloadLen, uint8_t moreFrags, uint16_t fragOffset ) {

  // We don't support outgoing IP header options.  (We'll ignore incoming
  // IP header options, but we at least know to look for them.)
  setIpHlen( sizeof( IpHeader ) );
  service_type = 0;


  // We used to always set ident to zero.  Now we increment so that each
  // packet we send has a unique IDENT.
  //
  // If somebody retransmits a packet they need to ensure that IDENT is
  // unique.  The code paths that hold their transmission buffers need
  // to update IDENT in those buffers and recalc the checksum when they
  // retransmit.
  //
  // If the moreFrags flag is set then do not increment IDENT.  We want
  // all of the fragments to have the same IDENT.
  
  if ( moreFrags == 0 ) ident = htons( IpIdent++ );

  // Fix me - combine these in one call
  setFlags( moreFrags );
  setFragOffset( fragOffset );

  ttl = 255;
  protocol = protocol_p;

  Ip::copy( ip_src, MyIpAddr );
  Ip::copy( ip_dest, dstHost );

  total_length = htons( sizeof(IpHeader) + payloadLen );

  chksum = 0;
  //chksum = Ip::genericChecksum( (uint16_t *)this, sizeof( IpHeader ) );
  chksum = ipchksum( (uint16_t *)this, sizeof( IpHeader ) );

}



// IpHeader::setDestEth
//
// Figure out if we are routing this packet to a machine on the local
// network or a machine elsewhere.  If on the local network we check
// the ARP cache for the machine.  If it is a machine on another network
// we check the ARP cache for the MAC address of our gateway.

// Returns 0 - Packet ready to be transmitted
//         1 - Not sent pending Arp

// Note: Once we know the target MAC addr for a socket connection we
// should not call this anymore.  Fix this.

// Definitions
//
// - Network =   0  Host = 0:  Refers to this network/this host - not valid
// - Network =   0  Host = x:  Refers to host x on 'this' network
// - Network =   1s Host = 1s: Limited broadcast
// - Network =   x  Host = 1s: Directed broadcast
// - Network =   x  Host = 0:  Refers network x
// - Network = 127  Host = x:  Loopack
//
// More notes
//
// - Directed broadcast to another network should work, as we'll detect
//   it is another network and send it to the gateway.
//
// - Directed broadcast to our own network will probably fail, as we'll
//   try to ARP it and get either nothing or multiple replies back.  Use
//   the Limited broadcast function instead.
//
// - Limited broadcast - implemented
//
// - Loopback - not implemented

int8_t IpHeader::setDestEth( EthAddr_t ethTarget ) {


  // Historically we've always had IP addresses as char[4], which is easy
  // to work with and keeps it in the correct byte order in memory.
  // But it's so much easier to work with uint32_t when doing anding and
  // or'ing.  So bite the bullet and convert the destination IP addr
  // to a uint32.

  // If you wanted to use the char[4] version you'd have to do three
  // byte swaps to get it in the right order.

  uint32_t destIpAddr_u = *(uint32_t *)(&ip_dest);

  // Is this a global broadcast?
  if ( destIpAddr_u == 0xfffffffful ) {
    Eth::copy( ethTarget, Eth::Eth_Broadcast );
    return 0;
  }


  // Probably the only program that does not have ARP compiled in is the DHCP
  // client.  If you need ARP and you fail to compile it in you are going to
  // have a bad time.
  //
  // DHCP does not get tripped up by this because it goes through the
  // broadcast code above.

  #ifdef COMPILE_ARP

    int rc;

    if ( (MyIpAddr_u & Netmask_u) != (destIpAddr_u & Netmask_u) ) {
      rc = Arp::resolve( Gateway, ethTarget );
    }
    else {
      rc = Arp::resolve( ip_dest, ethTarget );
    }

    return rc;

  #else

    return -1;

  #endif

}



#ifdef COMPILE_ICMP


// We only allow for one callback function.  This is used by Ping.
// ICMP doesn't get multiplexed among ports like UDP does so we don't
// need to be more elaborate.

void (*Icmp::icmpCallback)(const unsigned char *packet,
                           const IcmpHeader *icmp);


// Our Icmp implementation is minimal.  Keep one packet around for outgoing
// requests instead of allocating it from the heap.  This implies that when
// we get an incoming request that we have to create a response and push it
// out immediately

IcmpEchoPacket_t Icmp::icmpEchoPacket;



void Icmp::init( void ) {

  // Pre-initialize some fields in our Icmp packet

  icmpEchoPacket.eh.setSrc( MyEthAddr );
  icmpEchoPacket.eh.setType( 0x0800 );

  icmpCallback = NULL;
}



void near Icmp::process( uint8_t *packet, IpHeader *ip ) {

  IcmpHeader *icmp = (IcmpHeader *)(ip->payloadPtr( ));

  uint16_t icmpLen = ip->payloadLen( );

  TRACE_IP(( "Icmp: type: %u code: %u len: %u\n",
             icmp->type, icmp->code, icmpLen ));


  // Verify incoming checksum

  if ( ipchksum( (uint16_t *)icmp, icmpLen ) ) {
    TRACE_IP_WARN(( "Icmp: Bad chksum from %d.%d.%d.%d  type: %u code: %u len: %u\n",
                     ip->ip_src[0], ip->ip_src[1], ip->ip_src[2], ip->ip_src[3],
                     icmp->type, icmp->code, icmpLen ));
    Buffer_free( packet );
    return;
  }



  // Unlike Udp, we'll throw the packet away when the user is done.
  // That's because we still need to inspect it and possibly send a reply.
  // The user has more of an inspection hook than a handler.

  if ( icmpCallback ) icmpCallback( packet, icmp );



  if ( icmp->type == ICMP_ECHO_REQUEST ) {

    IcmpEchoPacket_t *reqPkt = (IcmpEchoPacket_t *)packet;

    uint16_t icmpOptDataLen = icmpLen -
               ( sizeof(IcmpHeader) + sizeof(uint16_t) + sizeof(uint16_t) );

    if ( icmpOptDataLen <= ICMP_ECHO_OPT_DATA ) {

      // Outgoing packet has same length as the incoming one.
      icmpEchoPacket.ip.set( IP_PROTOCOL_ICMP, ip->ip_src, icmpLen, 0, 0 );

      // Do we really need to do this if we just received it?  It should
      // be going back the same way it came.
      //
      // int8_t rc = icmpEchoPacket.ip.setDestEth( &icmpEchoPacket.eh.dest );
      //
      // We're going to sleaze out and just use the MAC addr that sent it.
      icmpEchoPacket.eh.setDest( reqPkt->eh.src );

      memcpy( &icmpEchoPacket.icmp, icmp, icmpLen );

      icmpEchoPacket.icmp.type = ICMP_ECHO_REPLY;

      icmpEchoPacket.icmp.checksum = 0;
      icmpEchoPacket.icmp.checksum = ipchksum( (uint16_t *)&icmpEchoPacket.icmp,
                                               icmpLen );

      // Now blast it out
      Packet_send_pkt( &icmpEchoPacket,
                       icmpLen + sizeof(EthHeader) + sizeof(IpHeader) );

      TRACE_IP(( "Icmp: Sent Echo reply, ident: %u  seq: %u\n",
                 ntohs(icmpEchoPacket.ident),
                 ntohs(icmpEchoPacket.seq)
              ));


      // end if safe len
    } else {
      TRACE_IP_WARN(( "Icmp: Packet too long to reply too.\n" ));
    }

  } // end if echo request


  // All done - throw it away
  Buffer_free( packet );
}


#endif
