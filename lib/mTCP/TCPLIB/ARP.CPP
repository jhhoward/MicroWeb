/*

   mTCP Arp.cpp
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


   Description: ARP code!

   Changes:

   2011-05-27: Initial release as open source software
   2013-03-23: Get rid of some duplicate strings
   2015-02-01: Do not respond to our own queries or add our own entry
               into the ARP cache; these are for detecting conflicts

*/



#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arp.h"
#include "timer.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "eth.h"
#include "ip.h"




// Static class variables

Arp::Pending_t Arp::pending[ARP_MAX_PENDING];
uint16_t Arp::pendingEntries = 0;

Arp::Rec_t Arp::arpTable[ARP_MAX_ENTRIES];
uint16_t Arp::entries = 0;


uint32_t Arp::RequestsReceived = 0;
uint32_t Arp::RepliesReceived = 0;
uint32_t Arp::RequestsSent = 0;
uint32_t Arp::RepliesSent = 0;
uint32_t Arp::CacheModifiedCount = 0;
uint32_t Arp::CacheEvictions = 0;


void Arp::dumpStats( FILE *stream ) {
  fprintf( stream, "Arp: Req Sent %lu Req Rcvd %lu Replies Sent %lu Replies Rcvd %lu\n"
                   "     Cache updates %lu Cache evictions %lu\n",
           RequestsSent, RequestsReceived, RepliesSent, RepliesReceived,
           CacheModifiedCount, CacheEvictions );
}



// We're going to send an ARP response for our MAC/IP
// address fairly often, so pre-build what we can first.
// To avoid any possible concurrency issue, these packets
// will serve as templates and will be copied to local
// storage when in use.  These original packets should not
// be used directly.
//
// 2006-12-21: On second thought, we don't really send that
// many ARP packets.  If you want to cut down on space and
// code, ditch the pre-built responses.

typedef struct {
  EthHeader eh;
  ArpHeader ah;
  char      padding[18];  // Packet has to be at least 60 bytes
} ArpPacket_t;

ArpPacket_t Arp_prebuiltResponse;
ArpPacket_t Arp_prebuiltRequest;




static char VanityString[] = "mTCP by M Brutman";


void Arp::init( void ) {

  clearPendingTable( );

  // Are we on a slip connection?  If so, stuff the table with the gateway
  // addr.  We'll just make a dummy entry, as the actual MAC addr does
  // not matter.

  char *slip = getenv( "MTCPSLIP" );
  if ( slip != NULL ) {
    Ip::copy( arpTable[0].ipAddr, Gateway );
    Eth::copy( arpTable[0].ethAddr, Eth::Eth_Broadcast );
    arpTable[0].updated = time( NULL );
    entries++;
  }

  // Initialize the pre-built Arp response packet
  // Don't call this unless we know our IP address and Eth address.

  Arp_prebuiltResponse.eh.setSrc( MyEthAddr );
  Arp_prebuiltResponse.eh.setType( 0x0806 );

  Arp_prebuiltResponse.ah.hardwareType = ntohs( 1 );
  Arp_prebuiltResponse.ah.protocolType = ntohs( 0x0800 );
  Arp_prebuiltResponse.ah.hlen = 6;
  Arp_prebuiltResponse.ah.plen = 4;
  Arp_prebuiltResponse.ah.operation = htons( 2 );
  Eth::copy( Arp_prebuiltResponse.ah.sender_ha, MyEthAddr );
  Ip::copy(  Arp_prebuiltResponse.ah.sender_ip, MyIpAddr );

  strcpy( Arp_prebuiltResponse.padding, VanityString );  // 18 bytes including the NULL


  // Initialize the pre-built Arp request packet.

  Arp_prebuiltRequest.eh.setDest( Eth::Eth_Broadcast );
  Arp_prebuiltRequest.eh.setSrc( MyEthAddr );
  Arp_prebuiltRequest.eh.setType( 0x0806 );

  Arp_prebuiltRequest.ah.hardwareType = ntohs( 1 );
  Arp_prebuiltRequest.ah.protocolType = ntohs( 0x0800 );
  Arp_prebuiltRequest.ah.hlen = 6;
  Arp_prebuiltRequest.ah.plen = 4;
  Arp_prebuiltRequest.ah.operation = htons( 1 );

  Eth::copy( Arp_prebuiltRequest.ah.sender_ha, MyEthAddr );
  Ip::copy( Arp_prebuiltRequest.ah.sender_ip, MyIpAddr );
  Eth::copy( Arp_prebuiltRequest.ah.target_ha, Eth::Eth_Broadcast );

  strcpy( Arp_prebuiltRequest.padding, VanityString );  // 18 bytes including the NULL
}



void Arp::clearPendingTable( void ) {
  for (uint16_t i=0; i < ARP_MAX_PENDING; i++ ) {
    pending[i].attempts = -1;
  }
  pendingEntries = 0;
}



#ifndef NOTRACE

void Arp::dumpTable( void ) {

  if ( TRACE_ON_ARP ) {

    Trace_tprintf( "Arp: table entries = %d\n", entries );

    for ( uint16_t i = 0; i < entries; i++ ) {
      fprintf( Trace_Stream, "%02x.%02x.%02x.%02x.%02x.%02x <-> %d.%d.%d.%d %u\n",
        arpTable[i].ethAddr[0], arpTable[i].ethAddr[1],
        arpTable[i].ethAddr[2], arpTable[i].ethAddr[3],
        arpTable[i].ethAddr[4], arpTable[i].ethAddr[5],
        arpTable[i].ipAddr[0], arpTable[i].ipAddr[1],
        arpTable[i].ipAddr[2], arpTable[i].ipAddr[3],
        arpTable[i].updated );
    }

  }

}

#endif



// Returns -1 if not found
// Gives you an index if found
// If you give a buffer you get the hardware address as well.

int8_t Arp::findEth( const IpAddr_t target_ip, EthAddr_t target ) {

  for ( uint16_t i=0; i < entries; i++ ) {

    if ( Ip::isSame( arpTable[i].ipAddr, target_ip ) ) {
      if ( target != NULL ) {
        Eth::copy( target, arpTable[i].ethAddr );
      }
      return i;
    }

  }
  return -1;
}




void Arp::processArp( uint8_t *packet, uint16_t packetLen ) {

  ArpHeader *ah = (ArpHeader *)(packet + sizeof(EthHeader));

  uint16_t op = ntohs( ah->operation );

  if ( op == 1 ) { // Incoming ARP request

    RequestsReceived++;

    TRACE_ARP(( "Arp: Req: %d.%d.%d.%d wants to know who %d.%d.%d.%d is\n",
                ah->sender_ip[0], ah->sender_ip[1],
                ah->sender_ip[2], ah->sender_ip[3],
                ah->target_ip[0], ah->target_ip[1],
                ah->target_ip[2], ah->target_ip[3] ));

    // Respond if we are the target, and add the requestor to our cache.
    // Unless we made the request while trying to find impostors.

    if ( Ip::isSame( ah->target_ip, MyIpAddr ) && !Ip::isSame( ah->sender_ip, MyIpAddr) ) {
      TRACE_ARP(( "Arp: Sending reply to %d.%d.%d.%d\n",
                  ah->sender_ip[0], ah->sender_ip[1],
                  ah->sender_ip[2], ah->sender_ip[3] ));
      updateOrAddCache( ah->sender_ha, ah->sender_ip );
      sendArpResponse( ah );
    }

  }
  else if ( op == 2 ) { // Incoming ARP response

    RepliesReceived++;

    TRACE_ARP(( "Arp: reply from %d.%d.%d.%d\n",
                ah->sender_ip[0], ah->sender_ip[1],
                ah->sender_ip[2], ah->sender_ip[3] ));

    // Any incoming ARP response should be in our pending list.
    // If it is not in our pending list then don't add it.
    //
    // If the sender or target are in our cache already, then
    // update the cache.
    //
    // If we ARPed ourselves we are looking for impostors.  Ignore any incoming
    // response that has our correct MAC address; we only want things that
    // have different addresses, and thus would be a conflict.

    bool pendingSatisfied = false;
    for ( uint16_t i=0; i < ARP_MAX_PENDING; i++ ) {
      if ( (pending[i].attempts != -1) && Ip::isSame( ah->sender_ip, pending[i].target ) ) {
        if ( !Ip::isSame( ah->sender_ip, MyIpAddr ) || !Eth::isSame( ah->sender_ha, MyEthAddr) ) {
          updateOrAddCache( ah->sender_ha, ah->sender_ip );
          pending[i].attempts = -1;
          pendingEntries--;
          pendingSatisfied = true;
          TRACE_ARP(( "Arp: reply satisfied pending req\n" ));
          break;
        }
      }
    }

    if ( pendingSatisfied ) {
      int8_t rc = findEth( ah->target_ip, NULL );
      if ( rc != -1 ) {
        updateEntry( rc, ah->target_ha );
      };
      rc = findEth( ah->sender_ip, NULL );
      if ( rc != -1 ) {
        updateEntry( rc, ah->sender_ha );
      }
    }

  }


  Buffer_free( packet );
}





void Arp::driveArp2( void ) {

  // Check our pending list.  If there is anything old then bump
  // the count and send the request again.  When the count hits
  // the configured level remove the request from the pending list.

  // We don't have a way to tell the end user that ARP resolution
  // failed.  They might know that their send was pending ARP
  // resolution, so if it never gets out of that state that should
  // give them a clue.

  for ( uint16_t i=0; i < ARP_MAX_PENDING; i++ ) {

    if ( pending[i].attempts != -1 ) {

      if ( pending[i].attempts == ARP_RETRIES ) {
        pending[i].attempts = -1;
        pendingEntries--;
        TRACE_ARP(( "Arp: Req timeout on %d.%d.%d.%d\n",
                pending[i].target[0], pending[i].target[1],
                pending[i].target[2], pending[i].target[3] ));
      }
      else {

        clockTicks_t current = TIMER_GET_CURRENT( );
        if ( Timer_diff( pending[i].start, current ) > TIMER_MS_TO_TICKS( ARP_TIMEOUT ) ) {

          pending[i].start = current;
          pending[i].attempts++;
          TRACE_ARP(( "Arp: Retry req for %d.%d.%d.%d, attempt=%d\n",
                  pending[i].target[0], pending[i].target[1],
                  pending[i].target[2], pending[i].target[3],
                  pending[i].attempts ));
          sendArpRequest2( pending[i].target );

        }

      }
    }
  }

}




void Arp::updateEntry( uint16_t index, const EthAddr_t newEthAddr ) {
  Eth::copy( arpTable[ index ].ethAddr, newEthAddr );
  arpTable[index].updated = time( NULL );
  TRACE_ARP(( "Arp: Updated entry %d.%d.%d.%d\n",
          arpTable[ index ].ipAddr[0], arpTable[ index ].ipAddr[1],
          arpTable[ index ].ipAddr[2], arpTable[ index ].ipAddr[3] ));
}



// If we have the mapping it will be updated.
// If we don't have it, it will be added.  We may have to toss something
// out first to do that.

void Arp::updateOrAddCache( EthAddr_t newEthAddr, IpAddr_t newIpAddr ) {

  CacheModifiedCount++;

  // Do we have this?
  int8_t rc = findEth( newIpAddr, NULL );

  if ( rc != -1 ) {
    // Update the time and copy the new hardware address.
    // It's not worth checking to see if it's different.
    updateEntry( rc, newEthAddr );
  }
  else {
    uint16_t target;
    if ( entries < ARP_MAX_ENTRIES ) {
      target = entries;
      entries++;
    }
    else {
      // Naive - toss the oldest.
      // Better would be to toss the least used IP addr.
      uint16_t lowestTimeIndex = 0;
      for ( uint16_t i=1; i < entries; i++ ) {
        if ( arpTable[i].updated < arpTable[lowestTimeIndex].updated ) {
          lowestTimeIndex = i;
        }
      }
      target = lowestTimeIndex;

      CacheEvictions++;

      TRACE_ARP_WARN(( "Arp: Throwing out table entry: %d.%d.%d.%d",
          arpTable[ target ].ipAddr[0], arpTable[ target ].ipAddr[1],
          arpTable[ target ].ipAddr[2], arpTable[ target ].ipAddr[3] ));

    }

    // Now put it in the table.

    Eth::copy( arpTable[ target ].ethAddr, newEthAddr );
    Ip::copy( arpTable[ target ].ipAddr, newIpAddr );

    arpTable[ target ].updated = time( NULL );

    TRACE_ARP(( "Arp: Placed %d.%d.%d.%d in slot %d\n",
                newIpAddr[0], newIpAddr[1], newIpAddr[2], newIpAddr[3], target ));
  }


}



void Arp::sendArpRequest( IpAddr_t target_ip ) {

  // If there is a pending request do nothing; there is no point in
  // bombarding the network with requests.

  uint16_t i;
  for ( i=0; i < ARP_MAX_PENDING; i++ ) {
    if ( (pending[i].attempts != -1) && Ip::isSame( pending[i].target, target_ip ) ) {
      return;
    }
  }


  // No pending entries so we need to add one.


  // Too many pending requests?

  if (pendingEntries == ARP_MAX_PENDING ) {
    TRACE_ARP_WARN(( "Arp: Too many pending entries: %d\n", pendingEntries ));
    return;
  }



  // Find the empty slot.  We know we have one.

  for ( i=0; i < ARP_MAX_PENDING; i++ ) {
    if ( pending[i].attempts == -1 ) break;
  }

  Ip::copy( pending[i].target, target_ip );
  pending[i].start = TIMER_GET_CURRENT( );
  pending[i].attempts = 0;
  pendingEntries++;

  RequestsSent++;

  sendArpRequest2( target_ip );

}


void Arp::sendArpRequest2( IpAddr_t target_ip ) {

  // Send the request

  ArpPacket_t arpRequest;

  arpRequest = Arp_prebuiltRequest;

  Ip::copy( arpRequest.ah.target_ip, target_ip );

  Packet_send_pkt( &arpRequest, sizeof( arpRequest ) );

  TRACE_ARP(( "Arp: Sent req for %d.%d.%d.%d\n",
          target_ip[0], target_ip[1], target_ip[2], target_ip[3] ));

}



void Arp::sendArpResponse( ArpHeader *ah ) {

  ArpPacket_t arpResponse;

  arpResponse = Arp_prebuiltResponse;

  arpResponse.eh.setDest( ah->sender_ha );
  Eth::copy( arpResponse.ah.target_ha, ah->sender_ha );
  Ip::copy(  arpResponse.ah.target_ip, ah->sender_ip );

  Packet_send_pkt( &arpResponse, sizeof( arpResponse ) );

  RepliesSent++;

  TRACE_ARP(( "Arp: Sent reply to %d.%d.%d.%d\n",
          ah->sender_ip[0], ah->sender_ip[1],
          ah->sender_ip[2], ah->sender_ip[3] ));

}





// High level function to resolve an IP address.  IP uses this.
//
// Returns 0 if an Ethernet address was returned
//         1 if the packet is pending because of ARP.
//           The ARP request is automatically sent in this case.

int8_t Arp::resolve( IpAddr_t target_ip, EthAddr_t eth_dest ) {

  int8_t rc = findEth( target_ip, eth_dest );
  if ( rc != -1 ) {
    return 0;
  }

  sendArpRequest( target_ip );
  return 1;
}


