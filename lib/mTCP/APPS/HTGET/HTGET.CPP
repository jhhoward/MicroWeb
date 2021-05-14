/*

   mTCP HTGet.cpp
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


   Description: Htget for DOS, inspired by a version that Ken Yap
   (ken@syd.dit.csiro.au) wrote in 1997.  

   Changes:

   2011-07-17: Initial version
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.
   2018-10-29: Add more error checking on file writes;
               Add a quiet mode.
   2019-01-04: Rewrite to support chunked transfers.

*/


#include <bios.h>
#include <io.h>
#include <fcntl.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

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


#define HOSTNAME_LEN        (80)
#define PATH_LEN           (256)
#define OUTPUTFILENAME_LEN  (80)

#define TCP_RECV_BUFFER  (16384)
#define INBUFSIZE         (8192)
#define LINEBUFSIZE        (512)
#define SOCK_PRINTF_SIZE  (1024)

#define CONNECT_TIMEOUT  (10000ul)



// Options set by user supplied args

bool     Verbose = false;
bool     QuietMode = false;
bool     HeadersOnly = false;
bool     ShowHeaders = false;
bool     ModifiedSince = false;

enum HttpVersion { HTTP_v09, HTTP_v10, HTTP_v11 };
HttpVersion HttpVer = HTTP_v11;


// Globals filled in as a result of an HTTP response

bool     NotModified = false;
bool     TransferEncoding_Chunked = false;
bool     ExpectedContentLengthSent = false;
uint32_t ExpectedContentLength = 0;
uint16_t HttpResponse = 500;



// Server and file information

char Hostname[ HOSTNAME_LEN ];
char Path[ PATH_LEN ];
char outputFilename[OUTPUTFILENAME_LEN] = {0};

char *PassInfo = NULL;

IpAddr_t HostAddr;
uint16_t ServerPort = 80;

TcpSocket *sock;


// Buffers

char lineBuffer[ LINEBUFSIZE ];

uint8_t  *inBuf;                 // Input buffer
uint16_t  inBufStartIndex = 0;   // First unconsumed char in inBuf
uint16_t  inBufLen=0;            // Index to next char to fill


// Misc

bool     IsStdoutFile = false;


// Timestamp handling

struct stat statbuf;
struct tm *mtime;

char *dayname[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *monthname[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };



// Return code table
//
// If we get a specific HTTP return code we can map it to a program
// return code with this table.
//
// In general, a return code 0 means "good communications but unrecognized
// HTTP response code".  A return code 1 is some form of hard error.  Anything
// else that is interesting should be described by this table.


typedef struct {
  uint16_t httpCodeStart;
  uint16_t httpCodeEnd;
  uint8_t  dosRc;
  uint8_t  reserved;
} ReturnCodeRec_t;


ReturnCodeRec_t rcMappingTable[] = {

  { 100, 199, 10 },  // Default for 100 to 199 if not mapped out

  { 200, 299, 20 },  // Default for 200 to 299 if not mapped out
  { 200, 200, 21 },  // OK
  { 201, 201, 22 },  // Created
  { 202, 202, 23 },  // Accepted
  { 203, 203, 24 },  // Non-Authoritative Information
  { 204, 204, 25 },  // No Content
  { 205, 205, 26 },  // Reset Content
  { 206, 206, 27 },  // Partial Content

  { 300, 399, 30 },  // Default for 300 to 399 if not mapped out
  { 300, 300, 31 },  // Multiple Choices
  { 301, 301, 32 },  // Moved Permanently
  { 302, 302, 33 },  // Found
  { 303, 303, 34 },  // See Other
  { 304, 304, 35 },  // Not Modified 
  { 305, 305, 36 },  // Use Proxy
  { 307, 307, 37 },  // Temporary Redirect

  { 400, 499, 40 },  // Default for 400 to 499 if not mapped out
  { 400, 400, 41 },  // Bad Request
  { 401, 401, 42 },  // Unauthorized
  { 402, 402, 43 },  // Payment Required
  { 403, 403, 44 },  // Forbidden
  { 404, 404, 45 },  // Not Found
  { 410, 410, 46 },  // Gone

  { 500, 599, 50 },  // Default for 500 to 599 if not mapped out
  { 500, 500, 51 },  // Internal Server Error
  { 501, 501, 52 },  // Not Implemented
  { 503, 503, 53 },  // Service Unavailable
  { 505, 505, 54 },  // HTTP Version Not Supported
  { 509, 509, 55 },  // Bandwidth Limit Exceeded

};


uint8_t mapResponseCode( uint16_t httpRc ) {

  uint8_t rc = 0;

  for ( uint8_t i = 0; i < sizeof(rcMappingTable)/sizeof(ReturnCodeRec_t); i++ ) {

    if ( httpRc >= rcMappingTable[i].httpCodeStart && httpRc <= rcMappingTable[i].httpCodeEnd ) {
      rc = rcMappingTable[i].dosRc;
      if ( httpRc == rcMappingTable[i].httpCodeStart && httpRc == rcMappingTable[i].httpCodeEnd ) {
        // Found our exact code - no point in scanning the rest of the table
        break;
      }
    }

  }

  return rc;
}



// Error and Verbose message handling
//
// Yes, these are very similar ... 

inline void errorMessage( char *fmt, ... ) {
  if ( !QuietMode ) {
    va_list ap;
    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
  }
}

inline void verboseMessage( char *fmt, ... ) {
  if ( Verbose ) {
    va_list ap;
    va_start( ap, fmt );
    vfprintf( stderr, fmt, ap );
    va_end( ap );
  }
}



// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}




uint8_t userWantsOut( void ) {

  if ( CtrlBreakDetected ) {
    errorMessage( "Ctrl-Break detected - aborting!\n" );
    return 1;
  }

  if ( bioskey(1) != 0 ) {
    char c = bioskey(0);
    if ( (c == 27) || (c == 3) ) {
      errorMessage( "Esc or Ctrl-C detected - aborting!\n");
      return 1;
    }
  }

  return 0;
}




// Ends the TCP/IP stack and ends the program in a sane way.
// Use this after TCP/IP has been successfully initialized.

static void shutdown( int rc ) {
  verboseMessage( "DOS errorlevel code: %d\n", rc );
  Utils::endStack( );
  exit( rc );
}



// base64Encoder
//
// Output strings are 33% larger than input strings!
// Returns 0 if successful, -1 if the buffer is not big enough.

static char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int8_t base64Encoder(const char *in, char *out_p, uint16_t bufferLen) {

  char *out = out_p;

  while ( *in ) {

    // Ensure we have enough room in the output buffer.

    if ( (out-out_p) + 5 > bufferLen ) {
      *out = 0;
      return -1;
    }


    // We use a 24 bits of a 32 bit integer because 24 is divisible by
    // both 8 bits (the input) and six bits (the output).  Gather three bytes
    // of input and send four bytes as encoded output.  Keep track of
    // how much we filled the 32 bit word so we know how much padding
    // is required.

    uint32_t t = (uint32_t)(*in++) << 16;
    uint8_t padChars = 2;

    if ( *in ) {
      t = t | (*in++ << 8);
      padChars--;
      if ( *in ) {
        t = t | *in++;
        padChars--;
      }
    }

    // By this point t has up to three characters of input and in is pointing
    // to more input or the terminating null character.

    *out++ = base64Chars[ t>>18 ];  
    *out++ = base64Chars[ (t>>12) & 0x3F ];
    
    if ( padChars == 0 ) {
        *out++ = base64Chars[ (t>>6) & 0x3F ];
        *out++ = base64Chars[ t & 0x3F ];
    }
    else if (padChars == 1) {
      *out++ = base64Chars[ (t>>6) & 0x3F ];
      *out++ = '=';
    }
    else {
        *out++ = '=';
        *out++ = '=';
    }
   
  }
         
  *out = 0;

  return 0;
}


enum StopCode {
  NotDone,
  UserBreak,
  FileError,
  SocketError,
  SocketClosed,
  ProtocolError,
  AllDoneAndGood
};

char *StopCodeStrings[] = {
  "Not Done",
  "User Break",
  "File Error",
  "Socket Error",
  "Socket Closed",
  "Protocol Error",
  "All Finished"
};



// drainAndCloseSocket
//
// Uses inBuf and will overwrite anything in it, so make sure you are
// totally done with it.

void drainAndCloseSocket( void ) {

  // Drain socket for a reasonable amount of time before closing

  verboseMessage( "Closing socket\n" );

  clockTicks_t start = TIMER_GET_CURRENT( );

  uint32_t bytesRead = 0;

  while ( 1 ) {

    PACKET_PROCESS_MULT( 5 );
    Tcp::drivePackets( );
    Arp::driveArp( );

    int16_t rc = sock->recv( inBuf, INBUFSIZE );
    if ( rc > 0 ) bytesRead += rc;

    if ( sock->isRemoteClosed( ) || (Timer_diff( start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 5000 )) ) {
      break;
    }

  }

  verboseMessage( "%lu bytes read while draining the socket\n", bytesRead );

  sock->close( );
}



// fillInBuf
//
// Fill inBuf to the max or until no data is available from the socket.
// inBuf will be compacted if needed.

StopCode fillInBuf( void ) {

  StopCode rc = NotDone;

  // Compact inBuf first if needed

  if ( inBufLen == 0 ) {
    // If everything has been consumed it's safe and cheap to reset.
    inBufStartIndex = 0;
  } else if ( inBufStartIndex + LINEBUFSIZE + 128 > INBUFSIZE ) {
    // Need room for one maximum length header line.  If we don't have
    // that then compact to make room for one.
    memmove( inBuf, inBuf+inBufStartIndex, inBufLen );
    inBufStartIndex = 0;
  }

  // Could add a third case for compacting: inBufStartIndex + inBufLen = INBUFSIZE.
  // But assuming reasonable values for INBUFSIZE and LINEBUFSIZE this won't
  // help you very much.


  uint16_t bytesToRead = INBUFSIZE - (inBufStartIndex + inBufLen);

  TRACE(( "HTGET: fillInBuf start: inBufStartIndex=%u, inBufLen=%u\n",
          inBufStartIndex, inBufLen ));


  while ( (rc == NotDone) && bytesToRead ) {

    if ( userWantsOut( ) ) {
      rc = UserBreak;
      break;
    }

    // Service the connection
    PACKET_PROCESS_MULT( 5 );
    Arp::driveArp( );
    Tcp::drivePackets( );

    int16_t recvRc = sock->recv( inBuf + inBufStartIndex + inBufLen, bytesToRead );

    if ( recvRc > 0 ) {

      // Some bytes read.  Keep going
      inBufLen += recvRc;
      bytesToRead -= recvRc;

    } else if ( recvRc < 0 ) {

      rc = SocketError;

    } else if ( recvRc == 0 ) {

      // Nothing read.  Could be just nothing available, or it could
      // be a closed socket.

      if ( sock->isRemoteClosed( ) ) break;

    }

  } // end while

  TRACE(( "HTGET: fillInBuf end: inBufStartIndex=%u, inBufLen=%u, rc=%u\n",
          inBufStartIndex, inBufLen, rc ));

  return rc;
}



// If there is a full line of input in the input buffer:
//
// - return a copy of the line in target
// - adjust the input buffer to remove the line
//
// Removing a full line of input and sliding the remaining buffer down
// is slow, but makes the buffer code easier.
//
// Note that this code does not search indefinitely.  You have to have
// a CR/LF within the first LINEBUFSIZE bytes and the output buffer should be
// LINEBUFSIZE bytes too.  If you violate this you will probably hang the
// program up.  No HTTP header is coming back that large though.

uint16_t getLineFromInBuf( char *target ) {

  if ( inBufLen == 0 ) return 1;

  for ( int i=inBufStartIndex,len=0; len < (inBufLen-1); i++,len++ ) {

    if ( inBuf[i] == '\r' && inBuf[i+1] == '\n' ) {

      // Found delimiter

      int bytesToCopy = len;
      if ( bytesToCopy > (LINEBUFSIZE-1) ) {
        bytesToCopy = LINEBUFSIZE-1;
        errorMessage( "Warning: Long header truncated. (Was %u bytes long.)\n", len );
      }

      memcpy( target, inBuf+inBufStartIndex, bytesToCopy );
      target[bytesToCopy] = 0;

      inBufLen -= len + 2;    // Adjust buffer length.
      inBufStartIndex = i+2;  // Adjust buffer start.

      TRACE(( "HTGET: Header line: %s\n", target ));
      return 0;
    }

  }


  if ( inBufLen > LINEBUFSIZE  ) {
    // There should have been enough data to read a header.  
    // Wipe out the inBuf and see what happens.
    inBufStartIndex = inBufLen = 0;
    errorMessage( "Could not find the end of a header; clearing the buffer\n" );
  }

  // Not yet
  return 1;
}




// sock_getline
//
// Read lines from the socket that are terminated with a CR/LF.  If a
// full line is not available yet then buffer the partial contents.
// If we don't get a line in a reasonable amount of time then time out
// and return, which is probably fatal to the app.
//
// Returns 0 if successful, -1 if error

int sock_getline( char *buffer) {

  // Have previous data to check already?
  if ( getLineFromInBuf( buffer ) == 0 ) return 0;

  clockTicks_t start = TIMER_GET_CURRENT( );

  while ( 1 ) {

    if (Timer_diff( start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( CONNECT_TIMEOUT ) ) {
      errorMessage( "Timeout reading from socket\n" );
      return -1;
    }

    StopCode rc = fillInBuf( );
    if ( rc != NotDone) return -1;

    if ( getLineFromInBuf( buffer ) == 0 ) break;

  }

  // Good return!
  return 0;
}




// sock_printf
//
// This will loop until it can push all of the data out.
// Does not check the incoming data length, so don't flood it.
// (The extra data will be ignored/truncated ...)
//
// Returns 0 on success, -1 on error


static char spb[ SOCK_PRINTF_SIZE ];

static int sock_printf( char *fmt, ... ) {

  va_list ap;
  va_start( ap, fmt );
  int vsrc = vsnprintf( spb, SOCK_PRINTF_SIZE, fmt, ap );
  va_end( ap );

  if ( (vsrc < 0) || (vsrc >= SOCK_PRINTF_SIZE) ) {
    errorMessage( "Formatting error in sock_printf\n" );
    return -1;
  }

  uint16_t bytesToSend = vsrc;
  uint16_t bytesSent = 0;

  while ( bytesSent < bytesToSend ) {

    // Process packets here in case we have tied up the outgoing buffers.
    // This will give us a chance to push them out and free them up.

    PACKET_PROCESS_MULT(5);
    Arp::driveArp( );
    Tcp::drivePackets( );

    int rc = sock->send( (uint8_t *)(spb+bytesSent), bytesToSend-bytesSent );
    if (rc > 0) {
      bytesSent += rc;
    }
    else if ( rc == 0 ) {
      // Out of send buffers maybe?  Loop around to process packets
    }
    else {
      return -1;
    }

  }

  return 0;
}




int8_t resolve( char *ServerAddrName, IpAddr_t &serverAddr ) {

  int8_t rc = Dns::resolve( ServerAddrName, serverAddr, 1 );
  if ( rc < 0 ) return -1;

  uint8_t done = 0;

  while ( !done ) {

    if ( userWantsOut( ) ) break;

    if ( !Dns::isQueryPending( ) ) break;

    PACKET_PROCESS_MULT(5);
    Arp::driveArp( );
    Tcp::drivePackets( );
    Dns::drivePendingQuery( );

  }

  // Query is no longer pending or we bailed out of the loop.
  rc = Dns::resolve( ServerAddrName, serverAddr, 0 );


  if ( rc != 0 ) {
    errorMessage( "Error resolving %s\n", Hostname );
    return -1;
  }

  verboseMessage( "Hostname %s resolved to %d.%d.%d.%d\n",
                  Hostname,
                  serverAddr[0], serverAddr[1],
                  serverAddr[2], serverAddr[3] );

  return 0;
}



int8_t connectSocket( void ) {

  uint16_t localport = 2048 + rand( );

  sock = TcpSocketMgr::getSocket( );
  if ( sock->setRecvBuffer( TCP_RECV_BUFFER ) ) {
    errorMessage( "Error creating socket\n" );
    return -1;
  }

  if ( sock->connectNonBlocking( localport, HostAddr, ServerPort ) ) return -1;

  int8_t rc = -1;

  clockTicks_t start;
  clockTicks_t lastCheck;
  start = lastCheck = TIMER_GET_CURRENT( );

  while ( 1 ) {

    if ( userWantsOut( ) ) break;

    PACKET_PROCESS_MULT(5);
    Tcp::drivePackets( );
    Arp::driveArp( );

    if ( sock->isConnectComplete( ) ) {
      rc = 0;
      break;
    }

    if ( sock->isClosed( ) || (Timer_diff( start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( CONNECT_TIMEOUT )) ) {
      break;
    }

    // Sleep so that we are not spewing TRACE records.
    while ( lastCheck == TIMER_GET_CURRENT( ) ) { };
    lastCheck = TIMER_GET_CURRENT( );

  }

  if ( rc == 0 ) {
    verboseMessage( "Connected using local port %u!\n", localport );
  }
  else {
    errorMessage( "Connection failed!\n" );
  }

  return rc;
}


// sendHeaders
//
// Returns 0 if all went well, -1 if an error occurs

int sendHeaders( void ) {

  int rc;

  if ( HttpVer == HTTP_v09 ) {

    // Caution - early return!
    verboseMessage( "Sending HTTP 0.9 request\n");
    return sock_printf( "GET %s\r\n", Path );

  } else if ( HttpVer == HTTP_v10 ) {

    verboseMessage( "Sending HTTP 1.0 request\n");
    rc = sock_printf( "%s %s HTTP/1.0\r\n"
                          "User-Agent: mTCP HTGet " __DATE__ "\r\n",
                          HeadersOnly ? "HEAD" : "GET",
                          Path,
                          Hostname );

  } else {

    verboseMessage( "Sending HTTP 1.1 request\n");
    rc = sock_printf( "%s %s HTTP/1.1\r\n"
                          "User-Agent: mTCP HTGet " __DATE__ "\r\n"
                          "Host: %s\r\n"
                          "Connection: close\r\n",
                          HeadersOnly ? "HEAD" : "GET",
                          Path,
                          Hostname );

  }

  if ( rc ) return -1;

  if ( PassInfo ) {
    if ( base64Encoder(PassInfo, lineBuffer, LINEBUFSIZE) ) {
      errorMessage( "Authentication string too long\n" );
      return -1;
    }
    else {
      rc = sock_printf( "Authorization: Basic %s\r\n", lineBuffer );
      if ( rc ) return -1;
    }
  }

  if ( ModifiedSince ) {
    rc = sock_printf( "If-Modified-Since: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n",
                      dayname[mtime->tm_wday], mtime->tm_mday,
                      monthname[mtime->tm_mon], mtime->tm_year + 1900,
                      mtime->tm_hour, mtime->tm_min, mtime->tm_sec );
    if ( rc ) return -1;
  }

  rc = sock_printf( "\r\n" );
  if ( rc ) return -1;

  return 0;
}


// readHeaders
//
// Returns 0 if we can read everything successfully, -1 if not.
// Note that even reading a bad HTTP return code is success as
// far as we are concerned; we are only reporting socket and
// parsing errors.
//
// As a side-effect HttpReponse will be set with the numeric
// code we get from the server.

int8_t readHeaders( void ) {

  if ( HttpVer == HTTP_v09 ) return 0;

  uint16_t response;

  // Get and parse the first line (version and response code)

  if ( sock_getline( lineBuffer ) ) {
    return -1;
  }

  if (HeadersOnly) fprintf( stdout, "\n%s\n", lineBuffer );
  if (ShowHeaders) fprintf( stderr, "\n%s\n", lineBuffer );

  if ( (strncmp(lineBuffer, "HTTP/1.0", 8) != 0) && (strncmp(lineBuffer, "HTTP/1.1", 8) != 0) ) {
    errorMessage( "Not an HTTP 1.0 or 1.1 server\n" );
    return -1;
  }

  // Skip past HTTP version number
  char *s = lineBuffer + 8;
  char *s2 = s;

  // Skip past whitespace
  while ( *s ) {
    if ( *s != ' ' && *s !='\t' ) break;
    s++;
  }

  if ( (s == s2) || (*s == 0) || (sscanf(s, "%3d", &response) != 1) ) {
    errorMessage( "Malformed HTTP version line\n" );
    return -1;
  }

  HttpResponse = response;

  // Report the return code to the user on the screen if they are not looking at headers.
  if ( !HeadersOnly && !ShowHeaders ) {
    errorMessage( "Server return code: %s\n", s );
  }


  while ( sock_getline( lineBuffer ) == 0 ) {

    if (HeadersOnly) fprintf( stdout, "%s\n", lineBuffer );
    if (ShowHeaders) fprintf( stderr, "%s\n", lineBuffer );

    if ( *lineBuffer == 0 ) break;

                  
    if ( strnicmp( lineBuffer, "Content-Length:", 15 ) == 0) {
      // Skip past Content-Length: 
      s = lineBuffer + 15;
      ExpectedContentLength = atol(s);
      ExpectedContentLengthSent = true;
    }
    else if (strnicmp(lineBuffer, "Location:", 9 ) == 0) {
      if (response == 301 || response == 302) {
        if (!HeadersOnly) {
          errorMessage( "New location: %s\n", lineBuffer+10 );
        }
      }
    }
    else if (stricmp(lineBuffer, "Transfer-Encoding: chunked") == 0) {
      TransferEncoding_Chunked = true;
    }

  }

  if ( ExpectedContentLengthSent ) {
    verboseMessage( "Expected content length: %lu\n", ExpectedContentLength );
  }
  else {
    verboseMessage( "No content length header sent\n" );
  }

  if ( TransferEncoding_Chunked ) {
    verboseMessage( "Chunked transfer encoding being used\n" );
  }


  if ( response == 304 ) {
    NotModified = true;
    if (!HeadersOnly) {
      errorMessage( "Server copy not modified; not altering %s\n", outputFilename );
    }
    ExpectedContentLengthSent = true;
    ExpectedContentLength = 0;
  }

  return 0;
}


void fileWriteError( int localErrno ) {
  errorMessage( "File write error: %s\n", strerror(localErrno) );
}


int fileWriter( uint8_t *buffer, uint16_t bufferLen, FILE *outputFile ) {
  // Remember, fwrite only fails this check if there is an error.
  if ( fwrite( buffer, 1, bufferLen, outputFile ) != bufferLen ) {
    int localErrno = errno;
    fileWriteError( localErrno );
    return 1;
  }
  return 0;
}


// Return:
//   n: a number that we parsed and another that says how many chars we consumed.
//  -1: Not enough chars; come back with a bigger buffer
//  -2: Hard error; we could not parse this.

int32_t getChunkSize( uint8_t *buffer, uint16_t bufferLen, uint16_t *bytesConsumed ) {

  TRACE(( "HTGET: getChunkSize: Start bufferLen %d\n", bufferLen ));

  int32_t rc = -2;

  // Scan what we have to see if we can parse the entire thing.
  int i=0;
  while ( i < bufferLen ) {
    if ( isdigit(buffer[i]) || (buffer[i] >= 'A' && buffer[i] <= 'F') ||  (buffer[i] >= 'a' && buffer[i] <= 'f') ) {
      i++;
    } else {
      break;
    }
  }

  if ( i == bufferLen ) return -1;
  if ( i > 6 ) return -2; // Six hex digits ... f-ck off.

  if ( buffer[i] == ';' ) {
    // Great ..  we have a chunk extension.  Ignore it.
    while ( i < bufferLen ) {
      if ( buffer[i] != '\r' ) i++; else break;
    }
    if ( i == bufferLen ) {
      return -1;
    }
  }
  else if ( buffer[i] != '\r' ) {
    return -2;
  }


  // At this point we are safely sitting on a carriage return, but
  // we need both a carriage return and a line fed.

  i++;
  if ( i == bufferLen ) return -1;     // Need more chars
  if ( buffer[i] != '\n' ) return -2;  // Parse error

  i++; // Consume the \n

  // All good.  Parse the hex

  if ( sscanf( (char *)buffer, "%lx", &rc ) != 1 ) {
    return -2;
  }

  TRACE(( "HTGET: getChunkSize: bytes consumed = %d\n", i ));

  *bytesConsumed = i;
  return rc;
}


int readContent( void ) {

  // We were explicitly told to expect content or were not told but were not reading
  // just headers, so something might come.

  verboseMessage( "Receiving content\n" );

  // Open output file now if specified.  If not, it goes to stdout.  And if
  // STDOUT is redirected, set the file mode to binary mode.

  FILE *outputFile;

  if ( *outputFilename ) {
    outputFile = fopen( outputFilename, "wb" );
    if ( outputFile == NULL ) {
      fileWriteError( errno );
      return -1;
    }
  } else {
    outputFile = stdout;
    if ( IsStdoutFile ) setmode( 1, O_BINARY );
  }


  // By this point:
  // - outputFile points to a file or stdout stream.
  // - inBuf has some leftover bytes from it.



  uint32_t TotalBytesReceived = 0;  // Actually, just content bytes.

  StopCode stopCode = NotDone;

  while ( stopCode == NotDone ) {

    TRACE(( "HTGET: recv content loop: inBufStartIndex=%u inBufLen=%u\n",
            inBufStartIndex, inBufLen ));

    int32_t nextChunkSize;

    if ( TransferEncoding_Chunked ) {

      TRACE(( "HTGET: gettingNextChunkSize\n" ));

      uint16_t bytesConsumed = 0;
      nextChunkSize = getChunkSize( inBuf + inBufStartIndex, inBufLen, &bytesConsumed );

      while ( nextChunkSize == -1 ) {

        TRACE(( "Not enough bytes to read chunk size." ));

        stopCode = fillInBuf( );
        if ( stopCode != NotDone ) break;

        nextChunkSize = getChunkSize( inBuf, inBufLen, &bytesConsumed );

        // We've tried to read the socket for more data.  If you still don't have enough
        // data and the socket is closed, you are done.
        if ( (nextChunkSize == -1) && sock->isRemoteClosed( ) ) {
          stopCode = ProtocolError;
          break;
        }

      } // end while

      TRACE(( "HTGET: nextChunkSize=%ld, stopCode=%u\n", nextChunkSize, stopCode ));

      if ( stopCode == NotDone ) {

        // By this point we have a next chunk size or a parse error.

        if ( nextChunkSize == 0 ) {
          stopCode = AllDoneAndGood;
        } else if ( nextChunkSize == -2 ) {
          stopCode = ProtocolError;
        } else {
          inBufStartIndex += bytesConsumed;
          inBufLen -= bytesConsumed;
        }

      }

    } else {
      TRACE(( "HTGET: Not using chunked transfers, nextChunkSize set to a large value\n" ));
      nextChunkSize = INBUFSIZE;
    }


    if ( stopCode != NotDone ) break;


    while ( nextChunkSize && (stopCode == NotDone) ) {

      TRACE(( "HTGET: File write loop: nextChunkSize=%lu, inBufStartIndex=%u, inBufLen=%u\n",
              nextChunkSize, inBufStartIndex, inBufLen ));

      if ( userWantsOut( ) ) {
        stopCode = UserBreak;
        break;
      }

      // If inBuf is empty fill it.  Reading more than we need for this chunk is fine.

      if ( inBufLen == 0 ) {

        stopCode = fillInBuf( );

        // Just tried to read the socket.  If we did not get any data
        // and the socket is closed then we will not get any data.

        if ( (inBufLen == 0) && sock->isRemoteClosed( ) ) {
          if ( TransferEncoding_Chunked ) {
            stopCode = ProtocolError;
          } else {
            stopCode = AllDoneAndGood;
          }
          break;
        }

      }

      if ( stopCode != NotDone ) break;

      // Write whatever is in inBuf.

      uint16_t bytesToWrite = nextChunkSize;
      if ( bytesToWrite > inBufLen ) bytesToWrite = inBufLen;

      TRACE(( "HTGET: before write: inBufStartIndex=%u, bytesToWrite=%u\n", inBufStartIndex, inBufLen ));

      if ( fileWriter( inBuf + inBufStartIndex, bytesToWrite, outputFile ) ) {
        stopCode = FileError;
        break;
      }

      TotalBytesReceived += bytesToWrite;
      nextChunkSize = nextChunkSize - bytesToWrite;
      inBufLen = inBufLen - bytesToWrite;
      inBufStartIndex += bytesToWrite;

    } // end while


    if ( TransferEncoding_Chunked ) {

      // We finished reading a chunk.  There should be a CR/LF pair after the
      // chunk.

      // If we don't have enough bytes then read some more.  If there are less
      // than two bytes available and the socket closes then this is a protocol
      // error.

      while ( inBufLen < 2 ) {

        stopCode = fillInBuf( );
        if ( stopCode != NotDone ) break;

        if ( (inBufLen < 2 ) && sock->isRemoteClosed( ) ) {
          stopCode = ProtocolError;
          break;
        }

      }

      if ( inBuf[inBufStartIndex] == '\r' && inBuf[inBufStartIndex+1] == '\n' ) {
        inBufStartIndex += 2;
        inBufLen -= 2;
        TRACE(( "HTGET: Read trailing CR LF at end of chunk\n" ));
      } else {
        stopCode = ProtocolError;
        TRACE(( "HTGET: Looking for CR LF, found %u and %u\n",
                inBuf[inBufStartIndex], inBuf[inBufStartIndex+1] ));
      }

    }

  } // end big while


  verboseMessage( "Receive content exit: %s\n", StopCodeStrings[stopCode] );

  if ( fclose( outputFile ) ) {
    int localErrno = errno;
    fileWriteError( localErrno );
    return -1;
  }
    

  int rc = -1;

  if ( stopCode == AllDoneAndGood ) {
    if ( !ExpectedContentLengthSent || (ExpectedContentLength == TotalBytesReceived) ) {
      rc = 0;
    } else {
      errorMessage( "Warning: expected %lu bytes, received %lu bytes\n", ExpectedContentLength, TotalBytesReceived );
    }
  } 
 
  verboseMessage( "Received %lu bytes\n", TotalBytesReceived );


  return rc;
}



static char CopyrightMsg1[] = "mTCP HTGet by M Brutman (mbbrutman@gmail.com) (C)opyright 2011-2020\n";
static char CopyrightMsg2[] = "Version: " __DATE__ "\n\n";

char *HelpText = {
  "usage: htget [options] <URL>\n\n"
  "Options:\n"
  "  -h                       Shows this help\n"
  "  -v                       Print verbose status messages\n"
  "  -quiet                   Quiet mode (does not apply to usage errors)\n"
  "  -headers                 Fetch only the HTTP headers\n"
  "  -showheaders             Fetch content, but show headers too\n"
  "  -m                       Fetch content only if modified (use with -o option)\n"
  "  -o <file>                Write content to file\n"
  "  -pass <ident:password>   Send authorization for BASIC auth\n"
  "  -09                      Use HTTP 0.9 protocol\n"
  "  -10                      Use HTTP 1.0 protocol\n"
  "  -11                      Use HTTP 1.1 protocol (default)\n\n"
  "Press Ctrl-Break or ESC during a transfer to abort\n\n"
};


void usageError( char *format, char *msg ) {

  fprintf( stderr, "%s  %s", CopyrightMsg1, CopyrightMsg2 );

  if ( format != NULL ) {
    fprintf( stderr, format, msg );
    fprintf( stderr, "\n" );
  }

  fprintf( stderr, "%s", HelpText );

  exit( 1 );
}


static void parseArgs( int argc, char *argv[] ) {

  int i=1;
  for ( ; i<argc; i++ ) {

    if ( stricmp( argv[i], "-h" ) == 0 ) {
      usageError( NULL, NULL );
    }
    else if ( stricmp( argv[i], "-quiet" ) == 0 ) {
      QuietMode = true;
    }
    else if ( stricmp( argv[i], "-v" ) == 0 ) {
      Verbose = true;
    }
    else if ( stricmp( argv[i], "-headers" ) == 0 ) {
      HeadersOnly = true;
    }
    else if ( stricmp( argv[i], "-showheaders" ) == 0 ) {
      ShowHeaders = true;
    }
    else if ( stricmp( argv[i], "-pass" ) == 0 ) {

      i++;
      if ( i == argc ) {
        usageError( "%s", "Need to provide a userid and password\n" );
      }

      PassInfo = argv[i];
    }
    else if ( stricmp( argv[i], "-o" ) == 0 ) {
      i++;
      if ( i == argc ) {
        usageError( "%s", "If using -o you need to provide a filename with it\n" );
      }

      strncpy( outputFilename, argv[i], OUTPUTFILENAME_LEN );
      outputFilename[ OUTPUTFILENAME_LEN - 1 ] = 0;
    }
    else if ( stricmp( argv[i], "-m" ) == 0 ) {
      ModifiedSince = true;
    }
    else if ( stricmp( argv[i], "-09" ) == 0 ) {
      HttpVer = HTTP_v09;
    }
    else if ( stricmp( argv[i], "-10" ) == 0 ) {
      HttpVer = HTTP_v10;
    }
    else if ( stricmp( argv[i], "-11" ) == 0 ) {
      HttpVer = HTTP_v11;
    }
    else if ( argv[i][0] != '-' ) {
      // End of options
      break;
    }
    else {
      usageError( "Unknown option: %s\n", argv[i] );
    }
   

  }

  if ( QuietMode && Verbose ) {
    usageError( "%s", "Do not specify both -quiet and -v\n" );
  }
    

  if ( ModifiedSince && (*outputFilename == 0) ) {
    usageError( "%s", "Need to specify a filename with -o if using -m\n" );
  }


  if ( i == argc ) {
    usageError( "%s", "Need to provide a URL to fetch\n" );
  }

  if ( HttpVer == HTTP_v09 ) {
    if ( PassInfo != NULL ) {
      usageError( "%s", "Can not send authentication with HTTP/0.9\n" );
    }
    if ( ModifiedSince ) {
      usageError( "%s", "HTTP/0.9 does not support checking modification times\n" );
    }
    if ( HeadersOnly || ShowHeaders ) {
      usageError( "%s", "HTTP/0.9 does not have header support\n" );
    }
  }


  // Parse out the URL

  char *url = argv[i];

  if ( strnicmp( url, "http://", 7 ) == 0 ) {

    char *hostnameStart = url + 7;

    // Scan ahead for another slash; if there is none then we
    // only have a server name and we should fetch the top
    // level directory.

    char *proxy = getenv( "HTTP_PROXY" );
    if ( proxy == NULL ) {

      char *pathStart = strchr( hostnameStart, '/' );
      if ( pathStart == NULL ) {

        strncpy( Hostname, hostnameStart, HOSTNAME_LEN );
        Hostname[ HOSTNAME_LEN - 1 ] = 0;

        Path[0] = '/';
        Path[1] = 0;

      }
      else {

        strncpy( Hostname, hostnameStart, pathStart - hostnameStart );
        Hostname[ HOSTNAME_LEN - 1 ] = 0;
      
        strncpy( Path, pathStart, PATH_LEN );
        Path[ PATH_LEN - 1 ] = 0;

      }

    }
    else { 

      strncpy( Hostname, proxy, HOSTNAME_LEN );
      Hostname[ HOSTNAME_LEN - 1 ] = 0;

      strncpy( Path, url, PATH_LEN );
      Path[ PATH_LEN - 1 ] = 0;

    }
    

    char *portStart = strchr( Hostname, ':' );

    if ( portStart != NULL ) {
      ServerPort = atoi( portStart+1 );
      if ( ServerPort == 0 ) {
        usageError( "%s", "Invalid port on server\n" );
      }

      // Truncate hostname early
      *portStart = 0;
    }

  }
  else {
    usageError( "%s", "Need to specify a URL starting with http://\n");
  }


}


void probeStdout( void ) {

  union REGS inregs, outregs;

  inregs.x.ax = 0x4400;
  inregs.x.bx = 1;

  intdos( &inregs, &outregs );

  if ( outregs.x.cflag == 0 ) {
    if ( (outregs.x.dx & 0x0080) == 0 ) {
      IsStdoutFile = true;
    }
  }

}


int main( int argc, char *argv[] ) {

  probeStdout( );

  parseArgs( argc, argv );

  // If you get this far there are no usage errors.

  if ( !QuietMode ) fprintf( stderr, "%s  %s", CopyrightMsg1, CopyrightMsg2 );


  // Allocate memory

  inBuf = (uint8_t *)malloc( INBUFSIZE );
  if ( !inBuf ) {
    errorMessage( "Error: Could not allocate memory\n" );
    exit(1);
  }


  // If the user only wants us to pull down a file that is newer than the
  // specified file initialize the timezone and get the modification time
  // of the file.

  if ( ModifiedSince ) {

    char *tzStr = getenv( "TZ" );
    if ( tzStr == NULL ) {
      errorMessage( "Warning: the TZ environment variable is not set.  Assuming\n"
            "Eastern Standard Time.  See the docs for how to set it properly.\n" );
    }

    tzset( );

    int rc = stat( outputFilename, &statbuf );

    if ( rc == 0 ) {
      mtime = gmtime( &statbuf.st_mtime );
    }
    else {
      errorMessage( "Warning: Could not find file %s to read file timestamp.\nIgnoring -m option\n", outputFilename );
      ModifiedSince = false;
    }

  }



  // Initialize TCP/IP

  if ( Utils::parseEnv( ) != 0 ) {
    exit(1);
  }

  if ( Utils::initStack( 1, TCP_SOCKET_RING_SIZE, ctrlBreakHandler, ctrlBreakHandler ) ) {
    errorMessage( "\nFailed to initialize TCP/IP - exiting\n" );
    exit(1);
  }

  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.


  // Resolve and connect

  verboseMessage( "Server: %s:%u\nPath: %s\n", Hostname, ServerPort, Path );

  if ( resolve(Hostname, HostAddr) ) shutdown( 1 );

  if ( connectSocket( ) ) shutdown( 1 );

  if ( sendHeaders( ) ) {
    errorMessage( "Error sending HTTP request\n" );
    shutdown( 1 );
  }

  if ( readHeaders( ) ) {
    errorMessage( "Error reading HTTP headers\n" );
    shutdown( 1 );
  }


  int rc;

  if ( HeadersOnly || (ExpectedContentLengthSent && (ExpectedContentLength == 0)) || (NotModified) ) {

    // If only reading headers or we were told explicitly not to expect any
    // content then skip reading any content.  Also do nothing if we already
    // have an up to date copy of the content.

    verboseMessage( "No content expected so none read\n" );
    rc = 0;

  } else {

    rc = readContent( );

  }

  drainAndCloseSocket( );

  if ( rc == 0 ) {
    rc = mapResponseCode( HttpResponse );
  }
  else {
    rc = 1;
  }

  shutdown( rc );
}
