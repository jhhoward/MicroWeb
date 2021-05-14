/*

   mTCP SpdTest.cpp
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


   Description: SpdTest TCP socket benchmarking program

   Changes:

   2011-05-27: Initial release as open source software
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.
   2020-03-06: Bug fix: Outgoing TCP packets are not always carrying
               1460 bytes; the user might have a smaller MTU set.
               Add some stats on avg packet size and warnings for
               suboptimal MTU and MSS sizes.  Remove testing of
               undocumented environment variables.  Remove srcport
               option which was probably never used.

*/


// Purpose
//
// This program can be used to test the speed of TCP sends and receives.
// No real work is done with the data that is sent or received; the point
// is to find the upper limit on what the TCP stack can do given a
// particular machine.  A real use of TCP/IP would presumably do some
// processing which takes time.
//
// This code is based on the original netcat code; everything is stripped out
// that doesn't need to be here.
//
// The code should be run against a fast machine on the local network.  The
// maximum possible MTU size should be used.  Timing varies from machine to
// machine depending on how long interrupts get disabled on the machine, so
// use the other machine to time the test or use wall time.
//
//
// Receive performance testing:
//
// There are two ways to compile this gated by the RECV_INTERFACE flag.
//
// With that flag undefined the code uses a lower level API where received
// data is presented in the packet it was received.  The user code is given
// ownership of the incoming packet buffer and can examine everything.
// This API is unnatural and probably should never be used.  However it does
// skip some memcopies which is important for finding out the maximum speed
// of the machine.
//
// The more realistic test has the flag turned on.  With that API TCP will
// buffer bytes in a receive buffer owned by the socket.  User code gets
// that data using the recv call on the socket.  This is more natural way
// receive data and nearly all of the mTCP programs use this API instead.
// However, don't use it unless you want to find the speed of the stack
// relative to the other API.  Testing memcpy speed is not productive.
//
//
// Send performance testing:
//
// This program cheats and uses a low level API to send data.  With that
// API the user fills in a transmit buffer and hands it to the TCP stack
// to send.  Contrast that to calling "send" on a socket where you provide
// bytes and the TCP library fills in the outgoing packets.  That involves
// extra memcpy operations though, so it is slower.


#include <bios.h>
#include <dos.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"

#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"
#include "tcpsockm.h"
#include "udp.h"
#include "dns.h"


// Function prototypes

void parseArgs( int argc, char *argv[] );
void shutdown( int rc );
void initTcpXmitBuffers( void );


// Globals

uint32_t SpeedTestBytes = 4194304ul;  // 4MB default

char     serverAddrName[80];

uint16_t Port = 0;       // Randomly set unless specified for -listen
IpAddr_t serverAddr;     // Only used when we are a client.
uint16_t serverPort;     // Only used when we are a client.
int8_t   Listening = -1; // Are we the server (1) or the client (0)?
int8_t   Direction = -1; // Sending == 0, Receiving == 1

uint32_t TotalBytesReceived = 0;
uint32_t TotalBytesSent     = 0;




// RECV_INTERFACE is normally disabled.  See the block of comments above
// what what it is and why we leave it disabled.
// 
// #define RECV_INTERFACE


#ifdef RECV_INTERFACE

// Congratulations, you have selected to use the high level recv call to read
// data from your sockets.  This incurs more overhead because TCP will buffer
// incoming data for you, but it is more natural than reading data from
// individual packets.

// Size of the TCP socket recv buffer to use.
uint16_t RECV_BUF_SIZE = 16384;


// If using the higher level TCP recv call to receive data (the RECV_INTERFACE)
// then we need a buffer to receive data into.  We're never actually going to
// do anything with this buffer, but a normal program would write it to disk
// or do something with the contents.
uint16_t WRITE_BUF_SIZE = 16384;

#else

// Tell the TCP library not to allocate a recv buffer in the socket.
uint16_t RECV_BUF_SIZE = 0;

#endif






#define OUTGOINGBUFFERS (TCP_SOCKET_RING_SIZE * 2)

// Used for outgoing data
typedef struct {
  TcpBuffer b;
  uint8_t data[];  // Variable length; depends on the TCP MSS setting.
} DataBuf;




// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}




static char CopyrightMsg1[] = "mTCP SpeedTest by M Brutman (mbbrutman@gmail.com) (C)opyright 2010-2020\n";
static char CopyrightMsg2[] = "Version: " __DATE__ "\n\n";

int main( int argc, char *argv[] ) {

  printf( "%s  %s", CopyrightMsg1, CopyrightMsg2 );

  parseArgs( argc, argv );


  // Initialize TCP/IP

  if ( Utils::parseEnv( ) != 0 ) {
    exit(-1);
  }

  if ( Utils::initStack( 2, OUTGOINGBUFFERS, ctrlBreakHandler, ctrlBreakHandler ) ) {
    puts( "\nFailed to initialize TCP/IP - exiting" );
    exit(-1);
  }

  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.

  if ( MyMTU < 1500 ) {
    printf( "Warning: MTU size is set to %u.  Less than 1500 lowers performance.\n\n",
            MyMTU );
  }

  initTcpXmitBuffers( );

  TcpSocket *mySocket;

  int8_t rc;
  if ( Listening == 0 ) {

    // Resolve the name and definitely send the request
    int8_t rc2 = Dns::resolve( serverAddrName, serverAddr, 1 );
    if ( rc2 < 0 ) {
      printf( "Error resolving server: %s\n", serverAddrName );
      shutdown( -1 );
    }

    uint8_t done = 0;

    while ( !done ) {

      if ( CtrlBreakDetected ) {
	break;
      }

      if ( !Dns::isQueryPending( ) ) break;

      PACKET_PROCESS_SINGLE;
      Arp::driveArp( );
      Tcp::drivePackets( );
      Dns::drivePendingQuery( );

    }

    // Query is no longer pending or we bailed out of the loop.
    rc2 = Dns::resolve( serverAddrName, serverAddr, 0 );

    if ( rc2 != 0 ) {
      printf( "Error resolving server: %s\n", serverAddrName );
      shutdown( -1 );
    }

    Port = rand( ) + 1024;

    printf( "Connecting to %d.%d.%d.%d:%u on local port %u\n\n",
            serverAddr[0], serverAddr[1], serverAddr[2], serverAddr[3],
            serverPort, Port );

    mySocket = TcpSocketMgr::getSocket( );
    mySocket->setRecvBuffer( RECV_BUF_SIZE );
    rc = mySocket->connect( Port, serverAddr, serverPort, 10000 );
  }
  else {

    printf( "Waiting for a connection on port %u. Press [ESC] to abort.\n\n", Port );

    TcpSocket *listeningSocket = TcpSocketMgr::getSocket( );
    listeningSocket->listen( Port, RECV_BUF_SIZE );

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
	printf( "Connection received from %d.%d.%d.%d:%u\n\n",
		mySocket->dstHost[0], mySocket->dstHost[1],
		mySocket->dstHost[2], mySocket->dstHost[3],
		mySocket->dstPort );
	break;
      }

      if ( bioskey(1) != 0 ) {

	char c = bioskey(0);

	if ( (c == 27) || (c == 3) ) {
	  rc = -1;
	  break;
	}
      }


    }


  }

  if ( rc != 0 ) {
    puts( "Socket open failed" );
    shutdown( -1 );
  }


  DosTime_t start;
  gettime( &start );


  // Even though we have a local MTU that sets the largest packet size we can
  // accept, the remote side might have a lower limit.  So take our maximum
  // sent size for a packet from the socket, not from our local MSS.
  int maxPacketSize = mySocket->maxEnqueueSize;

  printf( "MTU: %u, Maximum Segment Size: Local: %u, Remote: %u\n",
          MyMTU, TcpSocketMgr::MSS_to_advertise, mySocket->maxEnqueueSize );
  if ( maxPacketSize < TcpSocketMgr::MSS_to_advertise ) {
    printf( "Warning: Remote MSS is smaller than our MSS" );
  }


#ifdef RECV_INTERFACE

  uint8_t *fileWriteBuffer = (uint8_t *)malloc( WRITE_BUF_SIZE );

  if ( fileWriteBuffer == NULL ) {
    printf( "Not enough memory\n" );
    shutdown( -1 );
  }

  uint16_t bytesToRead = WRITE_BUF_SIZE;

#else

  uint32_t packetsDequeued = 0;

#endif


  uint8_t done = 0;
  uint8_t remoteDone = 0;


  uint16_t bytesRead = 0;

  uint16_t bytesToSend = 0;
  uint16_t bytesSent = 0;
  uint8_t  endOfInputFile = 0;



  if ( Direction == 1 ) {
    // Receiving only; do not send data
    SpeedTestBytes = 0;
    puts( "Receive test: ends automatically when the server closes the socket\n" );
  }
  else {
    printf( "Send test: sending %lu bytes\n\n", SpeedTestBytes );
  }


  while ( !done && !remoteDone ) {

    if ( CtrlBreakDetected ) {
      puts( "\nCtrl-Break detected - aborting" );
      done = 1;
      break;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );



    // Process incoming packets first.

    if ( remoteDone == 0 ) {


#ifdef RECV_INTERFACE

      // TCP/IP has a receive buffer allocated and it is copying new data
      // at the end of the buffer.  The buffer is a ring buffer; each time
      // we read we take from the front of the buffer.
      //
      // This causes two extra memcpy operations - one when the data is
      // processed by the stack, and here when we make this call.
      // This should always be slower than the raw interface.

      // Simulate a user filling up a buffer, and then writing that buffer
      // once it is full

      while ( 1 ) {

	int16_t recvRc = mySocket->recv( fileWriteBuffer+bytesRead, bytesToRead );

	if ( recvRc > 0 ) {

	  TotalBytesReceived += recvRc;
	  bytesRead += recvRc;
	  bytesToRead -= recvRc;

	  if ( bytesToRead == 0 ) {

            // Buffer has been filled; normally it would be written or
            // processed, but here we are just simulating that.

            // Setup to fill the buffer again.
	    bytesToRead = WRITE_BUF_SIZE;
	    bytesRead = 0;
	  }
	}
	else {
          // Error path - the socket is probably dead.
	  break;
	}

      }

#else

      // Raw interface
      //
      // Get a pointer to the contents of the incoming packet.  Normally the
      // user would do some processing on it; here we are just going to throw
      // it away.

      uint8_t *packet;

      while ( packet = ((uint8_t *)mySocket->incoming.dequeue( )) ) {

        packetsDequeued++;

	IpHeader *ip = (IpHeader *)(packet + sizeof(EthHeader) );
	TcpHeader *tcp = (TcpHeader *)(ip->payloadPtr( ));
	uint8_t *userData = ((uint8_t *)tcp)+tcp->getTcpHlen( );
	uint16_t len = ip->payloadLen( ) - tcp->getTcpHlen( );

	// Well, we at least looked in the packet to find the length of
	// user payload.
	TotalBytesReceived += len;

	// Return the packet so it can be reused.
	Buffer_free( packet );
      }

#endif


      // Check to see if they are done with us now.
      remoteDone = mySocket->isRemoteClosed( );
    }


    PACKET_PROCESS_SINGLE;


    // Send path
    //
    // Sending always works the same way; we take the preformatted packets
    // and enqueue them for sending.

    while ( SpeedTestBytes && mySocket->outgoing.hasRoom( ) && mySocket->sent.hasRoom( ) ) {

      DataBuf *buf = (DataBuf *)TcpBuffer::getXmitBuf( );

      if ( buf == NULL ) break;

      uint16_t chunkLen = maxPacketSize;
      if ( SpeedTestBytes < chunkLen ) {
        chunkLen = SpeedTestBytes;
        done = 1;
        mySocket->shutdown( TCP_SHUT_WR );
      }

      SpeedTestBytes -= chunkLen;
      TotalBytesSent += chunkLen;

      buf->b.dataLen = chunkLen;

      int16_t rc = mySocket->enqueue( &buf->b );
      if ( rc ) {
        printf( "Error enqueuing packet: %d\n", rc );
        done = 1;
        mySocket->shutdown( TCP_SHUT_WR );
        break;
      }

    } // end while data to send and room to send in the outgoing queue

  }  // end while


  mySocket->close( );

  TcpSocketMgr::freeSocket( mySocket );

  DosTime_t endTime;
  gettime( &endTime );


  // Compute stats

  uint16_t t = Utils::timeDiff( start, endTime );

  printf( "Elapsed time: %u.%03u, Bytes sent: %lu, Received: %lu\n",
          (t/100), (t%1000), TotalBytesSent, TotalBytesReceived );

  #ifndef RECV_INTERFACE
    if ( packetsDequeued ) {
      uint32_t avgPacketSize = (TotalBytesReceived*10ul) / packetsDequeued;
      if (avgPacketSize % 10 >= 5) {
        avgPacketSize = avgPacketSize / 10ul + 1;
      } else {
        avgPacketSize = avgPacketSize / 10ul ;
      }
      printf( "Data packets received: %lu, Avg packet size: %lu\n",
              packetsDequeued, avgPacketSize );
      if ( (packetsDequeued) > 20 && (avgPacketSize < TcpSocketMgr::MSS_to_advertise) ) {
        printf( "Warning: avgerage packet size was smaller than expected.\n");
      }
    }
  #endif

  puts("");

  shutdown( 0 );

  return 0;
}




char *HelpText =
  "Usage:\n\n"
  "  spdtest <mode> -target <ipaddr> <port> [send options]\n"
  "    or\n"
  "  spdtest <mode> -listen <port> [send options]\n\n"
  "Mode is either:\n"
  "  -receive      Do a receive test\n"
  "  -send         Do a send test\n\n"
  "Send options:\n"
  "  -mb <n>       Megabytes to send during a send test\n";

char *ErrorText[] = {
  "Specify -listen or -target, but not both",
  "Specify -send or -receive, but not both"
};


void usage( void ) {
  printf( HelpText );
  exit( 1 );
}


void errorMsg( char *msg ) {
  printf( "%s\n\n", msg );
  usage( );
  exit( 1 );
}


void parseArgs( int argc, char *argv[] ) {

  uint8_t mbSet = 0;

  int i=1;
  for ( ; i<argc; i++ ) {

    if ( stricmp( argv[i], "-help" ) == 0 ) {
      usage( );
    }

    else if ( stricmp( argv[i], "-target" ) == 0 ) {

      if ( Listening != -1 ) {
	errorMsg( ErrorText[0] );
      }

      i++;
      if ( i == argc ) {
        errorMsg( "Need to provide an IP address with the -target option" );
      }
      strcpy( serverAddrName, argv[i] );

      i++;
      if ( i == argc ) {
	errorMsg( "Need to provide a target port on the server" );
      }
      serverPort = atoi( argv[i] );

      Listening = 0;
    }

    else if ( stricmp( argv[i], "-listen" ) == 0 ) {

      if ( Listening != -1 ) {
	errorMsg( ErrorText[0] );
      }

      i++;
      if ( i == argc ) {
        errorMsg( "Need to provide a port number with the -listen option" );
      }
      Port = atoi( argv[i] );

      if ( Port == 0 ) {
	errorMsg( "Use a non-zero port to listen on" );
      }

      Listening = 1;
    }

    else if ( stricmp( argv[i], "-send" ) == 0 ) {
      if ( Direction != -1 ) {
        errorMsg( ErrorText[1] );
      }
      Direction = 0;
    }

    else if ( stricmp( argv[i], "-receive" ) == 0 ) {
      if ( Direction != -1 ) {
        errorMsg( ErrorText[1] );
      }
      Direction = 1;
    }

    else if ( stricmp( argv[i], "-mb" ) == 0 ) {

      i++;
      if ( i == argc ) {
        errorMsg( "Need to provide a number of megabytes with the -mb option" );
      }

      uint32_t tmp = atoi( argv[i] );
      if ( tmp < 1 || tmp > 64 ) {
        errorMsg( "The value for -mb needs to be between 1 and 64" );
      }

      SpeedTestBytes = tmp * 1048576ul;
      mbSet = 1;
    }

    else {
      printf( "Unknown option %s\n", argv[i] );
      usage( );
    }

  }


  if ( Listening == -1 ) {
    errorMsg( "Must specify either -listen or -target" );
  }

  if ( Direction == -1 ) {
    errorMsg( "Must specify either -send or -receive" );
  }

  if ( mbSet && Direction == 1 ) {
    errorMsg( "-mb only makes sense when sending." );
  }

}


void shutdown( int rc ) {
  Utils::endStack( );
  Utils::dumpStats( stdout );
  exit( rc );
}



// initTcpXmitBuffers
//
// Initialize the buffers that will be used for sending data with a known
// pattern.  This is not necessary at all but it helps debugging if we are
// not sending what looks like random garbage in our packets.

void initTcpXmitBuffers( void ) {

  DataBuf *buffers[ OUTGOINGBUFFERS ];

  // Get pointers to all xmit buffers

  for ( uint16_t i=0; i < OUTGOINGBUFFERS; i++ ) {
    buffers[i] = (DataBuf *)TcpBuffer::getXmitBuf( );
    if ( buffers[i] == NULL ) {
      puts( "Init error: could not fill buffers with dummy data" );
      shutdown( 1 );
    }
  }

  // dataSize is the data portion of the packet, as the TCP and IP headers
  // are already accounted for in the DataBuf structure.  This should
  // match what was allocated in TcpBuffer::init.

  int dataSize = TcpSocketMgr::MSS_to_advertise;

  // Initalize data in the first buffer and then use memcpy to copy it to
  // the other buffers.

  for ( uint16_t j=0; j < dataSize; j++ ) {
    buffers[0]->data[j] = (j%95)+32;
  }

  for ( uint16_t i=1; i < OUTGOINGBUFFERS; i++ ) {
    memcpy( buffers[i]->data, buffers[0]->data, dataSize );
  }


  // Return all buffers so that they can be used.

  for ( uint16_t i=0; i < OUTGOINGBUFFERS; i++ ) {
    TcpBuffer::returnXmitBuf( (TcpBuffer *)buffers[i] );
  }

}
