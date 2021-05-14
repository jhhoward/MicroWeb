/*

   mTCP Ping.cpp
   Copyright (C) 2009-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Ping application

   Changes:

   2012-04-29: Fix a small bug; outgoing packets were too large
   2011-05-27: Initial release as open source software
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.
   2015-02-03: Code cleanup

*/


// A simple Ping utility.  This one was a nightmare to code because it
// requires much higher resolution on the timer than BIOS normally gives
// us.  There is some code to reprogram the hardware timer to tick faster
// and a new interrupt handler that only calls the BIOS timer tick every
// so often to preserve the correct view of time at the BIOS level.
//
// Using this trick we can get sub millisecond accuracy out of the slowest
// machines, which is a nice trick.  And it is much better than trying to
// code processor dependent delay loops, which don't work anyway.
//
// Return codes
//   0 - All good, and all sent packets received a response
//   1 - Usage error
//   2 - DNS error
//   3 - No responses received
//   4 - Some responses received
//


#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "udp.h"
#include "dns.h"
#include "timer.h"



char    *ServerName;
IpAddr_t ServerAddr;
uint16_t PacketCount = 4;
uint16_t PacketPayload = 32;

uint16_t TimeoutSecs = 1;

uint16_t PacketsSent = 0;
uint16_t RepliesReceived = 0;
uint16_t RepliesLost = 0;
uint32_t ReplyTime = 0;

uint16_t icmpLen;




// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}




IcmpEchoPacket_t icmpEchoPacket;

uint8_t ResponseReceived = 0;
uint8_t LastTTL = 0;


// Function prototypes

void icmpHandler( const unsigned char *packet, const IcmpHeader *icmp );
void sendAndWait( void );
void parseArgs( int argc, char *argv[] );
void shutdown( int rc );



volatile uint32_t ping_ticks2 = 0;
volatile uint16_t ping_ticks = 0;


void ( __interrupt __far *ping_old_tick_handler)( );

void __interrupt __far ping_tick_handler( )
{
  ping_ticks2++;

  ping_ticks++;
  if ( ping_ticks == 63 ) {
    ping_ticks = 0;
    _chain_intr( ping_old_tick_handler );
  }
  else {
    outportb( 0x20, 0x20 );
  }
}



void ping_hook( void ) {

  disable_ints( );

  ping_old_tick_handler = getvect( 0x08 );
  setvect( 0x08, ping_tick_handler );

  // Tell the 8253 that we want to use Mode 3 on timer 0.
  // The counter will start at 0x400 and count down to 0
  // instead of the usual 0xFFFF, which makes it 64 times
  // faster than usual.

  _asm {
    mov al, 0x36
    out 0x43, al
    mov al, 0x00
    out 0x40, al
    mov al, 0x04
    out 0x40, al
  }

  enable_ints( );
}

void ping_unhook( void ) {

  disable_ints( );

  _asm {
    mov al, 0x36
    out 0x43, al
    mov al, 0xFF
    out 0x40, al
    mov al, 0xFF
    out 0x40, al
  }

  setvect( 0x08, ping_old_tick_handler );

  enable_ints( );
}




static char *CopyrightMsg= "\nmTCP Ping by M Brutman (mbbrutman@gmail.com) (C)opyright 2009-2020\nVersion: " __DATE__ "\n";


int main( int argc, char *argv[] ) {

  puts( CopyrightMsg );

  parseArgs( argc, argv );


  // Initialize TCP/IP
  if ( Utils::parseEnv( ) != 0 ) {
    exit( 1 );
  }


  // No sockets, no buffers TCP buffers
  if ( Utils::initStack( 0, 0, ctrlBreakHandler, ctrlBreakHandler ) ) {
    puts( "\nFailed to initialize TCP/IP - exiting" );
    exit( 1 );
  }


  // Hook the timer interrupt to reprogram it for higher precision.  This will
  // not affect the rate at which the BIOS tick happens.

  ping_hook( );


  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.  It is actually hooked
  // twice; once by TCP/IP and then by this code.


  // Start resolving the name.
  Dns::resolve( ServerName, ServerAddr, 1 );

  while ( !CtrlBreakDetected && Dns::isQueryPending( ) ) {
    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Dns::drivePendingQuery( );
  }


  // Query is no longer pending or we bailed out of the loop.
  if ( Dns::resolve( ServerName, ServerAddr, 0 ) != 0 ) {
    printf( "Error resolving server name: %s\n", ServerName );
    shutdown( 2 );
  }


  if ( Ip::isSame( ServerAddr, MyIpAddr ) || (ServerAddr[0] == 127) ) {
    puts( "Pinging your own address or loopback addresses is not supported." );
    shutdown( 1 );
  }



  // Register Icmp handler
  Icmp::icmpCallback = icmpHandler;


  // Build up our packet

  icmpLen = PacketPayload + sizeof( IcmpHeader ) + sizeof( uint16_t ) * 2;


  // Eth header source and type
  icmpEchoPacket.eh.setSrc( MyEthAddr );
  icmpEchoPacket.eh.setType( 0x0800 );


  // Ip Header
  icmpEchoPacket.ip.set( IP_PROTOCOL_ICMP, ServerAddr, icmpLen, 0, 0 );

  // Icmp header
  icmpEchoPacket.icmp.type = ICMP_ECHO_REQUEST;
  icmpEchoPacket.icmp.code = 0;
  icmpEchoPacket.icmp.checksum = 0;

  // Icmp data

  icmpEchoPacket.ident = 0x6048; // Will be 4860 when transmitted; the PCjr model number
  icmpEchoPacket.seq = 0;

  for ( int i=0; i < PacketPayload; i++ ) {
    icmpEchoPacket.data[i] = (i % 26) + 'A';
  }

  // Icmp checksum
  //icmpEchoPacket.icmp.checksum = Ip::genericChecksum( (uint16_t *)&icmpEchoPacket.icmp, icmpLen );
  icmpEchoPacket.icmp.checksum = ipchksum( (uint16_t *)&icmpEchoPacket.icmp, icmpLen );



  // Eth header dest - has to be called after Ip header is set.
  // Should return a 1 the first time because we haven't ARPed yet.
  int8_t arpRc = icmpEchoPacket.ip.setDestEth( icmpEchoPacket.eh.dest );



  // Wait for ARP response

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( arpRc ) {

    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 4000 ) ) {
      break;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );

    arpRc = icmpEchoPacket.ip.setDestEth( icmpEchoPacket.eh.dest );

  }

  if ( arpRc ) {
    puts( "Timeout waiting for ARP response." );
    shutdown( 3 );
  }


  // At this point we ARPed and got a response.  Time to start sending
  // packets.

  printf( "ICMP Packet payload is %u bytes.\n\n", PacketPayload );

  sendAndWait( );

  if ( PacketCount > 1 ) {

    for ( uint16_t l = 1; l < PacketCount; l++ ) {

      if ( CtrlBreakDetected ) break;

      // Setup the packet.
      icmpEchoPacket.icmp.checksum = 0;
      icmpEchoPacket.seq = htons( l );
      //icmpEchoPacket.icmp.checksum = Ip::genericChecksum( (uint16_t *)&icmpEchoPacket.icmp, icmpLen );
      icmpEchoPacket.icmp.checksum = ipchksum( (uint16_t *)&icmpEchoPacket.icmp, icmpLen );

      sendAndWait( );

    }

  }

  printf( "\nPackets sent: %u, Replies received: %u, Replies lost: %u\n",
	  PacketsSent, RepliesReceived, RepliesLost );

  if ( RepliesReceived ) {

    uint16_t big = ((ReplyTime * 85 ) / RepliesReceived) / 100;
    uint16_t small = ((ReplyTime * 85) / RepliesReceived) % 100;

    printf( "Average time for a reply: %u.%02u ms (not counting lost packets)\n",
	    big, small );
  }

  int rc = 4;
  if ( PacketsSent == RepliesReceived ) {
    rc = 0;
  } else if ( RepliesReceived == 0 ) {
    rc = 3;
  }

  shutdown( rc );

  return 0;
}





// Called by the low level Icmp Handler.  Check to see if this is the
// reply we are waiting for, and if so set a flag to indicate victory.

void icmpHandler( const unsigned char *packet, const IcmpHeader *icmp ) {

  if ( icmp->type == ICMP_ECHO_REPLY ) {

    IcmpEchoPacket_t *icmpReply = (IcmpEchoPacket_t *)packet;

    if ( icmpReply->ident == 0x6048 && icmpReply->seq == icmpEchoPacket.seq ) {

      // More checks ..

      uint16_t icmpPayloadLen = icmpReply->ip.payloadLen( ) - sizeof(IcmpHeader) - sizeof(uint16_t) * 2;

      if ( icmpPayloadLen == PacketPayload && memcmp(icmpReply->data, icmpEchoPacket.data, PacketPayload) == 0 ) {
	  ResponseReceived = 1;
	  LastTTL = icmpReply->ip.ttl;
      }

    }

  }

}



void sendAndWait( ) {

  PacketsSent++;

  ResponseReceived = 0;

  // Don't have to worry about a minimul length - icmpEchoPacket is guaranteed to be
  // bigger than 60 bytes.
  Packet_send_pkt( &icmpEchoPacket, icmpLen + sizeof(EthHeader) + sizeof(IpHeader) );

  uint32_t startTime = ping_ticks2;
  uint32_t startBios = TIMER_GET_CURRENT( );

  while ( 1 ) {

    if ( CtrlBreakDetected ) return;

    PACKET_PROCESS_SINGLE;

    if ( ResponseReceived ) {

      uint32_t elapsedTicks = ping_ticks2 - startTime;

      // float elapsed = elapsedTicks * 0.858209;
      uint16_t elapsedMs = (elapsedTicks * 85) / 100;
      uint16_t elapsedSmall = (elapsedTicks * 85) % 100;

      printf( "Packet sequence number %u received in %u.%02u ms, ttl=%u\n", ntohs( icmpEchoPacket.seq ), elapsedMs, elapsedSmall, LastTTL );
      RepliesReceived++;
      ReplyTime += elapsedTicks;
      break;
    }

    if ( (TIMER_GET_CURRENT( ) - startBios ) > (18*TimeoutSecs) ) {
      printf( "Packet sequence number %u: timeout!\n", ntohs( icmpEchoPacket.seq ) );
      RepliesLost++;
      break;
    }

  }

  // Wait for a second

  uint32_t startWait = TIMER_GET_CURRENT( );
  while ( 1 ) {

    Arp::driveArp( );
    PACKET_PROCESS_SINGLE;

    if ( (TIMER_GET_CURRENT( ) - startWait ) > 18 ) {
      break;
    }
  }


}



void usage( void ) {
  puts( "\nping [options] <ipaddr>\n\n"
        "Options:\n"
        "  -help        Shows this help\n"
        "  -count <n>   Number of packets to send, default is 4\n"
        "  -size <n>    Size of ICMP payload to send, default is 32\n"
        "  -timeout <n> Number of seconds to wait for a response packet" );
  exit( 1 );
}


void parseArgs( int argc, char *argv[] ) {

  int i=1;
  for ( ; i<argc; i++ ) {

    if ( argv[i][0] != '-' ) break;

    if ( stricmp( argv[i], "-help" ) == 0 ) {
      usage( );
    }
    else if ( stricmp( argv[i], "-count" ) == 0 ) {
      i++;
      if ( i == argc ) {
	usage( );
      }
      PacketCount = atoi( argv[i] );
      if ( PacketCount == 0 ) {
	puts( "Bad parameter for -count" );
	usage( );
      }
    }
    else if ( stricmp( argv[i], "-size" ) == 0 ) {
      i++;
      if ( i == argc ) {
	usage( );
      }
      PacketPayload = atoi( argv[i] );
      if ( PacketPayload > ICMP_ECHO_OPT_DATA ) {
	printf( "Bad parameter for -size: Limit is %u\n", ICMP_ECHO_OPT_DATA );
	usage( );
      }
    }
    else if ( stricmp( argv[i], "-timeout" ) == 0 ) {
      i++;
      if ( i == argc ) {
        usage( );
      }
      TimeoutSecs = atoi( argv[i] );
      if ( TimeoutSecs == 0 ) {
        puts( "Bad parameter for -timeout: Should be greater than 0" );
        usage( );
      }
    }
    else {
      printf( "Unknown option: %s\n", argv[i] );
      usage( );
    }

  }

  if ( i == argc ) {
    puts( "A machine name or IP address is required." );
    usage( );
  }

  ServerName = argv[i];

}





void shutdown( int rc ) {
  ping_unhook( );
  Utils::endStack( );
  exit( rc );
}
