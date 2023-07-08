/*

   mTCP Dhcp.cpp
   Copyright (C) 2008-2023 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Dhcp application

   Changes:

   2011-05-27: Initial release as open source software
   2011-10-01: Add one second delay at startup to help packet drivers
               that are slow to initialize.
   2013-03-29: Add numerical timestamp to DHCP timestamp message
               to make it easier to compute lease expiration.
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling; get
               rid of view packet stats command line option
   2015-04-10: Add preferred nameserver support; fix code to use
               the same buffers and lengths as the corresponding
               Utils code; fix makefile system to remove false
               dependency on ARP to make the filesize smaller.
   2015-04-19: Refactoring: let the Utils class parse the optional
               environment variables to reduce code duplication.
   2019-09-01: Enable warnings for trailing white space in the config file.
               Make usage of stderr more consistent.  Small refactors to
               try to claim back some executable size.
   2022-04-03: Bug fix: need to parse options on the DHCP ACK message;
               Clean up trace messages and make them more consistent;
               Add a config parameter for requesting a specific lease time.

*/



// The Dhcp client.  Dhcp looks more like an app than a base part of the
// protocol, so all of the data structures we need specific to Dhcp are
// here.


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <io.h>


#include "types.h"

#include "utils.h"
#include "timer.h"
#include "trace.h"
#include "packet.h"
#include "arp.h"
#include "udp.h"

#include "dhcp.h"




// Storage for the outgoing request.  This gets used on the initial
// DHCP_DISCOVER and reused on the DHCP_REQUEST.  This is global
// because it's easy to address that way.

Dhcp_t req;


// State we need to keep during the process

DhcpStatus_t DhcpStatusFlag;        // DHCP conversation status
IpAddr_t     ServerIdentity;        // Needed for the DHCPREQUEST packet


// mTCP Config filename from the process environment.

char *CfgFilename;


// Parameters that will be written to the config file.

IpAddr_t NewIpAddr;
IpAddr_t SubnetMask;
IpAddr_t GatewayAddr;
IpAddr_t NameServer;
IpAddr_t PreferredNameServer;
uint32_t OfferedLeaseTime = 0;


// These are responses from the router.  Interesting, but we don't do anything
// with them.
char Domain_assigned[40] = "";
char Hostname_assigned[40] = "";



// Command line parameters

uint8_t  Retries = 3;
uint16_t Timeout = 10;
uint32_t TimeoutMs = 10000ul;



// Misc config parameter handling.

bool PreferredNameServer_set = false;

uint32_t RequestedLeaseSecs = 28800; // Eight hours



// Function prototypes

void   parseArgs( int argc, char *argv[] );
int8_t parseEnv( void );
void   shutdown( int rc );

uint16_t     setupReqPacket( void );
DhcpStatus_t makeAttempt( int i );
int8_t       createNewCfg( void );

void udpHandler(const unsigned char *packet, const UdpHeader *udp );
void udpHandler2( Dhcp_t *resp );
void sendDhcpRequestMsg( void );



// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}



int printIpAddr( FILE *stream, const char *name, IpAddr_t addr ) {
  return fprintf( stream, "%s %d.%d.%d.%d\n", name, addr[0], addr[1], addr[2], addr[3] );
}



const char *DhcpMsgName[] = {
  "",
  "Discover",
  "Offer",
  "Request",
  "Decline",
  "Ack",
  "NAck",
  "Release",
  "Inform"
};

const char *CheckYourCabling_msg = "Check your cabling and packet driver settings, including the hardware IRQ.";

static char *CopyrightMsg = "mTCP DHCP Client by M Brutman (mbbrutman@gmail.com) (C)opyright 2008-2023\nVersion: " __DATE__ "\n\n";



int main( int argc, char *argv[] ) {

  printf( CopyrightMsg );

  parseArgs( argc, argv );

  // Random number generator: used for setting up sequence numbers
  srand((unsigned) time( NULL ));

  if ( parseEnv( ) ) {
    exit(1);
  }

  Ip::setMyIpAddr( 0ul );

  // No TCP sockets or TCP buffers
  if ( Utils::initStack( 0, 0, ctrlBreakHandler, ctrlBreakHandler ) ) {
    fprintf( stderr, "Could not initialize TCP/IP stack\n\n" );
    exit(1);
  }


  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.


  // This should never fail unless we build the library wrong.
  if ( Udp::registerCallback( DHCP_REPLY_PORT, &udpHandler ) ) {
    fprintf( stderr, "Could not setup DHCP reply handler\n\n" );
    shutdown( 1 );
  }

  printf( "Timeout per request: %u seconds, Retry attempts: %u\n"
          "Requesting a %lu second lease\n"
          "Sending DHCP requests, Press [ESC] to abort.\n\n",
          Timeout, Retries, RequestedLeaseSecs );

  // Delay 1 second; helps with some packet drivers that are not quite
  // ready to run by the time we send a packet out.

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( 1 ) {

    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_TICKS_PER_SEC ) {
      break;
    }

  }


  for ( int i=0; i < Retries; i++ ) {

    DhcpStatus_t rc = makeAttempt( i );

    if ( rc == Dhcp_Ack ) {
      // Success!  Break out of the loop early
      break;
    }

    if ( rc == Dhcp_UserAbort ) {
      // User wants out - break out of the loop early
      shutdown( 1 );
    }

  }


  uint8_t finalRc = 1;

  if ( DhcpStatusFlag == Dhcp_Ack ) {

    int8_t rc = createNewCfg( );
    if ( rc ) {
      fprintf( stderr, "\n"
                       "Error: DHCP address was assigned but we had a problem writing the config file.\n"
                       "No changes were made.\n" );
    }
    else {
      finalRc = 0;

      puts( "\nGood news everyone!\n" );

      printf( "%s %s\n",   Parm_Hostname,   MyHostname );
      printf( "%s %s\n",   Parm_Domain,     Domain_assigned );
      printIpAddr( stdout, Parm_IpAddr,     MyIpAddr );
      printIpAddr( stdout, Parm_Netmask,    Netmask );
      printIpAddr( stdout, Parm_Gateway,    Gateway );
      printIpAddr( stdout, Parm_Nameserver, NameServer );
      printf( "LEASE_TIME %lu seconds\n",   OfferedLeaseTime );

      if ( stricmp( MyHostname, Hostname_assigned ) != 0 ) {
        printf("\nWarning: Your DHCP server may not have honored your hostname request.\n" );
        printf("Requested hostname: \"%s\", Assigned hostname: \"%s\"\n", MyHostname, Hostname_assigned );
      }

      printf( "\nSettings written to '%s'\n", CfgFilename );

    }

  }
  else if ( DhcpStatusFlag == Dhcp_Timeout ) {

    // In case of a timeout try to give the user an idea of what happened.
    // The most severe causes are checked for first.

    if ( Packets_send_errs == Packets_sent ) {
      puts( "\nError: Your Ethernet card reported an error for every packet we sent." );
      puts( CheckYourCabling_msg );
    }
    else if ( Packets_received == 0 ) {
      puts( "\nError: Your DHCP server never responded and no packets were seen on the wire." );
      puts( CheckYourCabling_msg );
    }
    else {
      puts( "\nError: Your DHCP server never responded, but your Ethernet card is receiving\n"
            "packets.  Check your DHCP server, or increase the timeout period."
      );
    }

  }
  else {
    puts( "\nError: Could not get a DHCP address" );
  }

  shutdown( finalRc );

  // Never reached
  return 0;
}





DhcpStatus_t makeAttempt( int i ) {

  uint16_t reqLen = setupReqPacket( );

  DhcpStatusFlag = Dhcp_Start;

  // This does not fail at ARP resolution because we are using the IP broadcast
  // address which in turn becomes the local Ethernet broadcast address, and
  // thus ARP resolution is not attempted.

  Udp::sendUdp( IpBroadcastNonRoutable, DHCP_REPLY_PORT, DHCP_REQUEST_PORT, reqLen, (uint8_t *)&req, 1 );


  printf( "DHCP request sent, attempt %d: ", i+1 );

  // The UDP handler will update a global flag if it gets something.

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( DhcpStatusFlag == Dhcp_Start || DhcpStatusFlag == Dhcp_Offer ) {

    if ( CtrlBreakDetected ) {
      DhcpStatusFlag = Dhcp_UserAbort;
    }

    if ( biosIsKeyReady( ) ) {
      char c = biosKeyRead( );
      if ( (c == 27) || (c == 3) ) {
        DhcpStatusFlag = Dhcp_UserAbort;
      }
    }


    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( TimeoutMs ) ) {
      TRACE_DNS_WARN(( "Dhcp: Timeout waiting for response.\n" ));
      puts( "Timeout" );
      DhcpStatusFlag = Dhcp_Timeout;
      break;
    }

    PACKET_PROCESS_SINGLE;

    // No point in driving ARP because we don't know our IP address until
    // the very end!
    // Arp::driveArp( );

  }

  if ( DhcpStatusFlag == Dhcp_UserAbort ) {
    puts( "Aborting" );
  }

  return DhcpStatusFlag;
}




void shutdown( int rc ) {
  Utils::endStack( );
  exit( rc );
}





void printErrno( const char *desc ) {
  int localErrno = errno;
  fprintf( stderr, "Error while %s: %s\n", desc, strerror( localErrno ) );
}

const char *Msg_writing_to_temp_file = "writing to temp file";


int8_t createNewCfg( void ) {

  // Open the existing config file for reading and a new temp file for
  // writing.  Everthing except the DHCP specific lines will be copied
  // as is.  DHCP specific lines will be re-written.  The temp file will
  // be created in the same directory, and if it is written successfully
  // it will replace the old config file.

  // Many of the error conditions here will never happen because we
  // have already read the config file once.

  FILE *cfgFile = fopen( CfgFilename, "r" );
  if ( cfgFile == NULL ) {
    printErrno( "opening config file" );
    return -1;
  }

  // Normalize the path name, then extract the drive and pathname
  // so we know where to create the temp file.

  char drive[ 3 ];
  char dir[ DOS_MAX_PATH_LENGTH ];
  char tmpFilename[ DOS_MAX_PATHFILE_LENGTH ];

  if ( _fullpath( tmpFilename, CfgFilename, DOS_MAX_PATHFILE_LENGTH ) == NULL ) {
    printErrno( "extracting path of config file" );
    return -1;
  }

  _splitpath( tmpFilename, drive, dir, NULL, NULL );       // Returns no values.
  _makepath( tmpFilename, drive, dir, "mtcpcfg", "tmp" );  // Returns no values.

  FILE *newFile = fopen( tmpFilename, "w" );
  if ( newFile == NULL ) {
    printErrno( "opening temp file for writing" );
    return -1;
  }

  time_t currentTime;
  time( &currentTime );

  if ( (fprintf( newFile, "DHCPVER DHCP Client version %s\n", __DATE__ ) < 0) ||
       (fprintf( newFile, "TIMESTAMP ( %lu ) %s", currentTime, ctime( &currentTime ) ) < 0) )
  {
    printErrno( Msg_writing_to_temp_file );
    return -1;
  }

  char *parmName = Utils::parmName;
  char *lineBuffer = Utils::lineBuffer;

  int linesInFile = 0;

  while ( !feof( cfgFile ) ) {

    linesInFile++;

    if ( Utils::getLine( cfgFile, false, lineBuffer, UTILS_LINEBUFFER_LEN, linesInFile ) ) {
      // Line too long is an error.  We won't bother with the whitespace detection here.
      return -1;
    }

    char *nextTokenPtr = Utils::getNextToken( lineBuffer, parmName, UTILS_PARAMETER_LEN );

    if ( (stricmp( parmName, Parm_IpAddr ) == 0)     ||
         (stricmp( parmName, Parm_Gateway ) == 0)    ||
         (stricmp( parmName, Parm_Netmask ) == 0)    ||
         (stricmp( parmName, Parm_Nameserver ) == 0) ||
         (stricmp( parmName, Parm_Hostname_Assigned ) == 0)   ||
         (stricmp( parmName, Parm_Domain ) == 0)     ||
         (stricmp( parmName, "DHCPVER" ) == 0)       ||
         (stricmp( parmName, "TIMESTAMP" ) == 0)     ||
         (stricmp( parmName, "LEASE_TIME" ) == 0) )

    {
      // Do nothing .. we are going to fill these in
    }
    else {
      if ( fputs( lineBuffer, newFile ) == EOF ) {
        printErrno( Msg_writing_to_temp_file );
        return -1;
      }
    }

  } // end while

  if ( fclose( cfgFile ) ) {
    printErrno( "closing config file" );
    return -1;
  }


  if ( PreferredNameServer_set ) {
    TRACE(( "Dhcp provided nameserver %u.%u.%u.%u replaced by user with %u.%u.%u.%u\n",
            NameServer[0], NameServer[1], NameServer[2], NameServer[3],
            PreferredNameServer[0], PreferredNameServer[1], PreferredNameServer[2], PreferredNameServer[3] ));
    Ip::copy( NameServer, PreferredNameServer );
  }


  // Write new values

  if ( Hostname_assigned[0] ) {
    if ( fprintf( newFile, "%s %s\n", Parm_Hostname_Assigned, Hostname_assigned ) < 0 ) {
      printErrno( Msg_writing_to_temp_file );
      return -1;
    }
  }

  if ( Domain_assigned[0] ) {
    if ( fprintf( newFile, "DOMAIN %s\n", Domain_assigned ) < 0 ) {
      printErrno( Msg_writing_to_temp_file );
      return -1;
    }
  }


  if ( (printIpAddr( newFile, Parm_IpAddr,     MyIpAddr ) < 0 )   ||
       (printIpAddr( newFile, Parm_Netmask,    Netmask ) < 0 )    ||
       (printIpAddr( newFile, Parm_Gateway,    Gateway ) < 0 )    ||
       (printIpAddr( newFile, Parm_Nameserver, NameServer ) < 0 ) ||
       (fprintf( newFile, "LEASE_TIME %lu\n", OfferedLeaseTime ) < 0 ) )
  {
    printErrno( Msg_writing_to_temp_file );
    return -1;
  }



  if ( fclose( newFile ) ) {
    printErrno( Msg_writing_to_temp_file );
    return -1;
  }


  int rc = unlink( CfgFilename );
  if ( rc != 0 ) {
    fprintf( stderr, "Error deleting original config file '%s'\n", CfgFilename );
    return -1;
  }

  rc = rename( tmpFilename, CfgFilename );
  if ( rc != 0 ) {
    fprintf( stderr, "Error renaming '%s' to '%s'\n", tmpFilename, CfgFilename );
    return -1;
  }

  return 0;
}




int8_t parseEnv( void ) {

  CfgFilename = getenv( "MTCPCFG" );
  if ( CfgFilename == NULL ) {
    fprintf( stderr, "Error: You need to set the MTCPCFG environment variable to a valid config file.\n"
                     "The syntax is: set MTCPCFG=filename.ext\n" );
    return -1;
  }

  FILE *cfgFile = fopen( CfgFilename, "r" );
  if ( cfgFile == NULL ) {
    fprintf( stderr, "Error: Not able to open the config file named '%s'.\n"
                     "A config file is required.\n", CfgFilename );
    return -1;
  }


  uint16_t tmp1, tmp2, tmp3, tmp4;

  const char *errorParm = NULL;

  uint8_t packetInt = 0;

  bool errorWhileReadingLine = false;
  bool trailingWhitespaceDetected = false;
  int linesInFile = 0;

  char *parmName = Utils::parmName;
  char *lineBuffer = Utils::lineBuffer;

  while ( !feof( cfgFile ) && (errorParm == NULL) ) {

    linesInFile++;

    if ( errorWhileReadingLine = Utils::getLine( cfgFile, true, lineBuffer, UTILS_LINEBUFFER_LEN, linesInFile ) ) {
      break;
    }

    if ( Utils::rtrim( lineBuffer ) ) {
      fprintf( stderr, "mTCP: Warning - trailing whitespace detected on line %u of the config file.\n", linesInFile );
      trailingWhitespaceDetected = true;
    }

    char *nextTokenPtr = Utils::getNextToken( lineBuffer, parmName, UTILS_PARAMETER_LEN );
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

    else if ( stricmp( parmName, Parm_Mtu ) == 0 ) {
      uint16_t newMtu;
      int rc = sscanf( nextTokenPtr, "%d\n", &newMtu );
      if ( (rc != 1) || (newMtu < ETH_MTU_MIN) || (newMtu > ETH_MTU_MAX) ) {
        errorParm = Parm_Mtu;
      }
      MyMTU = newMtu;
    }

    else if ( stricmp( parmName, Parm_Nameserver_preferred ) == 0 ) {

      int rc = sscanf( nextTokenPtr, "%d.%d.%d.%d\n", &tmp1, &tmp2, &tmp3, &tmp4 );
      if ( rc != 4 ) {
        errorParm = Parm_Nameserver_preferred;
      }
      else {
        PreferredNameServer[0] = tmp1; PreferredNameServer[1] = tmp2;
        PreferredNameServer[2] = tmp3; PreferredNameServer[3] = tmp4;
        PreferredNameServer_set = true;
      }

    }

    else if ( stricmp( parmName, Parm_DHCPLeaseRequest ) == 0 ) {

      int rc = sscanf( nextTokenPtr, "%lu\n", &RequestedLeaseSecs );
      if ( rc != 1 ) {
        errorParm = Parm_DHCPLeaseRequest;
      }

    }

  }

  fclose( cfgFile );

  if ( errorWhileReadingLine ) {
    return -1;
  }

  if ( trailingWhitespaceDetected ) fprintf( stderr, "\n" );

  if ( packetInt == 0x0 ) {
    errorParm = Parm_PacketInt;
  }

  if ( errorParm != NULL ) {
    fprintf( stderr, "mTcp: '%s' is the wrong format or not set correctly.\n", errorParm );
    return -1;
  }

  // We have to do this explicitly because we don't use Utils::parseEnv which
  // would have set it.
  //
  Utils::setPacketInt( packetInt );


  // Parse optional environment variables
  Utils::parseOptionalEnvVars( );


  return 0;
}



// Initial settings for DHCPDISCOVER.  This packet
// gets reused on the DHCPREQUEST.

uint16_t setupReqPacket( void ) {

  req.operation = 1;
  req.hardwareType = 1;
  req.hardwareAddrLen = 6;
  req.hops = 0;

  // We don't care that we didn't put this in network byte order.
  // 15 bits of randomness is enough.
  req.transactionId = rand( );

  req.seconds = 0;
  req.flags = 0;
  req.clientIpAddr[0] = req.clientIpAddr[1] = req.clientIpAddr[2] = req.clientIpAddr[3] = 0;
  req.clientHdwAddr[0] = MyEthAddr[0];
  req.clientHdwAddr[1] = MyEthAddr[1];
  req.clientHdwAddr[2] = MyEthAddr[2];
  req.clientHdwAddr[3] = MyEthAddr[3];
  req.clientHdwAddr[4] = MyEthAddr[4];
  req.clientHdwAddr[5] = MyEthAddr[5];

  req.optionsCookie[0] =  99;
  req.optionsCookie[1] = 130;
  req.optionsCookie[2] =  83;
  req.optionsCookie[3] =  99;

  uint16_t current = 0;
  req.options[current++] =  53; // DHCP Message type
  req.options[current++] =   1; //      Length
  req.options[current++] =   1; //      DHCP Discover

  req.options[current++] =  55; // Parm list
  req.options[current++] =   3; //      Length
  req.options[current++] =   1; //      Subnet mask
  req.options[current++] =   3; //      Routers
  req.options[current++] =   6; //      Nameserver

  req.options[current++] =  12; // Hostname
  req.options[current++] = strlen( MyHostname ); // Length
  strcpy( (char *)(&req.options[current]), MyHostname );
  current += strlen( MyHostname );

  uint32_t tmp = htonl(RequestedLeaseSecs);
  req.options[current++] =  51; // Requested lease time
  req.options[current++] =   4; //      Length
  req.options[current++] =  ((uint8_t *)&tmp)[0];
  req.options[current++] =  ((uint8_t *)&tmp)[1];
  req.options[current++] =  ((uint8_t *)&tmp)[2];
  req.options[current++] =  ((uint8_t *)&tmp)[3];

  req.options[current++] = 255;

  return (240 + current);
}




// All UDP packets come through here.  We wrapper udpHandler2 with this
// function so that we can guarantee that we will recycle the packet
// no matter how we exit from udpHandler2.

void udpHandler(const unsigned char *packet, const UdpHeader *udp ) {

  Dhcp_t *resp = (Dhcp_t *)(packet);

  // We used to filter and only process packets that were directly sent to us.
  // There must have been a problem because that code was commented out, and
  // we are processing every possible packet.
  udpHandler2( resp );


  // We are done processing this packet.  Remove it from the front of
  // the queue and put it back on the free list.
  Buffer_free( packet );

}



// Remember, we are not getting here unless we get a UDP packet on
// the right port.  But we still have to make sure it is a reply for us.

void udpHandler2( Dhcp_t *resp ) {

  TRACE(( "Dhcp: UDP Handler entry\n" ));

  // Check if this is a reply
  if ( resp->operation != 2 ) {
    TRACE_WARN(( "Dhcp: Incoming UDP packet is not a reply\n" ));
    return;
  }

  // Check transactionId
  if ( resp->transactionId != req.transactionId ) {
    TRACE_WARN(( "Dhcp: Incoming packet transaction ID does not match\n" ));
    return;
  }


  // Check magic cookie in options
  if ( (resp->optionsCookie[0] !=  99) ||
       (resp->optionsCookie[1] != 130) ||
       (resp->optionsCookie[2] !=  83) ||
       (resp->optionsCookie[3] !=  99) )
  {
    TRACE_WARN(( "Dhcp: Reply packet magic cookie is wrong\n" ));
    return;
  }

  if ( resp->options[0] != 53 ) {
    TRACE_WARN(( "Dhcp: first option was not a Dhcp msg type\n" ));
    return;
  }

  uint8_t dhcpMsgType = resp->options[2];

  if ( (dhcpMsgType == 0) || (dhcpMsgType > 8) ) {
    TRACE_WARN(( "Dhcp: Invalid msg type" ));
    return;
  }

  TRACE(( "Dhcp msg type: %s\n", DhcpMsgName[dhcpMsgType] ));

  switch ( dhcpMsgType ) {

    case 2: { // DHCPOFFER
      printf( "Offer received, " );
      DhcpStatusFlag = Dhcp_Offer;
      break;
    }

    case 4: { // DHCPDECLINE
      puts( "Declined" );
      DhcpStatusFlag = Dhcp_Declined;
      return;
    }

    case 5: { // DHCPACK
      puts( "Acknowledged" );
      DhcpStatusFlag = Dhcp_Ack;
      break;
    }

    case 6: { // DHCPNAK
      puts( "Negative - Rejected!" );
      DhcpStatusFlag = Dhcp_Nack;
      return;
    }

    default: {
      // Discover and Request would be illegal here.
      // We don't handle Release or Inform.
      return;
    }

  }


  // Parse the received options.

  uint16_t current = 3;

  uint8_t done = 0;

  while ( !done ) {

    switch( resp->options[current] ) {

      case 51: { // Offered lease time
        uint32_t tmp = *((uint32_t *)&(resp->options[current+2]));
        OfferedLeaseTime = ntohl( tmp );
        current += 6;
        TRACE(( "Dhcp Option:  51 Lease time: %lu seconds\n", OfferedLeaseTime ));
        break;
      }

      case 54: { // Server identity
        ServerIdentity[0] = resp->options[current+2];
        ServerIdentity[1] = resp->options[current+3];
        ServerIdentity[2] = resp->options[current+4];
        ServerIdentity[3] = resp->options[current+5];
        current += 6;
        TRACE(( "Dhcp Option:  54 Server: %d.%d.%d.%d\n",
          ServerIdentity[0], ServerIdentity[1],
          ServerIdentity[2], ServerIdentity[3] ));
        break;
      }

      case 58: { // Renewal Time
        uint32_t tmp = *((uint32_t *)&(resp->options[current+2]));
        tmp = ntohl( tmp );
        current += 6;
        TRACE(( "Dhcp Option:  58 Lease renewal time: %lu seconds\n", tmp ));
        break;
      }

      case 59: { // Rebinding Time
        uint32_t tmp = *((uint32_t *)&(resp->options[current+2]));
        tmp = ntohl( tmp );
        current += 6;
        TRACE(( "Dhcp Option:  59 Lease rebinding time: %lu seconds\n", tmp ));
        break;
      }

      case 0: { // Pad
        current++;
        TRACE(( "Dhcp Option:   0 Pad\n" ));
        break;
      }

      case 1: { // Subnet mask
        SubnetMask[0] = resp->options[current+2];
        SubnetMask[1] = resp->options[current+3];
        SubnetMask[2] = resp->options[current+4];
        SubnetMask[3] = resp->options[current+5];
        current += 6;
        TRACE(( "Dhcp Option:   1 Subnet mask: %d.%d.%d.%d\n",
          SubnetMask[0], SubnetMask[1],
          SubnetMask[2], SubnetMask[3] ));
        break;
      }

      case 3: { // Routers (multiple possible, take the first)
        GatewayAddr[0] = resp->options[current+2];
        GatewayAddr[1] = resp->options[current+3];
        GatewayAddr[2] = resp->options[current+4];
        GatewayAddr[3] = resp->options[current+5];
        current += 2 + resp->options[current+1];
        TRACE(( "Dhcp Option:   3 Router: %d.%d.%d.%d\n",
          GatewayAddr[0], GatewayAddr[1],
          GatewayAddr[2], GatewayAddr[3] ));
        break;
      }

      case 6: { // DNS (multiple possible, take the first)
        NameServer[0] = resp->options[current+2];
        NameServer[1] = resp->options[current+3];
        NameServer[2] = resp->options[current+4];
        NameServer[3] = resp->options[current+5];
        current += 2 + resp->options[current+1];
        TRACE(( "Dhcp Option:   6 Nameserver: %d.%d.%d.%d\n",
          NameServer[0], NameServer[1],
          NameServer[2], NameServer[3] ));
        break;
      }

      case 12: { // Host name, but not necessarily a DNS host name
        int l = resp->options[current+1];
        strncpy( Hostname_assigned, (const char *)&(resp->options[current+2]), 40 );
        if ( l > 39 ) { l = 39; }
        Hostname_assigned[l] = 0;
        current += 2 + resp->options[current+1];
        TRACE(( "Dhcp Option:  12 Host Name (assigned): %s\n", Hostname_assigned ));
        break;
      }

      case 15: { // DNS Domain - info only.
        int l = resp->options[current+1];
        strncpy( Domain_assigned, (const char *)&(resp->options[current+2]), 40 );
        if ( l > 39 ) { l = 39; }
        Domain_assigned[l] = 0;
        current += 2 + resp->options[current+1];
        TRACE(( "Dhcp Option:  15 Domain Name (assigned): %s\n", Domain_assigned ));
        break;
      }

      case 255: { // End of options
        done = 1;
        TRACE(( "Dhcp Option: 255 End of options\n" ));
        break;
      }


      default: {
        TRACE(( "Dhcp Option: %3d Length: %d\n", resp->options[current], resp->options[current+1] ));
        current += 2 + resp->options[current+1];
      }


    } // end switch options


  } // end while options


  // We also need this, but it is from the message body and not the
  // DHCP options.
  NewIpAddr[0] = resp->yourIpAddr[0];
  NewIpAddr[1] = resp->yourIpAddr[1];
  NewIpAddr[2] = resp->yourIpAddr[2];
  NewIpAddr[3] = resp->yourIpAddr[3];


  if ( dhcpMsgType == 2 ) { // DHCP Offer message

    // If we got this far send a DHCPREQUEST back
    sendDhcpRequestMsg( );

  } else {

    // DHCP ACK message
    Ip::setMyIpAddr( NewIpAddr );
    Ip::setMyNetmask( SubnetMask );
    Ip::copy( Gateway, GatewayAddr );

  }


}

void sendDhcpRequestMsg( void ) {

  // We are reusing the request packet.  All of the original header fields
  // have already been set so we don't need to set them again.  Only set
  // the options.

  uint16_t current = 0;

  req.options[current++] =  53; // DHCP Message type
  req.options[current++] =   1; //      Length
  req.options[current++] =   3; //      DHCP Request

  req.options[current++] =  50; // Requested IP Addr
  req.options[current++] =   4; //      Length
  req.options[current++] =  NewIpAddr[0];
  req.options[current++] =  NewIpAddr[1];
  req.options[current++] =  NewIpAddr[2];
  req.options[current++] =  NewIpAddr[3];

  req.options[current++] =  54; // Server identifier
  req.options[current++] =   4; //      Length
  req.options[current++] =  ServerIdentity[0];
  req.options[current++] =  ServerIdentity[1];
  req.options[current++] =  ServerIdentity[2];
  req.options[current++] =  ServerIdentity[3];

  req.options[current++] =  12; // Hostname
  req.options[current++] = strlen( MyHostname ); // Length
  strcpy( (char *)(&req.options[current]), MyHostname );
  current += strlen( MyHostname );

  uint32_t tmp = htonl(RequestedLeaseSecs);
  req.options[current++] =  51; // Requested lease time
  req.options[current++] =   4; //      Length
  req.options[current++] =  ((uint8_t *)&tmp)[0];
  req.options[current++] =  ((uint8_t *)&tmp)[1];
  req.options[current++] =  ((uint8_t *)&tmp)[2];
  req.options[current++] =  ((uint8_t *)&tmp)[3];

  req.options[current++] = 255; // End of Options


  // 240 is the size of the request, not counting the UdpPacket_t header.
  // It's everything including the options cookie.
  uint16_t reqLen = 240 + current;

  // This can't fail because we are broadcasting it.
  Udp::sendUdp( IpBroadcastNonRoutable, DHCP_REPLY_PORT, DHCP_REQUEST_PORT, reqLen, (uint8_t *)&req, 1 );
}





void usage( FILE *stream ) {

  fprintf( stream,
    "\n"
    "Dhcp [options]\n\n"
    "Options:\n"
    "  -help\n"
    "  -retries <n>   Retry n times before giving up\n"
    "  -timeout <n>   Set timeout for each attempt to n seconds\n"
    "  -packetstats   Show packet statistics at the end\n\n"
  );
  exit( 1 );
}


void parseArgs( int argc, char *argv[] ) {

  uint8_t i=1;
  for ( ; i<argc; i++ ) {

    if ( stricmp( argv[i], "-retries" ) == 0 ) {
      i++;
      if ( i == argc ) {
        fprintf( stderr, "Need to provide a number with the -retries option\n" );
        usage( stderr );
      }
      Retries = atoi( argv[i] );
      if ( Retries == 0 ) {
        fprintf( stderr, "Bad number of retries specified\n" );
        usage( stderr );
      }
    }
    else if ( stricmp( argv[i], "-timeout" ) == 0 ) {
      i++;
      if ( i == argc ) {
        fprintf( stderr, "Need to provide a number of seconds with the -timeout option\n" );
        usage( stderr );
      }
      Timeout = atoi( argv[i] );
      if ( Timeout < 5 || Timeout > 120 ) {
        fprintf( stderr, "Bad timeout value specified - must be between 5 and 120\n" );
        usage( stderr );
      }
      TimeoutMs = Timeout * 1000ul;
    }
    else if ( stricmp( argv[i], "-help" ) == 0 ) {
      printf( "Options and usage ...\n" );
      usage( stdout );
    }
    else {
      fprintf( stderr, "Unknown option: %s\n", argv[i] );
      usage( stderr );
    }

  }

}
