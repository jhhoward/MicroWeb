/*

   mTCP Sample.cpp
   Copyright (C) 2011-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Sample program for learning how to use mTCP in
   applications.  This is a cut-down version of the netcat program that
   demonstrates how to initialize the TCP/IP stack, do DNS lookups,
   use TCP/IP sockets, handle keyboard and socket events, and do a proper
   shutdown.

   Changes:

   2011-07-31: Initial release as open source software
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.

*/


#include <bios.h>
#include <dos.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "udp.h"
#include "dns.h"
#include "tcp.h"
#include "tcpsockm.h"



// Function prototypes

static void parseArgs( int argc, char *argv[] );
static void shutdown( int rc );


// Global vars and flags

char     ServerAddrName[80];     // Server name
uint16_t ServerPort;             // Only used when we are a client.
uint16_t LclPort = 2048;         // Local port to use for our socket (0 means not set)

int8_t   Listening = -1;         // Are we the server (1) or the client (0)?


#define RECV_BUFFER_SIZE (1024)
uint8_t recvBuffer[ RECV_BUFFER_SIZE ];





// Ctrl-Break and Ctrl-C handlers.

// Check this flag once in a while to see if the user wants out.
volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}

void __interrupt __far ctrlCHandler( ) {
  // Do Nothing - Ctrl-C is a legal character
}





int main( int argc, char *argv[] ) {

  fprintf( stderr, "mTCP Sample program by M Brutman (mbbrutman@gmail.com) (C)opyright 2012-2020\n\n" );

  // Read command line arguments
  parseArgs( argc, argv );


  // Setup mTCP environment
  if ( Utils::parseEnv( ) != 0 ) {
    exit(-1);
  }

  // Initialize TCP/IP stack
  if ( Utils::initStack( 2, TCP_SOCKET_RING_SIZE, ctrlBreakHandler, ctrlCHandler ) ) {
    fprintf( stderr, "\nFailed to initialize TCP/IP - exiting\n" );
    exit(-1);
  }


  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.


  TcpSocket *mySocket;

  int8_t rc;
  if ( Listening == 0 ) {

    fprintf( stderr, "Resolving server address - press Ctrl-Break to abort\n\n" );

    IpAddr_t serverAddr;

    // Resolve the name and definitely send the request
    int8_t rc2 = Dns::resolve( ServerAddrName, serverAddr, 1 );
    if ( rc2 < 0 ) {
      fprintf( stderr, "Error resolving server\n" );
      shutdown( -1 );
    }

    uint8_t done = 0;

    while ( !done ) {

      if ( CtrlBreakDetected ) break;
      if ( !Dns::isQueryPending( ) ) break;

      PACKET_PROCESS_SINGLE;
      Arp::driveArp( );
      Dns::drivePendingQuery( );

    }

    // Query is no longer pending or we bailed out of the loop.
    rc2 = Dns::resolve( ServerAddrName, serverAddr, 0 );

    if ( rc2 != 0 ) {
      fprintf( stderr, "Error resolving server\n" );
      shutdown( -1 );
    }

    mySocket = TcpSocketMgr::getSocket( );

    mySocket->setRecvBuffer( RECV_BUFFER_SIZE );

    fprintf( stderr, "Server resolved to %d.%d.%d.%d - connecting\n\n",
             serverAddr[0], serverAddr[1], serverAddr[2], serverAddr[3] );

    // Non-blocking connect.  Wait 10 seconds before giving up.
    rc = mySocket->connect( LclPort, serverAddr, ServerPort, 10000 );

  }
  else {

    fprintf( stderr, "Waiting for a connection on port %u. Press [ESC] to abort.\n\n", LclPort );

    TcpSocket *listeningSocket = TcpSocketMgr::getSocket( );
    listeningSocket->listen( LclPort, RECV_BUFFER_SIZE );

    // Listen is non-blocking.  Need to wait
    while ( 1 ) {

      if ( CtrlBreakDetected ) {
        rc = -1;
        break;
      }

      PACKET_PROCESS_SINGLE;
      Arp::driveArp( );
      Tcp::drivePackets( );

      mySocket = TcpSocketMgr::accept( );
      if ( mySocket != NULL ) {
        listeningSocket->close( );
        TcpSocketMgr::freeSocket( listeningSocket );
        rc = 0;
        break;
      }

      if ( _bios_keybrd(1) != 0 ) {

        char c = _bios_keybrd(0);

        if ( (c == 27) || (c == 3) ) {
          rc = -1;
          break;
        }
      }


    }


  }

  if ( rc != 0 ) {
    fprintf( stderr, "Socket open failed\n" );
    shutdown( -1 );
  }

  if ( Listening == 0 ) {
    fprintf( stderr, "Connected!\n\n" );
  }
  else {
    fprintf( stderr, "Connection received from %d.%d.%d.%d:%u\n\n",
             mySocket->dstHost[0], mySocket->dstHost[1],
             mySocket->dstHost[2], mySocket->dstHost[3],
             mySocket->dstPort );
  }


  uint8_t done = 0;

  while ( !done ) {

    // Service the connection
    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    if ( mySocket->isRemoteClosed( ) ) {
      done = 1;
    }

    // Process incoming packets first.

    int16_t recvRc = mySocket->recv( recvBuffer, RECV_BUFFER_SIZE );

    if ( recvRc > 0 ) {
      write( 1, recvBuffer, recvRc );
    }
    else if ( recvRc < 0 ) {
      fprintf( stderr, "\nError reading from socket\n" );
      done = 1;
    }


    if ( CtrlBreakDetected ) {
      fprintf( stderr, "\nCtrl-Break detected\n" );
      done = 1;
    }

    if ( _bios_keybrd(1) ) {

      uint16_t key = _bios_keybrd(0);
      char ch = key & 0xff;

      if ( ch == 0 ) {

        uint8_t ekey = key >> 8;

        if ( ekey == 45 ) { // Alt-X
          done = 1;
        }
        else if ( ekey == 35 ) { // Alt-H
          fprintf( stderr, "\nSample: Press Alt-X to exit\n\n" );
        }

      }
      else {
        int8_t sendRc = mySocket->send( (uint8_t *)&ch, 1 );
        // Should check the return code, but we'll leave that
        // as an exercise to the interested student.
      }

    }


  }

  mySocket->close( );

  TcpSocketMgr::freeSocket( mySocket );

  shutdown( 0 );

  return 0;
}




char *HelpText[] = {
  "\nUsage: sample -target <ipaddr> <port>\n",
  "   or: sample -listen <port>\n\n",
  "<ipaddr> is either a name or numerial IP address\n",
  "<port>   is the port on the server to connect to, or the port\n",
  "         you want to listen on for incoming connections if using -listen\n\n",
  NULL
};


void usage( void ) {
  uint8_t i=0;
  while ( HelpText[i] != NULL ) {
    fprintf( stderr, HelpText[i] );
    i++;
  }
  exit( 1 );
}


void errorMsg( char *msg ) {
  fprintf( stderr, msg );
  usage( );
}



static void parseArgs( int argc, char *argv[] ) {

  int i=1;
  for ( ; i<argc; i++ ) {

    if ( stricmp( argv[i], "-help" ) == 0 ) {
      usage( );
    }
    else if ( stricmp( argv[i], "-target" ) == 0 ) {

      if ( Listening != -1 ) {
        errorMsg( "Specify -listen or -target, but not both\n" );
      }

      i++;
      if ( i == argc ) {
        errorMsg( "Need to provide a target server\n" );
      }

      strcpy( ServerAddrName, argv[i] );

      i++;
      if ( i == argc ) {
        errorMsg( "Need to provide a target port\n" );
      }

      ServerPort = atoi( argv[i] );

      Listening = 0;
    }
    else if ( stricmp( argv[i], "-listen" ) == 0 ) {

      if ( Listening != -1 ) {
        errorMsg( "Specify -listen or -target, but not both\n" );
      }

      i++;
      if ( i == argc ) {
        errorMsg( "Need to specify a port to listen on\n" );
      }

      LclPort = atoi( argv[i] );

      if ( LclPort == 0 ) {
        errorMsg( "Use a non-zero port to listen on\n" );
      }

      Listening = 1;
    }
    else {
      fprintf( stderr, "Unknown option %s\n", argv[i] );
      usage( );
    }

  }

  if ( Listening == -1 ) {
    errorMsg( "Must specify either -listen or -target\n" );
  }

}



static void shutdown( int rc ) {
  Utils::endStack( );
  Utils::dumpStats( stderr );
  exit( rc );
}


