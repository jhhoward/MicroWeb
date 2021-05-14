/*

   mTCP Arp.H
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


   Description: ARP data structures and functions

   Changes:

   2011-05-27: Initial release as open source software
   2014-05-18: Add some static checks to the configuration options

*/


#ifndef _ARP_HEADER_H
#define _ARP_HEADER_H



// Load and check the configuration options first

#include CFG_H
#include "types.h"

// Each entry in the ARP table is fairly modest - 14 bytes at the moment.
// If you get ARP table thrashing with the maximums defined here then you
// have a lot of hosts on your network segment.

static_assert( ARP_MAX_ENTRIES >=  4 );
static_assert( ARP_MAX_ENTRIES <= 32 );
static_assert( ARP_MAX_PENDING >=  1 );
static_assert( ARP_MAX_PENDING <=  8 );
static_assert( ARP_MAX_PENDING <= ARP_MAX_ENTRIES );
static_assert( ARP_RETRIES >= 1 );
static_assert( ARP_RETRIES <= 5 );
static_assert( ARP_TIMEOUT >=  100ul );
static_assert( ARP_TIMEOUT <= 1000ul );



// Continue with other includes

#include "Eth.h"



// General rules for ARP
//
// - If you get a request, add the requestor to your cache.  If they are
//   looking for you they will probably talk to you soon.
// - If you see somebody else get a reply, update your cache if needed
//   but don't add a new entry.
// - Drop a cache entry if it is older than 10 minutes.  (Not implemented yet)
// - If you are out of room, drop the oldest entry.
//
// We don't bother aging the ARP cache because this is DOS, and we really
// don't expect to be running for years at a time.  Machines generally don't
// change their MAC addresses unless something bad happens to them.
//
// Even worse, mTCP will actively cache the ARP address of the next hop for
// each socket to avoid having to constantly look up the next hop in the ARP
// cache. :-)  This is not standard behavior; it assumes that your network
// topology is not changing.  There will be a new configuration option to
// disable this optimization if it freaks you out.
//
//
// Detecting IP address conflicts
//
// To detect an IP address conflict just ARP your own IP address and wait
// for a response with a MAC address that is not yours.  If something winds
// up in the cache then you know that something else is using your IP address.
//
// This code does two things to help that.  It lets you ARP your own IP address
// and it will not respond to itself.  And if it gets an response back with
// the correct MAC address it ignores it.  (A DHCP server that just gave you
// an address might respond, and that is ok.)  But if something else responds,
// it gets added to the table where presumably it will be found and reported
// as an error.
//
// Right now, only Utils::initStack tries to detect address conflicts.
// Use that code as a model if you need to detect conflicts too.




class ArpHeader {

  public:

    uint16_t  hardwareType;
    uint16_t  protocolType;
    uint8_t   hlen;
    uint8_t   plen;
    uint16_t  operation;
    EthAddr_t sender_ha;
    IpAddr_t  sender_ip;
    EthAddr_t target_ha;
    IpAddr_t  target_ip;

};



class Arp {

  private:

    // We keep track of pending requests to avoid flooding the network
    // if an upper layer protocol keeps retrying a send that needs to
    // be resolved.

    typedef struct {
      IpAddr_t     target;
      clockTicks_t start;     // High resolution timer (55ms)
      int8_t       attempts;  // ( -1 if slot is not in use )
      uint8_t      padding;
    } Pending_t;

    static Pending_t pending[ARP_MAX_PENDING];
    static uint16_t pendingEntries;


    // Our Arp cache

    typedef struct {
      EthAddr_t     ethAddr;
      IpAddr_t      ipAddr;
      time_t        updated; // Lower resolution time.
    } Rec_t;

    static Rec_t arpTable[ARP_MAX_ENTRIES];
    static uint16_t entries;

    static void updateEntry( uint16_t target, const EthAddr_t newEthAddr );
    static void updateOrAddCache( EthAddr_t newEthAddr, IpAddr_t newIpAddr );

    static void sendArpRequest( IpAddr_t target_ip );
    static void sendArpRequest2( IpAddr_t target_ip );
    static void sendArpResponse( ArpHeader *ah );

    static void driveArp2( void );

    static int8_t findEth( const IpAddr_t target_ip, EthAddr_t target );
    static void   deleteCacheEntry( int target );


  public:

    static void init( void );

    // Returns 0 if resolved, 1 is ARP is pending
    static int8_t resolve( IpAddr_t target_ip, EthAddr_t target_eth );

    // Called by Packet.CPP when you get an incoming ARP packet
    static void processArp( uint8_t *ah, uint16_t packetLen );

    // Called to drive pending ARP queries
    static inline void driveArp( void ) {
      if ( pendingEntries ) { driveArp2( ); }
    }

    static void clearPendingTable( void );

    #ifndef NOTRACE
    static void dumpTable( void );
    #endif

    static void dumpStats( FILE *stream );

    static uint32_t RequestsReceived;
    static uint32_t RepliesReceived;
    static uint32_t RequestsSent;
    static uint32_t RepliesSent;
    static uint32_t CacheModifiedCount;
    static uint32_t CacheEvictions;

};


#endif
