/*

   mTCP Ip.H
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


   Description: IP data structures and functions

   Changes:

   2011-05-27: Initial release as open source software
   2014-05-19: Add some static checks to the configuration options
   2015-02-07: Change MyIpAddr_u and Netmask_u to be host order to
               save some code space and speed things up.

*/


#ifndef _IP_H
#define _IP_H



// Load and check the configuration options

#include CFG_H
#include "Utils.h"

// IP packets can be as large as 64KB.  Standard Ethernet MTU is 1500; anything
// larger than that falls into jumbo frame category.  This code is not tested
// with jumbo frames, but it should work.
//
// The number of fragmented packets you can reassemble is limited by the
// maximum size of the packets.  Everything has to fit inside of a 64K segment
// so don't go nuts.  The IP_BIGPACKET_SIZE does not include the IP header; it
// is just a payload number, which is why it is not 1500 even.
//
// Note that all of these only apply if fragments are turned on.  Otherwise,
// IP has no configuration defines.

#ifdef IP_FRAGMENTS_ON
static_assert( IP_MAX_FRAG_PACKETS >= 1 );
static_assert( IP_MAX_FRAG_PACKETS <= 8 );
static_assert( IP_MAX_FRAGS_PER_PACKET >=  2 );
static_assert( IP_MAX_FRAGS_PER_PACKET <= 16 );
static_assert( IP_BIGPACKET_SIZE >= 1480 );
static_assert( IP_BIGPACKET_SIZE <= 8192 );
static_assert( IP_FRAG_REASSEMBLY_TIMEOUT >= 2000ul );
static_assert( IP_FRAG_REASSEMBLY_TIMEOUT <= 8000ul );
#endif







#define IP_PROTOCOL_ICMP  (1)
#define IP_PROTOCOL_TCP   (6)
#define IP_PROTOCOL_UDP  (17)

#define IP_FLAGS_DNF 0x0040


extern char     MyHostname[];
extern IpAddr_t MyIpAddr;
extern IpAddr_t Gateway;
extern IpAddr_t Netmask;


// Cached versions of variables above.  Note that these are not in network
// byte order!  This is for performance; as long as you use the methods
// to set MyIpAddr and Netmask you will be fine.

extern uint32_t MyIpAddr_u;
extern uint32_t Netmask_u;



extern IpAddr_t IpBroadcast;
extern IpAddr_t IpThisMachine;


#ifdef __TURBOC__
extern "C" uint16_t ipchksum( uint16_t far *data, uint16_t len );
extern "C" uint16_t ip_p_chksum( IpAddr_t src, IpAddr_t target, uint16_t *udpPacket, uint8_t protocol, uint16_t len );
#else
extern "C" uint16_t cdecl ipchksum( uint16_t far *data, uint16_t len );
extern "C" uint16_t cdecl ip_p_chksum( IpAddr_t far src, IpAddr_t far target, uint16_t far *data, uint8_t protocol, uint16_t len );

// len is the header length; len2 is the data length
extern "C" uint16_t cdecl ip_p_chksum2( IpAddr_t far src, IpAddr_t far target, uint16_t far *data, uint8_t protocol, uint16_t len, uint16_t far *data2, uint16_t len2 );
#endif



class IpHeader {

  public:

    uint8_t  versHlen;       // vers:4, Hlen:4
    uint8_t  service_type;
    uint16_t total_length;

    // Fragmentation support
    //   flags 0 to 15
    //   0: always 0
    //   1: 0=May Fragment, 1=Don't Fragment
    //   2: 0=Last Fragment, 1=More Fragments
    //   3 to 15: Fragment offset in units of 8 bytes

    uint16_t ident;
    uint16_t flags;          // flags:3, frag_offset:13

    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t chksum;

    uint8_t ip_src[4];
    uint8_t ip_dest[4];

    inline void setIpHlen( uint8_t len ) {
      versHlen = (0x40 | (len>>2) );
    }

    inline uint16_t getIpHlen( void ) const {
      return (versHlen & 0xF) << 2;
    }


    // If the more fragments bit is zero and the fragement offset is
    // zero, then the packet is not a fragment.  Otherwise, it is.

    inline uint8_t isFragment( void ) {
      return (flags & 0xFF3F) != 0;  // Byte order is backwards, we're on x86
    }

    inline uint8_t isLastFragment( void ) {
      return (flags & 0x0020) == 0;  // Byte order is backwards, we're on x86
    }

    inline uint16_t fragmentOffset( void ) {
      uint16_t tmp = ntohs( flags );
      return (tmp & 0x1FFF) << 3;
    }

    void setFlags( uint8_t p_flags ) {
      flags = ntohs( flags );
      flags = ((p_flags & 0x7) << 13) | (flags & 0x1FFF);
      flags = htons( flags );
    }

    void setFragOffset( uint16_t p_offset ) {
      flags = ntohs( flags );
      flags = (flags & 0xE000) | ((p_offset>>3) & 0x1FFF);
      flags = htons( flags );
    }

    inline uint8_t *payloadPtr( void ) const {
      return (uint8_t *)((uint8_t *)(this) + this->getIpHlen( ));
    }

    inline uint16_t payloadLen( void ) const {
      return ntohs(total_length) - getIpHlen( );
    }

    void set( uint8_t protocol, const IpAddr_t dstHost, uint16_t payloadLen, uint8_t moreFrags, uint16_t fragOffset );

    int8_t setDestEth( EthAddr_t ethTarget );

    static uint16_t IpIdent;
};





class Ip {

  public:

    // Statistics

    static uint32_t icmpRecvPackets;
    static uint32_t ptrWrapCorrected;
    static uint32_t badChecksum;
    static uint32_t unhandledProtocol;
    static uint32_t fragsReceived;

    #ifdef IP_FRAGMENTS_ON
    static uint32_t goodReassemblies;
    static uint32_t timeoutReassemblies;
    static uint32_t notEnoughSlots;
    static uint32_t tooManyInFlight;
    static uint32_t payloadTooBig;
    #endif


    // Utilities 

    static inline int isSame( const IpAddr_t a, const IpAddr_t b ) {
      // return ( (a[0]==b[0]) && (a[1]==b[1]) && (a[2]==b[2]) && (a[3]==b[3]) );
      return *((uint32_t *)a) == *((uint32_t *)b);
    }

    static inline void copy( IpAddr_t target, const IpAddr_t source ) {
      //target[0] = source[0]; target[1] = source[1]; target[2] = source[2]; target[3] = source[3];
      *((uint32_t *)target) = *((uint32_t *)source);
    };


    // Setters for MyIpAddr and Netmask; these keep the cached _u versions in sync.

    static inline void setMyIpAddr( uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4 ) {
      MyIpAddr[0] = o1; MyIpAddr[1] = o2; MyIpAddr[2] = o3; MyIpAddr[3] = o4;
      MyIpAddr_u = *(uint32_t *)(MyIpAddr);
    }

    static inline void setMyIpAddr( const IpAddr_t source ) {
      *((uint32_t *)MyIpAddr) = *((uint32_t *)source);
      MyIpAddr_u = *(uint32_t *)(source);
    }

    // The incoming parameter is in host order, not network byte order!
    static inline void setMyIpAddr( uint32_t newAddr_u ) {
      *(uint32_t *)(MyIpAddr) = newAddr_u;
      MyIpAddr_u = newAddr_u;
    }

    static inline void setMyNetmask( uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4 ) {
      Netmask[0] = o1; Netmask[1] = o2; Netmask[2] = o3; Netmask[3] = o4;
      Netmask_u = *(uint32_t *)(Netmask);
    }

    static inline void setMyNetmask( const IpAddr_t source ) {
      *((uint32_t *)Netmask) = *((uint32_t *)source);
      Netmask_u = *(uint32_t *)(source);
    }


    static void process( uint8_t *packet, uint16_t packetLen );

    static void dumpStats( FILE *stream );


    #ifdef IP_FRAGMENTS_ON

    static int  initForReassembly( void );     // Used during initstack
    static void reassemblyStop( void );        // Used during shutdown

    static void returnBigPacket( uint8_t *targetPacket );

    static void purgeOverdue( void );          // If anything is being reassembled check for timeouts

    static uint8_t *ipReassemblyMemoryStart;   // Memory region for bigPackets start
    static uint8_t *ipReassemblyMemoryEnd;     // Memory region for bigPackets stop
    static uint8_t  fragsInReassembly;         // Only tracks partially assembled fragments

    inline static int isIpBigPacket( const uint8_t *packet ) {
      return ( (FP_SEG(packet) == FP_SEG(ipReassemblyMemoryStart)) &&
           ( (FP_OFF(packet) >= FP_OFF(ipReassemblyMemoryStart)) && (FP_OFF(packet) < FP_OFF(ipReassemblyMemoryEnd)) ) );
    }
    


    #endif
};



//-----------------------------------------------------------------------------
//
// ICMP

#ifdef COMPILE_ICMP

static_assert( ICMP_ECHO_OPT_DATA >= 32 );
static_assert( ICMP_ECHO_OPT_DATA <= 256 );


#include "Eth.h"


#define ICMP_ECHO_REPLY   (0)
#define ICMP_ECHO_REQUEST (8)

class IcmpHeader {

  public:

    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;

    inline uint8_t *payloadPtr( void ) const {
      return (uint8_t *)((uint8_t *)(this) + 4);
    }

};


typedef struct {
  EthHeader  eh;
  IpHeader   ip;
  IcmpHeader icmp;
  uint16_t   ident;
  uint16_t   seq;
  uint8_t    data[ ICMP_ECHO_OPT_DATA ];
} IcmpEchoPacket_t;



class Icmp {

  public:

    static void init( void );
    static void near process( uint8_t *packet, IpHeader *ip );

    // Optional user provided function.  Used by Ping.
    static void (*icmpCallback)(const unsigned char *packet,
                                const IcmpHeader *icmp );

    // Keep one IcmpEchoPacket around for replying to Icmp Echo requests.
    static IcmpEchoPacket_t icmpEchoPacket;
};




#endif


#endif
