/*

   mTCP Netcat.cpp
   Copyright (C) 2006-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Netcat for DOS.

   Changes:

   2011-05-27: Initial release as open source software
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.

*/



// Netcat for Dos.  This version only supports TCP ports.
//
// The RECV_INTERFACE define needs some explanation.  My original TCP
// code did not have a ring buffer for incoming data - it expected the
// user app to read raw packets instead of calling something that looks
// like a recv syscall.  This is faster because it avoids a memcpy, but
// it is more error prone because a user might forget to return the buffer
// to the TCP stack, thus running it out of incoming buffers.
//
// If RECV_INTERFACE is set we'll tell the TCP stack to setup a receive
// buffer for incoming data, which is how a normal user would expect
// to get data from a socket.  If it is not set, we make the code snarf
// data from raw packets.  Both options are present so that I can exercise
// both code paths to maintain them.
//
// Data is sent using a pretty low level interface too.  Don't program
// like this in your own apps. :-)  (This is the only app I have that
// still uses these interfaces.  They might not exist in future versions.)



// Netcat user interface
//
// Text mode
//
// The default behavior is to read and write stdin and stdout in text mode.
// This means that when CR/LF is read it will be changed to just a newline
// (LF) character, and when a newline (NL) is written it will be written as
// CR/LF.
//
//   Read from stdin: CR/LF -> NL (LF) -> NL (LF) received on remote side
//   Recv from socket: NL (LF) -> NL (LF) -> CR/LF written to stdout
//
// If you desire the NL to be sent as CR/LF then use the -telnet_nl option.
//
//   Read from stdin: CR/LF -> NL (LF) -> CR/LF received on remote side
//   Recv from socket: CR/LF -> NL (LF) -> CR/LF written to stdout
//
// Text mode also interprets Ctrl-Z as EOF.
//
//
// Binary mode
//
// No interpretation of CR/LF or NL at all - everything is read and written
// as is.  The -telnet_nl option is not valid.  Ctrl-Z is not interpreted.
//
//   Read from stdin: CR/LF -> CR/LF -> CR/LF
//   Recv from socket: CR/LF -> CR/LF -> CR/LF



// Keyboard handling
//
// Keyboard handling is close to the way that reading from stdin works.
// The key difference is in the Enter key.
//
//   BINARY mode: Enter generates a CR.
//   Text mode: Enter generates a Unix style NL (LF)
//   Text mode with Telnet: Enter generates Telnet style NL (CR/LF)
//
// Pressing Ctrl-M or pressing Ctrl-J generates CR and LF directly
// with no translation.  (LF may still be interpreted as a NL
// on the other side.)




//
// Default is that if the other side closes we exit the loop and close
//   right away.  Need a flag to override this behavior so that we close
//   only when stdin runs out.
//
// Default is that when we run out of stdin we close our half of the
//   connection.  Need a flag to determine how long to wait for the other
//   side to acknowledge our close before exiting the read loop and forcing
//   the socket closed.


#include <bios.h>
#include <dos.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <malloc.h>

#include "types.h"

#include "timer.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"
#include "tcpsockm.h"
#include "udp.h"
#include "dns.h"



// RECV_INTERFACE adds another layer of buffering, which is more
// convenient for the user but slower.  Use this option for testing,
// but leave it turned off for performance.

// #define RECV_INTERFACE
#ifdef RECV_INTERFACE
uint16_t RCV_BUF_SIZE = 8192;
#else
uint16_t RCV_BUF_SIZE = 0;
#endif





#define CR (13)   // Carriage return
#define LF (10)   // Line feed
#define NL (10)   // Unix style newline


// Function prototypes

static void parseArgs( int argc, char *argv[] );
static void parseEnv( void );
static void shutdown( int rc );
static void checkStdinStdout( void );

static int8_t  writeOutput( uint8_t *outputBuffer, uint16_t outputBufferLen );
static uint8_t lastCharWasCR = 0;


// Global vars and flags

char     ServerAddrName[80];     // Server name
uint16_t ServerPort;             // Only used when we are a client.
uint16_t LclPort = 0;            // Local port to use for our socket (0 means not set)

int8_t   Listening = -1;         // Are we the server (1) or the client (0)?

uint8_t  Verbose = 0;            // Verbose mode flag
uint8_t  BinaryMode = 0;         // Are we in ASCII (default) or BINARY mode?
uint8_t  Telnet_NL = 0;          // Are we sending newlines as CR/LF?
uint8_t  LocalEcho = 0;          // Are we providing local echo?
uint32_t WaitAfterClose = 0;     // How long to wait for incoming data after stdin closes (in ms)
int8_t   CloseOnRemoteClose = 1; // Close immediately if other side closes


// Internal flags and vars

uint8_t  IsStdinFile = 0;
uint8_t  IsStdoutFile = 0;

uint32_t TotalBytesReceived = 0;
uint32_t TotalBytesSent     = 0;

// Can be overridden by environment variables
uint16_t READ_BUF_SIZE  = 8192;  // Size of buffer used for reading stdin
uint16_t WRITE_BUF_SIZE = 8192;  // Size of buffer used for writing stdout




// Used for outgoing data
typedef struct {
  TcpBuffer b;
  uint8_t data[1460];
} DataBuf;




// Ctrl-Break and Ctrl-C handlers.

// Check this flag once in a while to see if the user wants out.
volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}

void __interrupt __far ctrlCHandler( ) {
  // Do Nothing - Ctrl-C is a legal character
}



static char CopyrightMsg1[] = "mTCP Netcat by M Brutman (mbbrutman@gmail.com) (C)opyright 2007-2020\n";
static char CopyrightMsg2[] = "Version: " __DATE__ "\n\n";


int main( int argc, char *argv[] ) {

  fprintf( stderr, "%s  %s", CopyrightMsg1, CopyrightMsg2 );

  parseArgs( argc, argv );
  parseEnv( );


  // Allocate buffer space early to see if we have enough memory.

  uint8_t *fileReadBuffer = (uint8_t *)malloc( READ_BUF_SIZE );
  uint8_t *fileWriteBuffer = (uint8_t *)malloc( WRITE_BUF_SIZE );

  if ( (fileReadBuffer == NULL) || (fileWriteBuffer == NULL) ) {
    fprintf( stderr, "\nFailed to allocate file buffers - try reducing them.\n" );
    exit(-1);
  }


  // Initialize TCP/IP

  if ( Utils::parseEnv( ) != 0 ) {
    exit(-1);
  }

  if ( Utils::initStack( 2, TCP_SOCKET_RING_SIZE, ctrlBreakHandler, ctrlCHandler ) ) {
    fprintf( stderr, "\nFailed to initialize TCP/IP - exiting\n" );
    exit(-1);
  }


  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.



  // Find out if we are interactive or redirecting to files.  (Note that
  // stdin and stdout are handled independently.)
  checkStdinStdout( );


  // Set stdin and stdout to binary mode if requested.  (These are handled
  // together!)
  if ( BinaryMode ) {
    setmode( 0, O_BINARY );
    setmode( 1, O_BINARY );
  }


  // Some debug output
  if ( Verbose ) {
    fprintf( stderr, "IsStdinFile: %s  IsStdoutFile: %s  BinaryMode: %s  Telnet_NL: %s\n", (IsStdinFile?"yes":"no"), (IsStdoutFile?"yes":"no"), (BinaryMode?"yes":"no"), (Telnet_NL?"yes":"no") );
    fprintf( stderr, "Close after Close received: %s  Wait seconds after stdin closes: %lu\n", (CloseOnRemoteClose?"yes":"no"), WaitAfterClose );
    fprintf( stderr, "File read buffer: %u  File write buffer: %u  TCP recv buffer: %u\n\n", READ_BUF_SIZE, WRITE_BUF_SIZE, RCV_BUF_SIZE );
  }


  // WaitAfterClose should be in milliseconds
  WaitAfterClose = WaitAfterClose * 1000ul;


  // Pick a random number for the source port.  (If we are listening then
  // LclPort is already set and will not be altered.)
  // Utils::initStack has already seeded the random number generator.
  if ( LclPort == 0 ) {
    LclPort = rand( ) + 1024;
  }


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
    rc2 = Dns::resolve( ServerAddrName, serverAddr, 0 );

    if ( rc2 != 0 ) {
      fprintf( stderr, "Error resolving server\n" );
      shutdown( -1 );
    }

    mySocket = TcpSocketMgr::getSocket( );

    #ifdef RECV_INTERFACE
    mySocket->setRecvBuffer( RCV_BUF_SIZE );
    #endif

    fprintf( stderr, "Server resolved to %d.%d.%d.%d - connecting\n\n", serverAddr[0], serverAddr[1], serverAddr[2], serverAddr[3] );

    rc = mySocket->connect( LclPort, serverAddr, ServerPort, 10000 );
  }
  else {

    fprintf( stderr, "Waiting for a connection on port %u. Press [ESC] to abort.\n\n", LclPort );

    TcpSocket *listeningSocket = TcpSocketMgr::getSocket( );
    listeningSocket->listen( LclPort, RCV_BUF_SIZE );

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



  // By this point we have a connection ...

  DosTime_t start;
  gettime( &start );

  int maxPacketSize = MyMTU - (sizeof(IpHeader) + sizeof(TcpHeader) );


  uint8_t errorStop = 0;
  uint8_t remoteClosed = 0;
  uint8_t stdinClosed = 0;

  clockTicks_t stdinClosedTime;

  uint16_t bytesRead = 0;
  uint16_t bytesToRead = WRITE_BUF_SIZE; // Bytes to read from socket

  uint16_t bytesToSend = 0;
  uint16_t bytesSent = 0;

  uint8_t  endOfInputFile = 0;

  uint8_t  isKeyCached = 0;  // Keyboard handling - did we have a keystroke?
  uint16_t cachedKey;        // Saved keystroke, if we have one.


  while ( 1 ) {

    if ( CtrlBreakDetected ) {
      fprintf( stderr, "\nNetcat: Ctrl-Break detected\n" );
      errorStop = 2;
      break;
    }

    // Service the connection
    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );


    // Process incoming packets first.
    if ( remoteClosed == 0 ) {

      // If our stdin closed already then we will only take bytes from the
      // socket for a limited time.  (The default is 0, so this will normally
      // trip quickly.
      if ( stdinClosed ) {
	if ( Timer_diff( stdinClosedTime, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS(WaitAfterClose) ) {
	  // Wait time is exceeded.  Break out of the main loop.
	  break;
	}
      }


      #ifdef RECV_INTERFACE

      while ( 1 ) {

	int16_t recvRc = mySocket->recv( fileWriteBuffer+bytesRead, bytesToRead );

	if ( recvRc > 0 ) {

	  TotalBytesReceived += recvRc;
	  bytesRead += recvRc;
	  bytesToRead -= recvRc;

	  if ( (bytesToRead == 0) || (IsStdoutFile == 0) ) {
	    if ( writeOutput( fileWriteBuffer, bytesRead ) != 0 ) {
	      errorStop = 1;
	      break;
	    }
	    bytesToRead = WRITE_BUF_SIZE;
	    bytesRead = 0;
	  }
	}
	else {
	  // No data or an error - break this local receive loop
	  // if ( recvRc < 0 ) {
	  //   if ( mySocket->isClosed( ) ) {
	  //     // It is only an error if the socket really went closed.
	  //     errorStop = 3;
	  //   }
	  // }
	  break;
	}

      }

      #else

      uint8_t *packet;

      while ( packet = ((uint8_t *)mySocket->incoming.dequeue( )) ) {

	IpHeader *ip = (IpHeader *)(packet + sizeof(EthHeader) );
	TcpHeader *tcp = (TcpHeader *)(ip->payloadPtr( ));
	uint8_t *userData = ((uint8_t *)tcp)+tcp->getTcpHlen( );
	uint16_t len = ip->payloadLen( ) - tcp->getTcpHlen( );

	TotalBytesReceived += len;


	// Copy to our filebuffer.  If we are close to the end of
	// the file buffer, write it out.

	memcpy( fileWriteBuffer+bytesRead, userData, len );
	bytesRead += len;

	// Get rid of the incoming buffer as soon as possible.
	Buffer_free( packet );

	if ( ((WRITE_BUF_SIZE - bytesRead) < MyMTU) || (IsStdoutFile == 0) ) {
	  if ( writeOutput( fileWriteBuffer, bytesRead ) != 0 ) {
	    errorStop = 1;
	    break;
	  }
	  bytesRead = 0;
	}

      }
      #endif


      // If we exited the receive loop early because of an error, break out
      // of the main loop.
      if ( errorStop ) break;


      remoteClosed = mySocket->isRemoteClosed( );

      if ( remoteClosed ) {
	if ( Verbose ) fprintf( stderr, "\nNetcat: Remote side closed: " );
	if ( CloseOnRemoteClose ) {
	  // User wants to hang around after the other side closes.
	  if ( Verbose ) fprintf( stderr, "Closing our side\n" );
	  break;
	}
	else {
	  if ( Verbose ) fprintf( stderr, "-nocorc used, leaving our side open!\n" );
	}
      }

    }
    else {
      // It is possible that the other side closed before we did, or even
      // simultaneously.  If they are closed already and we are closed,
      // then leave the main loop.
      if ( stdinClosed ) break;
    }



    // Check for keyboard input.

    if ( (isKeyCached == 0) && bioskey(1) ) {

      // We are not holding a key and a new keystroke is ready.

      uint16_t key = bioskey( 0 );
      uint8_t ekey = key >> 8;

      // Is this a special key?
      if ( (key & 0xff) == 0 ) {


	if ( ekey == 45 ) { // Alt-X
	  stdinClosed = 1;
	  stdinClosedTime = TIMER_GET_CURRENT( );
	  mySocket->shutdown( TCP_SHUT_WR );
	  if ( Verbose ) fprintf( stderr, "\nNetcat: Local side closed\n" );
	}
	else if ( ekey == 18 ) { // Alt-E
	  LocalEcho = !LocalEcho;
	  if ( LocalEcho ) {
	    sound(500); delay(50); sound(750); delay(50); nosound( );
	  }
	  else {
	    sound(500); delay(50); nosound( );
	  }
	}
	else if ( ekey == 31 ) { // Alt-S
	  fprintf( stderr, "\nNetcat: Bytes Sent: %lu  Rcvd: %lu  Stdin closed: %s  Remote closed: %s\n",
			   TotalBytesSent, TotalBytesReceived, (stdinClosed?"yes":"no"), (remoteClosed?"yes":"no") );
	}
	else if ( ekey == 35 ) { // Alt-H
	  fprintf( stderr, "\n\nNetcat quick help:\n" );
	  fprintf( stderr, "  Alt-X: Close   Alt-E: Toggle Echo   Alt-S: Status   Ctrl-Break: Exit\n\n" );
	}

      }

      else {

	// If we are being redirected from a file throw the key away.
	// If we are reading interactively from the keyboard save the
	// key

	if ( IsStdinFile == 0 ) {
	  isKeyCached = 1;
	  cachedKey = key;
	}

      }

    }




    // Do we have outgoing packets?

    if ( (IsStdinFile == 0) ) {

      if ( isKeyCached ) {

	// Special keys are already handled by this point, so it is a normal
	// key we are dealing with.

	isKeyCached = 0;

	uint16_t key = cachedKey;

	uint8_t keyChar = key & 0xff;
	uint8_t ekey = key >> 8;

	uint8_t tmpBuf[2];
	uint8_t tmpBufLen = 1; // Probably always one char generated
	tmpBuf[0] = keyChar;   // Probably always the original char

	if ( (BinaryMode == 0) && (keyChar == 3) ) {
	  fprintf( stderr, "\nNetcat: Ctrl-C detected and sent.  Use Alt-X to quit.\n" );
	}

	else if ( keyChar == 26 ) {
	  if ( BinaryMode == 0 ) {
	    stdinClosed = 1;
	    stdinClosedTime = TIMER_GET_CURRENT( );
	    mySocket->shutdown( TCP_SHUT_WR );
	    if ( Verbose ) fprintf( stderr, "\nNetcat: Local side closed\n" );
	  }
	  else {
	    fprintf( stderr, "\nNetcat: Ctrl-Z detected and sent in binary mode.  Use Alt-X to signal EOF.\n" );
	  }
	}

	else if ( (keyChar == 13) && (ekey == 0x1c) ) {
	  // This is the Enter key by itself
	  if ( BinaryMode == 0 ) {
	    if ( Telnet_NL == 0 ) {
	      // Convert to Unix NL
	      tmpBuf[0] = NL;
	      }
	    else {
	      tmpBuf[0] = CR;
	      tmpBuf[1] = LF;
	      tmpBufLen = 2;
	    }
	  }
	}


	if ( stdinClosed == 0 ) {

	  if ( LocalEcho ) {
	    putchar( tmpBuf[0] );
	    if ( tmpBufLen == 2 ) putchar( tmpBuf[1] );

            #if defined ( __WATCOMC__ ) || defined ( __WATCOM_CPLUSPLUS__ )
            // This is brutal but needed for Watcom.  It shouldn't hurt
            // since the user is typing interactively.
            fflush( stdout );
            #endif
	  }


	  DataBuf *buf = (DataBuf *)TcpBuffer::getXmitBuf( );

	  if ( buf != NULL ) {
	    buf->data[0] = tmpBuf[0];
	    buf->data[1] = tmpBuf[1];
	    buf->b.dataLen = tmpBufLen;

            // Fixme: check return code
	    mySocket->enqueue( &buf->b );
	    TotalBytesSent++;
	  }
	  else {
	    fprintf(stderr, "\nNetcat: Warning - no transmit buffers!\n");
	  }

	}

      }

    } // End if reading interactively


    else {

      // If we are out of data to send and we have not already encountered
      // the end of file, then try to read some more data.

      if ( (bytesToSend == 0) && (!endOfInputFile) ) {

	bytesToSend = read( 0, fileReadBuffer, READ_BUF_SIZE );
	bytesSent = 0;
	if ( bytesToSend == 0 ) {
	  endOfInputFile = 1;
	  stdinClosed = 1;
	  stdinClosedTime = TIMER_GET_CURRENT( );
	  mySocket->shutdown( TCP_SHUT_WR );
	  fprintf( stderr, "\nNetcat: EOF detected on STDIN\n" );
	}

      }

      // Push packets out


      while ( bytesToSend ) {

	if ( !mySocket->outgoing.hasRoom() ) break;

	DataBuf *buf = (DataBuf *)TcpBuffer::getXmitBuf( );

	if ( buf == NULL ) break;

	uint16_t bytesConsumed;
	uint16_t packetLen;

	if ( (BinaryMode == 1) || (Telnet_NL == 0) ) {

	  bytesConsumed = maxPacketSize;
	  if ( bytesToSend < maxPacketSize ) {
	    bytesConsumed = bytesToSend;
	  }

	  memcpy( buf->data,
		  fileReadBuffer+bytesSent,
		  bytesConsumed );

	  packetLen = bytesConsumed;

	}
	else {

	  uint16_t offset = 0;

	  uint16_t limit = maxPacketSize-1;
	  if ( bytesToSend < limit ) {
	    limit = bytesToSend;
	  }

	  // Scan for NL and add a CR if we find it.
          uint16_t i;
	  for ( i=0; i < limit; i++ ) {
	    if ( fileReadBuffer[bytesSent+i] == 10 ) {
	      buf->data[i] = 13; buf->data[i+1] = 10; offset = 1; i++; break;
	    }
	    else {
	      buf->data[i] = fileReadBuffer[bytesSent+i];
	    }
	  }

	  bytesConsumed = i;
	  packetLen = bytesConsumed + offset;
	}

	TotalBytesSent += packetLen;
	buf->b.dataLen = packetLen;

	int16_t rc = mySocket->enqueue( &buf->b );
	if ( rc ) {
	  fprintf( stderr, "\nNetcat: Error enqueuing packet: %d\n", rc );
	  errorStop = 4;
	  mySocket->shutdown( TCP_SHUT_WR );
	  break;
	}

	bytesSent += bytesConsumed;
	bytesToSend -= bytesConsumed;

      }


    } // end if reading from a file

  }


  // Flush remaining socket receive buffer.

  if ( bytesRead ) {
    if ( writeOutput( fileWriteBuffer, bytesRead ) != 0 ) errorStop = 1;
  }

  // It is possible that we were holding onto a CR that we never wrote.
  // (This happens if we call writeOutput and the very last char was a CR.)

  if ( lastCharWasCR ) {
    write( 1, "\xD", 1 );
  }


  if ( errorStop ) {
    fprintf(stderr, "\nWarning: netcat ended early: " );
    switch ( errorStop ) {
      case 1: fprintf( stderr, "Probable error writing output\n\n" ); break;
      case 2: fprintf( stderr, "You pressed Ctrl-Break\n\n" ); break;
      case 3: fprintf( stderr, "Error on socket receive\n\n" ); break;
      case 4: fprintf( stderr, "Error on socket send\n\n" ); break;
      default: fprintf( stderr, "Error code: %d\n\n", errorStop ); break;
    }
  }


  mySocket->close( );

  TcpSocketMgr::freeSocket( mySocket );


  DosTime_t endTime;
  gettime( &endTime );


  // Compute stats

  uint16_t t = Utils::timeDiff( start, endTime );

  fprintf( stderr, "\nElapsed time: %u.%02u   Bytes sent: %ld  Received: %ld\n",
		   (t/100), (t%100), TotalBytesSent, TotalBytesReceived );

  shutdown( 0 );

  return 0;
}



// writeOutput - handles all writing to stdout
//
// If we are in BINARY mode or not translating NL then this is easy.
//
// If we ae in TEXT mode and translating NL then we need to find incoming
// CR/LF pairs and reduce them to just NL.  DOS will turn around and expand
// this to CR/LF.  If we don't do this, DOS will see CR/LF and write CR/CR/LF
// which is wrong.
//
// We might be able to sleaze out and just mark stdout as a binary file
// but I'm not sure of what other semantics in DOS might change if we do.
// I know that on input handling Ctrl-Z is EOF even if the file size is
// longer.  For now don't do this - we are not talking about a lot of code
// here and if you have TEXT mode and are translating NL then you are not
// performance sensitive.
//
// (As an aside we should probably be scanning for ctrl-Z and
// close stdout if we hit it, but if there is additional good data after
// ctrl-z arrives then we can legitimately claim that is an error on the
// sending side.)
//
// Returns:
//
//  0 if all goes well
// -1 on error

int8_t writeOutput( uint8_t *outputBuffer, uint16_t outputBufferLen ) {

  int8_t rc = 0; // Assume successful

  if ( (BinaryMode == 1) || (Telnet_NL == 0) ) {
    // If write doesn't write the full amount it is probably an error.
    if ( write( 1, outputBuffer, outputBufferLen ) != outputBufferLen ) rc = -1;
  }
  else {

    // Corner case: If a CR/LF pair was split across two calls to this
    // function we would normally miss it and wind up with CR/CR/LF in
    // the file.  To prevent this, if there is one unconsumed char in
    // a buffer and it is a CR then lastCharWasCR gets set on.  On entry
    // if we see that and the first char is LF, then swallow that previous
    // CR.  If the first char is not LF then do an ugly single byte write
    // of the CR.

    if ( lastCharWasCR && (outputBuffer[0] != LF) ) {
      if ( write( 1, "\xD", 1 ) != 1 ) return -1;
    }

    // Need to scan for CR/LF and convert to NL (LF) before giving to DOS.
    // This is only needed when expecting Telnet style newlines.

    uint16_t bytesWritten = 0;  // Starting place in buffer to write
    uint16_t curCharIndex = 0;  // How far into the buffer have we scanned

    while ( curCharIndex < outputBufferLen ) {

      // Scan ahead to next CR/LF pair.

      while ( curCharIndex < outputBufferLen ) {

	if ( outputBuffer[curCharIndex] == CR ) {
	  if ( curCharIndex < (outputBufferLen-1) ) {
	    if ( outputBuffer[curCharIndex+1] == LF ) {
	      outputBuffer[curCharIndex] = NL;
	      if ( write( 1, outputBuffer+bytesWritten, curCharIndex-bytesWritten+1 ) != (curCharIndex-bytesWritten+1) ) return -1;
	      curCharIndex += 2;
	      bytesWritten = curCharIndex;
	      break;
	    }
	  }
	}

	curCharIndex++;

      }

    }

    // Final write
    if ( bytesWritten < curCharIndex ) {

      if ( ((curCharIndex - bytesWritten) == 1 ) && (outputBuffer[bytesWritten] == CR) ) {
	lastCharWasCR = 1;
      }
      else {
	lastCharWasCR = 0;
	if ( write( 1, outputBuffer+bytesWritten, curCharIndex-bytesWritten ) != curCharIndex-bytesWritten ) rc = -1;
      }
    }

  }

  return rc;
}







char *HelpText[] = {
  "\nnc -target <ipaddr> <port> [options]\n",
  "nc -listen <port> [options]\n\n",
  "Options:\n",
  "  -help        Shows this help\n",
  "  -verbose     Print extra status messages\n",
  "  -bin         Treat files as binary\n",
  "  -telnet_nl   Send and receive newline (NL) as telnet newline (CR/LF)\n",
  "  -echo        Turn on local echoing when in interactive mode\n",
  "  -w <n>       How long to wait for network traffic after stdin closes\n",
  "  -nocorc      Do not Close on remote close\n",
  "  -srcport <n> Specify local port number for connections\n\n",
  "You can redirect using stdin and stdout, or use interactively.\n",
  NULL
};

char *ErrorText[] = {
  "Specify -listen or -target, but not both and only once\n"
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
    else if ( stricmp( argv[i], "-verbose" ) == 0 ) {
      Verbose = 1;
    }
    else if ( stricmp( argv[i], "-bin" ) == 0 ) {
      BinaryMode = 1;
    }
    else if ( stricmp( argv[i], "-telnet_nl" ) == 0 ) {
      Telnet_NL = 1;
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

    else if ( stricmp( argv[i], "-echo" ) == 0 ) {
      LocalEcho = 1;
    }

    else if ( stricmp( argv[i], "-srcport" ) == 0 ) {

      if ( Listening == -1 ) {
	errorMsg( "Specify a target to connect to first\n" );
      }
      if ( Listening == 1 ) {
	errorMsg( "The -srcport option is not valid with -listen\n" );
      }

      i++;
      if ( i == argc ) {
	errorMsg( "Need to specify a port number with -srcport\n" );
      }

      LclPort = atoi( argv[i] );
    }

    else if ( stricmp( argv[i], "-nocorc" ) == 0 ) {
      CloseOnRemoteClose = 0;
    }

    else if ( stricmp( argv[i], "-w" ) == 0 ) {
      i++;
      if ( i == argc ) {
	fprintf( stderr, "Need to specify the number of seconds with -w\n" );
	exit(-1);
      }
      WaitAfterClose = atoi( argv[i] );
    }
    else {
      fprintf( stderr, "Unknown option %s\n", argv[i] );
      usage( );
    }

  }

  if ( BinaryMode && Telnet_NL ) {
    errorMsg( "Do not specify -bin and -telnet_nl together\n" );
  }


  if ( Listening == -1 ) {
    errorMsg( "Must specify either -listen or -target\n" );
  }

}



static void parseEnv( void ) {

  char *c;
  #ifdef RECV_INTERFACE
  c = getenv( "TCPRCVBUF" );
  if ( c!= NULL ) {
    RCV_BUF_SIZE = atoi( c );
  }
  #endif

  c = getenv( "READBUF" );
  if ( c!= NULL ) {
    uint16_t tmp = atoi( c );
    if ( (tmp >= 512) && (tmp <=32768) ) READ_BUF_SIZE = tmp;
  }

  c = getenv( "WRITEBUF" );
  if ( c!= NULL ) {
    uint16_t tmp = atoi( c );
    if ( (tmp >= 512) && (tmp <=32768) ) WRITE_BUF_SIZE = tmp;
  }
}




static void shutdown( int rc ) {
  Utils::endStack( );
  Utils::dumpStats( stderr );
  exit( rc );
}



static void checkStdinStdout( void ) {

  union REGS inregs, outregs;

  inregs.x.bx = 0;
  inregs.x.ax = 0x4400;
  intdos( &inregs, &outregs );

  if ( (outregs.x.cflag == 0) && ((outregs.x.dx & 0x0080) == 0) ) {
    IsStdinFile = 1;
  }

  inregs.x.bx = 1;
  inregs.x.ax = 0x4400;
  intdos( &inregs, &outregs );

  if ( (outregs.x.cflag == 0) && ((outregs.x.dx & 0x0080) == 0) ) {
    IsStdoutFile = 1;
  }

}


