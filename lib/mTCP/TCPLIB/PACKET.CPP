
/*

   mTCP Packet.cpp
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


   Description: Packet driver buffer management and packet driver
     interface code.

   Changes:

   2011-05-27: Initial release as open source software
   2015-01-10: Changes to decouple the upper layers from this layer;
               Change to use self modifying code to gain a little
               speed back.
   2015-04-30: Undo the self modifying code; it seems to have a very
               bad effect when running spdtest on the Pentium 133
               with the Linksys card.  (The packet driver is constantly
               reporting sending errors; it's not worth fighting for.)

*/




#include <dos.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <i86.h>

#include "packet.h"
#include "trace.h"
#include "utils.h"

#ifdef IP_FRAGMENTS_ON
#include "ip.h"
#endif


// Buffer management
//
// Remember, this is code that defines buffers for incoming data.  Buffers
// for data being sent are managed by the part of the stack sending the data.
//
// We use a stack to store the free packets.  This should give us quick
// access when we need it.  It's also cache friendly on newer systems.
//
// We use ring buffer to store packets that have arrived in order.  The ring
// buffer is processed one packet at a time in order.  As each packet is
// processed the app may return it to the free pool or hold onto it.  Ether
// way, it's out of the ring buffer.  The ring buffer is one slot larger than
// the number of buffers so that we never get confused between a full buffer
// and an empty one.
//
// All of this data should be treated as "private" except for the
// lowFreeCount, which is advertised in the .H file.


// Ring buffer (pointer to buffer and incoming length of each buffer)
// We don't ever use the incoming length, so as an optimization we
// can #define it out.
//
static uint8_t *Buffer[ PACKET_RB_SIZE ];
static uint16_t Buffer_len[ PACKET_RB_SIZE ];

// Ring buffer indices
uint8_t   Buffer_first;   // Oldest packet in the ring, first to process
uint8_t   Buffer_next;    // Newest packet in the ring, add incoming here.


// Free list, implemented as a stack.
static uint8_t  *Buffer_fs[ PACKET_BUFFERS ];
static uint8_t   Buffer_fs_index;


// Used for deallocating memory.
static void     *BufferMemPtr;


// For use by the packet driver in between receive calls.  Don't touch this.
static uint8_t  *Buffer_packetBeingCopied;


// Track this statistic so that we know if we get close to running out of
// buffers.  That might mean we need more buffers or we need to process the
// incoming buffers more quickly.
//
uint8_t   Buffer_lowFreeCount;




// Buffer_init
//
// We are using the normal malloc call so you can not allocate more than
// 64KB of storage for receive buffers.  Also keep in mind that the memory
// model makes a difference too; the small memory models may have much
// less than 64KB available for buffers depending on what else has used
// the heap.
//
// This code should work no matter which memory model you choose.  In the
// large data models the buffer pointers are created and normalized
// to avoid offset wrapping problems later on.
//
// Even though everything is initialized at the end of the function, we do
// not setup the free buffer count to its true value until we are told to
// do so later on.  This allows us to setup other data structures in higher
// levels of the stack before turning on the flow of packets.  (Once we set
// the number of free buffers to something other than 0, the packet driver
// will be able to start using buffers.)

int8_t Buffer_init( void ) {

  // We are using malloc here, which allows us to allocate up to 64K of
  // data in a single call.  (The parameter to it is an unsigned int.)

  uint8_t *tmp = (uint8_t *)(malloc( PACKET_BUFFERS * PACKET_BUFFER_LEN ));
  if ( tmp == NULL ) {
    return -1;
  }

  // Put pointers to packets in the free stack.

  for ( uint8_t i=0; i < PACKET_BUFFERS; i++ ) {

    #if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)

      // In these memory models the stack and help share the same segment
      // and we only have an offset to data.  It would be nice to normalize
      // our pointers, but since we have only offsets we can't do that.
      // Leave our pointers as is, and let the routines that care worry
      // about it.
      //
      // We could force the use of far pointers, but that has a lot of
      // ripple effect.  Malloc starts toward the low end of the data
      // segment so the chances on a pointer being near the end of the
      // segment are pretty slim anyway.

      Buffer_fs[i] = tmp + (i*PACKET_BUFFER_LEN);

    #else

      // Normalize each pointer to make the offset as small as possible
      // here so that we don't run into segment wrap problems with
      // LODS, LODSW or related instructions later on as we are
      // computing checksums in IP.CPP.
      //
      // This is slightly expensive, but we only do it once for the life
      // of a buffer pointer.

      uint8_t *t = tmp+(i*PACKET_BUFFER_LEN);
      uint16_t seg = FP_SEG( t );
      uint16_t off = FP_OFF( t );
      seg = seg + (off/16);
      off = off & 0x000F;

      Buffer_fs[i] = (uint8_t *)MK_FP( seg, off );

    #endif
  }


  // Initialize the fs_index to zero so that we don't start receiving
  // data before the other data structures are ready.  This happens because
  // we have to initialize the packet driver to get our MAC address, but
  // we need the MAC address to initialize things like ARP.  This allows
  // us to initialize the packet driver without fear of receiving a packet.
  // (Any packet we get in this state will be tossed.)

  Buffer_fs_index = 0;

  Buffer_lowFreeCount = PACKET_BUFFERS;

  Buffer_first = 0;
  Buffer_next = 0;

  return 0;
}



// Buffer_startReceiving
//
// Use this when you are ready to have the packet driver start receiving
// packets and writing into buffers.  Your stack should be fully initialized
// before calling this.

void Buffer_startReceiving( void ) { Buffer_fs_index = PACKET_BUFFERS; }


// Buffer_free
//
// Use this function to return a buffer to the free stack.  Do this when
// you are completely done with the buffer.

void Buffer_free( const uint8_t *buffer ) {

  #ifdef IP_FRAGMENTS_ON

    // No need to protect this by disabling interrupts; the packet driver
    // doesn't care about it.

    if ( Ip::isIpBigPacket( buffer ) ) {
      Ip::returnBigPacket( (uint8_t *)buffer );
      return;
    }
  #endif

  // This has to be protected because the packet driver can interrupt
  // at any time to grab a packet from the free list.

  disable_ints( );
  Buffer_fs[ Buffer_fs_index ] = (uint8_t *)buffer;
  Buffer_fs_index++;
  enable_ints( );
}


// Buffer_stopReceiving
//
// Use this to prevent the packet driver from being able to receive any more
// packets and putting them into the ring buffer.  This is usually done when
// you are preparing to shut things down.

void Buffer_stopReceiving( void ) { Buffer_fs_index = 0; }



// Buffer_stop
//
// This is about the last step before shutting everything down.  It just has
// to return the memory that was allocated.

void Buffer_stop( void ) { if ( BufferMemPtr) free( BufferMemPtr ); }






//--------------------------------------------------------------------------
//
// Packet driver data


// Stats
//
// All of these stats are scoped to the life of the running program.  The
// packet driver probably has larger numbers scoped to the lifetime of the
// packet driver.

uint32_t Packets_dropped = 0;       // Packets lost due to lack of buffer space
uint32_t Packets_received = 0;      // Packets received
uint32_t Packets_sent = 0;          // Packets sent
uint32_t Packets_send_errs = 0;     // Failures even after multiple retries
uint32_t Packets_send_retries = 0;  // Retry attempts


// Visible for anyting that wants to use it.
const char * PKT_DRVR_EYE_CATCHER = "PKT DRVR";


// Both of these are cached so that we don't have to keep passing them around.
//
static uint16_t Packet_handle;     // Provided by the packet driver
static uint8_t  Packet_int = 0x0;  // Provided during initialization



//--------------------------------------------------------------------------
//
// EtherType registration data and code

// This is the array of function pointers that will be called for each EtherType.
//
void (*Packet_EtherTypeHandler[PACKET_HANDLERS])(uint8_t *packet, uint16_t len);

// This is the array of EtherTypes that correspond to the function array above.
// EtherTypes are stored in network byte order in this table.  This makes it
// quicker to evaluate each packet when it comes in.
//
EtherType Packet_EtherTypeVal[PACKET_HANDLERS];

// The number of registered EtherType handlers
uint8_t  Packet_EtherTypeHandlers = 0;

// An optional handler for EtherTypes that do not have a dedicated function.
void (*Packet_typeUnhandled)(uint8_t *packet, uint16_t len);


// EtherType handlers are searched in the order that they are added, so add the
// mostly commonly seen EtherTypes first.

int8_t Packet_registerEtherType( EtherType val, void (*f)(uint8_t *packet, uint16_t) ) {
  if (Packet_EtherTypeHandlers == PACKET_HANDLERS ) return -1;
  Packet_EtherTypeVal[ Packet_EtherTypeHandlers ] = htons( val );
  Packet_EtherTypeHandler[ Packet_EtherTypeHandlers ] = f;
  Packet_EtherTypeHandlers++;
  return 0;
}

void Packet_registerDefault( void (*f)(uint8_t *packet, uint16_t) ) {
  Packet_typeUnhandled = f;
}




//--------------------------------------------------------------------------
//
// Packet driver code




// The magic receiver function.
//
// This is the function called by the packet driver when it receives a packet.
// There are actually two calls; one call to get a buffer for the new packet
// and one call to tell us when the packet has been copied into the buffer.
//
// If no buffers are available or the incoming packet is bigger than our buffer
// size then tell the packet driver to drop the packet.  Otherwise, provide
// the address of the next Buffer to use.
//
// Once the second call is made a new buffer is available to use for processing.
// It is added to the the end of the ring buffer.

static void far interrupt receiver( union INTPACK r ) {

  if ( r.w.ax == 0 ) {

    #ifdef TORTURE_TEST_PACKET_LOSS
    if ( (r.w.cx>PACKET_BUFFER_LEN) || (Buffer_fs_index == 0) || ((rand() % TORTURE_TEST_PACKET_LOSS) == 0 )) {
    #else
    if ( (r.w.cx>PACKET_BUFFER_LEN) || (Buffer_fs_index == 0) ) {
    #endif

      r.w.es = r.w.di = 0;
      Packets_dropped++;
    }
    else {
      Buffer_fs_index--;
      Buffer_packetBeingCopied = Buffer_fs[ Buffer_fs_index ];
      r.w.es = FP_SEG( Buffer_fs[ Buffer_fs_index ] );
      r.w.di = FP_OFF( Buffer_fs[ Buffer_fs_index ] );
    }
  }
  else {
    Packets_received++;
    Buffer[ Buffer_next ] = Buffer_packetBeingCopied;
    Buffer_len[Buffer_next] = r.w.cx;

    Buffer_next++;
    if ( Buffer_next == PACKET_RB_SIZE ) Buffer_next = 0;


    if (Buffer_lowFreeCount > Buffer_fs_index ) {
      Buffer_lowFreeCount = Buffer_fs_index;
    }
  }

  // Custom epilog code.  Some packet drivers can handle the normal
  // compiler generated epilog, but the Xircom PE3-10BT drivers definitely
  // can not.

_asm {
  pop ax
  pop ax
  pop es
  pop ds
  pop di
  pop si
  pop bp
  pop bx
  pop bx
  pop dx
  pop cx
  pop ax
  retf
};


}



// Packet_init
//
// First make sure that there is a packet driver located where the caller
// has specified.  If so, try to register to receive all possible EtherTypes
// from it.

int8_t Packet_init( uint8_t packetInt ) {

  uint16_t far *intVector = (uint16_t far *)MK_FP( 0x0, packetInt * 4 );

  uint16_t eyeCatcherOffset = *intVector;
  uint16_t eyeCatcherSegment = *(intVector+1);

  char far *eyeCatcher = (char far *)MK_FP( eyeCatcherSegment, eyeCatcherOffset );

  eyeCatcher += 3; // Skip three bytes of executable code

  if ( _fmemcmp( PKT_DRVR_EYE_CATCHER, eyeCatcher, 8 ) != 0 ) {
    TRACE_WARN(( "Packet: eye catcher not found at %x\n", packetInt ));
    return -1;
  }


  union REGS inregs, outregs;
  struct SREGS segregs;

  inregs.h.ah = 0x2;                 // Function 2 (access_type)
  inregs.h.al = 0x1;                 // Interface class (ethernet)
  inregs.x.bx = 0xFFFF;              // Interface type (card/mfg)
  inregs.h.dl = 0;                   // Interface number (assume 0)
  segregs.ds = FP_SEG( NULL );       // Match all EtherTypes
  inregs.x.si = FP_OFF( NULL );
  inregs.x.cx = 0;
  segregs.es = FP_SEG( receiver );   // Receiver function
  inregs.x.di = FP_OFF( receiver );

  int86x( packetInt, &inregs, &outregs, &segregs );

  if ( outregs.x.cflag ) {
    TRACE_WARN(( "Packet: %u error on access_type call\n", outregs.h.dh ));
    return outregs.h.dh;
  }

  Packet_int = packetInt;
  Packet_handle = outregs.x.ax;

  return 0;

}


int8_t Packet_release_type( void ) {

  int8_t rc = -1;

  union REGS inregs, outregs;
  struct SREGS segregs;

  inregs.h.ah = 0x3;
  inregs.x.bx = Packet_handle;

  int86x( Packet_int, &inregs, &outregs, &segregs );

  if ( outregs.x.cflag ) {
    TRACE_WARN(( "Packet: Err releasing handle\n" ));
  }
  else {
    TRACE(( "Packet: Handle released\n" ));
    rc = 0;
  }

  return rc;
}


void Packet_get_addr( uint8_t *target ) {

  union REGS inregs, outregs;
  struct SREGS segregs;

  inregs.h.ah = 0x6;
  inregs.x.bx = Packet_handle;
  segregs.es = FP_SEG( target );
  inregs.x.di = FP_OFF( target );
  inregs.x.cx = 6;

  int86x( Packet_int, &inregs, &outregs, &segregs );

}


// Packet_send_pkt
//
// This is the packet send function that the rest of the world sees.  Given a
// buffer and a length it will have the packet driver transmit the contents.
//
// This is generally assumed to work, hence the lack of a return code.

void Packet_send_pkt( void *buffer, uint16_t bufferLen ) {

  Packets_sent++;

  #ifdef TORTURE_TEST_PACKET_LOSS
    if ( (rand() % TORTURE_TEST_PACKET_LOSS) == 0 ) {
      return;
    }
  #endif

  #ifndef NOTRACE
  if ( TRACE_ON_DUMP ) {
    uint16_t dumpLen = ( bufferLen > PKT_DUMP_BYTES ? PKT_DUMP_BYTES : bufferLen );
    TRACE(( "Packet: Sending %u bytes, dumping %u\n", bufferLen, dumpLen ));
    Utils::dumpBytes( Trace_Stream, (unsigned char *)buffer, dumpLen );
  }
  #endif


  // Hate to do this but ...
  //
  // Some drivers reject runt packets.  Intel Gigabit drivers are
  // an example.  We might wind up transmitting junk, but I don't really care.
  // (Yes, it's a possible security leak.  Quite a long shot though.)

  if ( bufferLen < 60 ) bufferLen = 60;


  // There are a wide variety of cards out there and some are not as capable
  // as others.  If we send packets too fast we might overrun the card.  Retry
  // a few times before giving up on this packet, and keep some stats just in
  // case we need to debug a problem report.

  union REGS inregs, outregs;
  struct SREGS segregs;

  inregs.h.ah = 0x4;

  inregs.x.cx = bufferLen;

  inregs.x.si = FP_OFF( buffer );
  segregs.ds = FP_SEG( buffer );

  uint8_t attempts = 0;

  while ( attempts < 5 ) {
    int86x( Packet_int, &inregs, &outregs, &segregs);
    if ( !outregs.x.cflag ) return;
    attempts++;
  }

  TRACE_WARN(( "Packet: send error\n" ));
  Packets_send_errs++;


  // Be careful with the early returns ...
}



// Packet_process_internal
//
// This is the code that takes the next packet off of the ring buffer and
// passes it up the stack for processing.  Usually this is wrapped by a
// macro.  This should not be called if there is nothing on the ring buffer
// to process.
//
// The receiver adds to the ring buffer at Buffer_next, which is the head
// of the ring buffer.  This code dequeues from the tail of the ring buffer
// which is the oldest packet that has not been processed yet.

void Packet_process_internal( void ) {

  // Dequeue the first buffer in the ring.  If we got here then we know that
  // there is at least one packet in the buffer.  The user is responsible
  // for freeing the buffer using Buffer_free when they are done with it.
  // Holding buffers too long will cause us to run out of free buffers,
  // which then causes the packet driver to drop packets on the floor.
  //
  // Both the receiver function and this code operate on the ring buffer so
  // we must disable interrupts for a little bit.  This is probably overkill
  // as the receiver function adds new packets while this only takes off
  // existing packets.

  disable_ints( );
  uint8_t *packet = Buffer[ Buffer_first ];
  uint16_t packet_len = Buffer_len[ Buffer_first ];
  Buffer_first++;
  if ( Buffer_first == PACKET_RB_SIZE ) Buffer_first = 0;
  enable_ints( );


  #ifndef NOTRACE
  if ( TRACE_ON_DUMP ) {
    uint16_t dumpLen = ( packet_len > PKT_DUMP_BYTES ? PKT_DUMP_BYTES : packet_len );
    TRACE(( "Packet: Received %u bytes, dumping %u\n", packet_len, dumpLen ));
    Utils::dumpBytes( Trace_Stream, packet, dumpLen );
  }
  #endif



  // Packet routing.
  //
  // Bytes 13 and 14 (packet[12] and packet[13]) have the protocol type
  // in them.
  //
  //   Arp: 0806
  //   Ip:  0800
  //
  // Compare 16 bits at a time.  Because we are on a little-endian machine
  // flip the bytes that we are comparing with when treating the values
  // as 16 bit words.  (The registered EtherType word is already flipped
  // to be in network byte order.)

  EtherType protocol = ((uint16_t *)packet)[6];

  for ( uint8_t i=0; i < Packet_EtherTypeHandlers; i++ ) {
    if ( Packet_EtherTypeVal[i] == protocol ) {
      Packet_EtherTypeHandler[i]( packet, packet_len );
      return;
    }
  }

  // If you got here, your handler was not found.  If you registered a default
  // handler it will be called.  Otherwise, we are throwing the packet away.

  if ( Packet_typeUnhandled ) {
    Packet_typeUnhandled( packet, packet_len );
  } else {
    Buffer_free( packet );
  }

  // Be careful with the early returns above ...
}



void Packet_dumpStats( FILE *stream ) {
  fprintf( stream, "Pkt: Sent %lu Rcvd %lu Dropped %lu SndErrs %lu LowFreeBufs %u SndRetries %u\n",
	  Packets_sent, Packets_received, Packets_dropped, Packets_send_errs, Buffer_lowFreeCount, Packets_send_retries );
};





// Packet_getSoftwareInt
//
// Used if you are curious to see which software interrupt the stack was initialized
// with.  I think that only IRCjr cares about this and it really does not need it.

uint8_t  Packet_getSoftwareInt( void ) { return Packet_int; }


// Packet_getHandle
//
// Packet_getHandle is not really interesting unless you are writing code that
// needs to talk to the packet driver directly.  Right now, aside from this code
// only pkttool.cpp needs to do this.

uint16_t Packet_getHandle( void ) { return Packet_handle; }
