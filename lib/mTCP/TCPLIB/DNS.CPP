/*

   mTCP Dns.cpp
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


   Description: DNS resolving functions

   Changes:

   2011-05-27: Initial release as open source software
   2013-03-23: Get rid of some duplicate strings

*/



// Dns resolver.  This version uses UDP only.  I need to do a TCP
// version some year.
//
// I've got some serious logic bugs when recursion is not used.
// Leave the recursion on for real use, and turn it off only for
// fun and excitement.  (Most of the bugs are loops with servers
// pointing at each other.)


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



uint16_t Dns::handlerPort = 0;
IpAddr_t Dns::NameServer = { 0, 0, 0, 0 };  // Set by environment variable


Dns::DNS_Rec_t Dns::dnsTable[ DNS_MAX_ENTRIES ];
uint8_t Dns::entries = 0;


uint8_t Dns::queryPending = 0;
int8_t  Dns::lastQueryRc = 0;
Dns::DNS_Pending_Rec_t Dns::pendingQuery;



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

int8_t Dns::init( uint16_t port ) {

  if ( Ip::isSame(NameServer, IpThisMachine) ) {
    TRACE_DNS_WARN(( "Dns: NameServer not set\n" ));
  }

  if ( port == 0 ) return -1;  // Choose something other than 0

  handlerPort = port;
  int8_t rc = Udp::registerCallback( handlerPort, &Dns::udpHandler );

  return rc;
}



void Dns::stop( void ) {
  if ( handlerPort ) {
    Udp::unregisterCallback( handlerPort );
    handlerPort = 0;
  }
}



void Dns::dumpTable( void ) {
  for ( uint8_t i=0; i < entries; i++ ) {
    printf( "%3d.%3d.%3d.%3d  %s\n",
	    dnsTable[i].ipAddr[0], dnsTable[i].ipAddr[1],
	    dnsTable[i].ipAddr[2], dnsTable[i].ipAddr[3],
	    dnsTable[i].name );
  }
}





// Resolve
//
// Cheap hack alert - if we think that you passed in a numeric IP address
// we report that it is in the cache and just return it back to you.
//
// Use lower-case names - we are case sensitive.
// If you want a request generated, pass a 1 to sendReq.
//
//  0: Was in the cache; addr returned
//  1: Sent request; check back later
//  2: Busy with another request
//  3: Not in cache, and no req sent because user said not to
//
// -1: Name too long
// -2: No NameServer set

int8_t Dns::resolve( const char *serverName, IpAddr_t target, uint8_t sendReq ) {

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

  if ( Ip::isSame(NameServer, IpThisMachine) ) {
    return -2;
  }

  if ( len >= DNS_MAX_NAME_LEN ) {
    return -1;  // Name too long
  }

  // Fixme: Do aging here so we don't give out stale results?

  // Is this name in our cache?
  int8_t index = find( serverName );
  if ( index != -1 ) {
    Ip::copy( target, dnsTable[index].ipAddr );
    return 0;
  }

  if ( queryPending ) {
    return 2;  // Busy with another request

  }

  if ( sendReq == 0 ) return 3;

  // Send request
  queryPending = 1;
  lastQueryRc  = 0;  // Not valid until queryPending = 0
  memset( &pendingQuery, 0, sizeof(pendingQuery) );
  strcpy( pendingQuery.name, serverName );
  pendingQuery.ident = rand( );
  pendingQuery.start = TIMER_GET_CURRENT( );
  pendingQuery.lastUpdate = pendingQuery.start;
  Ip::copy( pendingQuery.nameServerIpAddr, NameServer );
  sendRequest( NameServer, serverName, pendingQuery.ident );

  return 1;
}



void Dns::sendRequest( IpAddr_t resolver, const char *serverName, uint16_t ident ) {

  TRACE_DNS(( "Dns: Sending query to %d.%d.%d.%d for %s\n",
	  resolver[0], resolver[1], resolver[2], resolver[3], serverName ));

  DNSpacket query;

  // Encode header

  query.ident = htons( ident );
  query.qrFlag = 0;
  query.opCode = 0;
  query.authoritativeAnswer = 0;
  query.truncationFlag = 0;
  query.recursionDesired = DNS_RECURSION_DESIRED;
  query.recursionAvailable = 1;
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
  uint16_t len = strlen( serverName ); // Length of the server name

  // Go until the end of the string is hit
  while ( index < len ) {

    while ( index < len ) {

      if ( serverName[index] == '.' ) {
	*lenByte = sublen;
	lenByte = target;
	sublen = 0;
      }
      else {
	*target = serverName[index];
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

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  int8_t rc = Udp::sendUdp( resolver, handlerPort, 53,
                            reqLen, (uint8_t *)&query, 1 );

  if ( rc == -1 ) {
    // Fatal error from UDP.  This probably never happens unless you are
    // low on memory.  If it does happen a debug trace will expose it.
    return;
  }


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

    rc = Udp::sendUdp( resolver, handlerPort, 53,
                       reqLen, (uint8_t *)&query, 1 );

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

  // Name first
  while ( nextLen = *current ) {

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
      strcpy( nameBuf, "TOO_BIG" );
      TRACE_DNS_WARN(( "Dns: Name in a response was too long.\n" ));
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



void Dns::addOrUpdate( char *targetName, IpAddr_t addr ) {

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



int8_t Dns::find( const char *name ) {

  int8_t rc = -1;

  for ( uint8_t i=0; i < entries; i++ ) {
    if ( strcmp( dnsTable[i].name, name ) == 0 ) {
      rc = i;
      break;
    }
  }

  return rc;
}




static char DnsReceivedAddr[] = "Dns:   Good, received addr for nameserver\n";

// We received a response.  If we got an answer then add it to
// the table.  If we didn't get an answer but they were helpful
// and gave us an address for the nameserver, resend the request
// to the next nameserver.
//
// If they give us the name of the next nameserver but not an
// address for it, we should try to resolve it, cache it, and
// then eventually retry the original request.


void Dns::udpHandler(const unsigned char *packet,
		   const UdpHeader *udp ) {

  TRACE_DNS(( "Dns: UDP Handler entry\n" ));

  DNSpacket *qr = (DNSpacket *)(packet);

  uint16_t ident = ntohs( qr->ident );

  TRACE_DNS(( "Dns: Ident: %04x  Q/R: %d  Opcode: %d  Trun: %d  AA: %d Rc: %d\n",
	  ident, qr->qrFlag, qr->opCode,
	  qr->truncationFlag, qr->authoritativeAnswer,
	  qr->responseCode ));

  uint8_t numQuestions  = ntohs( qr->numQuestions );
  uint8_t numAnswers    = ntohs( qr->numAnswers );
  uint8_t numAuthority  = ntohs( qr->numAuthority );
  uint8_t numAdditional = ntohs( qr->numAdditional );

  TRACE_DNS(( "Dns:   Questions: %d  Answers: %d  Authority: %d  Additional: %d\n",
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

    TRACE_DNS(( "Dns: Question Name: %s  Type: %d  Class: %d\n",
	    questionName, type, cls ));

  }


  // Used to check for a bad return code here and bug out, but we'll
  // defer that to later now because it is interesting for the trace
  // to see what is in the record.


  uint8_t nsIpAddrFilledInThisPass = 0;

  // Answers=1, Authority=2, Additional=3
  for ( uint8_t j = 1; j < 4; j++ ) {

    uint8_t limit;

    switch ( j ) {
      case 1: limit=numAnswers; break;
      case 2: limit=numAuthority; break;
      case 3: limit=numAdditional; break;
    }


    for ( i=0; i < limit; i++ ) {

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

      char section[12];

      switch (j) {
	case 1: strcpy( section, "Answer" ); break;
	case 2: strcpy( section, "Authority" ); break;
	case 3: strcpy( section, "Additional" ); break;
	default: strcpy( section, "Unknown" ); break;
      }

      TRACE_DNS(( "Dns: Section: %s   Name: %s\n", section, tmpName ));
      TRACE_DNS(( "Dns:   Type: %d  Class: %d  TTL: %ld  RDL: %d\n", type, cls, ttl, rdl ));

      if ( TRACE_ON_DNS && TRACE_ON_DUMP ) {
        Trace_tprintf( "Dns:   Raw Data:\n" );
	Utils::dumpBytes( Trace_Stream, (uint8_t *)current, rdl );
      }

      #endif

      if ( type == 1 ) { // Received an address

	IpAddr_t *addr = (IpAddr_t *)current;

	TRACE_DNS(( "Dns:   IP Addr received: %d.%d.%d.%d\n",
		(*addr)[0], (*addr)[1], (*addr)[2], (*addr)[3] ));


	// If it is an answer to our current question then file it.

	if ( j==1 ) { // Answers section

	  // If they gave us an addr for our direct query or the
	  // canonical name then we are done.

	  if ( (strcmp(tmpName, pendingQuery.name) == 0) ||
	       (strcmp(tmpName, pendingQuery.canonical) == 0 ) ) {

	    addOrUpdate( pendingQuery.name, *addr );

	    queryPending = 0;
	    lastQueryRc = 0;

	    TRACE_DNS(( "Dns:   Query done, addr received\n" ));

	  }


	  // Did we get an address for a nameserver?
	  //
	  // If we were told a nameserver but not an addr for that nameserver
	  // we have to query for the nameserver directly.  If this happens,
	  // the incoming question will be for the nameserver, not the
	  // original query.  This is ok, as we know it's our request because
	  // of the ident field.

	  else if ( strcmp(tmpName, pendingQuery.nameServer ) == 0 ) {

	    Ip::copy( pendingQuery.nameServerIpAddr, *addr );
	    TRACE_DNS(( DnsReceivedAddr ));
	    nsIpAddrFilledInThisPass = 1;

	  }
	  else {
	    TRACE_DNS_WARN(( "Dns: Don't know what to do with this addr.\n" ));
	  }

	} else if ( j == 3 ) {

	  // Additional info: Addr of a nameserver that we need?

	  if ( strcmp( tmpName, pendingQuery.nameServer ) == 0 ) {
	    Ip::copy( pendingQuery.nameServerIpAddr, *addr );
	    TRACE_DNS(( DnsReceivedAddr ));
	    nsIpAddrFilledInThisPass = 1;
	  }
	  else {
	    TRACE_DNS(( "Dns:   Nameserver addr ignored, have one already\n" ));
	  }

	}

	current += rdl;
      }

      else if ( type == 2 ) { // Name Server

	current = decodeName( qr, current, tmpName );

	// We are being told the next nameserver to use.
	// Take whatever they give us.
	//
	// Optimization: They may give us several, record only the first.

	// Another gotcha - if we just got the IP addr of a nameserver
	// we were querying for, don't pick up a new one!

	if ( (i == 0) && (nsIpAddrFilledInThisPass == 0) ) {

	  // Save previous nameserver name and addr.  We have to use it
	  // until we get an IP addr for this nameserver.
	  strcpy( pendingQuery.prevNS, pendingQuery.nameServer );

	  // Interesting path.  They might give us two nameservers back to
	  // back without an IP address in the middle.  This would cause us
	  // to copy a bogus address into prevNIIpAddr.  Don't do the copy
	  // unless it is a real address.
	  //
	  // If things are stupid and we get stuck pingponging back and forth
	  // between servers, the timeout will catch it.  Better yet, punt
	  // and start with our original nameserver.

	  if ( pendingQuery.nameServerIpAddr[0] != 0 ) {
	    Ip::copy( pendingQuery.prevNSIpAddr, pendingQuery.nameServerIpAddr );
	  }
	  else {
	    TRACE_DNS_WARN(( "Dns: Got another NS before first resolved; defaulting to our NS\n" ));
	    Ip::copy( pendingQuery.prevNSIpAddr, NameServer );
	  }

	  strcpy( pendingQuery.nameServer, tmpName );
	  pendingQuery.nameServerIpAddr[0] = 0;
	}


	TRACE_DNS(( "Dns:   Name Server: %s, Using this: %s\n",
		    tmpName, (i==0) ? "Yes" : "No" ));

      }
      else if ( type == 5 ) { // Canonical name
	current = decodeName( qr, current, tmpName );
	TRACE_DNS(( "Dns: Canonical Name: %s\n", tmpName ));


	if ( strcmp(questionName, pendingQuery.name) == 0 ) {
	  strcpy( pendingQuery.canonical, tmpName );
	  TRACE_DNS(( "Dns:   Took canonical name\n" ));
	}
	else {
	  TRACE_DNS_WARN(( "Dns:   Warning: Gave us canonical name for a different question?\n" ));
	}


      }
      else {
	TRACE_DNS(( "Dns: Record type: %d\n", type ));
	// Unknown record type - just skip it
	current += rdl;
      }

    }

  }


  // Bad return code?  If so, we are done.
  if ( qr->responseCode != 0 ) {
    queryPending = 0;
    lastQueryRc = qr->responseCode;
  }

  // We are done processing this packet.  Remove it from the front of
  // the queue and put it back on the free list.
  Buffer_free( packet );

  // No answer?  Let's send another one!
  if ( queryPending ) {
    drivePendingQuery2( );
  }

}



// Dns::drivePendingQuery
//
// The user must call this periodically to drive a pending query.
// Under normal circumstances the DNS UDP responses coming back will
// keep things flowing.  However, this is UDP so it's possible for
// packets to be dropped.  If something goes too long this function
// will pick up where we left off.

void Dns::drivePendingQuery( void ) {

  if ( queryPending == 0 ) {
    TRACE_DNS_WARN(( "Dns: drivePendingQuery called with no pending query\n" ));
    return;
  }

  if ( Timer_diff( pendingQuery.lastUpdate, TIMER_GET_CURRENT( ) ) < TIMER_MS_TO_TICKS( DNS_RETRY_THRESHOLD ) ) return;

  if ( Timer_diff( pendingQuery.start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( DNS_TIMEOUT ) ) {
    queryPending = 0;
    lastQueryRc = -1;
    TRACE_DNS_WARN(( "Dns: Timout finding: %s\n", pendingQuery.name ));
    return;
  }

  TRACE_DNS_WARN(( "Dns: No activity, pushing again\n" ));
  drivePendingQuery2( );
}


void Dns::drivePendingQuery2( void ) {

  pendingQuery.lastUpdate = TIMER_GET_CURRENT( );



  // We are here because we don't have an answer yet.

  // Do we have a next nameserver to talk to?
  if ( pendingQuery.nameServer[0] == 0 ) {

    // No - our nameserver must not have responded yet.  Resend
    // the initial query.

    TRACE_DNS(( "Dns: Resending initial query\n" ));
    pendingQuery.ident++;
    sendRequest( NameServer, pendingQuery.name, pendingQuery.ident );

    return;
  }



  // Ok, we at least know the name of the next nameserver.
  //
  // If we need it's IP address, ask the last name server for it.
  // If we have it's IP address, ask it to resolve our pending name.

  if ( pendingQuery.nameServerIpAddr[0] == 0 ) {

    TRACE_DNS(( "Dns: Query %d.%d.%d.%d for IPaddr of NS %s\n",
		pendingQuery.prevNSIpAddr[0], pendingQuery.prevNSIpAddr[1],
		pendingQuery.prevNSIpAddr[2], pendingQuery.prevNSIpAddr[3],
		pendingQuery.nameServer ));

    pendingQuery.ident++;
    sendRequest( pendingQuery.prevNSIpAddr, pendingQuery.nameServer, pendingQuery.ident );

  }
  else {

    if ( pendingQuery.canonical[0] != 0 ) {
      TRACE_DNS(( "Dns: Query %s for canonical %s\n",
		  pendingQuery.nameServer, pendingQuery.canonical ));
      pendingQuery.ident++;
      sendRequest( pendingQuery.nameServerIpAddr, pendingQuery.canonical, pendingQuery.ident );
    }
    else {
      TRACE_DNS(( "Dns: Query %s for %s\n",
		  pendingQuery.nameServer, pendingQuery.name ));

      pendingQuery.ident++;
      sendRequest( pendingQuery.nameServerIpAddr, pendingQuery.name, pendingQuery.ident );
    }

  }

}

