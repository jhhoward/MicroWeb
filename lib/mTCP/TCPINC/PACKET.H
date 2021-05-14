/*

   mTCP Packet.H
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


   Description: Packet driver buffer handling and packet driver
     interfacing code

   Changes:

   2011-05-27: Initial release as open source software
   2014-05-18: Add static asserts for configuration #defines
   2015-01-10: Changes to decouple the packet layer from the higher layers

*/


#ifndef _PACKET_H
#define _PACKET_H



// Load and check the configuration options

#include CFG_H
#include "types.h"

// The number of packet buffers is limited by the amount of memory you can
// allocate with malloc.
//
// The packet buffer minimum length is based on SLIP connections with emulated Ethernet.
// We want that MSS to be 256 + 40 bytes for IP and TCP headers, and 14 bytes for the
// emulated Ethernet header.  Less might work but do your research; at this minimum you
// are already in danger of fragmenting DNS packets.

static_assert( PACKET_BUFFERS > 4 );
static_assert( PACKET_BUFFERS <= 42 );
static_assert( PACKET_BUFFER_LEN <= 1514 );
static_assert( PACKET_BUFFER_LEN >= 310 );
static_assert( PKT_DUMP_BYTES <= PACKET_BUFFER_LEN );





// Packet driver and Buffer management
//
// This header file describes the code that interfaces directly with the
// packet driver.
//
// The first major chunk of code is the code that talks to the packet driver.
// This is generally done by software interrupt.  There are functions to
// register with the packet driver, get the MAC address, send a packet, unload,
// etc.  There is also code that the packet driver will call when it receives
// a packet from the wire.
//
// The second chunk of code deals with receive buffer management.  Send
// buffers and receive buffers are handled differently; this code does not
// manage the buffers used to send data.  When the packet driver receives
// a packet from the wire it makes two calls; one to get a buffer to copy
// the packet into, and one to tell this code that the copy is done.  If
// no buffers are available to give to the packet driver you can tell it
// that, but that will cause the packet driver to drop the packet into
// the bit bucket.  So this is to be avoided.



// Buffer management
//
// These functions are used for setting up and controlling the pool of buffers
// that the packet driver will use when a packet is received.  The number of
// buffers to create and their size is set in the PACKET_BUFFERS and
// PACKET_BUFFER_LEN defines
//
// Buffer_init: Allocate the buffers and initialize the data structures.  The
//   data structures are a free buffer stack and a ring buffer of buffers
//   that will hold any new data from the packet driver that needs to be
//   processed.  Returns 0 if all went well, non-zero if an error happened.
//
// Buffer_startReceiving: When first initialized the buffers are created
//   but we set up the data structures such that if the packet driver asks
//   for a buffer we tell it none are available.  This allows us to setup
//   the higher levels of the network stack and make sure everything is
//   ready before allowing packets to start being processed.  When you are
//   ready call this function and then the packet driver will be able
//   to receive data into buffers.
//
// Buffer_free: Higher levels of the stack (ARP, IP, etc.) use this function
//   to return a buffer to the free pool so it can be reused again.
//
// Buffer_stopReceiving: The opposite of startReceiving.  This tells the
//   buffer layer to lie to the packet driver by saying there are no
//   free buffers available.  This is the first step toward unloading
//   everything.
//
// Buffer_stop: Tear everything down and release the memory.  (Make sure
//   no buffers are still in use before calling this.  This should be the
//   last thing that you do; see Utils::endStack( ) to see an example of
//   how to shut things down safely.

extern int8_t Buffer_init( void );
extern void   Buffer_startReceiving( void );
extern void   Buffer_free( const uint8_t *buffer );
extern void   Buffer_stopReceiving( void );
extern void   Buffer_stop( void );


// Stats
//
// If your machine is too slow or you are not servicing the packets fast enough
// you can run out of free buffers to receive new data into and cause new
// incoming packets to be dropped in the bit bucket.  This variable serves as a
// "low water" mark so that you know how close to running out of buffers your
// code has come.  If this hits zero you might need to add more buffers or
// make your code faster.
//
extern uint8_t Buffer_lowFreeCount;


// These need to be visible for the PACKET_PROCESS_SINGLE macro (and variants)
// but really should not be touched by anything except the code in packet.cpp.

extern uint8_t  Buffer_first;
extern uint8_t  Buffer_next;


#define PACKET_RB_SIZE (PACKET_BUFFERS+1)




// Packet driver interfacing
//
// From the packet driver specification version 1.09:
//
// 1  BAD_HANDLE       Invalid handle number
// 2  NO_CLASS         No interfaces of specified class found
// 3  NO_TYPE          No interfaces of specified type found
// 4  NO_NUMBER        No interfaces of specified number found
// 5  BAD_TYPE         Bad packet type specified
// 6  NO_MULTICAST     This interface does not support multicast
// 7  CANT_TERMINATE   This packet driver cannot terminate
// 8  BAD_MODE         An invalid receiver mode was specified
// 9  NO_SPACE         Operation failed because of insufficient space
// 10 TYPE_INUSE       The type had previously been accessed, and not released
// 11 BAD_COMMAND      The command was out of range, or not implemented
// 12 CANT_SEND        The packet couldn't be sent (usually hardware error)
// 13 CANT_SET         Hardware address couldn't be changed (more than 1 handle open)
// 14 BAD_ADDRESS      Hardware address has bad length or format
// 15 CANT_RESET       Couldn't reset interface (more than 1 handle open)


// Useful constants

#define PKTDRV_BASIC                       (1)
#define PKTDRV_BASIC_EXTENDED              (2)
#define PKTDRV_BASIC_HIGH_PERF             (5)
#define PKTDRV_BASIC_HIGH_PERF_EXTENDED    (6)
#define PKTDRV_NOT_INSTALLED             (255)

typedef struct {
  uint32_t packets_in;
  uint32_t packets_out;
  uint32_t bytes_in;
  uint32_t bytes_out;
  uint32_t errors_in;
  uint32_t errors_out;
  uint32_t packets_lost;
} PacketStats_t;




// Packet driver
//
// This code registers with the packet driver and tries to take every possible
// Ethernet frame type (EtherType) for itself.  The only way this would cause
// a conflict is if you had a TSR tht was already using the packet driver.
// That could only work if that TSR did not try to handle IP or ARP frames;
// fix this code if it is a problem for you.
//
// Once initialized, you have to tell this code (packet.cpp) what the handlers
// are for each EtherType and/or provide a default handler for anything you
// did not explicitly provide a handler for.  If you do not provide a default
// the offending packets are just tossed away.  Providing a default would allow
// you to use some generic code to inspect their contents.
//
// Our lowest level packet driver interface code tells the packet driver that it
// is going to handle all EtherTypes.  You need to tell the packet driver
// interface code where the handlers for each EtherType are, or provide a default
// handler.  (If you don't provide a default handler, unmatched/handled packets
// are just tossed away.)
//
// This code uses some self modifying code to speed the process of sending a
// packet.  The software interrupt instruction is modified to reflect the
// software interrupt that is being used.  As a result, this code can only be
// used with one active packet driver at a time because the software interrupt
// can only point at one packet driver at a time.  Self modifying code is ugly
// but the library overhead for not having it was horrible.
//
// Use Packet_init to setup communications with the packet driver.
//     Packet_registerEtherType to register a handler for a specific etherType
//     Packet_registerDefault to register a default handler for other etherTypes
//     Packet_send_pkt to send packets on the wire
//     Packet_release_type to unhook from the packet driver
//     Packet_get_addr to get our Ethernet MAC address
//     Packet_dumpStats to dump some crude statistics
//     Packet_getHandle to see what handle the packet driver assigned us
//     Packet_getSoftwareInt to see what software interrupt we are using

extern int8_t   Packet_init( uint8_t softwareInt );
extern int8_t   Packet_registerEtherType( EtherType val, void (*f)(uint8_t *packet, uint16_t len) );
extern void     Packet_registerDefault( void (*f)(uint8_t *packet, uint16_t len) );
extern void     Packet_send_pkt( void *buffer, uint16_t bufferLen );
extern int8_t   Packet_release_type( void );
extern void     Packet_get_addr( uint8_t *target );
extern void     Packet_dumpStats( FILE *stream );
extern uint8_t  Packet_getSoftwareInt( void );
extern uint16_t Packet_getHandle( void );


// Needs to be visible for the PACKET_PROCESS_SINGLE macro (and variants).
// You can call it directly if you want but the macros are better.
//
void Packet_process_internal( void );


// Packet drivers put "PKT_DRVR" in a certain spot so that you can scan the
// interrupt vectors and find loaded packet drivers.  This is a pointer to
// a string that this code uses during the scan.  It is extern so that the
// pkttool program can use it too.
//
extern const char * PKT_DRVR_EYE_CATCHER;


// Stats - collected during the life of the program.  (The packet driver also
// keeps stats which are collected during the life of the packet driver.)
//
extern uint32_t Packets_dropped;
extern uint32_t Packets_received;
extern uint32_t Packets_sent;
extern uint32_t Packets_send_errs;
extern uint32_t Packets_send_retries;




#endif
