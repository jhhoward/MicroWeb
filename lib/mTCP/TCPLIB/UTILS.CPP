/*

   mTCP Utils.cpp
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


   Description: Utility functions

   Changes:

   2008-07-30 Move to a file based configuration
   2011-05-27: Initial release as open source software
   2013-03-24: Fix dumpBytes to not need the string of spaces;
               Use a common static line buffer and make it larger;
               Use a common static buffer for the parameter name;
               Add warning for a config file that is too long or
               not properly terminated with a CR/LF
   2013-03-30: Add DHCP lease expired warning code
   2015-01-18: Move Ctrl-Break and Ctrl-C handling into initStack
               since basically all code does the same thing; more
               changes to decouple tracing from utils.
   2015-04-10: Add preferred nameserver support.
   2019-09-02: Rewrite dumpBytes so it doesn't call fwrite 17x per
               line of output.

*/




// Miscellaneous utilities for mTCP.  Functions include:
//
// - opening and parsing the configuration file
// - starting and stopping the stack in an orderly manner




#include <ctype.h>
#include <dos.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include "utils.h"
#include "trace.h"
#include "timer.h"
#include "packet.h"
#include "eth.h"
#include "ip.h"


#ifdef COMPILE_ARP
#include "arp.h"
#endif

#ifdef COMPILE_UDP
#include "udp.h"
#endif

#ifdef COMPILE_TCP
#include "tcp.h"
#include "tcpSockM.h"
#endif

#ifdef COMPILE_DNS
#include "dns.h"
#endif







#ifdef SLEEP_CALLS

// Defaults for our sleep calls.
//
// Unless somebody overrides us using the environment variable we will
// try to call int 28 after checking for a packet to process and not
// finding one.  If int 2f/1680 is available we will call that next.

uint8_t mTCP_sleepCallEnabled = 1;
uint8_t mTCP_releaseTimesliceEnabled = 0;

#endif






const char Parm_PacketInt[]            = "PACKETINT";
const char Parm_Hostname[]             = "HOSTNAME";
const char Parm_IpAddr[]               = "IPADDR";
const char Parm_Gateway[]              = "GATEWAY";
const char Parm_Netmask[]              = "NETMASK";
const char Parm_Nameserver[]           = "NAMESERVER";
const char Parm_Nameserver_preferred[] = "NAMESERVER_PREFERRED";
const char Parm_Mtu[]                  = "MTU";



const char ConfigFileErrMsg[] = "Config file '%s' not found\n";
const char InitErrorMsg[] = "Init: could not setup %s\n";



// Preferred nameserver: if the configuration file specifies a preferred
// nameserver then use it instead of any other nameserver that is specified.
// This override mechanism allows you to use a third party DNS server like
// Google without having DHCP constantly overwrite it.

IpAddr_t Preferred_nameserver;
bool     Preferred_nameserver_set = false;



// Utils class data storage

uint8_t Utils::packetInt = 0;
FILE   *Utils::CfgFile = NULL;
char   *Utils::CfgFilenamePtr = NULL;

void ( __interrupt __far *Utils::oldCtrlBreakHandler)( );

char Utils::lineBuffer[UTILS_LINEBUFFER_LEN];
char Utils::parmName[UTILS_PARAMETER_LEN];




// Utils::dumpBytes
//
// Just a generic utility to do a nice hexidecimal dump of data.
//
// The original code was simple but made 17 calls to fprintf
// for each line of output.  This version only calls fputs
// once for each line of output.  The code size is about the
// same but this requires some extra static storage for the
// output buffer.

// Output format (for figuring out column locations):
//
// 0                                                 50               67
// v                                                 v                v
// xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx xx   0123456789abcdef_

static char dumpBytesBuffer[68];
static const char hexDigits[] = "0123456789ABCDEF";

void Utils::dumpBytes( FILE *stream, unsigned char *buffer, unsigned int len ) {

  dumpBytesBuffer[0] = 0;  // Set to null in case they give us len=0;
  dumpBytesBuffer[48] = ' ';
  dumpBytesBuffer[49] = ' ';

  int index1 = 0;
  int index2 = 50;

  int i;
  for ( i = 0; i < len; i++ ) {

    dumpBytesBuffer[index2++] = ((buffer[i] > 31) && (buffer[i]<127)) ? buffer[i] : '.';

    dumpBytesBuffer[index1++] = hexDigits[(buffer[i] >> 4)];
    dumpBytesBuffer[index1++] = hexDigits[(buffer[i] & 0xF)];
    dumpBytesBuffer[index1++] = ' ';

    if ( index2 == 66 ) {
      dumpBytesBuffer[ index2++ ] = '\n';
      dumpBytesBuffer[ index2 ] = 0;
      fputs( dumpBytesBuffer, stream );
      index1 = 0;
      index2 = 50;
    }

  }

  if ( index1 ) {
    while ( index1 < 48 ) dumpBytesBuffer[index1++] = ' ';
    dumpBytesBuffer[ index2++ ] = '\n';
    dumpBytesBuffer[ index2++ ] = '\n';  // This was a partial line so we know there is room.
    dumpBytesBuffer[ index2 ] = 0;
  }
  else {
    dumpBytesBuffer[ 0 ] = '\n';
    dumpBytesBuffer[ 1 ] = 0;
  }

  fputs( dumpBytesBuffer, stream );
}



bool Utils::rtrim( char *buffer ) {

  char *index = buffer;
  while ( *index ) index++;

  index--;

  bool trailingWhitespaceDetected = false;

  while ( (index >= buffer) && ((*index == ' ') || (*index == '\t')) ) {
    *index = 0;
    index--;
    trailingWhitespaceDetected = true;
  }

  return trailingWhitespaceDetected;
}



// Read a line from a text file.  If there is a read error or the line is
// truncated then return an error.

int Utils::getLine( FILE *inputFile, bool removeNewLine, char *buffer, int bufferLen, int lineNumber ) {

  *buffer = 0;

  bool eofDetected = false;

  if ( fgets( buffer, bufferLen, inputFile ) == NULL ) {

    if ( !feof( inputFile ) ) {

      // End of file is fine.  But did we have a read error?
      int localErrno = errno;
      if ( localErrno ) {
        fprintf( stderr, "mTCP: Config file read error: %s\n", strerror( localErrno ) );
        return -1;
      }
    }

    eofDetected = true;
  }

  // If we are at the end of the file there will be no newline character, so
  // suppress the error message and don't try to remove the newline character.

  if ( !eofDetected ) {

    // Safe because fgets always terminates the buffer with a null char.
    char *index = buffer;
    while ( *index ) index++;

    index--;
    if ( *index != '\n') {
      fprintf( stderr, "mTCP: Line too long at line %u of the config file.\n", lineNumber );
      return -1;
    }

    if ( removeNewLine ) *index = 0;
  }

  return 0;
}



// parseEnv
//
// Common code to setup the TCP/IP parameters.  Most apps will use
// this.  The exception is the DHCP client, which uses a subset of it.
//
// If this returns anything but 0 you have failed.

#ifndef DHCP_CLIENT

int8_t Utils::parseEnv( void ) {

  CfgFilenamePtr = getenv( "MTCPCFG" );
  if ( CfgFilenamePtr == NULL ) {
    fprintf( stderr, "Need to set MTCPCFG env variable\n" );
    return -1;
  }


  FILE *cfgFile = fopen( CfgFilenamePtr, "r" );
  if ( cfgFile == NULL ) {
    fprintf( stderr, ConfigFileErrMsg, CfgFilenamePtr );
    return -1;
  }


  time_t dhcpTimestamp = 0;
  time_t dhcpLease = 0;
  time_t dhcpLeaseThreshold = DHCP_LEASE_THRESHOLD;

  uint16_t tmp1, tmp2, tmp3, tmp4;

  const char *errorParm = NULL;

  bool errorWhileReadingLine = false;
  bool trailingWhitespaceDetected = false;
  int linesInFile = 0;

  while ( !feof( cfgFile ) && (errorParm == NULL) ) {

    linesInFile++;

    if ( errorWhileReadingLine = getLine( cfgFile, true, lineBuffer, UTILS_LINEBUFFER_LEN, linesInFile ) ) {
      break;
    }

    if ( rtrim( lineBuffer ) ) {
      fprintf( stderr, "mTCP: Warning - trailing whitespace detected on line %u of the config file.\n", linesInFile );
      trailingWhitespaceDetected = true;
    }


    char *nextTokenPtr = getNextToken( lineBuffer, parmName, UTILS_PARAMETER_LEN );
    if ( *parmName == 0 ) continue; // Blank line

    if ( stricmp( parmName, Parm_PacketInt ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%x", &packetInt );
      if ( rc != 1 ) {
	errorParm = Parm_PacketInt;
      }
    }

    else if ( stricmp( parmName, Parm_Hostname ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%s", MyHostname );
      if ( rc != 1 ) {
	errorParm = Parm_Hostname;
      }
    }

    else if ( stricmp( parmName, Parm_IpAddr ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%d.%d.%d.%d\n", &tmp1, &tmp2, &tmp3, &tmp4 );
      if ( rc != 4 ) {
	errorParm = Parm_IpAddr;
      }
      Ip::setMyIpAddr( tmp1, tmp2, tmp3, tmp4 );
    }

    else if ( stricmp( parmName, Parm_Netmask ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%d.%d.%d.%d\n", &tmp1, &tmp2, &tmp3, &tmp4 );
      if ( rc != 4 ) {
	errorParm = Parm_Netmask;
      }
      Ip::setMyNetmask( tmp1, tmp2, tmp3, tmp4 );
    }

    else if ( stricmp( parmName, Parm_Gateway ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%d.%d.%d.%d\n", &tmp1, &tmp2, &tmp3, &tmp4 );
      if ( rc != 4 ) {
	errorParm = Parm_Gateway;
      }
      Gateway[0] = tmp1; Gateway[1] = tmp2;
      Gateway[2] = tmp3; Gateway[3] = tmp4;
    }


    #ifdef COMPILE_DNS
    else if ( stricmp( parmName, Parm_Nameserver ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%d.%d.%d.%d\n", &tmp1, &tmp2, &tmp3, &tmp4 );
      if ( rc != 4 ) {
	errorParm = Parm_Nameserver;
      }
      Dns::NameServer[0] = tmp1; Dns::NameServer[1] = tmp2;
      Dns::NameServer[2] = tmp3; Dns::NameServer[3] = tmp4;
    }
    else if ( stricmp( parmName, Parm_Nameserver_preferred ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%d.%d.%d.%d\n", &tmp1, &tmp2, &tmp3, &tmp4 );
      if ( rc != 4 ) {
	errorParm = Parm_Nameserver_preferred;
      }
      Preferred_nameserver[0] = tmp1; Preferred_nameserver[1] = tmp2;
      Preferred_nameserver[2] = tmp3; Preferred_nameserver[3] = tmp4;
      Preferred_nameserver_set = true;
    }
    #endif

    else if ( stricmp( parmName, Parm_Mtu ) == 0 ) {
      uint16_t newMtu;
      int rc = sscanf( nextTokenPtr, "%d\n", &newMtu );
      if ( (rc != 1) || (newMtu < ETH_MTU_MIN) || (newMtu > ETH_MTU_MAX) ) {
	errorParm = Parm_Mtu;
      }
      MyMTU = newMtu;
    }

    else if ( stricmp( parmName, "TIMESTAMP" ) == 0 ) {
      // Note the leading whitespace before the paren ... it needs to be there.
      int rc = sscanf( nextTokenPtr, " ( %lu )", &dhcpTimestamp );
      if ( rc != 1 ) dhcpTimestamp = 0;
    }

    else if ( stricmp( parmName, "LEASE_TIME" ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%lu", &dhcpLease );
      if ( rc != 1 ) dhcpLease = 0;
    }

    else if ( stricmp( parmName, "DHCP_LEASE_THRESHOLD" ) == 0 ) {
      int rc = sscanf( nextTokenPtr, "%lu", &dhcpLeaseThreshold );
      if ( rc != 1 ) dhcpLeaseThreshold = DHCP_LEASE_THRESHOLD;
    }

  }

  fclose( cfgFile );


  // If we had a file read error or a line was too long we already complained.
  if ( errorWhileReadingLine ) return -1;


  // Trailing whitespace was detected; this is just cosmetic.  In the future
  // it will be a hard error and we will return an error.
  if ( trailingWhitespaceDetected ) fprintf( stderr, "\n" );


  // If we spotted an error complain and exit.
  if ( errorParm != NULL ) {
    fprintf( stderr, "mTcp: '%s' is the wrong format or not set correctly.\n", errorParm );
    return -1;
  }


  // Check for errors of ommision and blatantly wrong values

  if ( packetInt == 0x0 ) {
    errorParm = Parm_PacketInt;
  }

  if ( Ip::isSame( MyIpAddr, IpBroadcast ) ) {
    errorParm = Parm_IpAddr;
  }

  if ( Ip::isSame( Netmask, IpBroadcast ) ) {
    errorParm = Parm_Netmask;
  }


  if ( errorParm != NULL ) {
    fprintf( stderr, "mTCP: '%s' must be set.\n", errorParm );
    return -1;
  }



  // If we found a DHCP timestamp in the file and the current
  // time on the machine is greater than Jan 1 2008 then assume
  // that they are keeping the time up to date and check for a
  // DHCP lease expiration.
  //
  // Any lease over a year long does not need to be checked.
  // This is to address routers that hand back MAXINT, which
  // then causes our arithmetic to wrap around.

  if ( (dhcpTimestamp != 0) && ((dhcpLease > 0) && (dhcpLease < 31536000ul)) ) {

    time_t currentTime;
    time( &currentTime );

    if ( currentTime > 1199145600ul ) {

      if ( dhcpTimestamp + dhcpLease < currentTime ) {
        fprintf( stderr, "Your DHCP lease has expired!  Please run DHCP.EXE.\n" );
        return -1;
      }
      else if ( (dhcpTimestamp + dhcpLease) - currentTime < dhcpLeaseThreshold ) {
        fprintf( stderr, "Your DHCP lease expires in less than %lu seconds!  Please run DHCP.EXE.\n",
                 dhcpLeaseThreshold );
        return -1;
      }

    }

  }
    

  #ifdef COMPILE_DNS
  if ( Preferred_nameserver_set ) {
    Ip::copy( Dns::NameServer, Preferred_nameserver );
  }
  #endif


  parseOptionalEnvVars( );

  return 0;
}

#endif



// This does not fail; these environment variables are optional and we don't
// bother checking for usage errors.

void Utils::parseOptionalEnvVars( void ) {

  // Environment variables only

  #ifndef NOTRACE

  char *debugging  = getenv( "DEBUGGING" );
  if ( debugging != NULL ) {

    // First try to parse the result using hexadecimal.  If this fails then
    // fall back to integers.
    //
    // Why is setting Trace_Debugging done with an or operation?  Because a
    // program may have set it at startup.

    uint16_t tmp;

    if ( sscanf( debugging, "0x%x", &tmp ) == 1 ) {
      Trace_Debugging |= tmp;
    } else {
      Trace_Debugging |= atoi( debugging );
    }

    // If the user turned on the flush bit but nothing else, then nothing is turned on.
    if ( (Trace_Debugging & 0xFF) == 0 ) Trace_Debugging = 0;
  }

  char *logfile = getenv( "LOGFILE" );
  if ( logfile != NULL ) {
    strcpy( Trace_LogFile, logfile );
  }

  #endif


  #ifdef SLEEP_CALLS
  char *mtcpSleepVal = getenv( "MTCPSLEEP" );
  if ( mtcpSleepVal != NULL ) {
    mTCP_sleepCallEnabled = atoi( mtcpSleepVal );
  }
  #endif

}



FILE *Utils::openCfgFile( void ) {

  CfgFile = fopen( CfgFilenamePtr, "r" );
  if ( CfgFile == NULL ) {
    fprintf( stderr, ConfigFileErrMsg, CfgFilenamePtr );
  }

  return CfgFile;
}


void Utils::closeCfgFile( void ) {
  fclose( CfgFile );
}




// Get application specific values
//
// To keep things generic always return a string.  The user can convert
// it to whatever they need when they get it.
//
// The algorithm is pretty nasty:
//
// - The cfg file has to be open already
// - Fseek to the beginning of the file
// - Read key pairs
// - If we find our key return it.  Otherwise, keep going until we hit EOF
//
// Returns
//   0 if key is found
//   1 if not found
//  -1 if error
//
// Note: The config file line length has a practical limit.  (It is based
//       on the size of the static line buffer declared above.)

int8_t Utils::getAppValue( const char *key, char *val, uint16_t valBufLen ) {

  // printf( "Key: %s  Buflen: %u\n", key, valBufLen );

  *val = 0;

  if ( fseek( CfgFile, 0, 0 ) ) return -1;

  int linesInFile = 0;

  while ( !feof( CfgFile ) ) {

    linesInFile++;

    if ( getLine( CfgFile, true, lineBuffer, UTILS_LINEBUFFER_LEN, linesInFile ) ) {
      break;
    }

    // No need to issue the warnings here; that happened once already when we
    // read the entire config file in parseEnv.
    rtrim( lineBuffer );


    //printf( "Buffer: %s\n", lineBuffer );

    // Read the key
    char *nextTokenPtr = getNextToken( lineBuffer, parmName, UTILS_PARAMETER_LEN );

    // printf( "Parm: %s---\n", parmName );

    if ( (nextTokenPtr == NULL) || (stricmp( parmName, key ) != 0) ) {
      continue;
    }

    // We are on a space or at the end of the line.
    // Advance until first non-whitespace char.
    while ( 1 ) {
      if ( *nextTokenPtr == 0 ) break;
      if ( isspace(*nextTokenPtr) ) {
	nextTokenPtr++;
      }
      else {
	break;
      }
    }


    // The rest of the line is the val
    strncpy( val, nextTokenPtr, valBufLen-1 );
    val[valBufLen-1] = 0;

    // printf( "Val: %s---\n", val );

    return 0;

  }

  return 1;
}




// Utils::initStack
//
// Most applications do the same things to get started so that common code is
// provided here.
//
// All parameters are required to be provided but not all may be used.  Being
// more specific, the tcpSockets and tcpXmitBuffers parameters must be
// provided but may be 0 if TCP is not in use.  The Ctrl-Break and Ctrl-C
// handlers are required because we will hook the timer interrupt and we
// don't want to ever leave it dangling.
//
// This code is designed such that if it fails you are safe and you do not
// have to do anything.  Which means that within this function, if something
// fails the function is responsible for cleaning up nicely so the caller
// does not have to.

int8_t Utils::initStack( uint8_t tcpSockets,
                         uint8_t tcpXmitBuffers,
                         void __interrupt __far (*newCtrlBreakHandler)(),
                         void __interrupt __far (*newCtrlCHandler)() ) {

  // Random number generator: used for setting up sequence numbers
  srand((unsigned) time( NULL ));


  // Start tracing as early as possible.
  Trace_beginTracing( );


  // Initialize the packet layer code - buffers and packet driver interfaces.

  if ( Buffer_init( ) ) {
    fprintf( stderr, InitErrorMsg, "packet buffers" );
    return -1;
  }

  if ( Packet_init( packetInt ) ) {
    fprintf( stderr, InitErrorMsg, "packet driver" );
    return -1;
  }


  //---------------------------------------------------------------------------
  //
  // At this point the packet driver is live and trying to get buffers for
  // incoming packets from us.  If there is a failure to initialize in the
  // rest of the code we need to call endStack to clean everything up nicely.
  //
  //---------------------------------------------------------------------------


  // Install the new Ctrl-Break and Ctrl-C handlers.
  //
  oldCtrlBreakHandler = getvect( 0x1b );
  setvect( 0x1b, newCtrlBreakHandler );
  setvect( 0x23, newCtrlCHandler );


  // Get our Ethernet address now that we can talk to the packet driver.
  // (This should not fail.)
  Packet_get_addr( MyEthAddr );


  // Register our EtherType handlers
  //
  // The most commonly seen packets should be at the head of the list to reduce
  // the search time.  (It's a linear search through an array.)

  #ifdef COMPILE_ARP
  if ( Packet_registerEtherType( 0x0800, Ip::process ) || Packet_registerEtherType( 0x0806, Arp::processArp ) ) {
  #else
  if ( Packet_registerEtherType( 0x0800, Ip::process ) ) {
  #endif
    endStack( );
    fprintf( stderr, InitErrorMsg, "EtherTypes" );
    return -1;
  }
    


  // We want this to appear if any type of tracing is turned on.  The normal
  // tracing macros are insufficient for this so just use Utils::Debugging
  // and tprintf directly.

  #ifndef NOTRACE

  if ( Trace_Debugging ) {

    Trace_tprintf( "mTCP " MTCP_PROGRAM_NAME " Version: " __DATE__ "\n" );

    Trace_tprintf( "  %s=0x%x MAC=%02X.%02X.%02X.%02X.%02X.%02X %s=%d\n",
	           Parm_PacketInt, packetInt,
	           MyEthAddr[0], MyEthAddr[1], MyEthAddr[2],
	           MyEthAddr[3], MyEthAddr[4], MyEthAddr[5],
	           Parm_Mtu, MyMTU );

    Trace_tprintf( "  %s=%d.%d.%d.%d %s=%d.%d.%d.%d %s=%d.%d.%d.%d\n",
	          Parm_IpAddr, MyIpAddr[0], MyIpAddr[1], MyIpAddr[2], MyIpAddr[3],
	          Parm_Netmask, Netmask[0], Netmask[1], Netmask[2], Netmask[3],
	          Parm_Gateway, Gateway[0], Gateway[1], Gateway[2], Gateway[3] );

    uint16_t dosv = dosVersion( );

    Trace_tprintf( "  Debug level: 0x%x, DOS Version: %u.%02u\n",
                   Trace_Debugging,
                   (dosv & 0xff), (dosv >> 8) );

    #ifdef TORTURE_TEST_PACKET_LOSS
      Trace_tprintf( "  Torture testing: losing 1 in %u packets\n", TORTURE_TEST_PACKET_LOSS );
    #endif

  }

  #endif



  // Hook the timer interrupt.  Does not fail.
  Timer_start( );


  // Initialize Arp.  Does not fail.

  #ifdef COMPILE_ARP
  Arp::init( );
  #endif


  #ifdef IP_FRAGMENTS_ON
  if ( Ip::initForReassembly( ) ) {
    fprintf( stderr, InitErrorMsg, "IP reassembly buffers" );
    endStack( );
    return -1;
  }
  #endif


  #ifdef COMPILE_ICMP
  // Initialize ICMP.  Does not fail.
  Icmp::init( );
  #endif


  #ifdef COMPILE_TCP
    if ( TcpSocketMgr::init( tcpSockets ) ) {
      fprintf( stderr, InitErrorMsg, "TCP sockets" );
      endStack( );
      return -1;
    }

    if ( TcpBuffer::init( tcpXmitBuffers ) ) {
      fprintf( stderr, InitErrorMsg, "TCP buffers" );
      endStack( );
      return -1;
    }
  #endif


  #ifdef COMPILE_DNS

    if ( Dns::init( DNS_HANDLER_PORT ) ) {
      fprintf( stderr, InitErrorMsg, "DNS" );
      endStack( );
      return -1;
    }

    #ifndef NOTRACE
    if ( Trace_Debugging ) {
      Trace_tprintf( "  %s=%d.%d.%d.%d\n", Parm_Nameserver, Dns::NameServer[0],
                     Dns::NameServer[1], Dns::NameServer[2], Dns::NameServer[3] );
    }
    #endif

  #endif




  #ifdef SLEEP_CALLS

    // Test to see if we should be making idle calls to int 2f/1680.

    uint32_t far *int2F = (uint32_t far *)MK_FP( 0, (0x2F*4) );

    if (mTCP_sleepCallEnabled && *int2F) {

      // Sleep calls are enabled and there is something installed at
      // int 2f.  Try to call it.  If we get a zero back it is supported

      if ( releaseTimeslice( ) == 0 ) mTCP_releaseTimesliceEnabled = 1;

    }

    #ifndef NOTRACE
      if ( Trace_Debugging ) {
        Trace_tprintf( "  DOS Sleep calls enabled: int 0x28:%u  int 0x2f,1680:%u\n",
                       mTCP_sleepCallEnabled, mTCP_releaseTimesliceEnabled );
      }
    #endif

  #endif


  // We are ready to run!  This will make all of the free buffers visible
  // so that the packet driver can use them, instead of forcing it to throw
  // everything away.
  Buffer_startReceiving( );



  #ifdef COMPILE_ARP

  // Arp our own address for a little bit of time.  If we get a response back
  // then we know that the IP address is being used by another machine.
  //
  // mTCP does not respond to its own ARP packets, and if somebody sends us a
  // response with our own MAC address we don't add it to the table.  So if we
  // ever get a response in the table, that means it is a response were the
  // MAC address was not ours, and thus a conflict.
  //
  // This does not make sense to do during DHCP, as we do not know our IP
  // address.

  EthAddr_t tmpEthAddr;
  clockTicks_t startTime;
  clockTicks_t lastCheck;
  startTime = lastCheck = TIMER_GET_CURRENT( );

  while ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) < TIMER_MS_TO_TICKS( ARP_TIMEOUT ) ) {

    if ( Arp::resolve( MyIpAddr, tmpEthAddr ) == 0 ) {
      fprintf( stderr, "Init: IP address conflict!\nA machine with MAC address %02x:%02x:%02x:%02x:%02x:%02x is already using %d.%d.%d.%d\n",
        tmpEthAddr[0], tmpEthAddr[1], tmpEthAddr[2], tmpEthAddr[3], tmpEthAddr[4], tmpEthAddr[5],
        MyIpAddr[0], MyIpAddr[1], MyIpAddr[2], MyIpAddr[3] );
      endStack( );
      return -1;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );

    // Delay for a little bit to avoid trace record spew
    while ( lastCheck == TIMER_GET_CURRENT( ) ) { };
    lastCheck = TIMER_GET_CURRENT( );

  }

  // Most things that need ARP resolution wait until they get it or a very
  // large connect timeout happens.  We are not waiting very long;  clear the
  // pending table out.

  Arp::clearPendingTable( );

  #endif


  // All is good ...
  return 0;
}




// Utils::endStack
//
// Do the opposite of initStack - terminate things in the correct order.  This
// should always be safe to call, even from within initStack.

void Utils::endStack( void ) {

  // Set the number of free incoming buffers for packets to zero so that the
  // packet driver can not give us any more work to do.  (All incoming packets
  // will get dropped on the floor by the packet driver after this.)
  //
  // Also, drop the packet driver.  We don't want later code to accidentally
  // return an incoming buffer to the free list, giving the packet driver
  // something to put on our incoming ring buffer.

  Buffer_stopReceiving( );
  Packet_release_type( );


  #ifdef COMPILE_DNS
    Dns::stop( );
  #endif


  #ifdef COMPILE_TCP

    // These next two calls just return memory.  We are not going to try to
    // cleanly close the sockets down; that was the responsibility of the
    // user.
    //
    // If the user had other memory allocated (receive buffers for sockets)
    // they need to clean those up too!  The OS will probably protect us
    // but really all of the sockets should have been closed and recycled
    // before getting here.

    TcpSocketMgr::stop( );
    TcpBuffer::stop( );

  #endif


  // No need to do anything for ICMP

  #ifdef IP_FRAGMENTS_ON
    // Returns any packets being used for fragment reassembly to the
    // incoming buffer pool and frees the memory for the BigPackets.
    Ip::reassemblyStop( );
  #endif


  // No need to do anything for ARP


  // Unhook the timer interrupt
  Timer_stop( );


  // At this point the packet driver is not active and we've unhooked from
  // the BIOS timer tick interrupt.  Unload the users Ctrl-Break handler.
  // (We do not need to unload a Ctrl-C handler; DOS will do that.)
  //
  setvect( 0x1b, oldCtrlBreakHandler);


  // This just frees memory.
  Buffer_stop( );


  // If any form of tracing was active then write the final stats out.
  #ifndef NOTRACE
  if ( Trace_Debugging ) dumpStats( Trace_Stream );
  #endif


  if ( _heapchk( ) != _HEAPOK ) {
    fprintf( stderr, "End: heap is corrupted!\n" );
  }


  Trace_endTracing( );

  fflush( NULL );
}


void Utils::dumpStats( FILE *stream ) {

  #ifdef COMPILE_TCP
  Tcp::dumpStats( stream );
  #endif

  #ifdef COMPILE_UDP
  Udp::dumpStats( stream );
  #endif

  Ip::dumpStats( stream );

  #ifdef COMPILE_ARP
  Arp::dumpStats( stream );
  #endif

  Packet_dumpStats( stream );

};



uint32_t Utils::timeDiff( struct dostime_t startTime, struct dostime_t endTime ) {

  uint32_t rc;
  uint32_t st = startTime.hsecond + startTime.second * 100l +
		startTime.minute * 6000l + startTime.hour * 360000l;

  uint32_t et = endTime.hsecond + endTime.second * 100l +
		endTime.minute * 6000l + endTime.hour * 360000l;

  if ( et < st ) {
    rc = (et + 8640000l) - st;
  }
  else {
    rc = et - st;
  }

  return rc;
}




// bufLen includes the NULL character at the end
//
// Puts the next token in target.
// Returns pointer to next spot in buffer or NULL if:
//
//   - if input is NULL
//   - if input is all whitespace
//   - If you bump into the end of the string
//

char *Utils::getNextToken( char *input, char *target, uint16_t bufLen ) {

  if ( input == NULL ) {
    *target = 0;
    return NULL;
  }

  // Skip leading whitespace
  int l = strlen(input);
  int i=0;
  while ( (i<l) && (isspace(input[i])) ) {
    i++;
  }

  if ( i == l ) {
    *target=0;
    return NULL;
  }

  /*
  int j=0;
  // We are at the first non-space char
  for ( ; (i<l) && (!isspace(input[i])); i++,j++ ) {
    if ( j < bufLen ) target[j] = input[i];
  }
  */


  // State machine
  //
  // Normal        -> Quote                -> QuoteSeen
  // Normal        -> Space                -> Delimeter found, bail out
  // Normal        -> Normal Char          -> Normal: (Add char to str)
  // QuoteSeen     -> Normal Char or Space -> InQuoteRegion: (Add char to str)
  // QuoteSeen     -> Quote                -> Normal: (add quote to str)
  // InQuoteRegion -> Normal Char or Space -> InQuoteRegion: (Add char to str)
  // InQuoteRegion -> Quote                -> InQ_QSeen
  // InQ-QSeen     -> Normal Char          -> InQuoteRegion: (Add quote and char to str)
  // InQ-QSeen     -> Space                -> Delimeter found, bail out
  // InQ-QSeen     -> Quote                -> InQuoteRegion: (add quote to str)


  // We are at the first non-space character

  enum States { Normal, QuoteSeen, InQuoteRegion, InQuoteRegionQuoteSeen, DelimFound };

  States st = Normal;
  int j=0;

  for ( ; i<l; i++ ) {

    switch (st) {
      case Normal: {
        if ( input[i] == '"' ) { st = QuoteSeen; break; }
        if ( isspace( input[i] ) ) { st = DelimFound; break; }
        if ( j < bufLen) target[j++] = input[i];
        break;
      }
      case QuoteSeen: {
        if ( input[i] == '"' ) {
          st = Normal;
          if ( j < bufLen ) target[j++] = '"';
          break;
        }
        st = InQuoteRegion;
        if ( j < bufLen ) target[j++] = input[i];
        break;
      }
      case InQuoteRegion: {
        if ( input[i] == '"' ) {
          st = InQuoteRegionQuoteSeen;
          break;
        }
        if ( j < bufLen ) target[j++] = input[i];
        break;
      }
      case InQuoteRegionQuoteSeen: {
        if ( isspace( input[i] ) ) {
          st = DelimFound;
          break;
        }
        if ( input[i] == '"' ) {
          st = InQuoteRegion;
          target[j++] = '"';
          break;
        }
        st = InQuoteRegion;
        target[j++] = '"';
        target[j++] = input[i];
        break;
      }
    }

    if ( st == DelimFound ) break;

  }

  if ( j < bufLen ) {
    target[j] = 0;
  }
  else {
    target[bufLen-1] = 0;
  }

  if ( i == l ) {
    return NULL;
  }

  return &input[i];
}




