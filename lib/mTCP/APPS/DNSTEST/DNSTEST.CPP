/*

   mTCP DnsTest.cpp
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


   Description: DnsTest application

   Changes:

   2011-05-27: Initial release as open source software
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.

*/



// Command line DNS resolver.

#include <bios.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"

#include "timer.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "udp.h"
#include "dns.h"

#ifdef COMPILE_TCP
#include "tcp.h"
#endif



void parseArgs( int argc, char *argv[] );
void shutdown( int rc );




// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}




char TargetName[DNS_MAX_NAME_LEN] = "";
uint8_t Verbose = 0;
uint8_t DNSRecursion = 1;
clockTicks_t DNSTimeout = 10000ul;



char *DnsErrors[] = {
  "(0) No error",
  "(1) Format error",
  "(2) Server failure",
  "(3) Name error - Name probably does not exist",
  "(4) Not implemented",
  "(5) Server Refused Us!"
};



static char CopyrightMsg1[] = "mTCP DNSTest by M Brutman (mbbrutman@gmail.com) (C)opyright 2009-2020\n";
static char CopyrightMsg2[] = "Version: " __DATE__ "\n\n";



int main( int argc, char *argv[] ) {

  printf( "%s  %s", CopyrightMsg1, CopyrightMsg2 );

  parseArgs( argc, argv );


  // Initialize TCP/IP

  if ( Utils::parseEnv( ) != 0 ) {
    exit(-1);
  }

  // No TCP sockets and no TCP Xmit buffers
  if ( Utils::initStack( 0, 0, ctrlBreakHandler, ctrlBreakHandler ) ) {
    fprintf( stderr, "\nFailed to initialize TCP/IP - exiting\n" );
    exit(-1);
  }


  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.


  printf( "Timeout set to %lu seconds, DNS Recursion = %s\n",
	  (DNSTimeout/1000ul), (DNSRecursion ? "on" : "off") );
  puts( "Press [ESC] or [Ctrl-C] to quit early\n" );



  IpAddr_t newAddr;

  int8_t rc = Dns::resolve( TargetName, newAddr, 1 );

  if ( rc == -1 ) {
    fprintf( stderr, "Error: Machine name to long, the limit is %u characters\n",
	    (DNS_MAX_NAME_LEN-1) );
    shutdown( -1 );
  }
  else if ( rc == -2 ) {
    fprintf( stderr, "Error: You have not set a nameserver up.  Check the mTCP config file\n" );
    shutdown( -1 );
  }



  clockTicks_t startTicks = TIMER_GET_CURRENT( );

  uint8_t userQuit = 0;

  while ( 1 ) {

    if ( CtrlBreakDetected ) {
      userQuit = 1;
      puts( "\nCtrl-Break detected - ending!" );
      break;
    }

    if ( bioskey(1) != 0 ) {
      char c = bioskey(0);
      if ( (c == 27) || (c == 3) ) {
	userQuit = 1;
	puts( "\nCtrl-C or ESC detected - ending!" );
	break;
      }
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Dns::drivePendingQuery( );


    if ( !Dns::isQueryPending( ) ) {
      break;
    }

  }

  if ( Verbose ) puts("");

  if ( !userQuit ) {

    int8_t queryRc = Dns::getQueryRc( );

    if ( queryRc == -1 ) {
      puts( "Query timed out" );
    }
    else if (queryRc == 0 ) {

      // Should have the name in the DNS cache now.
      // No need to check for errors; we've done that already
      int rc2 = Dns::resolve( TargetName, newAddr, 0 );

      clockTicks_t elapsedTicks = Timer_diff( startTicks, TIMER_GET_CURRENT( ) );

      uint32_t milliseconds = elapsedTicks * TIMER_TICK_LEN;

      printf( "Machine name %s resolved to %d.%d.%d.%d\n",
	      TargetName, newAddr[0], newAddr[1], newAddr[2], newAddr[3] );

      printf( "Elapsed time in seconds: %lu.%03lu\n",
	      (milliseconds/1000ul), (milliseconds % 1000ul) );

      TRACE(( "Machine name %s resolved to %d.%d.%d.%d\n",
	      TargetName, newAddr[0], newAddr[1], newAddr[2], newAddr[3] ));
    }
    else {

      if ( (queryRc > 0) && (queryRc < 6) ) {
	printf( "Dns server error: %s\n", DnsErrors[queryRc] );
      }
      else {
	printf( "Dns server returned error code %d\n", queryRc );
      }

    }

  }


  shutdown( 0 );

}


void shutdown( int rc ) {
  Utils::endStack( );
  if ( Verbose == 0 ) Utils::dumpStats( stderr );
  exit( rc );
}


char *HelpText[] = {
  "\ndnstest -name <machine name> [options]\n\n",
  "Options:\n",
  "  -help        (Shows this help)\n",
  "  -timeout <n> (Set timeout to n seconds)\n",
  "  -norecurse   (Do not request a recursive lookup (default is do)\n",
  "  -verbose     (Show lots of fun output)\n",
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




void parseArgs( int argc, char *argv[] ) {

  int i=1;
  for ( ; i<argc; i++ ) {

    if ( stricmp( argv[i], "-help" ) == 0 ) {
      usage( );
    }
    else if ( stricmp( argv[i], "-name" ) == 0 ) {
      i++;
      if ( i == argc ) {
	fprintf( stderr, "You must specify a machine name to resolve on the -name parameter\n" );
	usage( );
      }
      strncpy( TargetName, argv[i], DNS_MAX_NAME_LEN );
      TargetName[DNS_MAX_NAME_LEN-1] = 0;
    }
    else if ( stricmp( argv[i], "-verbose" ) == 0 ) {
      Verbose = 1;
      Trace_Debugging = Trace_Debugging | 0x41;
    }
    else if ( stricmp( argv[i], "-timeout" ) == 0 ) {
      i++;
      if ( i == argc ) {
	fprintf( stderr, "You must specify a number of seconds on the -timeout option\n" );
	usage( );
      }
      DNSTimeout = atoi( argv[i] );
      if ( DNSTimeout < 1 ) DNSTimeout = 1;
      DNSTimeout = DNSTimeout * 1000ul;
    }
    else if ( stricmp( argv[i], "-norecurse" ) == 0 ) {
      DNSRecursion = 0;
    }
    else {
      fprintf( stderr, "Unknown option %s\n", argv[i] );
      usage( );
    }

  }

  if ( TargetName[0] == 0 ) {
    fprintf( stderr, "You must specify the -name parameter.\n" );
    usage( );
  }

}
