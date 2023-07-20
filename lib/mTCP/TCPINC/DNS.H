/*

   mTCP Dns.H
   Copyright (C) 2008-2023 Michael B. Brutman (mbbrutman@gmail.com)
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
static_assert( DNS_MAX_DOMAIN_LEN >= 30 );
static_assert( DNS_MAX_DOMAIN_LEN <= 120 );
static_assert( DNS_MAX_ENTRIES  >=   1 );
static_assert( DNS_MAX_ENTRIES  <= 127 );  // Only 7 bits ...
static_assert( DNS_HANDLER_PORT !=   0 );
static_assert( DNS_INITIAL_SEND_TIMEOUT >=   100ul );
static_assert( DNS_INITIAL_SEND_TIMEOUT <=  2000ul );
static_assert( DNS_RETRY_THRESHOLD      >=   500ul );
static_assert( DNS_RETRY_THRESHOLD      <=  4000ul );

#ifndef DNS_TIMEOUT_IS_VAR
static_assert( DNS_TIMEOUT              >=  5000ul );
static_assert( DNS_TIMEOUT              <= 20000ul );
#endif



// Continue with other includes

#include <time.h>

#include "udp.h"



#ifdef DNS_ITERATIVE
#define DNS_NAME_STACK (12)
#endif




// DNS rules
//
// A label is from 1 to 63 characters.  (0 is legal, but only for the root.)
// A label consists of letters, numbers, and the dash character.
// A label is not case sensitive.
//
// Messages using UDP are restricted to 512 bytes, not including the IP or UDP
// headers.  This is to ensure no fragmentation.

// Domain name
//
// If a domain name string is provided it will be appended to single
// label (not fully qualified) searches, effectively making those searches
// fully qualified.


// DNS opCodes
//
// 0 Query (standard query)
// 1 iQuery (obsolete)
// 2 Status (server status)
// 3 Reserved
// 4 Notify (server to server)
// 5 Update (Dynamic DNS support, RFC 2136)





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
    //
    // Note that the zero field is fine, but newer implementations
    // have added "Authentic Data" and "Checking Disabled" bits there.
    // We don't use those bits so we still have 3 bits for zero.
    uint8_t     responseCode:4;
    uint8_t     zero:3;
    uint8_t     recursionAvailable:1;

    uint16_t    numQuestions;
    uint16_t    numAnswers;
    uint16_t    numAuthority;
    uint16_t    numAdditional;

    uint8_t     data[512];
};



// DNS Response codes (for responseCode field)

typedef enum {
  UnknownError   = -2,  // mTCP special: Unknown error
  Timeout        = -1,  // mTCP special: Timeout
  Good           =  0,  // Good
  FormatError    =  1,  // Format error on query
  ServerFailed   =  2,  // Server failed to complete the query
  NameError      =  3,  // Name error: name does not exist in the domain
  NotImplemented =  4,  // Not implemented: server doesn't handle this query
  Refused        =  5,  // Refused: server refused for policy reasons
  YXDomain       =  6,  // YX Domain: a name exists when it should not
  YXRRSet        =  7,  // YX RR Set: A RR set exists that should not
  NXRRSet        =  8,  // NX RR Set: A RR set that should exist does not
  NotAuth        =  9,  // Not Auth: Server is not authoritative
  NotZone        = 10   // Not Zone: Name in the message is not within the zone specified
} DNS_Response_Code_t;




// Dns class overview
//
// * To resolve an address call resolve.
// * If the name is not immediately known, then loop
//   until a response is received or you get tired of waiting.
// * While responses can be cached, there can only be one
//   query pending at a time.


// Recursive vs. Iterative queries
//
// The default for mTCP is to require the DNS server to handle recursive
// queries.  A full featured DNS resolver that works iteratively is not
// trivial to write, and takes up a lot of resources too.
//
// However, if you insist, there is a crude implementation that handles
// iterative queries.  I mean, it's really crude.  Disgusting even.
// But it can work.  Sometimes.  It's only enabled for DNSTEST.  In the
// future I'll make it better, but right now it's basically an experiment
// and it works better than the previous version.

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
      char originalTarget[DNS_MAX_NAME_LEN]; // Preserve this to put in the cache.
      IpAddr_t nsIpAddr;

      #ifndef DNS_ITERATIVE
      char targetName[DNS_MAX_NAME_LEN];     // Single target name to resolve
      #else
      char nameStack[DNS_NAME_STACK][DNS_MAX_NAME_LEN];  // A stack of targets to resolve
      int si;
      #endif

    } DNS_Pending_Rec_t;


    static void   sendRequest( IpAddr_t resolver, const char *target, uint16_t ident );
    static void   drivePendingQuery1( void );
    static void   drivePendingQuery2( void );
    static void   addOrUpdate( const char *targetName, IpAddr_t addr );
    static int8_t find( const char *name );

    static void udpHandler(const unsigned char *packet, const UdpHeader *udp );

    static DNS_Rec_t dnsTable[ DNS_MAX_ENTRIES ];
    static uint8_t entries;

    static uint8_t queryPending;             // Set if we are in a query
    static DNS_Response_Code_t lastQueryRc;  // RC of last query
    static DNS_Pending_Rec_t pendingQuery;

    static uint16_t handlerPort;

    static void scanHostsFile( const char *target1,
                               const char *target2,
                               IpAddr_t result );

  public:

    static int8_t init( void );
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
    static inline DNS_Response_Code_t getQueryRc( void ) { return lastQueryRc; }

    // User needs to call this if a query is pending
    static inline void drivePendingQuery( void ) {
      if ( queryPending ) drivePendingQuery1( );
    }

    static void flushCache( void );
    static void deleteFromCache( const char *target );

    static IpAddr_t NameServer;
    static char Domain[];

    static char HostsFilename[];

};


#endif
