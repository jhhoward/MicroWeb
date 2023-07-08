/*

   mTCP Sntp.cpp
   Copyright (C) 2010-2023 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Your typical run-of-the-mill SNTP (simple network time
     protocol client).  DOS is kind of limited by the system timer,
     which only has a resolution of 55ms.  So don't go overboard on
     the accuracy because it just doesn't matter.  Getting to within
     a few dozen milliseconds of a public NTP server is more than
     good enough.

   Changes:

   2011-05-27: Initial release as open source software
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.
   2022-01-22: Use the NTP fractional seconds to set the time just
               a little more accurately.

*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"

#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "dns.h"
#include "timer.h"

#include "sntp.h"
#include "sntplib.h"


#define SERVER_ADDR_NAME_LEN (80)



// Return codes

#define MAIN_RC_GOOD               (0)
#define MAIN_RC_OTHER_ERROR        (1)
#define MAIN_RC_USAGE_ERROR        (2)
#define MAIN_RC_CONFIG_FAIL        (3)
#define MAIN_RC_NO_TIMEZONE        (4)
#define MAIN_RC_NETWORK_INIT_FAIL  (5)
#define MAIN_RC_USER_ABORT         (6)
#define MAIN_RC_DNS_FAIL           (7)
#define MAIN_RC_ERROR_SETTING_TIME (8)
#define MAIN_RC_SERVER_TIMEOUT     (9)


char     ServerAddrName[ SERVER_ADDR_NAME_LEN ];
IpAddr_t ServerAddr;
uint16_t ServerPort = 123;

uint16_t TimeoutSecs = 3;
uint16_t Retries = 3;
uint8_t  Verbose = 0;
uint8_t  Mode =    0;
uint8_t  SetTime = 0;

SntpLib::Callback_data_t SntpCallbackResponse;
NTP_packet_t SntpPacket;

int SetDosTimeRC;


// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}



// Function prototypes

void callback( SntpLib::Callback_data_t d );
void printResponse( SntpLib::Callback_data_t d );
void parseArgs( int argc, char *argv[] );
void shutdown( int rc );
void continuous( void );




bool checkUserExit( void ) {

  if ( CtrlBreakDetected ) {
    puts( "\nCtrl-Break detected: aborting\n" );
    return true;
  }

  if ( biosIsKeyReady( ) ) {
    char c = biosKeyRead( );
    if ( (c == 27) || (c == 3) ) {
      puts( "\nCtrl-C or ESC detected: aborting\n" );
      return true;
    }
  }

  return false;
}



static char CopyrightMsg1[] = "mTCP SNTP Client by M Brutman (mbbrutman@gmail.com) (C)opyright 2009-2023\n";
static char CopyrightMsg2[] = "Version: " __DATE__ "\n\n";


int main( int argc, char *argv[] ) {

  int mainRc = MAIN_RC_OTHER_ERROR;

  printf( "%s  %s", CopyrightMsg1, CopyrightMsg2 );

  parseArgs( argc, argv );

  TimeoutSecs = TimeoutSecs * 1000;

  // Initialize TCP/IP
  if ( Utils::parseEnv( ) != 0 ) {
    exit( MAIN_RC_CONFIG_FAIL );
  }


  char *tzStr = getenv( "TZ" );
  if ( tzStr == NULL ) {
    puts( "Error: The TZ environment variable must be set.  See the mTCP\n"
          "documentation for how to set it properly for your time zone.\n" );
    exit( MAIN_RC_NO_TIMEZONE );
  }

  tzset( );

  if ( Verbose ) {
    printf( "Timezone name[0]: %s  name[1]: %s\n", tzname[0], tzname[1] );
    printf( "Timezone offset in seconds: %ld\n", timezone );
    printf( "Daylight savings time supported: %d\n\n", daylight );
  }


  // No sockets, no buffers TCP buffers
  if ( Utils::initStack( 0, 0, ctrlBreakHandler, ctrlBreakHandler ) ) {
    puts( "Failed to initialize the network." );
    exit( MAIN_RC_NETWORK_INIT_FAIL );
  }


  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.


  printf( "Resolving %s, press [ESC] to abort.\n", ServerAddrName );

  // Resolve the name and definitely send the request
  int8_t rc = Dns::resolve( ServerAddrName, ServerAddr, 1 );
  if ( rc < 0 ) {
    puts( "Error resolving server" );
    shutdown( MAIN_RC_DNS_FAIL );
  }

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( 1 ) {

    if ( checkUserExit( ) ) {
      shutdown( MAIN_RC_USER_ABORT );
    }

    if ( !Dns::isQueryPending( ) ) break;

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Dns::drivePendingQuery( );

  }

  // Query is no longer pending.  Did we get an answer?
  if ( Dns::resolve( ServerAddrName, ServerAddr, 0 ) != 0 ) {
    puts( "Error resolving server name" );
    shutdown( MAIN_RC_DNS_FAIL );
  }

  uint32_t t = Timer_diff( startTime, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;
  printf( "NTP server ip address is: %d.%d.%d.%d, resolved in %ld.%02ld seconds\n",
           ServerAddr[0], ServerAddr[1], ServerAddr[2], ServerAddr[3],
           (t/1000), (t%1000)
        );



  // By this point we have resolved the server address and are ready to send the query.

  SntpLib::init( ServerAddr, ServerPort, callback );


  if ( Mode == 1 ) {
    continuous( );
    // Never returns.
  }



  // At this point we can fail if the user cancels the request, if we can't
  // send the UDP request for some reason, or if we timeout.
  //
  // Assume that we are going to timeout.  If we get a response then we'll fix
  // the return code.

  mainRc = MAIN_RC_SERVER_TIMEOUT;

  for ( int i=0; i < Retries; i++ ) {

    if ( checkUserExit( ) ) {
      shutdown( MAIN_RC_USER_ABORT );
    }

    if ( Verbose ) {
      printf( "\nSending request # %d\n", i );
    }

    uint32_t outTime, outTimeFrac;
    int rc = SntpLib::sendSNTPRequest( true, &outTime, &outTimeFrac );

    if ( Verbose ) {
      printf( "Outgoing transmit time: %s\n",
              SntpLib::printTimeStamp( outTime, outTimeFrac, false ) );
    }

    if ( rc < 0 ) {
      if ( rc == -2 ) {
        // ARP timeout or error sending a UDP packet.  Try again.
        puts( "Warning: ARP timeout sending request - check your gateway setting" );
      }
      continue;
    }


    // Spin until we get a response

    clockTicks_t startTime = TIMER_GET_CURRENT( );

    while ( !SntpLib::replyReceived( ) ) {

      if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( TimeoutSecs ) ) {
        TRACE_WARN(( "Sntp: Timeout waiting for sntp response\n" ));
        puts( "Timeout waiting for server response" );
        break;
      }

      PACKET_PROCESS_SINGLE;
      Arp::driveArp( );

    }

    // Did we get a timestamp?
    if ( SntpLib::replyReceived( ) ) {
      mainRc = MAIN_RC_GOOD;
      break;
    }

  }
  

  if ( mainRc == MAIN_RC_GOOD ) {

    // We got a reponse back.  Print the details.
    printResponse( SntpCallbackResponse );

    if ( SetTime ) {
      if ( SetDosTimeRC == 0 ) {
        puts( "\nSystem time set to new value" );
      }
      else {
        puts( "\nError setting system time!" );
        mainRc = MAIN_RC_ERROR_SETTING_TIME;
      }
    }
    else {
      puts( "\nSystem time not updated; use the -set option if you want that." );
    }

  }


  shutdown( mainRc );

  // Never get here - we return from shutdown
  return 0;
}



char *HelpText = 
  "\nsntp [options] <ipaddr>\n\n"
  "Options:\n"
  "  -help          Shows this help\n"
  "  -port <n>      Contact server on port <n> (default=123)\n"
  "  -retries <n>   Number of times to retry if no answer (default=1)\n"
  "  -set           Set the system time (default is not to)\n"
  "  -timeout <n>   Seconds to wait for a server response (default=3)\n"
  "  -verbose       Turn on verbose messages\n"
  "  -continuous    Send queries once a second. (Press ESC to end)\n";




// Default return code if usage( ) is called.
static int usageRc = MAIN_RC_USAGE_ERROR;

void usage( ) {
  puts( HelpText );
  exit( usageRc );
}



void parseArgs( int argc, char *argv[] ) {

  int i=1;
  for ( ; i<argc; i++ ) {

    if ( argv[i][0] != '-' ) break;

    if ( stricmp( argv[i], "-help" ) == 0 ) {
      usageRc = 0;
      usage( );
    }
    else if ( stricmp( argv[i], "-port" ) == 0 ) {
      i++;
      if ( i == argc ) {
        usage( );
      }
      ServerPort = atoi( argv[i] );
      if ( ServerPort == 0 ) {
        puts( "Bad parameter for -port: can not use 0" );
        usage( );
      }
    }
    else if ( stricmp( argv[i], "-retries" ) == 0 ) {
      i++;
      if ( i == argc ) {
        usage( );
      }
      Retries = atoi( argv[i] );
      if ( Retries == 0 ) {
        puts( "Bad parameter for -retries: Should be greater than 0" );
        usage( );
      }
    }
    else if ( stricmp( argv[i], "-set" ) == 0 ) {
      SetTime = 1;
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
    else if ( stricmp( argv[i], "-verbose" ) == 0 ) {
      Verbose = 1;
    }
    else if ( stricmp( argv[i], "-continuous" ) == 0 ) {
      Mode = 1;
    }
    else {
      printf( "Unknown option %s\n", argv[i] );
      usage( );
    }

  }

  if ( i == argc ) {
    puts( "You need to specify a machine name or IP address" );
    usage( );
  }

  strncpy( ServerAddrName, argv[i], SERVER_ADDR_NAME_LEN );
  ServerAddrName[ SERVER_ADDR_NAME_LEN - 1 ] = 0;

}



void shutdown( int rc ) {
  Utils::endStack( );
  exit( rc );
}





// This gets called from the UDP handler while it is still holding onto the
// received response packet.  Copy the data so that we can return quickly.
// (That is not really necessary for this program, but it is good practice.)

void callback( SntpLib::Callback_data_t d ) {

  // Set the DOS time as close to the response being received as possible.
  // Printing everything out first will cost us milliseconds on slow machines
  // and throw off our accuracy even more.

  if ( SetTime ) {
    SetDosTimeRC = SntpLib::setDosDateTime( );
  }

  // Copy the returned structure, the underlying packet, and ensure the pointer
  // points to our new copy of the underlying packet, not into the buffer that
  // will soon be freed.
  SntpPacket = *d.ntp;
  SntpCallbackResponse = d;
  SntpCallbackResponse.ntp = &SntpPacket;

}




void printResponse( SntpLib::Callback_data_t d ) {

  if ( Verbose ) {
    printf( "\nReponse packet from ntp server: \n" );
    printf( "  Leap indicator: %d\n", (d.ntp->mode & 0xc0) >> 6 );
    printf( "  Version number: %d\n", (d.ntp->mode & 0x38) >> 3 );
    printf( "  Stratum:        %d\n", d.ntp->stratum );
    printf( "  Reference ts:   %s UTC\n", SntpLib::printTimeStamp( ntohl( d.ntp->refTimeSecs ) - NTP_OFFSET, ntohl(d.ntp->refTimeFrac), false ) );
    printf( "  Original ts:    %s UTC\n", SntpLib::printTimeStamp( ntohl( d.ntp->origTimeSecs ) - NTP_OFFSET, ntohl(d.ntp->origTimeFrac), false ) );
    printf( "  Receive ts:     %s UTC\n", SntpLib::printTimeStamp( ntohl( d.ntp->recvTimeSecs ) - NTP_OFFSET, ntohl(d.ntp->recvTimeFrac), false ) );
    printf( "  Transmit ts:    %s UTC\n", SntpLib::printTimeStamp( ntohl( d.ntp->transTimeSecs ) - NTP_OFFSET, ntohl(d.ntp->transTimeFrac), false ) );
  }

  printf( "\nYour selected timezone is: %s\n", tzname[0] );

  printf( "\nCurrent system time is: %s\n", SntpLib::printTimeStamp( d.currentTime, d.currentTimeFrac , true ) );
  printf( "Time should be set to:  %s\n\n", SntpLib::printTimeStamp( d.targetTime, d.targetTimeFrac, true ) );

  if ( ((d.diffSecs * 1000) + d.diffMs) < 600000 ) {
    printf( "Difference between suggested time and system time is: %lu.%03u seconds\n", d.diffSecs, d.diffMs );
  }
  else {
    puts( "Difference between suggested time and system time is greater than 10 minutes!" );
  }

  puts( "(Remember, the smallest increment of time for DOS is 55 milliseconds.)" );
}



void continuous( void ) {

  while ( 1 ) {

    if ( checkUserExit( ) ) {
      shutdown( MAIN_RC_USER_ABORT );
    }

    uint32_t outTime, outTimeFrac;
    int rc = SntpLib::sendSNTPRequest( true, &outTime, &outTimeFrac );

    // Spin until we get a response

    clockTicks_t startTime = TIMER_GET_CURRENT( );

    while ( !SntpLib::replyReceived( ) ) {
      if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( TimeoutSecs ) ) {
        break;
      }
      PACKET_PROCESS_SINGLE;
      Arp::driveArp( );

    }

    if ( SntpLib::replyReceived( ) ) {
      printf( "Delta between local and server time: %lu.%03u seconds\n", SntpCallbackResponse.diffSecs, SntpCallbackResponse.diffMs );
    } else {
      puts( "Timeout waiting for server response" );
    }


    // Consume the rest of the second so that we send a request once a second
    while ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) < TIMER_MS_TO_TICKS( 1000 ) ) { }
  }

}


