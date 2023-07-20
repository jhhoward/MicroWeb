/*

   mTCP Dns.cpp
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


   Description: DNS resolving functions.  UDP only for now.

   Changes:

   2011-05-27: Initial release as open source software
   2013-03-23: Get rid of some duplicate strings
   2022-03-18: Add support for a hosts file
   2022-03-22: Rewrite the handler to better handle iterative queries
   2022-03-25: Refactor so recursive queries are the default, while
               leaving the messy iterative queries confined to DNSTEST.

*/


#include <conio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#include "dns.h"
#include "timer.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"

#ifdef COMPILE_TCP
#include "tcp.h"
#endif




// Global configuration parameters

IpAddr_t Dns::NameServer = { 0, 0, 0, 0 };
char     Dns::Domain[DNS_MAX_DOMAIN_LEN] = "";
char     Dns::HostsFilename[DOS_MAX_PATHFILE_LENGTH] = "";


// Pending query data structure.  // (Only one query may be pending at a time.)

uint8_t Dns::queryPending = 0;
DNS_Response_Code_t  Dns::lastQueryRc = Good;
Dns::DNS_Pending_Rec_t Dns::pendingQuery;



// DNS cache table data and functions

Dns::DNS_Rec_t Dns::dnsTable[ DNS_MAX_ENTRIES ];
uint8_t Dns::entries = 0;


void Dns::flushCache( void ) {
  // Brutal, but effective.
  entries = 0;
  TRACE_DNS(( "Dns: Cache flushed\n" ));
}


int8_t Dns::find( const char *name ) {

  int8_t rc = -1;

  for ( uint8_t i=0; i < entries; i++ ) {
    if ( stricmp( dnsTable[i].name, name ) == 0 ) {
      rc = i;
      break;
    }
  }

  return rc;
}


void Dns::addOrUpdate( const char *targetName, IpAddr_t addr ) {

  // See if we have this name first
  int8_t index = find( targetName );

  if ( index > -1 ) {
    // Update existing entry
    Ip::copy( dnsTable[index].ipAddr, addr );
  }
  else {

    // Can we make a new entry?
    if ( entries < DNS_MAX_ENTRIES ) {
      index = entries;
      entries++;
    }
    else {

      // Knock the oldest out
      index = 0;
      for ( uint8_t i=1; i < entries; i++ ) {
        if ( dnsTable[i].updated < dnsTable[index].updated ) {
          index = i;
        }
      }

    }

    // Add (or overlay the oldest)
    strcpy( dnsTable[index].name, targetName );
    Ip::copy( dnsTable[index].ipAddr, addr );
  }

  dnsTable[index].updated = time( NULL );
}


void Dns::deleteFromCache( const char *targetName ) {

  int8_t index = find( targetName );
  if ( index == -1 ) return;

  uint8_t last = entries - 1;
  dnsTable[index] = dnsTable[last];

  entries--;
}




// DNS_ITERATIVE
//
// DNS queries can either request recursion from the server or be iterative.
// A recursive query is very easy for the client; the server does all of the
// hard work, but not all servers allow it.  An iterative query can give you
// authoritative answers, but it requires the client to keep issuing new
// queries as it works its way closer to the final authoritative nameserver.
//
// We generally want people to use a public DNS service run by their ISP,
// Google, Cloudflare, or whoever that handles recursive queries.  This keeps
// the required client code small, simple and reliable.  The default for mTCP
// queries has always been to use recursive queries, except for the DNSTest
// program.
//
// DNSTest could be told to do iterative queries, but that code was very
// limited and effectively broken - it always assumed that the server would
// point it to the next nameserver, with an address too.  Having a nameserver
// in .COM for a .ORG address would kill it.  But it worked in simple cases.
//
// In this version of the code I've hacked in a crude fix that should work
// for almost any query.  The code keeps a stack of nameservers that need
// to be resolved, and when you get a new nameserver that does not have
// a glue record to give you the IP address it starts at the top of the DNS
// tree again.  This is wasteful but it works and it's much easier than
// doing the correct algorithm, which I will get to some day.  Aggressive
// caching is also used, assuming that some of the higher level nameservers
// might get used more than once.
//
// Only DNSTest is allowed to use iterative queries, as it's just an
// experiment anyway.  Every other mTCP program will use recursive queries.

#ifdef DNS_ITERATIVE
// Used for EDNS0 larger UDP buffer sizes.  DNS tries to avoid
// fragments by keeping the default response size no more than
// 512 bytes.  EDNS0 allows us to lift that; we are basing it
// on our MTU.

static int16_t UDP_max_response = 512;
#endif




// Init:
//
//  0 is good, negative numbers are bad.
// -1 Error
//
// If the UDP port for DNS is not available you get a hard error.
// If the NameServer is not set we can continue, but we had better
// be given numerical IP addresses.
//
// After init no entries are in the table and no queries are pending.
// Nothing needs to be explicitly initialized because they are static
// and will be set to 0 at program startup.

int8_t Dns::init( void ) {

  if ( Ip::isSame(NameServer, IpInvalid) ) {
    TRACE_DNS_WARN(( "Dns: NameServer not set\n" ));
  }

  int8_t rc = Udp::registerCallback( DNS_HANDLER_PORT, &Dns::udpHandler );

  #ifdef DNS_ITERATIVE
  // EDNS0 support - set the maximum UDP response we think we can receive
  // without fragmentation.  Allowing for larger responses helps us read
  // more glue records that might have useful IP addresses in them.
  if ( MyMTU > 576 ) {
    UDP_max_response = MyMTU - 20 - 8;
  }
  #endif

  return rc;
}


void Dns::stop( void ) {
  Udp::unregisterCallback( DNS_HANDLER_PORT );
}





// Resolve
//
// Cheap hack alert - if we think that you passed in a numeric IP address
// we report that it is in the cache and just return it back to you.
//
// Use lower-case names - we are case sensitive.
// If you want a request generated, pass a 1 to sendReq.
//
//  0: Was resolved. (Numerical addr, hosts file or cache); addr returned
//  1: Sent request to the nameserver; check back later
//  2: Busy with another request
//  3: Not resolved and no request sent because user said not to
//
// -1: Name too long
// -2: No NameServer set
// -3: Bad input

static char fullServerName[DNS_MAX_NAME_LEN];

int8_t Dns::resolve( const char *serverName, IpAddr_t target, uint8_t sendReq ) {

  if ( *serverName == 0 ) return -3;

  // Is it an IP address?

  uint8_t allNumeric = 1;
  uint8_t len = strlen( serverName );
  for ( uint8_t i=0; i < len; i++ ) {
    if ( !(isdigit( serverName[i]) || serverName[i]=='.') ) {
      allNumeric = 0;
      break;
    }
  }

  if ( allNumeric ) {
    uint16_t tmp1, tmp2, tmp3, tmp4;
    int rc = sscanf( serverName, "%d.%d.%d.%d", &tmp1, &tmp2, &tmp3, &tmp4 );
    if ( rc == 4 ) {
      target[0] = tmp1;
      target[1] = tmp2;
      target[2] = tmp3;
      target[3] = tmp4;
      return 0;
    }
  }

  if ( Ip::isSame(NameServer, IpInvalid) ) {
    return -2;  // No nameserver set.
  }

  if ( len >= DNS_MAX_NAME_LEN-1 ) {
    return -1;  // Name too long.
  }



  // They passed in a name, not an IP address.  Is the name in our cache?

  int8_t index = find( serverName );
  if ( index != -1 ) {
    Ip::copy( target, dnsTable[index].ipAddr );
    return 0;
  }


  // See if this is a single label or a fully qualified name.
  //
  // If it is a single label and we have a domain, append the domain.
  // Otherwise, just search for the single label.  (A single label will work
  // when querying something like a local router that assigns addresses via
  // DHCP.)

  bool dotFound = false;
  for ( uint8_t i=0; i < len; i++ ) {
    if ( serverName[i] == '.' ) {
      dotFound = true;
      break;
    }
  }

  strcpy( fullServerName, serverName );

  if ( (dotFound == false) && (Domain[0]) ) {

    strcat( fullServerName, "." );
    strcat( fullServerName, Domain );

    // We changed the name.  Search the cache again.
    index = find( fullServerName );
    if ( index != -1 ) {
      Ip::copy( target, dnsTable[index].ipAddr );
      return 0;
    }

  }


  // Is it in the host file?
  //
  // We pass in two possible names in case we added a domain to
  // the original label.  This can waste a few cycles if they are the
  // same, but it is better than scanning the file twice.
  //
  // If we do get a hit, cache the original name that was requested
  // because if they query again, that is what is most likely to be
  // passed in.

  scanHostsFile( serverName, fullServerName, target );
  if ( target[0] != 0 ) {
    addOrUpdate( serverName, target );
    return 0;
  }


  if ( queryPending ) {
    return 2;  // Busy with another request

  }

  if ( sendReq == 0 ) return 3;

  // Setup our data strucutures and send a request.

  queryPending = 1;
  lastQueryRc  = Good;  // Not valid until queryPending = 0
  memset( &pendingQuery, 0, sizeof(pendingQuery) );
  pendingQuery.ident = rand( );
  pendingQuery.start = TIMER_GET_CURRENT( );
  pendingQuery.lastUpdate = pendingQuery.start;
  strcpy( pendingQuery.originalTarget, fullServerName );
  Ip::copy( pendingQuery.nsIpAddr, NameServer );

  #ifndef DNS_ITERATIVE
  strcpy( pendingQuery.targetName, fullServerName );
  #else
  strcpy( pendingQuery.nameStack[pendingQuery.si], fullServerName );
  #endif

  sendRequest( NameServer, fullServerName, pendingQuery.ident );

  return 1;
}



// SendRequest
//
// Form a UDP packet and send it.  This does not use any of the pending
// query state directly; everything has to be passed in as a parameter.

void Dns::sendRequest( IpAddr_t resolver, const char *targetName, uint16_t ident ) {

  TRACE_DNS(( "Dns: Query %d.%d.%d.%d for %s\n",
          resolver[0], resolver[1], resolver[2], resolver[3], targetName ));

  DNSpacket query;

  // Encode header

  query.ident = htons( ident );
  query.qrFlag = 0;
  query.opCode = 0;
  query.authoritativeAnswer = 0;
  query.truncationFlag = 0;
  query.recursionDesired = DNS_RECURSION_DESIRED;

  // Although it's legal to set this bit and a server will turn it off if it
  // wants to in the response, I've found root servers that won't even answer
  // with this bit turned on.
  query.recursionAvailable = 0;

  query.zero = 0;
  query.responseCode = 0;

  query.numQuestions = htons(1);
  query.numAnswers = 0;
  query.numAuthority = 0;
  query.numAdditional = 0;


  uint8_t  sublen = 0;                 // Length of each section of the name
  uint8_t *lenByte = &query.data[0];   // Ptr to byte to receive the sublen
  uint8_t *target = &query.data[1];    // Ptr to the byte to get next char

  uint16_t index = 0;                  // Index into the server name
  uint16_t len = strlen( targetName ); // Length of the server name

  // Go until the end of the string is hit
  while ( index < len ) {

    while ( index < len ) {

      if ( targetName[index] == '.' ) {
        *lenByte = sublen;
        lenByte = target;
        sublen = 0;
      }
      else {
        *target = targetName[index];
        sublen++;
      }

      index++;
      target++;
    }

  }

  // Fell out of the bottom ... set the last length byte and end of string
  *lenByte = sublen;
  *target++ = 0;

  *target++ = 0;  // Question Type
  *target++ = 1;
  *target++ = 0;  // Question Class
  *target++ = 1;

  uint8_t reqLen = 12 + len + 2 + 4;


  #ifdef DNS_ITERATIVE
  // Add EDNS0 option for larger UDP responses.  This is based on our
  // MTU size, but that is not guaranteed to avoid IP fragments.
  // We handle some IP fragmentation, so it should be fine.  (Assuming
  // you remembed to compile that in.)
  //
  // (512 bytes in the minimum.  We don't honor anything less than that.)

  if ( UDP_max_response > 512 ) {
    query.numAdditional = htons(1);
    *target++ = 0x00;
    *target++ = 0x00;
    *target++ = 0x29;
    *target++ = UDP_max_response >> 8;
    *target++ = UDP_max_response & 0xFF;
    *target++ = 0x00;
    *target++ = 0x00;
    *target++ = 0x00;
    *target++ = 0x00;
    *target++ = 0x00;
    *target++ = 0x00;

    reqLen += 11;
  }
  #endif


  clockTicks_t startTime = TIMER_GET_CURRENT( );

  int8_t rc = Udp::sendUdp( resolver, DNS_HANDLER_PORT, 53, reqLen, (uint8_t *)&query, 1 );

  if ( rc == -1 ) {
    // Fatal error from UDP.  This probably never happens unless you are
    // low on memory.  If it does happen a debug trace will expose it.
    return;
  }


  // rc will be 0 if the packet was sent or 1 if it is pending
  // ARP resolution.


  // Spin and process packets until we can resolve ARP and send our request
  while ( rc == 1 ) {

    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) >
         TIMER_MS_TO_TICKS( DNS_INITIAL_SEND_TIMEOUT ) )
    {
      TRACE_DNS_WARN(( "Dns: Timeout sending initial request\n" ));
      break;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );

    #ifdef COMPILE_TCP
    Tcp::drivePackets( );
    #endif

    rc = Udp::sendUdp( resolver, DNS_HANDLER_PORT, 53, reqLen, (uint8_t *)&query, 1 );

    if ( rc == -1 ) {
      // This should not happen at this point; it would have been caught
      // up above the first time we send the UDP packet.
      return;
    }

  }

  // Ok, by this point it is gone.
}



static char *decodeName( DNSpacket *qr, char *current, char *nameBuf ) {

  uint8_t tmpNameIndex = 0;
  uint8_t nextLen;

  char *prePtrCurrent = current;
  uint8_t ptrHit = 0;

  // Early short circuit so that we null terminate an empty name
  // correctly.
  if ( *current == 0 ) {
    nameBuf[0] = 0;
    current++;
    return current;
  }

  // Name first
  while ( nextLen = *current ) {

    #ifdef DNS_ITERATIVE
    if ( current - ((char *)qr->data) > UDP_max_response ) {
      TRACE_DNS_WARN(( "Dns: Name decode failed due to truncation\n" ));
      strcpy( nameBuf, "!TRUNCATED" );
      tmpNameIndex = 11;
      break;
    }
    #endif

    current++;

    if ( nextLen > 191 ) { // Pointer
      if ( ptrHit == 0 ) prePtrCurrent = current;
      ptrHit = 1;
      current = (char *)&(qr->data[ (*current) -12 + ((nextLen & 0x3F)<<8) ]);
      continue;
    }

    if ( (tmpNameIndex + nextLen) >= DNS_MAX_NAME_LEN ) {
      // We're going to have a buffer overflow.  Bomb the string so
      // that it is unusable and issue a warning.
      TRACE_DNS_WARN(( "Dns: Name in a response was too long.\n" ));
      strcpy( nameBuf, "!TOO_BIG" );
      tmpNameIndex = 9;
      break;
    }
    else {
      memcpy( nameBuf + tmpNameIndex, current, nextLen );
      tmpNameIndex += nextLen;
      nameBuf[tmpNameIndex] = '.';
      tmpNameIndex++;
    }
    current += nextLen;
  }

  if ( ptrHit ) {
    current = prePtrCurrent;
  }

  current++;
  nameBuf[tmpNameIndex-1] = 0;

  return current;
}









static char *SectionNames[] = { "Unknown", "Answer", "Authority", "Additional" };

#ifdef DNS_ITERATIVE
static char DnsReceivedAddr[] = "Dns:   Good, received addr for nameserver\n";
#endif


// We received a response.  If we got an answer then add it to
// the table.  If we didn't get an answer but they were helpful
// and gave us an address for the nameserver, resend the request
// to the next nameserver.
//
// If they give us the name of the next nameserver but not an
// address for it, we should try to resolve it, cache it, and
// then eventually retry the original request.


void Dns::udpHandler(const unsigned char *packet, const UdpHeader *udp ) {

  DNSpacket *qr = (DNSpacket *)(packet);

  uint16_t ident = ntohs( qr->ident );

  TRACE_DNS(( "Dns: Ident: %04x  Q/R: %d  Opcode: %d  AA: %d  Trun: %d  RA: %d  Rc: %d\n",
              ident, qr->qrFlag, qr->opCode, qr->authoritativeAnswer,
              qr->truncationFlag, qr->recursionAvailable,
              qr->responseCode ));

  uint8_t numQuestions  = ntohs( qr->numQuestions );
  uint8_t numAnswers    = ntohs( qr->numAnswers );
  uint8_t numAuthority  = ntohs( qr->numAuthority );
  uint8_t numAdditional = ntohs( qr->numAdditional );

  TRACE_DNS(( "Dns: Questions: %d  Answers: %d  Authority: %d  Additional: %d\n",
              numQuestions, numAnswers, numAuthority, numAdditional ));


  if ( ident != pendingQuery.ident ) {

    TRACE_DNS_WARN(( "Dns: Ident mismatch: Received ident: %u, should be %u\n", ident, pendingQuery.ident ));

    // Early exit

    // We are done processing this packet.  Remove it from the front of
    // the queue and put it back on the free list.
    Buffer_free( packet );
    return;
  }


  // Print out the fields

  char *current = (char *)&(qr->data[0]);

  char questionName[DNS_MAX_NAME_LEN];
  char tmpName[DNS_MAX_NAME_LEN];


  // Questions
  int i;
  for ( i=0; i < numQuestions; i++ ) {

    current = decodeName( qr, current, questionName );

    uint16_t type = ntohs( *((uint16_t*)current) );
    current += 2;
    uint16_t cls  = ntohs( *((uint16_t*)current) );
    current += 2;

    TRACE_DNS(( "Dns: Question: %s  Type: %d  Class: %d\n", questionName, type, cls ));

  }


  // Used to check for a bad return code here and bug out, but we'll
  // defer that to later now because it is interesting for the trace
  // to see what is in the record.


  #ifdef DNS_ITERATIVE
  bool tryAgain = false;
  bool receivedAnAnswer = false;
  int originalSi = pendingQuery.si;
  #endif


  // Answers=1, Authority=2, Additional=3
  for ( uint8_t j = 1; j < 4; j++ ) {

    uint8_t limit;

    switch ( j ) {
      case 1: limit=numAnswers; break;
      case 2: limit=numAuthority; break;
      case 3: limit=numAdditional; break;
    }

    #ifdef DNS_ITERATIVE
    uint8_t answerToUse = 0;
    if ( limit ) {
      answerToUse = rand( ) % limit;
    }

    if ( (current - (char*)qr->data) > UDP_max_response ) break;
    #endif


    for ( i=0; i < limit; i++ ) {

      #ifdef DNS_ITERATIVE
      if ( (current - (char*)qr->data) > UDP_max_response ) break;
      #endif

      current = decodeName( qr, current, tmpName );

      uint16_t type = ntohs( *((uint16_t*)current) );
      current += 2;
      uint16_t cls  = ntohs( *((uint16_t*)current) );
      current += 2;
      uint32_t ttl  = ntohl( *((uint32_t*)current) );
      current += 4;
      uint16_t rdl  = ntohs( *((uint16_t*)current) );
      current += 2;



      #ifndef NOTRACE

      int sectionNameIndex = j;
      if ( j > 3 ) sectionNameIndex = 0;

      TRACE_DNS(( "Dns: Section: %s   Name: %s\n", SectionNames[sectionNameIndex], tmpName ));
      TRACE_DNS(( "Dns:   Type: %d  Class: %d  TTL: %ld  Len: %d\n", type, cls, ttl, rdl ));

      if ( TRACE_ON_DNS && TRACE_ON_DUMP ) {
        Trace_tprintf( "Dns:   Raw Data:\n" );
        Utils::dumpBytes( Trace_Stream, (uint8_t *)current, rdl );
      }

      #endif

      if ( type == 1 ) { // Received an address

        IpAddr_t *addr = (IpAddr_t *)current;

        TRACE_DNS(( "Dns:   IP Addr received: %d.%d.%d.%d\n", (*addr)[0], (*addr)[1], (*addr)[2], (*addr)[3] ));

        #ifndef DNS_ITERATIVE
        if ( j==1 ) { // Answers section

          if ( stricmp( pendingQuery.targetName, tmpName ) == 0 ) {
            addOrUpdate( pendingQuery.originalTarget, *addr );
            queryPending = 0;
            lastQueryRc = Good;
          }

        }
        #else
        if ( (j==1) || (j==3) ) { // Answers section or Additional info section

          if ( queryPending ) {
            // Cache any address we receive.  But not if we might knock our
            // answer out of the cache.
            addOrUpdate( tmpName, *addr );
            TRACE_DNS(( "Dns:   Added to cache\n" ));
          }


          // Check all of the pending targets to see if this name matches one
          // of them.  If it does, we can truncate the stack.

          for ( int k=0; k <= pendingQuery.si; k++ ) {

            if ( stricmp(tmpName, pendingQuery.nameStack[k]) == 0 ) {

              // Use this to suppress picking another nameserver ..  we have
              // an answer we can use already.
              receivedAnAnswer = true;

              TRACE_DNS(( "Dns:   Addr received for %s, stack#: %d\n", tmpName, k ));

              if ( k == 0 ) {
                addOrUpdate( pendingQuery.originalTarget, *addr );
                queryPending = 0;
                lastQueryRc = Good;
              } else {
                Ip::copy( pendingQuery.nsIpAddr, *addr );
                pendingQuery.si = k-1;
                tryAgain = true;
              }

            }

          }

        }
        #endif

        current += rdl;
      }

      else if ( type == 2 ) { // Name Server

        current = decodeName( qr, current, tmpName );
        TRACE_DNS(( "Dns:   Nameserver: %s\n", tmpName ));

        #ifdef DNS_ITERATIVE
        // We are being given a list of nameservers.  Pick one randomly
        // to avoid stumbling on the same broken machine repeatedly.
        //
        // If we get glue records then we might get an address for this
        // nameserver, which would be great.  If we don't get an address
        // then we have to do another query.
        //
        // If we do get an address, we'll look at all of the pending
        // queries.  If we are lucky, we'll satisfy more than just the
        // request at the top of the stack and save some work.

        if ( (receivedAnAnswer == false) && (i == answerToUse) ) {

          TRACE_DNS(( "Dns:  *Using this nameserver\n" ));

          // If it is in our cache already then just use it directly as our
          // next nameserver.

          int8_t cacheIndex = find( tmpName );
          if ( cacheIndex != -1 ) {

            Ip::copy( pendingQuery.nsIpAddr, dnsTable[cacheIndex].ipAddr );
            tryAgain = true;

            TRACE_DNS(( "Dns:   Found in cache: %d.%d.%d.%d\n",
                        pendingQuery.nsIpAddr[0], pendingQuery.nsIpAddr[1],
                        pendingQuery.nsIpAddr[2], pendingQuery.nsIpAddr[3] ));

          } else {

            TRACE_DNS(( "Dns:   Adding to stack at position %d\n", pendingQuery.si+1 ));

            // Push the new target onto the top of the stack.  If we are lucky
            // they will give use the IP address in the Additional info section.
            // Otherwise, we'll do another query.

            if ( pendingQuery.si < (DNS_NAME_STACK-1) ) {
              pendingQuery.si++;
              strcpy( pendingQuery.nameStack[pendingQuery.si], tmpName );
              tryAgain = true;
            } else {
              TRACE_DNS_WARN(( "Whoops .. stack overlow.\n" ));
              tryAgain = false;
            }

          }

        }
        #endif
      }

      else if ( type == 5 ) { // Canonical name

        // We are being told the canonical name of the thing we just queried.
        // Replace the item at the top of the stack with the canonical name.
        //
        // Note that we kept the original name that the caller used, and that
        // is what we add to our cache.  If we added the canonical name, then
        // we'd have to do resolution all over again if the original item
        // falls out of the cache.

        current = decodeName( qr, current, tmpName );
        TRACE_DNS(( "Dns:   Canonical Name: %s\n", tmpName ));

        #ifndef DNS_ITERATIVE
        strcpy( pendingQuery.targetName, tmpName );
        #else
        if ( stricmp(questionName, pendingQuery.nameStack[pendingQuery.si] ) == 0 ) {
          strcpy( pendingQuery.nameStack[pendingQuery.si], tmpName );
          tryAgain = true;
        }
        #endif

      }

      else {
        TRACE_DNS(( "Dns: Record type: %d\n", type ));
        // Unknown record type - just skip it because we don't care.
        current += rdl;
      }

    }

  }


  #ifdef DNS_ITERATIVE
  if ( (current - (char*)qr->data) > UDP_max_response ) {
    TRACE_DNS_WARN(( "Dns: UDP message truncated\n" ));
  }
  #endif


  // We are done processing this packet.  Remove it from the front of
  // the queue and put it back on the free list.
  Buffer_free( packet );


  if ( queryPending == 0 ) {
    // We got our answer!
    return;
  }



  // Bad return code from the server?  If so, we are done.
  if ( qr->responseCode != 0 ) {
    queryPending = 0;
    lastQueryRc = (DNS_Response_Code_t)qr->responseCode;
    return;
  }


  #ifndef DNS_ITERATIVE
  // The return code was 0 but we might not have gotten the info that we need.
  queryPending = 0;
  lastQueryRc = UnknownError;
  #else
  // If we were told to use another nameserver and did not get an address
  // for it, then restart from the root nameserver.

  if ( (receivedAnAnswer == false) && (originalSi < pendingQuery.si) ) {
    Ip::copy( pendingQuery.nsIpAddr, NameServer );
    tryAgain = true;
  }

  // The return code was 0 but we might not have gotten the info
  // that we need.  If we need to go around again, do that.
  // Otherwise, there is nothing left to do as we did not get
  // an answer or be told where to look for an answer.

  if ( tryAgain ) {
    drivePendingQuery2( );
  }
  else {
    queryPending = 0;
    lastQueryRc = UnknownError;
  }
  #endif

}



// Dns::drivePendingQuery
//
// The user must call this periodically to drive a pending query.
// Under normal circumstances the DNS UDP responses coming back will
// keep things flowing.  However, this is UDP so it's possible for
// packets to be dropped.  If something goes too long this function
// will pick up where we left off.

void Dns::drivePendingQuery1( void ) {

  if ( Timer_diff( pendingQuery.lastUpdate, TIMER_GET_CURRENT( ) ) < TIMER_MS_TO_TICKS( DNS_RETRY_THRESHOLD ) ) return;

  if ( Timer_diff( pendingQuery.start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( DNS_TIMEOUT ) ) {
    queryPending = 0;
    lastQueryRc = Timeout;
    TRACE_DNS_WARN(( "Dns: Timeout finding: %s\n", pendingQuery.originalTarget ));
    return;
  }

  TRACE_DNS_WARN(( "Dns: No response, trying again\n" ));
  drivePendingQuery2( );

}


void Dns::drivePendingQuery2( void ) {

  // We are here because we don't have an answer yet and we have passed
  // the retry time threshold.
  //
  pendingQuery.lastUpdate = TIMER_GET_CURRENT( );
  pendingQuery.ident++;

  #ifndef DNS_ITERATIVE
  sendRequest( pendingQuery.nsIpAddr, pendingQuery.targetName, pendingQuery.ident );
  #else
  sendRequest( pendingQuery.nsIpAddr, pendingQuery.nameStack[pendingQuery.si], pendingQuery.ident );
  #endif

}



// Hosts file format:
//
// Lines starting with are comments.
// Blank lines are ok.
// IP addr canonical name [alias] [alias] [...]


static char lineBuffer[128];
static char token[DNS_MAX_NAME_LEN];

void Dns::scanHostsFile( const char *target1, const char *target2, IpAddr_t result ) {

  result[0] = result[1] = result[2] = result[3] = 0;
  if ( HostsFilename[0] == 0 ) return;

  FILE *hostsFile = fopen( HostsFilename, "r" );
  if ( hostsFile == NULL ) {
    TRACE_DNS_WARN(( "Dns: Error reading hosts file.\n" ));
    return;
  }

  uint16_t tmp1, tmp2, tmp3, tmp4;

  bool found = false;


  while ( !feof( hostsFile ) ) {

    if ( fgets( lineBuffer, 80, hostsFile ) == NULL ) {
      break;
    }

    // printf( "Linebuffer: %s", lineBuffer );

    if ( lineBuffer[0] == '#' ) {
      continue;
    }

    char *nextTokenPtr = Utils::getNextToken( lineBuffer, token, DNS_MAX_NAME_LEN );

    if ( *token == 0 ) {
      continue;
    }


    int rc = sscanf( token, "%d.%d.%d.%d", &tmp1, &tmp2, &tmp3, &tmp4 );
    if ( rc != 4 ) {
      puts( "Skipping invalid IP address in hosts file." );
      continue;
    }

    while ( nextTokenPtr ) {

      nextTokenPtr = Utils::getNextToken( nextTokenPtr, token, DNS_MAX_NAME_LEN );
      if ( *token == 0 ) {
        break;
      }

      // printf( "Token: %s\n", token );

      if ( (stricmp( token, target1 ) == 0) || ((*target2 != 0) && (stricmp( token, target2) == 0)) ) {
        found = true;
        result[0] = tmp1; result[1] = tmp2; result[2] = tmp3; result[3] = tmp4;
        // printf( "Match: %d.%d.%d.%d\n", tmp1, tmp2, tmp3, tmp4 );
        break;
      }

    }

  }

  fclose( hostsFile );

}
