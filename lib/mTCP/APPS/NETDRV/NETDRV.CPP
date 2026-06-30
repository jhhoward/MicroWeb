/*

   mTCP NetDrive (NETDRV.CPP)
   Copyright (C) 2023-2025 Michael B. Brutman (mbbrutman@gmail.com)
   mTCP web page: http://www.brutman.com/mTCP/mTCP.html


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


   Description: Command line utility for interacting with the NetDrive server.

*/


#include <dos.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "types.h"

#include "trace.h"
#include "timer.h"
#include "packet.h"
#include "utils.h"
#include "Arp.h"
#include "Ip.h"
#include "Dns.h"


#include "REQHDR.H"
#include "SHARED.H"



#define SERVER_NAME_MAXLEN           (80)
#define DRIVE_NAME_MAXLEN            (80)
#define OS_STRING_MAXLEN             (40)
#define MAX_PAYLOAD_LEN            (1024)
#define TAG_MAXLEN                   (65)      // Includes room for the nul char at the end.







#define SERVER_PROTOCOL_VERSION (2)

// Command line parameters
char     TargetServerAddrName[SERVER_NAME_MAXLEN];   // Target server name
IpAddr_t TargetServerAddr;                           // Target server Ip address
uint16_t TargetServerPort = 0;                       // Target server port
char     TargetDriveName[DRIVE_NAME_MAXLEN];         // Drive to connect to, includes NUL at end

// If you are doing goto_cp and a Tag is specified, then TagNum is 0.
// If TagNum is non-zero then use that.
char     Tag[TAG_MAXLEN];
int      TagNum = 0;
bool     TagDelMarker = false;

uint8_t  TargetDriveLetter = 0;                      // A=1, B=2, C=3, ... (0 is invalid)
bool     ReadOnly = false;

uint16_t TimeoutTicks = 2*18;
uint16_t TimeoutRetries = 3;

// 0 = not set, 1 = connect, 2 = disconnect, 3 = status, 4 = mark cp, 5 = goto cp, 6 = list cp
int Operation = 0;

// Local UDP port we will use for talking to the server.  Selected at random.
uint16_t OurPort = 0;


uint8_t SecondaryPDMsg = 0;


// Memory model notes
//
// If you use a memory model with far pointers your life is great, as you can
// use the mTCP library routines to copy Ethernet and IP addresses.  But if you
// use a small memory model (16 bit pointers) the compiler gets upset because
// all of the mTCP library routines will be using small pointers and you'll have
// some far pointers to work with.
//
// Same thing with the standard library routines like strcpy; they don't handle
// mixed pointer types in the same call.
//
// We want to use small memory models to cut down on the executable size, so define
// some helper macros to copy things one byte at a time.  It's not worth optimizing
// these.
//
// Use these because the standard mTCP ones won't work with mixed pointer types.
//
#define copyEth( target, source ) { for (int i=0; i<6; i++) { target[i] = source[i]; } }
#define copyIp( target, source ) { for (int i=0; i<4; i++) { target[i] = source[i]; } }
#define copyBytes( target, source, len ) { for (int i=0; i<len; i++) { target[i] = source[i]; } }

// Always leaves room for the terminating NULL.
#define copyStr( target, source, max ) { \
  int _i=0;                               \
  while (source[_i]) {                    \
    if ( _i == ((max)-1) ) break;         \
    target[_i] = source[_i];               \
    _i++;                                 \
  }                                      \
  target[_i]=0;                           \
}



// IOCTL pragmas
//
// Usage:
//
//  int rc = ioctl_read( netHandle, sizeof(net_data), &net_data );
//  int rc = ioctl_write( netHandle, sizeof(Net_data_t), &net_data );
//
// Returns 0 if an error or the number of bytes read if successful.
//
// Memory model note: The IOCTL call wants the buffer to be in ds:dx.  We're
// using the small memory model so the compiler doesn't recognize ds.  Help
// it out by setting up ds to be the same as the stack segment.

#if defined(__SMALL__) || defined(__MEDIUM__)

extern uint16_t ioctl_read( uint8_t handle, uint16_t len, void far * buf);
#pragma aux ioctl_read = \
  "mov ax, ss"           \
  "mov ds, ax"           \
  "mov ax, 4404h"        \
  "int 21h"              \
  "jnc ioctl_read1"      \
  "mov ax, 0"            \
  "ioctl_read1:"         \
  parm [bl] [cx] [dx]    \
  modify [ax]            \
  value  [ax];

extern uint16_t ioctl_write( uint8_t handle, uint16_t len, void far * buf );
#pragma aux ioctl_write = \
  "mov ax, ss"           \
  "mov ds, ax"           \
  "mov ax, 4405h"        \
  "int 21h"              \
  "jnc ioctl_write1"     \
  "mov ax, 0"            \
  "ioctl_write1:"        \
  parm [bl] [cx] [dx] \
  modify [ax]            \
  value  [ax];

#else

extern uint16_t ioctl_read( uint8_t handle, uint16_t len, void far * buf);
#pragma aux ioctl_read = \
  "mov ax, 4404h"        \
  "int 21h"              \
  "jnc ioctl_read1"      \
  "mov ax, 0"            \
  "ioctl_read1:"         \
  parm [bl] [cx] [ds dx] \
  modify [ax]            \
  value  [ax];

extern uint16_t ioctl_write( uint8_t handle, uint16_t len, void far * buf );
#pragma aux ioctl_write = \
  "mov ax, 4405h"        \
  "int 21h"              \
  "jnc ioctl_write1"     \
  "mov ax, 0"            \
  "ioctl_write1:"        \
  parm [bl] [cx] [ds dx] \
  modify [ax]            \
  value  [ax];

#endif



Net_data_t far *net_data;
Unit_t far *curr_unit;



// Command packet structure
//
// This is the command header between us and the server.  A data payload is
// optional and will appear after this structure on the wire.

#pragma enum minimum ;
typedef enum {
  Op_Connect    = 1,
  Op_Disconnect = 2,
  Op_Read       = 3,
  Op_Write      = 4,
  Op_Write_V    = 5,
  Op_Mark_Chkp  = 6,
  Op_Goto_Chkp  = 7,
  Op_List_Chkp  = 8
} Operation_t;

typedef struct {
  uint16_t    Version;
  uint16_t    Session;
  uint16_t    Sequence;
  Operation_t Operation;
  uint8_t     Result;
  uint32_t    StartSec;
  uint16_t    SecCount;
} Command_Packet_t;


// Outgoing Command packet structure.  Includes space for the Ethernet,
// IP and UDP headers so that we don't have to let the UDP library malloc
// them and do a memcpy.

typedef struct {
  UdpPacket_t      udpHdr;     // Space for Ethernet, IP and UDP headers.
  Command_Packet_t cmdPkt;     // Command structure
  uint8_t          payload[256];
} Outgoing_Command_Packet_t;


typedef struct {
  uint8_t          macAddr[6];
  uint16_t         clientType;
  uint16_t         blockSize;
  char             osString[OS_STRING_MAXLEN];       // Note, this includes a terminating NUL char
  char             remoteImage[DRIVE_NAME_MAXLEN];   // And this does too ... because why not.
} ConnectPayload_t;

// Global variables: One incoming and one outgoing Command packet.
Outgoing_Command_Packet_t OutgoingCmdPkt;
Command_Packet_t ReceivedCmdPkt;

// The biggest incoming packet for this program should be an error
// message, which will never be this large.
uint8_t  ReceivedCmdPkt_OptPayload[MAX_PAYLOAD_LEN+1];
uint16_t ReceivedCmdPkt_OptPayloadLen;

bool RespReceived;


// Randomly generated sequence number used when we first establish
// a connection.  Subsequent commands should use increasing sequence
// numbers so that we can detect old duplicate UDP packets.
uint16_t Sequence = 0;

uint16_t Session = 0;


#pragma pack (1)

typedef struct {
  uint32_t filesize;
  uint16_t flags;           // Right now 0x00 for read-write, 0x01 for read only.
  uint8_t  jump[3];         // Boot sector start.
  uint8_t  oemName[8];      // OEM name.
  union {
    uint8_t  bpbBytes[25];  // 25 bytes of the BPB that we care about.
    BPB_t    bpb;
  } bpb_u;
  uint8_t  reserved[28];    // Only want the first 64 bytes of the boot sector.
  uint8_t  mediaDesc;       // From the first byte of the FAT.
} Connect_Resp_t;



// BPB structures for DOS 1.x 160 and 320K diskettes.

static const BPB_t DOS_1_160KB = {512, 1, 1, 2,  64, 320, 0xfe, 1, 8, 1, 0, 0};
static const BPB_t DOS_1_320KB = {512, 2, 1, 2, 112, 640, 0xff, 1, 8, 2, 0, 0};


// Signature to verify we have found the NetData structure in memory.
// The last two bytes are the major and minor revision numbers.
static const char Signature[] = {'N', 'E', 'T', 'D', 'R', 'I', 'V', 'E', 0x00, 0x03};


// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}



int  doConnect( void );
int  doConnect_internal( void );
void doDisconnect( void );
void doStatus( void );
void doMarkChkp( void );
void doGotoChkp( void );
void doListChkp( void );
void parseArgs( int argc, char *argv[] );




static char *CopyrightMsg = "mTCP NetDrive by M Brutman (mbbrutman@gmail.com) (C)opyright 2008-2025\nVersion: " __DATE__ "\n\n";
static char *NoConnectedDrives = "No connected drives";

int main( int argc, char *argv[] ) {

  printf( CopyrightMsg );

  parseArgs( argc, argv );


  // We use IOCTL to get a pointer to the net_data structure and the unit the
  // user wants to manipulate.

  typedef struct {
    Net_data_t far *net_data;
    Unit_t     far *unit;
  } ioctl_data_t;

  ioctl_data_t ioctl_data;

  // We need a far pointer to a far pointer that points at a Net_data_t structure.

  ioctl_data_t far **p = (ioctl_data_t far **)MK_FP(FP_SEG(&ioctl_data), FP_OFF(&ioctl_data));

  int rc = ioctl_read( TargetDriveLetter, sizeof(ioctl_data), p );
  if ( rc != sizeof(ioctl_data) ) {
    puts( "Error: Not a NetDrive device or wrong version." );
    exit(1);
  }

  // Copy the two pointers to globals for convenience.
  net_data = ioctl_data.net_data;
  curr_unit = ioctl_data.unit;


  // Look for our eye catcher before the NetData structure to verify
  // that we have found the driver and NetData structure.  This is more
  // than just an eye catcher; the major and minor version number are embedded
  // in it too.  At some point we'll need another error message for a version
  // mismatch.

  char far *v = ((char far *)net_data) - sizeof(Signature);
  bool match = true;
  for (int i=0; i<sizeof(Signature); i++) {
    if (v[i] != Signature[i]) match = false;
  }

  if (match == false) {
    puts( "Error: Version mismatch between this program and the NetDrive device driver." );
    exit(1);
  }

  printf( "NetDrive device opened, IOCTL_read return code: %x %04X:%04X %04X:%04X\n\n",
      rc, FP_SEG(net_data), FP_OFF(net_data), FP_SEG(curr_unit), FP_OFF(curr_unit) );


  // Sleazy - see if we need to set the timeout or retry values.

  char *tmpValStr = getenv( "NETDRIVE_TIMEOUT" );
  if ( tmpValStr != NULL ) {
    uint16_t tmpVal = atoi(tmpValStr);
    if ( (tmpVal > 0) && (tmpVal < 21) ) {
      TimeoutTicks = tmpVal * 18;
      net_data->timeoutThreshold = TimeoutTicks;
    }
  }

  tmpValStr = getenv( "NETDRIVE_RETRIES" );
  if ( tmpValStr != NULL ) {
    uint16_t tmpVal = atoi(tmpValStr);
    if ( (tmpVal > 0) && (tmpVal < 11) ) {
      TimeoutRetries = tmpVal;
      net_data->retries = TimeoutRetries;
    }
  }


  if ( Operation == 3 ) {
    doStatus( );
  } else {

    if ( Utils::parseEnv( ) != 0 ) { exit(1); }

    /*
    Utils::openCfgFile( );
    char packetIntParm[10];
    if ( Utils::getAppValue("ND_PACKETINT", packetIntParm, 9) != 0 ) {
      puts("ND_PACKETINT not specified or a bad format.");
      exit(1);
    }
    uint8_t nd_packetInt;
    int rc = sscanf( packetIntParm, "%x", &nd_packetInt );
    if ( rc != 1 ) {
      puts("ND_PACKETINT has a bad format.");
      exit(1);
    }

    Utils::setPacketInt( nd_packetInt );
    */


    if (MyMTU < 1200) {
      printf("Your MTU setting of %d is below the minimum requirement of 1200.\n"
             "Please update your MTU setting in the mTCP config file.\n", MyMTU);
      exit(1);
    }

    if ( Operation == 1 ) {
      doConnect( );
    } else {
    
      if ( curr_unit->session == 0 ) {
        puts( NoConnectedDrives );
      } else {

        switch (Operation) {
          case 2: { doDisconnect( ); break; }
          case 4: { doMarkChkp( ); break; }
          case 5: { doGotoChkp( ); break; }
          case 6: { doListChkp( ); break; }
        }

      }

    }

  }

  return 0;
}



static const char ExitMsg[] = "User exit detected: ending\n";

bool checkUserExit( void ) {

  if ( CtrlBreakDetected ) {
    puts( ExitMsg );
    return true;
  }

  if ( biosIsKeyReady( ) ) {
    char c = biosKeyRead( );
    if ( (c == 27) || (c == 3) ) {
      puts( ExitMsg );
      return true;
    }
  }

  return false;
}



// resolveServer
//
// Call after you have initialized mTCP so that we can do ARP and DNS
// resolution.  Returns 0 if good, -1 if not.

int resolveServer( void ) {

  printf( "Resolving %s, press [ESC] to abort.\n", TargetServerAddrName );

  // Resolve the name and definitely send the request
  int8_t rc = Dns::resolve( TargetServerAddrName, TargetServerAddr, 1 );

  if ( rc >= 0 ) {
    while ( 1 ) {
      if ( checkUserExit( ) ) { return -1; }
      if ( !Dns::isQueryPending( ) ) break;

      PACKET_PROCESS_SINGLE;
      Arp::driveArp( );
      Dns::drivePendingQuery( );
    }
  }

  // Query is no longer pending.  Did we get an answer?
  if ( Dns::resolve( TargetServerAddrName, TargetServerAddr, 0 ) != 0 ) {
    printf( "Error: Could not resolve %s to an IP address.\n", TargetServerAddrName );
    return -1;
  }

  printf( "Server ip address is: %u.%u.%u.%u\n",
          TargetServerAddr[0], TargetServerAddr[1], TargetServerAddr[2], TargetServerAddr[3] );

  return 0;
}


// Incoming UDP packet handler that is used for receiving responses from the server.
//
// Checks the incoming packet to see if the Session, Sequence and Operation matches
// the last outgoing packet.  If it does then the packet and optional payload are
// copied to global variables where they can be inspected later.
//
// If we sent the server a bad sequence number or we receive a bad sequence number
// then it will look like the request we sent never got a response because we only
// set RespReceived to true when the Sequence, Session and Operation match.

void udpHandler( const uint8_t *packet, const UdpHeader *udp ) {

  // Is this from the correct IP address and port?
  //
  // Remember that the IP header is always after the Ethernet header,
  // but the UDP header depends on if IP options were sent.

  IpHeader *ip = (IpHeader *)(packet + sizeof(EthHeader));

  if ( Ip::isSame( ip->ip_src, TargetServerAddr ) && (ntohs(udp->src) == TargetServerPort) ) {

    // Command starts right after the UDP header.
    Command_Packet_t *cmdResp = (Command_Packet_t *)(udp+1);


    if ( cmdResp->Version != SERVER_PROTOCOL_VERSION ) {
      printf( "Error: Wrong server protocol version.  Expected %d, received %d\n",
          SERVER_PROTOCOL_VERSION, cmdResp->Version);
    } else {

      // Validate the incoming operation and sequence number.

      if ( (cmdResp->Operation == OutgoingCmdPkt.cmdPkt.Operation) && (cmdResp->Sequence == Sequence) ) {

        // Now check the session number too.  (The Connect command is a special
        // case as there is no Session number to compare against.)
        if ( (cmdResp->Operation == Op_Connect) ||
             ((cmdResp->Operation > Op_Connect) && (cmdResp->Session == Session)) ) {

          memcpy( &ReceivedCmdPkt, cmdResp, sizeof(Command_Packet_t) );
          RespReceived = true;

          ReceivedCmdPkt_OptPayloadLen = ntohs(udp->len) - sizeof(Command_Packet_t) - sizeof(UdpHeader);

          if (ReceivedCmdPkt_OptPayloadLen > MAX_PAYLOAD_LEN) {
            ReceivedCmdPkt_OptPayloadLen = MAX_PAYLOAD_LEN;
          }
          memcpy( ReceivedCmdPkt_OptPayload, (uint8_t *)(cmdResp+1), ReceivedCmdPkt_OptPayloadLen );

          // Set the last byte after the optional payload to a nul char.  This
          // covers the case where the server sends us an information message
          // in the payload.  (We left space for this nul in the payload.)
          //
          // Normally we just get one reply back from the server when this program
          // is run so we don't have to do this because the buffer is initialized
          // to zeros.  But if we ever sent more than one message we
          // can't count on that.
          ReceivedCmdPkt_OptPayload[ReceivedCmdPkt_OptPayloadLen] = 0;
        }
        else {
          printf( "Error: Bad session.  Expected %d, received %d\n", Session, cmdResp->Session );
        }
      }
      else {
        printf( "Error: Unexpected response: Session: %u, Seq: %u, Op: %u\n",
                cmdResp->Session, cmdResp->Sequence, cmdResp->Operation );
      }

    }

  }

  Buffer_free( packet );

}


// sendMsgAndWait
//
// Return codes:
//
// 0 - Good, message sent and a response received. (It might be a bad
//     return code, but you got a response.
// 1 - Error trying to send.
// 2 - Timeout waiting for a response.

// Fixme: Add return codes for good, hard error and timeout
//
// For now, 0 is good and anything else is bad.

int sendMsgAndWait( Operation_t op, int16_t payloadLen, char *msgName, int opTimeout ) {

  // Setup the easy parts of the packet header.

  // Zero out the UDP header and the fixed portion of the packet.
  // If there is a payload then it was already setup before calling here
  // so don't touch it.
  memset( &OutgoingCmdPkt, 0, sizeof(UdpPacket_t) + sizeof(Command_Packet_t) );

  OutgoingCmdPkt.cmdPkt.Operation = op;
  OutgoingCmdPkt.cmdPkt.Version = SERVER_PROTOCOL_VERSION;
  OutgoingCmdPkt.cmdPkt.Session = Session;
  OutgoingCmdPkt.cmdPkt.Sequence = Sequence;

  int outgoingLen = sizeof(Command_Packet_t) + payloadLen;

  RespReceived = false;
  ReceivedCmdPkt_OptPayloadLen = 0;


  // Send the UDP packet.  This should always work, as we have already
  // gone through ARP resolution and we are not using features that would
  // cause a hard failure from sendUdp.

  int rc = Udp::sendUdp( TargetServerAddr, OurPort, TargetServerPort, outgoingLen, (uint8_t *)&OutgoingCmdPkt, 1 );

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( rc != 0 ) {

    if ( rc == -1 ) {
      printf( "Error: Failed to send %s message.\n", msgName );
      return 1;
    }

    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 2000 ) ) {
      printf( "Error: ARP timeout while sending %s message.\n", msgName );
      return 1;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );

    rc = Udp::sendUdp( TargetServerAddr, OurPort, TargetServerPort, outgoingLen, (uint8_t *)&OutgoingCmdPkt, 1 );

  }


  // Now wait for the UDP response

  while ( RespReceived == false ) {
    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_SECS_TO_TICKS( opTimeout) ) {
      printf( "Error: Timeout waiting for %s message response.\n", msgName );
      return 2;
    }
    PACKET_PROCESS_SINGLE;
  }

  return 0;
}




void doStatus( void ) {

  uint8_t far *units_p = (uint8_t far *)MK_FP(FP_SEG(net_data), 0x0A);

  Unit_t far *unitStart = (Unit_t far *)(MK_FP(FP_SEG(net_data), net_data->unit_start_offset));


  // If we know the first drive letter use it.  Otherwise for DOS 2.x
  // we can derive it based on the drive letter the user passed in and
  // the pointer to the per-unit data obtained from the IOCTL call.
  // Clever, eh?

  char firstDrive[] = "A:";
  firstDrive[0] = net_data->firstDriveLetter;
  if ( net_data->firstDriveLetter == 0 ) {
    // Figure out the starting drive letter
    int precedingUnits = curr_unit - unitStart;
    firstDrive[0] = (TargetDriveLetter - precedingUnits) - 1 + 'A';
  }

  printf( "NetDrive units: %u, First drive letter: %s\n\n", *units_p, firstDrive );

  if (net_data->pkt_drv_int == 0) {
    puts( NoConnectedDrives );
    return;
  }

  printf( "Our IP addr: %u.%u.%u.%u, UDP Port:%u, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
          net_data->my_ip[0], net_data->my_ip[1], net_data->my_ip[2], net_data->my_ip[3], net_data->my_port,
          net_data->my_mac[0], net_data->my_mac[1], net_data->my_mac[2],
          net_data->my_mac[3], net_data->my_mac[4], net_data->my_mac[5] );

  // printf( "Packet Int: 0x%02X, Sent Arp Replies: %lu\n", net_data->pkt_drv_int, net_data->count_arp_resp );

  printf( "Packet driver int: 0x%X, Netdrive shim int: ", net_data->pkt_drv_int );

  if ( net_data->pkt_drv_int2 ) {
    printf("0x%X\n", net_data->pkt_drv_int2 );
  } else {
    puts( "NA" );
  }

  printf( "Received packets: All: %lu, To our MAC addr: %lu, To our IP/UDP: %lu\n",
          net_data->count_r_total, net_data->count_r_our_mac, net_data->count_r_our_udp );
  printf( "Arp replies sent: %lu, Packet send errors: %lu\n\n",
          net_data->count_arp_resp, net_data->count_pkt_send_errors );

  puts( "#: Server address, Session number, Remote Image, Blks In, Blks Out, Retries\n" );
  for (int i=0; i < *units_p; i++ ) {

    // The driver only holds 40 chars, so that's all we're going to get here
    // even though we might have sent up to 80 chars to the server.
    char tmpDriveName[DRIVE_NAME_MAXLEN];
    copyStr( tmpDriveName, unitStart[i].remoteName, DRIVE_NAME_MAXLEN);

    char letter = firstDrive[0] + i;

    if (unitStart[i].session != 0) {
      printf( "%c: %u.%u.%u.%u:%u, %5u, %s %s, %lu, %lu, %lu\n", letter,
            unitStart[i].server_ip[0], unitStart[i].server_ip[1],
            unitStart[i].server_ip[2], unitStart[i].server_ip[3],
            unitStart[i].server_port, unitStart[i].session, tmpDriveName,
            unitStart[i].readonly?"(RO)":"(RW)",
            unitStart[i].blocks_read, unitStart[i].blocks_written, unitStart[i].retries);
    } else {
      printf( "%c: (Not connected)\n", letter);
    }

  }

}



// Just a wrapper around the mTCP library initStack call that spits out an
// error message for us.

int localInitStack( void ) {
  // No sockets, no buffers TCP buffers
  int rc = Utils::initStack( 0, 0, ctrlBreakHandler, ctrlBreakHandler );
  if ( rc ) {
    puts( "Failed to initialize the network." );
  }
  return rc;
}


// connnectNetDrive and disconnectNetDrive
//
// These two functions connect and disconnect NetDrive to the packet driver.
// The key part is managing the handle and receiver functions.
//
// In the simple case there is just one software interrupt for a packet
// driver specified in the config file.  If there is a second then we
// actively manage it, installing and removing our shim packet driver
// when we connect or disconnect.
//
// At the end of execution, if there are any connected netdrives we
// need to ensure we are connected again.  Likewise, if there are no
// connected hard drives then basically "unload" our shim packet driver
// so everything operates normally against the real packet driver.


// disconnectNetDriveFromPD
//
// Disconnect disables the secondary software interrupt, so any messages sent
// from this program will use the primary software interrupt (the real
// packet driver).

void disconnectNetDriveFromPD( void ) {

  // First tell the driver to stop processing incoming packets.
  net_data->pkt_recv_on = 0;

  // Disconnect from the packet driver using release_type.
  Packet_setHandle( net_data->pkt_drv_handle );
  Packet_setInt( net_data->pkt_drv_int );
  Packet_release_type( );

  // Wipe out pkt_drv_handle and pkt_drv_int to make it clear we are not connected.
  net_data->pkt_drv_handle = 0;
  net_data->pkt_drv_int = 0;

  // Unhook the secondary packet interrupt if it was specified.  We're just
  // going to use zeros, which works even on a PCjr.  Only unhook it if we
  // hooked it ourselves so we don't clobber something else.
  if ( Utils::getPacketInt2() ) {
    uint16_t far *intVector = (uint16_t far *)MK_FP( 0x0, Utils::getPacketInt2() * 4 );
    if ( (*intVector == net_data->shimOffset) && (*(intVector+1) == net_data->segment) ) {
      *intVector = 0;
      *(intVector+1) = 0;
      net_data->pkt_drv_int2 = 0;
    }
  }
}


void connectNetDriveToPD( void ) {

  int rc = Packet_init2( Utils::getPacketInt( ), net_data->segment, net_data->receiverFuncOffset );

  // This should never happen.
  if ( rc == -1 ) {
    puts( "Error: Failed to connect to the packet driver." );
    exit(1);
  }

  net_data->pkt_drv_handle = Packet_getHandle( );
  net_data->pkt_drv_int = Utils::getPacketInt( );

  // Forgive me ... yes, we are modifying the actual code to put
  // the interrupt number in it.
  uint8_t far * intNumber = (uint8_t far *)MK_FP(net_data->segment, net_data->intInstOffset);
  *(intNumber+1) = Utils::getPacketInt( );


  // Hook the secondary packet interrupt if it was specified.

  SecondaryPDMsg = 0;

  uint8_t secondaryInt = Utils::getPacketInt2();

  if ( secondaryInt ) {

    uint16_t far * intCode = (uint16_t far *)MK_FP( 0x0, secondaryInt * 4 );

    bool vacant = ((*intCode == 0x0000) && (*(intCode+1) == 0x0000)) ||
                  ((*intCode == 0xF815) && (*(intCode+1) == 0xF000));

    if ( vacant ) {
      uint16_t far *intVector = (uint16_t far *)MK_FP( 0x0, secondaryInt * 4 );
      *intVector = net_data->shimOffset;
      *(intVector+1) = net_data->segment;
      net_data->pkt_drv_int2 = secondaryInt;
      SecondaryPDMsg = 1;
    }

    // If it's not vacant we would not have gotten here ..  Utils::initStack
    // would have tripped on it first.

  }

}



void fakeStat( void ) {

  // Set Media Check so that it reports the media changed to start.
  curr_unit->media_check_value = 0xff;

  // Stat the README.TXT file to force the BPB change.  This doesn't even
  // have to work, it just has to force DOS to look at the FAT and see
  // that the media changed.

  struct stat statbuf;
  char fn[] = "A:\\README.TXT";
  fn[0] = TargetDriveLetter+64;
  stat(fn, &statbuf);
}



// doConnect
//
// Send a connect message to the server to see if it has the virtual
// hard drive that we want.  If it does, setup the device driver to
// take over packet handling.
//
// doConnect is actually a wrapper for doConnect_internal, which does the
// hard work.  There are a lot of ways for doConnect_internal to fail so
// it makes error handling easier to use a wrapper.

int doConnect( void ) {

  if ( curr_unit->session != 0 ) {
    puts( "Error: A remote disk image is already connected." );
    return 1;
  }

  int rc = doConnect_internal( );

  if ( rc != 0 ) {

    // Something went wrong.  If there are other connected drive letters then we need
    // to reconnect the packet driver or they are dead.
    //
    // If you get here and other drive letters are connected, then we disconnected.
    // Restore the connection to the device driver for those other drives.
    if ( net_data->connectedCount ) {
      connectNetDriveToPD( );
      net_data->pkt_recv_on = 1;
    }
  }

  return rc;

}


int doConnect_internal( void ) {

  // If no drives are connected yet then we are going to fully initialize the
  // net_data structure, except for the first eight bytes that are read-only for us.
  //
  // The driver starts with all of these as clean, so this only has an impact after
  // we start, connect, and then disconnect.  (While drives are connected we need
  // to preserve everything.)

  if ( net_data->connectedCount == 0 ) {
    for (int i=12; i < sizeof(Net_data_t); i++) { ((uint8_t far *)net_data)[i] = 0; }
  }

  // Always clear out the unit specific data that we are going to touch.
  for (int i=0; i < sizeof(Unit_t); i++) { ((uint8_t far *)curr_unit)[i] = 0; }


  // If there are drives already connected then the device driver owns the packet
  // driver.  We have to temporarily steal it so we can use it.

  if ( net_data->connectedCount > 0 ) {
    disconnectNetDriveFromPD( );
  }

  // Setup the normal mTCP stack so we can send and receive UDP messages.
  if ( localInitStack( ) ) {
    return 1;
  }

  // From this point forward you have to call Utils::endStack( ) routine to
  // exit because we have the timer interrupt and Ctrl-Break interrupt hooked.

  EthAddr_t local_my_mac;
  Packet_get_addr( local_my_mac );

  if ( resolveServer( ) != 0 ) {
    Utils::endStack( );
    return 1;
  }


  // Figure out the next hop address for the specified server.
  //
  // We cheat by determining the next hop address (going to the server
  // directly on the same network or going through the gateway) to
  // keep the device driver simple.
  //
  // This works by asking the IP code to fill in the destination Ethernet
  // address (the next hop) for a fake IP packet.  Only the destination field
  // needs to be set.  ARP resolution will happen and the next hop Ethernet
  // address will be given to us.

  IpHeader i;
  Ip::copy( i.ip_dest, TargetServerAddr );

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  bool nextHopIsSet = false;
  while ( !nextHopIsSet ) {

    if ( checkUserExit( ) ) {
      Utils::endStack( );
      return 1;
    }

    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_SECS_TO_TICKS( 2 ) ) break;

    EthAddr_t local_next_hop_mac;
    int rc = i.setDestEth( local_next_hop_mac );
    if ( rc == 0 ) {
      copyEth(curr_unit->next_hop_mac, local_next_hop_mac);
      nextHopIsSet = true;
      break;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
  }

  if ( !nextHopIsSet ) {
    printf( "Error: ARP resolution for %u.%u.%u.%u failed.\n",
            TargetServerAddr[0], TargetServerAddr[1], TargetServerAddr[2], TargetServerAddr[3] );
    Utils::endStack( );
    return 1;
  }

  printf( "Next hop address: %02X:%02X:%02X:%02X:%02X:%02X\n",
          curr_unit->next_hop_mac[0], curr_unit->next_hop_mac[1], curr_unit->next_hop_mac[2],
          curr_unit->next_hop_mac[3], curr_unit->next_hop_mac[4], curr_unit->next_hop_mac[5] );



  // Next up ...  send a connect command to the server to estblish a session.

  OurPort = Udp::getUnusedCallbackPort( );
  Udp::registerCallback( OurPort, udpHandler );

  Sequence = rand( );
  Session = 0;

  // Setup the parts of the packet unique to this command
  ConnectPayload_t *p = (ConnectPayload_t *)&(OutgoingCmdPkt.payload);
  copyEth(p->macAddr, local_my_mac);
  p->clientType = 1;
  p->blockSize = 512;
  uint16_t dosv = dosVersion( );
  sprintf(p->osString, "DOS %d.%d", (dosv & 0xff), (dosv >> 8));
  strcpy(p->remoteImage, TargetDriveName);

  // We include the terminating nul on the drive image name just to be consistent.
  int payloadLen = sizeof(EthAddr_t) + 2 + 2 + OS_STRING_MAXLEN + strlen(TargetDriveName) + 1;

  if ( sendMsgAndWait( Op_Connect, payloadLen, "Connect", 2 ) ) {
    Utils::endStack( );
    return 1;
  }

  if ( ReceivedCmdPkt.Result != 0 ) {
    printf( "Error opening virtual hard drive: %s,\n  %s\n", TargetDriveName, ReceivedCmdPkt_OptPayload );
    Utils::endStack( );
    return 1;
  }


  // Always keep the drive driver Session and Sequence variables in sync.
  // (These will be copied to the unit data before they change.)
  Session = ReceivedCmdPkt.Session;
  Sequence++;

  printf( "Session (%u) started, virtual hard drive opened: %s\n", Session, TargetDriveName );


  // This horrible hack lets us do all of the endstack steps but leaves tracing on.
  // That includes ending incoming packet receiving, disconnecting the packet driver,
  // stopping IP reassembly, unhooking the timer, and restoring the old CtrlBreak handler.

  Utils::endStack2( true );


  // Now comes the part where we hook the device driver and the packet driver together.

  // Connect to the packet driver.  This will also tell it to start calling
  // the receiver funciton in the device driver, but that will default to
  // just throwing everything away until we tell it to do otherwise.
  //
  // This really should never fail - we just used the packet driver up above.

  connectNetDriveToPD( );

  printf( "Packet driver connected at interrupt: 0x%X\n", Utils::getPacketInt( ) );

  if ( SecondaryPDMsg == 0 ) {
    puts("NetDrive packet driver shim not enabled; do not use other mTCP programs." );
  } else if ( SecondaryPDMsg == 1 ) {
    printf("NetDrive packet driver shim connected at: 0x%X\n", Utils::getPacketInt2() );
  }

  // There could be a 3rd message here about the specified NetDrive shim already
  // being in use by something else, and therefore unavailable.  But you can never
  // actually get that far because when this program initially tries to talk to the
  // server it won't be able to because Utils::initStack will trip on the secondary
  // software interrupt first and not load.


  // Populate net_data structures that are shared by all units:

  if (net_data->connectedCount == 0) {

    net_data->timeoutThreshold = TimeoutTicks;
    net_data->retries = TimeoutRetries;

    copyIp( net_data->my_ip, MyIpAddr );
    net_data->my_port = OurPort;
    copyEth( net_data->my_mac, local_my_mac );

    // Setup the prebuilt ARP packet.  Fix me, not needed if we did it already.

    copyBytes( net_data->arpRespEyeCatcher, "ARPRESP ", sizeof(net_data->arpRespEyeCatcher) );
    copyEth( net_data->prebuiltArpResponse.src, net_data->my_mac );
    net_data->prebuiltArpResponse.ethType = 0x0608;
    net_data->prebuiltArpResponse.arpHeader.hardwareType = 0x0100;
    net_data->prebuiltArpResponse.arpHeader.protocolType = 0x0008;
    net_data->prebuiltArpResponse.arpHeader.hlen = 6;
    net_data->prebuiltArpResponse.arpHeader.plen = 4;
    net_data->prebuiltArpResponse.arpHeader.operation = 0x0200;
    copyEth( net_data->prebuiltArpResponse.arpHeader.sender_ha, net_data->my_mac );
    copyIp( net_data->prebuiltArpResponse.arpHeader.sender_ip, MyIpAddr );
    copyBytes( net_data->prebuiltArpResponse.padding,
               "mTCP by M Brutman ", 
               sizeof(net_data->prebuiltArpResponse.padding ));
  }


  // Populate data specific to the new drive letter
  // (next_hop_mac was set earlier.)

  copyIp( curr_unit->server_ip, TargetServerAddr );
  curr_unit->server_port = TargetServerPort;
  curr_unit->session = Session;
  curr_unit->sequence = Sequence;
  copyStr( curr_unit->remoteName, TargetDriveName, DRIVE_NAME_MAXLEN_DRVSIDE );


  // Before we copy the BPB see if we need to fix it up for DOS 1.x 160K or 320K diskettes.
  // Our fake BPBs work for for all versions of DOS, except chkdsk on DOS 5.x and up fails
  // with an invalid media type error.

  Connect_Resp_t *connectResp = (Connect_Resp_t *)ReceivedCmdPkt_OptPayload;

  if (connectResp->bpb_u.bpb.bytesPerSector != 512) {
    if ( (connectResp->filesize == 160*1024ul) && (connectResp->mediaDesc == 0xfe)) {
      puts( "Detected DOS 1.x 160KB floppy image without a BPB." );
      memcpy(connectResp->bpb_u.bpbBytes, &DOS_1_160KB, sizeof(BPB_t));
    } else if ( (connectResp->filesize == 320*1024ul) && (connectResp->mediaDesc == 0xff)) {
      puts( "Detected DOS 1.x 320KB floppy image without a BPB." );
      memcpy(connectResp->bpb_u.bpbBytes, &DOS_1_320KB, sizeof(BPB_t));
    }
  }


  // Copy the BPB for this remote drive.
  for ( int i=0; i<sizeof(BPB_t); i++ ) {
    curr_unit->BPB[i] = connectResp->bpb_u.bpbBytes[i];
  }

  // Prevent accidents and mark it read only even if the server is the one that
  // indicated it was read only.  This will keep writes from ever being sent.
  if ( (ReadOnly == true) || (connectResp->flags == 0x01) ) {
    curr_unit->readonly = 1;
  }

  net_data->connectedCount++;

  // Now we are fully initialized.  Enable the driver to process received packets.
  net_data->pkt_recv_on = 1;


  // Set the media changed flag and do a fake operation against the FAT to get
  // DOS to notice the change and request the new BPB.
  fakeStat( );


  // Messages come last ...

  printf( "\nDrive size: %lu, Media descriptor byte from FAT: %02X\n",
          connectResp->filesize, connectResp->mediaDesc );


  if ( ReadOnly == true ) {
    puts( "Readonly option found, writes will be blocked by the device driver." );
  } else {

    switch ( connectResp->flags ) {

      case 0x01: {
        puts( "Warning: Read only image detected, writes will fail." );
        curr_unit->readonly = 1;
        break;
      }

      case 0x02:
      case 0x03: {
        puts( "Session write overlay is turned on; writes will not persist." );
        break;
      }
    }

  }

  return 0;
}



void loadStateFromDeviceDriver( void ) {
  copyIp(TargetServerAddr, curr_unit->server_ip);
  TargetServerPort = curr_unit->server_port;
  OurPort = net_data->my_port;
  Session = curr_unit->session;
  Sequence = curr_unit->sequence;
}


// doDisconnect
//
// Always ensure that the packet driver is disconnected cleanly, even if we
// can't make contact with the server.  (Telling the server that we
// disconnected is nice, but not critical.)

void doDisconnect( void ) {

  loadStateFromDeviceDriver( );

  // Temporarily disconnect the packet driver from the device driver so we can
  // send a message to the server.
  disconnectNetDriveFromPD( );


  // Disable this unit by setting the session to 0 and force DOS to notice
  // the disconnect.  (We have the session number in our global variable
  // and will use that for the message to the server.)
  curr_unit->session = 0;
  net_data->connectedCount--;
  fakeStat( );
  puts( "Drive disconnected from network." );


  // Setup the normal mTCP stack so we can send and receive UDP messages.
  // Although this specific drive letter is now disconnected, there may be
  // other drive letters that are connected.  So if we fail here, reconnect
  // those if needed.

  if ( localInitStack( ) ) {
    if ( net_data->connectedCount ) {
      connectNetDriveToPD( );
      net_data->pkt_recv_on = 1;
    }
    return;
  }

  // From this point forward you have to call the Utils:endStack( ) routine to
  // exit because we have the timer interrupt hooked.

  Udp::registerCallback( OurPort, udpHandler );

  sendMsgAndWait( Op_Disconnect, 0, "disconnect", 2 );

  if ( RespReceived == false ) {
    puts( "(The network drive is disconnected but the server may not know it.)" );
  } else {
    if ( ReceivedCmdPkt.Result == 0 ) {
      printf( "Server session (%u) ended\n", Session );
    } else {
      printf( "Server error trying to end session (%u): %s\n", Session, ReceivedCmdPkt_OptPayload );
      puts( "(Either way, the network drive is disconnected.)" );
    }
  }

  Utils::endStack( );


  // Reconnect the packet driver to the device driver if there are any connected
  // drives remaining.

  if ( net_data->connectedCount ) {
    connectNetDriveToPD( );
    net_data->pkt_recv_on = 1;
  }

  return;
}


// doMarkChkp
//
// Send a message to the server to mark a checkpoint. 

void doMarkChkp( void ) {

  loadStateFromDeviceDriver( );

  // Past this point if you error out you have to remember to reconnect the packet driver.
  disconnectNetDriveFromPD( );

  // Setup the normal mTCP stack so we can send and receive UDP messages.
  // If this fails always reconnect to the device driver.

  if ( localInitStack( ) ) {
    connectNetDriveToPD( );
    net_data->pkt_recv_on = 1;
    return;
  }

  // From this point forward you have to call Utils::endStack( ) routine to
  // exit because we have the timer interrupt and Ctrl-Break interrupt hooked.

  Udp::registerCallback( OurPort, udpHandler );

  // Setup the parts of the packet unique to this command
  // Do not transmit the tag with the trailing nul char; it is not needed.
  strcpy((char *)(&OutgoingCmdPkt.payload), Tag);
  sendMsgAndWait( Op_Mark_Chkp, strlen(Tag), "mark checkpoint", 2 );


  // If sendMsgAndWait fails or times out there isn't much to do.
  // No matter what happens (timeout, fail or success) we are going
  // to reconnect the packet driver.

  if ( RespReceived == true ) {
    // If we get a response always advance the sequence number and keep it
    // in sync with the device driver.
    Sequence++;
    curr_unit->sequence = Sequence;
    if ( ReceivedCmdPkt.Result == 0 ) {
      puts( "Checkpoint marked" );
    } else {
      printf( "Server error: %s\n", ReceivedCmdPkt_OptPayload );
    }
  }

  Utils::endStack( );


  // Reconnect the packet driver to the device driver if there are any connected
  // drives remaining.

  connectNetDriveToPD( );
  net_data->pkt_recv_on = 1;

}



// doGotoChkp
//
// Send a message to the server to go back to a checkpoint.  Whether the
// checkpoint is found or not DOS will be forced to recognize a media change.

void doGotoChkp( void ) {

  printf(
    "Jumping back to a checkpoint will cause you to lose all of the\n"
    "writes that are newer than that checkpoint.  Are you sure?\n\n"
    "Type \"yes\" to confirm -> " );

  char answer[6];
  fgets(answer, 5, stdin);
  puts("");
  if ( stricmp(answer, "yes\n") != 0 ) {
    puts("You didn't answer with \"yes\" ... not doing it.");
    return;
  }

  loadStateFromDeviceDriver( );

  // Past this point if you error out you have to remember to reconnect the packet driver.
  disconnectNetDriveFromPD( );

  // Disable this unit temporarily by setting the session to 0, which causes
  // the driver to use the tiny RAM disk BPB.  Then call fakeStat to get DOS
  // to pick up the change.  (Newer DOS doesn't need the BPB change and
  // fakeStat step but DOS 2.x does.)

  curr_unit->session = 0;
  fakeStat( );

  // Setup the normal mTCP stack so we can send and receive UDP messages.
  // If this fails undo what we did and reconnect to the device driver.

  if ( localInitStack( ) ) {
    curr_unit->session = Session;
    connectNetDriveToPD( );
    net_data->pkt_recv_on = 1;
    fakeStat( );
    return;
  }

  // From this point forward you have to call Utils::endStack( ) routine to
  // exit because we have the timer interrupt and Ctrl-Break interrupt hooked.


  Udp::registerCallback( OurPort, udpHandler );

  // Setup the parts of the packet unique to this command
  // Do not transmit the tag with the trailing nul char; it is not needed.

  // Record format:
  //   1 byte  Type (0 for string, 1 for TagNum)
  //   1 byte  Delete mark too (true or false)
  //   2 bytes TagNum
  //   n bytes TagString

  typedef struct {
    uint8_t  type;
    uint8_t  delMarker;
    uint16_t tagNum;
    char     tag[TAG_MAXLEN];
  } goto_cp_msg;

  goto_cp_msg *msg = (goto_cp_msg *)(&OutgoingCmdPkt.payload);

  int len = 0;
  if ( TagNum == 0 ) {
    msg->type = 0;
    msg->tagNum = 0;
    strcpy(msg->tag, Tag);
    len = strlen(Tag) + 4;
  } else {
    msg->type = 1;
    msg->tagNum = TagNum;
    len = 4;
  }

  msg->delMarker = (TagDelMarker == true);


  puts( "This operation can take a long time if your journal file is large.\n"
        "Please be patient.\n" );

  // If sendMsgAndWait fails or times out there isn't much to do.
  // No matter what happens (timeout, fail or success) we are going
  // to reconnect the packet driver.

  int sendRc = sendMsgAndWait( Op_Goto_Chkp, len, "goto checkpoint", 10 );

  if ( sendRc == 0 ) {

    if ( RespReceived == true ) {
      // If we get a response always advance the sequence number and keep it
      // in sync with the device driver.
      Sequence++;
      curr_unit->sequence = Sequence;
      if ( ReceivedCmdPkt.Result == 0 ) {
        printf( "Successful: %s\n", ReceivedCmdPkt_OptPayload );
      } else {
        printf( "Server error: %s\n", ReceivedCmdPkt_OptPayload );
      }
    }

  } else if ( sendRc == 2 ) {
    puts( "\nIf the journal file is large the server might still be working so please\n"
          "wait for the drive to come ready, then check to see if the goto_cp worked\n"
          "before trying it again." );
  }

  Utils::endStack( );


  // Reconnect the packet driver to the device driver if there are any connected
  // drives remaining.

  // Restore the session number and force another media change.
  curr_unit->session = Session;
  connectNetDriveToPD( );
  net_data->pkt_recv_on = 1;
  fakeStat( );
}


void doListChkp( void ) {

  loadStateFromDeviceDriver( );

  // Past this point if you error out you have to remember to reconnect the packet driver.
  disconnectNetDriveFromPD( );

  // Setup the normal mTCP stack so we can send and receive UDP messages.
  // If this fails always reconnect to the device driver.

  if ( localInitStack( ) ) {
    connectNetDriveToPD( );
    net_data->pkt_recv_on = 1;
    return;
  }

  // From this point forward you have to call Utils::endStack( ) routine to
  // exit because we have the timer interrupt and Ctrl-Break interrupt hooked.

  Udp::registerCallback( OurPort, udpHandler );

  sendMsgAndWait( Op_List_Chkp, strlen(Tag), "list checkpoint", 2 );


  // If sendMsgAndWait fails or times out there isn't much to do.
  // No matter what happens (timeout, fail or success) we are going
  // to reconnect the packet driver.

  if ( RespReceived == true ) {
    // If we get a response always advance the sequence number and keep it
    // in sync with the device driver.
    Sequence++;
    curr_unit->sequence = Sequence;
    if ( ReceivedCmdPkt.Result == 0 ) {
      // The response message is not terminated, but our receive buffer
      // was initialized with nulls so we'll be ok.
      printf( "Checkpoints:\n%s\n", ReceivedCmdPkt_OptPayload );
    } else {
      printf( "Server error: %s\n", ReceivedCmdPkt_OptPayload );
    }
  }

  Utils::endStack( );


  // Reconnect the packet driver to the device driver if there are any connected
  // drives remaining.

  connectNetDriveToPD( );
  net_data->pkt_recv_on = 1;
}



char *HelpText = 
  "netdrive <command> <parameters>\n"
  "\n"
  "Commands:\n"
  "  connect <ipaddr:port> <network_drive_name> <drive:> [-ro]\n"
  "  disconnect <drive:>\n"
  "  status <drive:>\n"
  "\n"
  "Checkpoint commands:\n"
  "  list_cp <drive:>\n"
  "  mark_cp <drive:> <tag>\n"
  "  goto_cp <drive:> -tag <tag> or -num <cp_num> [-del_marker]\n"
  "\n"
  "<drive> is NetDrive drive letter, such as C:, D: or E: .\n"
  "The -ro option marks the drive as read-only.";

void usage( ) {
  puts( HelpText );
  exit( 1 );
}



void getDriveNumber( const char *s ) {

  if (s[1] != ':' || s[2] != 0) usage( );

  if ((s[0] >= 'A' && s[0] <= 'Z')) {
    TargetDriveLetter = s[0] - 'A' + 1;
  } else if ((s[0] >= 'a' && s[0] <= 'z')) {
    TargetDriveLetter = s[0] - 'a' + 1;
  }

  if ( TargetDriveLetter == 0 ) usage( );
  return;
}



const char *TagNameTooLong = "The tag name is too long.\n";

void parseArgs( int argc, char *argv[] ) {

  if ( argc == 1 ) usage( );

  if ( (stricmp(argv[1], "connect") == 0) || (stricmp(argv[1], "c") == 0) ) {

    if ( argc < 5) usage( );

    int j=0;
    bool colonFound = false;
    while ( argv[2][j] != 0 ) {
      if ( argv[2][j] != ':' ) {
        if ( j < (SERVER_NAME_MAXLEN-1) ) TargetServerAddrName[j] = argv[2][j];
      } else {
        TargetServerAddrName[j] = 0;
        colonFound = true;
        j++;
        TargetServerPort = atoi(argv[2]+j);
      }
      j++;
    }
    if ( colonFound == false ) usage( );

    if ( strlen(argv[3]) < DRIVE_NAME_MAXLEN ) {
      strcpy( TargetDriveName, argv[3]);
    } else {
      puts( "The remote drive name is too long.\n" );
      usage( );
    }

    getDriveNumber( argv[4] );

    Operation = 1;

    if ( argc == 6 ) {
      if ( stricmp( argv[5], "-ro" ) == 0 ) {
        ReadOnly = true;
      }
      else usage( );
    }

  }

  else if ( (stricmp(argv[1], "disconnect") == 0) || (stricmp(argv[1], "d") == 0) ) {
    if ( argc < 3 ) usage( );
    getDriveNumber( argv[2] );
    Operation = 2;
  }

  else if ( (stricmp(argv[1], "status") == 0) || (stricmp(argv[1], "s") == 0) ) {
    if ( argc < 3 ) usage( );
    getDriveNumber( argv[2] );
    Operation = 3;
  }

  else if ( stricmp( argv[1], "mark_cp" ) == 0 ) {
    if ( argc < 4 ) usage( );
    getDriveNumber( argv[2] );
    if ( strlen(argv[3]) < TAG_MAXLEN ) {
      strcpy(Tag, argv[3]);
    } else {
      puts( TagNameTooLong );
      usage( );
    }
    Operation = 4;
  }

  else if ( stricmp( argv[1], "goto_cp" ) == 0 ) {
    if ( argc < 5 ) usage( );
    getDriveNumber( argv[2] );

    if ( stricmp(argv[3], "-tag") == 0 ) {
      if ( strlen(argv[4]) < TAG_MAXLEN ) {
        strcpy(Tag, argv[4]);
      } else {
        puts( TagNameTooLong );
        usage( );
      }
    } else if ( stricmp(argv[3], "-num") == 0 ) {
      TagNum = atoi( argv[4] );
      if ( TagNum == 0 ) {
        puts( "Bad checkpoint number\n" );
        usage( );
      }
    } else {
      usage( );
    }

    if ( argc == 6 ) {
      if ( stricmp(argv[5], "-del_marker") == 0 ) {
        TagDelMarker = true;
      } else {
        usage( );
      }
    }

    Operation = 5;
  }

  else if ( stricmp( argv[1], "list_cp" ) == 0 ) {
    if ( argc < 3 ) usage( );
    getDriveNumber( argv[2] );
    Operation = 6;
  }

  else {
    usage( );
  }

}

