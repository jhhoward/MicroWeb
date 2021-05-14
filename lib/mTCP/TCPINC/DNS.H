/*

   mTCP Dns.H
   Copyright (C) 2008-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: DNS data structures and functions

   Changes:

   2011-05-27: Initial release as open source software
   2014-05-19: Add some static checks to the configuration options

*/


#ifndef _DNS_HEADER_H
#define _DNS_HEADER_H



// Load and check the configuration options first

#include CFG_H
#include "types.h"

static_assert( DNS_MAX_NAME_LEN >=  64 );
static_assert( DNS_MAX_NAME_LEN <= 192 );
static_assert( DNS_MAX_ENTRIES  >=   1 );
static_assert( DNS_MAX_ENTRIES  <= 127 );  // Only 7 bits ...
static_assert( DNS_HANDLER_PORT !=   0 );
static_assert( DNS_INITIAL_SEND_TIMEOUT >=   100ul );
static_assert( DNS_INITIAL_SEND_TIMEOUT <=  2000ul );
static_assert( DNS_RETRY_THRESHOLD      >=   500ul );
static_assert( DNS_RETRY_THRESHOLD      <=  4000ul );

#ifndef DNS_RECURSION_DESIRED_IS_VAR
static_assert( (DNS_RECURSION_DESIRED == 0) || (DNS_RECURSION_DESIRED == 1) );
#endif

#ifndef DNS_TIMEOUT_IS_VAR
static_assert( DNS_TIMEOUT              >=  5000ul );
static_assert( DNS_TIMEOUT              <= 20000ul );
#endif



// Continue with other includes

#include <time.h>

#include "udp.h"




// DNS Response codes (for responseCode field)
//
//  0 Good
//  1 Format error
//  2 Server fail
//  3 Name error
//  4 Not implemented
//  5 Refused
//  6 YX Domain
//  7 YX RR Set
//  8 NX RR Set
//  9 Not Auth
// 10 Not Zone


class DNSpacket {

  public:

    UdpPacket_t udpHdr; // Space for Ethernet, IP and UDP headers

    uint16_t    ident;

    // Bit fields are backwards on this compiler!
    uint8_t     recursionDesired:1;
    uint8_t     truncationFlag:1;
    uint8_t     authoritativeAnswer:1;
    uint8_t     opCode:4;
    uint8_t     qrFlag:1;

    // Bit fields are backwards on this compiler!
    uint8_t     responseCode:4;
    uint8_t     zero:3;
    uint8_t     recursionAvailable:1;


    uint16_t    numQuestions;
    uint16_t    numAnswers;
    uint16_t    numAuthority;
    uint16_t    numAdditional;

    uint8_t     data[512];
};





class Dns {

  private:


    typedef struct {
      char     name[DNS_MAX_NAME_LEN]; // ASCIIZ name of the target
      IpAddr_t ipAddr;                 // IP Address of the target
      time_t   updated;                // Time added
    } DNS_Rec_t;

    typedef struct {
      uint16_t ident;                        // Unique id for this request
      clockTicks_t start;                    // Time started
      clockTicks_t lastUpdate;               // Last activity
      char     name[DNS_MAX_NAME_LEN];       // ASCIIZ name of target
      IpAddr_t ipAddr;                       // IP Address of the target
      char     canonical[DNS_MAX_NAME_LEN];  // Canonical answer
      char     nameServer[DNS_MAX_NAME_LEN]; // ASCIIZ name of next NS
      IpAddr_t nameServerIpAddr;             // IP address of next NS
      char     prevNS[DNS_MAX_NAME_LEN];
      IpAddr_t prevNSIpAddr;
    } DNS_Pending_Rec_t;


    static void   sendRequest( IpAddr_t resolver, const char *target, uint16_t ident );
    static void   drivePendingQuery2( void );
    static void   addOrUpdate( char *targetName, IpAddr_t addr );
    static int8_t find( const char *name );

    static void udpHandler(const unsigned char *packet,
		       const UdpHeader *udp );


    static DNS_Rec_t dnsTable[ DNS_MAX_ENTRIES ];
    static uint8_t entries;

    static uint8_t queryPending;            // Set if we are in a query
    static int8_t lastQueryRc;              // RC of last query
    static DNS_Pending_Rec_t pendingQuery;

    static uint16_t handlerPort;

  public:

    static int8_t init( uint16_t handlerPort );
    static void   stop( void );

    // High level function to resolve a name
    // -1: Parm error (name too long)
    //  0: Was in the cache; addr returned
    //  1: Sent request; check back later
    //  2: Busy with another request
    //  3: Not in cache, and no req sent because user said not to
    static int8_t resolve( const char *name, IpAddr_t ipAddr, uint8_t sendReq );


    // Find out if a query is pending
    static inline uint8_t isQueryPending( void ) { return queryPending; }
    static inline int8_t  getQueryRc( void ) { return lastQueryRc; }

    // User needs to call this if a query is pending
    static void   drivePendingQuery( void );


    static void dumpTable( void );

    static IpAddr_t NameServer;

};


#endif
