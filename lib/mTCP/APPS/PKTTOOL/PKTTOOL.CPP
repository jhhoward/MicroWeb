/*

   mTCP pkttool.cpp
   Copyright (C) 2012-2020 Michael B. Brutman (mbbrutman@gmail.com)
   mTCP web page: http://www.brutman.com/mTCP


   This file is part of mTCP.

   Description: A tool that can scan for packet drivers, dump their statistics,
                and sniff packets on the wire.

   Changes: 2015-03: Initial version; adapted from the earlier diag tool.
            2020-03-07: Fix some serious bugs were we called extended packet
                        driver functions without knowing if they were
                        available.

*/


#include <bios.h>
#include <conio.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "inlines.h"
#include "utils.h"
#include "packet.h"
#include "eth.h"
#include "arp.h"
#include "ip.h"
#include "ipv6.h"
#include "ethtype.h"
#include "iptype.h"




// Function prototypes

int   scanForPacketDrivers( void );
int   listenForPackets( uint16_t softwareInt );
int   showPacketDriverStats( uint16_t softwareInt );
void  usage( int rc );


// Functions and globals used for faster printing

void  buildHexUints( void );
char *uint8ToHexStr( uint8_t t, char *buffer );
char *uint16ToHexStr( uint16_t t, char *buffer );

char *printMACAddr( EthAddr_t addr );
char *printIpAddr( IpAddr_t addr, char *buffer );
char *printIpv6Addr( Ipv6Addr_t addr, char *buffer );

char  PrintIpAddr_workspace1[40];
char  PrintIpAddr_workspace2[40];


// Globals that control behavior

bool     Verbose = false;
bool     RawPackets = false;
uint16_t Snaplen = 2000;
FILE    *OutputFile = stdout;
char    *OutputFilenameArg = NULL;


// Stats

uint32_t Packets_dropped_last = 0;


// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}

void ( __interrupt __far *oldCtrlBreakHandler)( );



// Return codes from main

#define PKTTOOL_RC_GOOD                  (0)
#define PKTTOOL_RC_USAGE_ERROR         (128)
#define PKTTOOL_RC_PKTDRV_NOT_FOUND    (129)
#define PKTTOOL_RC_STATS_NOT_AVAIL     (130)
#define PKTTOOL_RC_STATS_UNKNOWN_ERROR (131)
#define PKTTOOL_RC_FILE_ERROR          (132)
#define PKTTOOL_RC_MISC_ERROR          (150)

// Additional return codes for Scan
//
//     n -> number of packet drivers found
//
// Additional return codes for Listen
//
//     0 -> at least one packet received
//     1 -> no packets received



// Constant strings

const char *BadPacketDriverErrMsg = "Error initializing packet driver - was that the correct software interrupt?";
const char *BadHandleErrMsg = "Bad handle: Int: 0x%x, Handle: %x\n";

const char CopyrightMsg[] = "\nmTCP pkttool by M Brutman (mbbrutman@gmail.com) (C)opyright 2012-2020\nVersion: " __DATE__ "\n";

int main( int argc, char *argv[] ) {

  puts( CopyrightMsg );

  if ( argc < 2 ) usage( PKTTOOL_RC_USAGE_ERROR );

  buildHexUints( );

  int rc = 0;

  int i = 1;

  if ( stricmp( argv[i], "help" ) == 0 ) {

    usage( 0 );

  } else if ( stricmp(argv[i], "scan" ) == 0 ) {

    rc = scanForPacketDrivers( );
    if ( rc == 0 ) {
      puts( "No packet drivers found - did you load one?" );
    }

  } else if ( stricmp( argv[i], "listen" ) == 0 || stricmp( argv[i], "stats" ) == 0 ) {

    bool listen = (stricmp( argv[i], "listen" ) == 0);

    i++;
    if ( i == argc ) {
      printf( "Need to provide a packet driver software interrupt with the %s option\n\n",
              (listen ? "listen" : "stats") );
      usage( PKTTOOL_RC_USAGE_ERROR );
    }

    uint16_t targetSoftwareInt;
    int scanRc = sscanf( argv[i], "%x", &targetSoftwareInt );
    if ( scanRc != 1 ) {
      puts( "Parse error reading the packet driver software interrupt number.\n"
            "The format is 0xNN where NN are hexadecimal digits.  The usual range\n"
            "is between 0x60 and 0x80.\n" );
      usage( PKTTOOL_RC_USAGE_ERROR );
    }


    if ( listen ) {

      i++;

      for ( ; i < argc; i++ ) {

        if ( stricmp( argv[i], "-v" ) == 0 ) {
          Verbose = true;
        } else if ( stricmp( argv[i], "-raw" ) == 0 ) {
          RawPackets = true;
        } else if ( stricmp( argv[i], "-outputfile" ) == 0 ) {

          i++;
          if ( i == argc ) {
            puts( "You need to specify an output file with the -outputfile option.\n" );
            usage( PKTTOOL_RC_USAGE_ERROR );
          }

          OutputFilenameArg = argv[i];

        } else if ( stricmp( argv[i], "-s" ) == 0 ) {

          i++;
          if ( i == argc ) {
            puts( "You need to specify a length with the -s option.\n" );
            usage( PKTTOOL_RC_USAGE_ERROR );
          }

          Snaplen = atoi( argv[i] );
          if ( Snaplen == 0 ) {
            puts( "Error: bad length for -s option\n" );
            usage( PKTTOOL_RC_USAGE_ERROR );
          }

        } else {
          printf( "Unknown option: %s\n\n", argv[i] );
          usage( PKTTOOL_RC_USAGE_ERROR );
        }

      }

      rc = listenForPackets( targetSoftwareInt );

    }
    else {
      rc = showPacketDriverStats( targetSoftwareInt );
    }

  } else {
    printf( "Unknown option: %s\n\n", argv[i] );
    usage( PKTTOOL_RC_USAGE_ERROR );
  }

  return rc;
}


void usage( int rc ) {

  puts( "pkttool: [command]\n"
        "\n"
        "Commands:\n"
        "\n"
        "  help            Display this help text\n"
        "\n"
        "  scan            Scan for loaded packet drivers or the Trumpet TCP/IP TSR\n"
        "\n"
        "  stats <x>       Get statistics from the packet driver at software int x\n"
        "\n"
        "  listen <x> [-v] [-raw] [-s <n>] [-outputfile <filename>]\n"
        "\n"
        "      Listen for packets using the packet driver at software interrupt <x>.\n"
        "      Use -v to display packet headers.  Use -raw to get packet dumps.\n"
        "      Use -s to limit packet dumps to size <n> bytes.\n"
        "\n"
        "Software interrupts for the stats and listen commands are in hexadecimal\n"
        "form.  Examples are: 0x60, 0x61, 0x7A, etc." );

  exit( rc );
}



//=============================================================================
//
// Additional packet driver functions that are not needed in PACKET.CPP
// Do not call these until you have registered with the packet driver.


// Packet_getReceiveMode
//
// This is an extended packet driver function; don't call it unless your
// packet driver supports extended functions.

int Packet_getReceiveMode( void ) {

  union REGS inregs, outregs;
  struct SREGS segregs;

  inregs.h.ah = 21;
  inregs.x.bx = Packet_getHandle( );

  int86x( Packet_getSoftwareInt( ), &inregs, &outregs, &segregs );

  if ( outregs.x.cflag ) return -1;

  return outregs.x.ax;
}


// Packet_setReceiveMode
//
// This is an extended packet driver function; don't call it unless your
// packet driver supports extended functions.
//
int Packet_setReceiveMode( int newMode ) {

  union REGS inregs, outregs;
  struct SREGS segregs;

  inregs.h.ah = 20;
  inregs.x.cx = newMode;
  inregs.x.bx = Packet_getHandle( );

  int86x( Packet_getSoftwareInt( ), &inregs, &outregs, &segregs );

  if ( outregs.x.cflag ) return -1;

  return 0;
}


uint8_t Packet_driver_info( void ) {

  union REGS inregs, outregs;
  struct SREGS segregs;

  inregs.h.ah = 0x01;
  inregs.h.al = 0xff;
  inregs.x.bx = Packet_getHandle( );

  int86x( Packet_getSoftwareInt( ), &inregs, &outregs, &segregs );

  if ( outregs.x.cflag ) {
    printf( BadHandleErrMsg, Packet_getSoftwareInt( ), Packet_getHandle( ) );
    return 1;
  }

  uint16_t far *intVector = (uint16_t far *)MK_FP( 0x0, Packet_getSoftwareInt( ) * 4 );
  uint16_t entryPointOffset = *intVector;
  uint16_t entryPointSegment = *(intVector+1);

  printf( "Details for driver at software interrupt: 0x%X\n", Packet_getSoftwareInt( ) );
  printf( "  Name: %Ws\n", (char far *)MK_FP( segregs.ds, outregs.x.si ) );
  printf( "  Entry point: %04X:%04X\n", entryPointSegment, entryPointOffset );
  printf( "  Version: %u   Class: %u   Type: %u  Interface Number: %u\n",
             outregs.x.bx, outregs.h.ch, outregs.x.dx, outregs.h.cl );

  uint8_t functionality = outregs.h.al;

  printf( "  Function flag: %u  ", functionality );

  switch ( functionality ) {
    case 1: { puts( "(basic functions only)" ); break; }
    case 2: { puts( "(basic and extended functions)" ); break; }
    case 5: { puts( "(basic and high performance functions)" ); break; }
    case 6: { puts( "(basic, high performance, and extended functions)" ); break; }
    default: { puts( "(unknown flag value)" ); break; }
  }

  printf( "  Current receive mode: " );

  if ( (functionality != PKTDRV_BASIC_EXTENDED) && (functionality != PKTDRV_BASIC_HIGH_PERF_EXTENDED) ) {
    puts( "Driver doesn't support this checking this." );
  } else {

    switch ( Packet_getReceiveMode( ) ) {
      case 1: { puts( "receiver turned off" ); break; }
      case 2: { puts( "packets for this MAC" ); break; }
      case 3: { puts( "packets for this MAC and broadcast packets" ); break; }
      case 4: { puts( "packets for this MAC, broadcast and limited multicast" ); break; }
      case 5: { puts( "packets for this MAC, broadcast and all multicast" ); break; }
      case 6: { puts( "all packets (promiscuous mode)" ); break; }
      default: { puts( "(error reading receiver mode)" ); break; }
    }

  }

  uint8_t ethAddr[6];
  Packet_get_addr( ethAddr );

  printf( "  MAC address: %s\n\n", printMACAddr(ethAddr) );

  return functionality;
}



//=============================================================================
//

const char *TCP_DRVR_EYE_CATCHER = "TCP_DRVR";

int scanForPacketDrivers( void ) {

  int driversFound = 0;

  puts( "Scanning:\n" );

  for ( uint16_t i=0x60; i < 0x80; i++ ) {

    uint16_t far *intVector = (uint16_t far *)MK_FP( 0x0, i * 4 );

    uint16_t eyeCatcherOffset = *intVector;
    uint16_t eyeCatcherSegment = *(intVector+1);

    char far *eyeCatcher = (char far *)MK_FP( eyeCatcherSegment, eyeCatcherOffset );

    char far *tmpEyeCatcher = (char far *)(eyeCatcher + 3); // Skip three bytes of executable code

    if ( _fmemcmp( PKT_DRVR_EYE_CATCHER, tmpEyeCatcher, 8 ) == 0 ) {

      int initRc = Packet_init( i );
      if ( initRc == 0 ) {
        Packet_driver_info( );
        Packet_release_type( );
        driversFound++;
      }

    } else {

      // Check to see if Trumpet is loaded just to provide the warning.

      tmpEyeCatcher = eyeCatcher;

      for ( int j=0; j < 5; j++ ) {
        if ( _fmemcmp( TCP_DRVR_EYE_CATCHER, tmpEyeCatcher + j, 8 ) == 0 ) {
          printf( "Trumpet TCP/IP TSR possibly found at software interupt 0x%X.\n"
                  "  (Note: Trumpet is generally not compatible with mTCP or WATTCP programs.)\n", i );
        }
      }

    }


  }

  return driversFound;
}




//=============================================================================
//
// IpAddrTable just keeps a mapping of MAC addresses to IP addresses.
// The list will be sorted by MAC address when it is dumped out.

class IpAddrTable {

  public:

    static void addToTable( EthAddr_t macAddr, IpAddr_t ipAddr );
    static void dumpTable( FILE *outputFile );

  private:

    static int sortF( const void *a, const void *b );

    typedef struct {
      EthAddr_t macAddr;
      IpAddr_t  ipAddr;
    } table_record_t;

    static table_record_t records[IPADDR_TABLE_MAX_ENTRIES];
    static uint16_t  tableEntries;

};


IpAddrTable::table_record_t IpAddrTable::records[IPADDR_TABLE_MAX_ENTRIES];
uint16_t IpAddrTable::tableEntries;


void IpAddrTable::addToTable( EthAddr_t macAddr, IpAddr_t ipAddr ) {

  if ( ipAddr[0] == 0 ) return;

  for ( uint16_t i=0; i < tableEntries; i++ ) {
    if ( Ip::isSame( ipAddr, records[i].ipAddr ) && Eth::isSame( macAddr, records[i].macAddr ) ) {
      // This combination was found in the table so do not add it.
      return;
    }
  }

  if ( tableEntries < IPADDR_TABLE_MAX_ENTRIES ) {
    Eth::copy( records[tableEntries].macAddr, macAddr );
    Ip::copy( records[tableEntries].ipAddr, ipAddr );
    tableEntries++;
  }
}


int IpAddrTable::sortF( const void *a, const void *b ) {

  uint8_t *a1 = (uint8_t *)a;
  uint8_t *b1 = (uint8_t *)b;

  for ( int i = 0; i < sizeof( table_record_t ); i++ ) {
    if ( *a1 < *b1 ) {
      return -1;
    }
    else if ( *a1 > *b1 ) {
      return 1;
    }
    a1++; b1++;
  }
  return 0;
}


void IpAddrTable::dumpTable( FILE *outputFile ) {

  qsort( records, tableEntries, sizeof(table_record_t), sortF );

  fprintf( outputFile, "\nMAC addresses and their possible IP addresses (based on ARP traffic):\n\n" );

  for ( int i = 0; i < tableEntries; i++ ) {

    fprintf( outputFile, "%s  ->  %s\n",
             printMACAddr( records[i].macAddr ),
             printIpAddr( records[i].ipAddr, PrintIpAddr_workspace1 ) );

  }

  fprintf( outputFile, "\n" );

}



//=============================================================================
//

// EthernetStats represents a node on a linked list that holds MAC addresses
// and a list of EtherTypeCount for each one.  There is also a static pointer
// that holds the head of the list and another static pointer that points
// to a dummy node that hold global counts for all EtherTypes.

class EthernetStats {

  public:

  EthernetStats( EthAddr_t const addr_p, EthernetStats *next_p );

  void increment( EtherType etherType_p, uint16_t packetLen );
  void dumpEtherTypeCounts( FILE *outputFile );

  static void increment( EthAddr_t ethAddr_p, EtherType etherType_p, uint16_t packetLen );
  static void dumpAll( FILE *outputFile );

  static void incrementBroadcastPacketCount( void ) { broadcastPacketCount++; }

  static uint16_t getMACsDetected( void ) { return MACsDetected; }
  static uint32_t getBroadcastPacketCount( void ) { return broadcastPacketCount; }


  private:

    static int countsSortFunc( const void *a, const void *b );

    typedef struct {
      EtherType etherType;
      uint32_t  count;
      uint32_t  bytes;
    } EtherTypeCount_t;

    EthAddr_t         addr;
    uint32_t          packetsSent;
    uint32_t          bytesSent;
    EtherTypeCount_t  etherTypeCounts[ETHERNET_STATS_MAX_ETHERTYPES];
    uint16_t          countsIndex;
    EthernetStats    *next;

    static EthernetStats *hostsHead;
    static EthernetStats *globalCounts;

    static uint16_t MACsDetected;
    static uint32_t broadcastPacketCount;

};


EthernetStats *EthernetStats::hostsHead = NULL;
EthernetStats *EthernetStats::globalCounts = NULL;

uint16_t EthernetStats::MACsDetected = 0;
uint32_t EthernetStats::broadcastPacketCount = 0;


EthernetStats::EthernetStats( EthAddr_t const addr_p, EthernetStats *next_p ) {
  Eth::copy( addr, addr_p );
  packetsSent = 0;
  bytesSent = 0;
  countsIndex = 0;
  next = next_p;
}


EthernetStats::countsSortFunc( const void *a, const void *b ) {

  EtherTypeCount_t *a1 = (EtherTypeCount_t *)a;
  EtherTypeCount_t *b1 = (EtherTypeCount_t *)b;

  if ( a1->count < b1->count ) {
    return 1;
  } else if ( a1->count > b1->count ) {
    return -1;
  }

  // Should never get here as there should be no duplicates in the list.
  return 0;
}


// Given a particular EthernetStats object run the list of EtherTypeCounts
// and increment one or add a new one.

void EthernetStats::increment( EtherType etherType_p, uint16_t packetLen ) {

  // If it is less than 0x0600 it is a length, not a type.
  // Lump them all together into a special bucket.

  if ( etherType_p < 0x0600 ) {
    etherType_p = 0x0000;
  }

  bool found = false;
  for ( uint16_t i = 0; i < countsIndex; i++ ) {
    if ( etherTypeCounts[i].etherType == etherType_p ) {
      etherTypeCounts[i].count++;
      etherTypeCounts[i].bytes += packetLen;
      found = true;
      break;
    }
  }

  if ( found == false ) {
    if ( countsIndex < ETHERNET_STATS_MAX_ETHERTYPES ) {
      etherTypeCounts[countsIndex].etherType = etherType_p;
      etherTypeCounts[countsIndex].count = 1;
      etherTypeCounts[countsIndex].bytes = packetLen;
      countsIndex++;
    }
  }

  packetsSent++;
  bytesSent += packetLen;
};


// Find the target MAC address and update the EtherType count for it.
// If the MAC address is not found then create a new EthernetStats object
// and add it to the chain.

void EthernetStats::increment( EthAddr_t ethAddr_p, EtherType etherType_p, uint16_t packetLen ) {

  EthernetStats *currentHost = hostsHead;
  while ( currentHost ) {
    if ( Eth::isSame( currentHost->addr, ethAddr_p ) ) {
      currentHost->increment( etherType_p, packetLen );
      break;
    }
    currentHost = currentHost->next;
  }
  if ( currentHost == NULL ) {
    EthernetStats *tmp = new EthernetStats( ethAddr_p, hostsHead );
    tmp->increment( etherType_p, packetLen );
    hostsHead = tmp;
    MACsDetected++;
  }

  if ( globalCounts == NULL ) {
    globalCounts = new EthernetStats( Eth::Eth_Broadcast, NULL );
  }
  globalCounts->increment( etherType_p, packetLen );
  
}

void EthernetStats::dumpEtherTypeCounts( FILE *outputFile ) {

  qsort( etherTypeCounts, countsIndex, sizeof(EtherTypeCount_t), countsSortFunc );

  for ( uint16_t i = 0; i < countsIndex; i++ ) {

    if ( etherTypeCounts[i].etherType > 0x0000 ) {
      fprintf( outputFile, "  Packets: %6lu  Bytes: %7lu  Type: %04X %s\n",
                           etherTypeCounts[i].count,
                           etherTypeCounts[i].bytes,
                           etherTypeCounts[i].etherType,
                           EtherType_findDescription( etherTypeCounts[i].etherType ) );
    } else {
      fprintf( outputFile, "  Packets: %6lu  Bytes: %7lu  Type: ---- %s\n",
                           etherTypeCounts[i].count,
                           etherTypeCounts[i].bytes,
                           EtherType_findDescription( etherTypeCounts[i].etherType ) );
    }

  }

  fprintf( outputFile, "\n" );

}



void EthernetStats::dumpAll( FILE *outputFile) {

  fprintf( outputFile, "\nPackets received:           %5lu\n", Packets_received );
  fprintf( outputFile, "Packets dropped:            %5lu\n", Packets_dropped );
  fprintf( outputFile, "Ethernet broadcast packets: %5lu\n", broadcastPacketCount );
  fprintf( outputFile, "Unique MACs detected:       %5u\n\n", MACsDetected );

  EthernetStats *currentHost = hostsHead;
  while ( currentHost ) {
    fprintf( outputFile, "MAC addr: %s  Packets sent: %lu  Bytes sent: %lu\n",
            printMACAddr( currentHost->addr ),
            currentHost->packetsSent,
            currentHost->bytesSent );
    currentHost->dumpEtherTypeCounts( outputFile );
    currentHost = currentHost->next;
  }

  if ( globalCounts ) {
    fprintf( outputFile, "\nAll hosts combined: Packets sent: %lu  Bytes sent: %lu\n",
            globalCounts->packetsSent, globalCounts->bytesSent );
    globalCounts->dumpEtherTypeCounts( outputFile );
  }

}
    

//=============================================================================
//


// This is getting a little crazy ...
//
// Call DOS to get the current time of day and convert the result to part
// of a text string.  A null will not be added at the end and we will return
// the updated buffer pointer so we can continue building the string.
//
// This elaborate mess is to avoid using a library function to get the time
// of day and formatting it using sprintf.
//
// After int 21h
//
//   CH = hour (0-23)
//   CL = minutes (0-59)
//   DH = seconds (0-59)
//   DL = hundredths (0-99)

extern char far * getTimeString( char far *buffer );
#pragma aux getTimeString = \
  "mov ah, 0x2c"      \
  "int 21h"           \
  "push dx"           \
  "mov dl, 10"        \
  "xor ah, ah"        \
  "mov al, ch"        \
  "idiv dl"           \
  "add ax,3030h"      \
  "mov word ptr es:bx, ax"     \
  "inc bx"            \
  "inc bx"            \
  "mov byte ptr es:bx, 58"     \
  "inc bx"            \
  "xor ah, ah"        \
  "mov al,cl"         \
  "idiv dl"           \
  "add ax,3030h"      \
  "mov word ptr es:bx, ax"     \
  "inc bx"            \
  "inc bx"            \
  "mov byte ptr es:bx, 58"     \
  "inc bx"            \
  "pop cx"            \
  "xor ah, ah"        \
  "mov al,ch"         \
  "idiv dl"           \
  "add ax,3030h"      \
  "mov word ptr es:bx, ax"     \
  "inc bx"            \
  "inc bx"            \
  "mov byte ptr es:bx, 46"     \
  "inc bx"            \
  "xor ah, ah"        \
  "mov al,cl"         \
  "idiv dl"           \
  "add ax,3030h"      \
  "mov word ptr es:bx, ax"     \
  "inc bx"            \
  "inc bx"            \
  parm [es bx]        \
  value [es bx]       \
  modify [ax cx dx];



char lineBuffer[8192];

void genericPacketHandler( uint8_t *packet, uint16_t packetLen ) {

  char *lineBufferOffset = lineBuffer;


  if ( Packets_dropped != Packets_dropped_last ) {
    lineBufferOffset += sprintf( lineBufferOffset,
                                 "\nWarning: %lu packets were lost\n",
                                 (Packets_dropped-Packets_dropped_last) );
    Packets_dropped_last = Packets_dropped;
  }

  if ( Eth::isSame( *(EthAddr_t *)(packet), Eth::Eth_Broadcast ) ) {
    EthernetStats::incrementBroadcastPacketCount( );
  }

  uint16_t etherType = ntohs(((uint16_t *)packet)[6]);

  EthAddr_t *fromEthAddr = (EthAddr_t *)(&packet[6]);
  EthernetStats::increment( *fromEthAddr, etherType, packetLen );

  if ( Verbose ) {

    *lineBufferOffset++ = '\n';

    lineBufferOffset = getTimeString( lineBufferOffset );

    lineBufferOffset += sprintf( lineBufferOffset, " To: %s", printMACAddr( packet ) );
    lineBufferOffset += sprintf( lineBufferOffset, " From: %s Len: %u Type: %04X\n",
                                 printMACAddr( packet + 6 ), packetLen, etherType );

  }

  if ( etherType == 0x0806 ) {

    ArpHeader *ah = (ArpHeader *)(packet + sizeof(EthHeader));

    uint16_t op = ntohs( ah->operation );

    if ( Verbose ) {
      lineBufferOffset += sprintf( lineBufferOffset, "ARP: Request: %s is looking for %s\n",
                                   printIpAddr( ah->sender_ip, PrintIpAddr_workspace1),
                                   printIpAddr( ah->target_ip, PrintIpAddr_workspace2) );
    }

    IpAddrTable::addToTable( ah->sender_ha, ah->sender_ip );

    if ( op == 2 ) { // ARP response

      if ( Verbose ) {
        lineBufferOffset += sprintf( lineBufferOffset, "ARP: Response: %s is %s\n",
                                     printIpAddr( ah->target_ip, PrintIpAddr_workspace1 ),
                                     printMACAddr( ah->target_ha ) );
      }

      IpAddrTable::addToTable( ah->target_ha, ah->target_ip );

    }

  } else if ( etherType == 0x0800 ) {

    if ( Verbose ) {
 
      IpHeader *ip = (IpHeader *)(packet + sizeof(EthHeader) );

      lineBufferOffset += sprintf( lineBufferOffset, "IPv4: From: %s To: %s Len: %u Type: (%u) %s\n",
                                   printIpAddr( ip->ip_src, PrintIpAddr_workspace1 ),
                                   printIpAddr( ip->ip_dest, PrintIpAddr_workspace2 ),
                                   ntohs(ip->total_length), ip->protocol, IpType_findDescription(ip->protocol) );
    }

    // This would be nice but our gateway looks like it has all sorts of
    // addresses when you do it.  So don't do it, unless you want to add
    // logic to detect the gateway and handle it in a different manner.
    //
    // IpAddrTable::addToTable( *fromEthAddr, ip->ip_src );

  } else if ( etherType == 0x86dd ) {

    if ( Verbose ) {

      Ipv6Header *ip = (Ipv6Header *)(packet + sizeof(EthHeader) );

      lineBufferOffset += sprintf( lineBufferOffset,
                                   "IPv6: From: %s\n        To: %s\n       Len: %u Type: (%u) %s\n",
                                   printIpv6Addr(ip->src, PrintIpAddr_workspace1),
                                   printIpv6Addr(ip->dest, PrintIpAddr_workspace2),
                                   ntohs(ip->payloadLen), ip->nextHeader, IpType_findDescription(ip->nextHeader) );

    }

  }


  // Write lineBuffer before Utils::dumpBytes goes to use it.
  fwrite( lineBuffer, 1, (lineBufferOffset - lineBuffer), OutputFile );

  if ( RawPackets ) {
    uint16_t bytes = packetLen;
    if ( packetLen > Snaplen ) bytes = Snaplen;
    Utils::dumpBytes( OutputFile, packet, bytes );
  }

  Buffer_free( packet );
}
  

int listenForPackets( uint16_t softwareInt ) {

  if ( OutputFilenameArg != NULL ) {
    OutputFile = fopen( OutputFilenameArg, "w" );
    if ( OutputFile == NULL ) {
      fprintf( stderr, "Error: bad filename specified for -outputfile\n" );
      return PKTTOOL_RC_FILE_ERROR;
    }
  }

  if ( Buffer_init( ) ) {
    fprintf( stderr, "Init: Failed creating buffers\n" );
    return PKTTOOL_RC_MISC_ERROR;
  }

  int rc = Packet_init( softwareInt );
  if ( rc != 0 ) {
    puts( BadPacketDriverErrMsg );
    return PKTTOOL_RC_PKTDRV_NOT_FOUND;
  }


  // Hook Ctrl-Break and Ctrl-C while the packet driver is live.  We don't
  // want the user to be able to terminate the program abnormally without
  // unhooking the packet driver first.

  oldCtrlBreakHandler = getvect( 0x1b );
  setvect( 0x1b, ctrlBreakHandler);
  setvect( 0x23, ctrlBreakHandler);


  uint8_t functionality = Packet_driver_info( );
  bool receiveModeSupport = (functionality == PKTDRV_BASIC_EXTENDED) ||
                            (functionality == PKTDRV_BASIC_HIGH_PERF_EXTENDED);


  // We won't use oldReceiveMode unless we know the driver supports it.
  // We set it to a reasonable default either way.
  int oldReceiveMode = 2;

  if ( receiveModeSupport ) {
    oldReceiveMode = Packet_getReceiveMode( );
    if ( Packet_setReceiveMode( 6 ) ) {
      puts( "Warning - failed to set promiscuous mode on your Ethernet card.\n" );
    }
  } else {
    puts( "Warning: Not attempting to put your card in promiscuous mode.\n" );
  }

  puts( "Listening for incoming packets.  Press [ESC] to quit.\n" );

  if ( !Verbose && !RawPackets ) {
    puts( "(Neither -v or -raw were specified, so nothing will be printed\n"
          "on-screen until you press [ESC] to quit.)\n" );
  }

  if ( OutputFilenameArg != NULL ) {
    printf( "(Packet data is being written to %s)\n\n", OutputFilenameArg );
    fprintf( OutputFile, "mTCP pkttool (Version: " __DATE__ ") packet trace\n\n" );
    flushall( );
  }

  Packet_registerDefault( genericPacketHandler );

  Buffer_startReceiving( );

  time_t startTime = time( NULL );

  while ( 1 ) {
    if ( CtrlBreakDetected ) break;
    if ( bioskey(1) ) {
      char c = getch( );
      if ( c == 27 ) break;
    }
    PACKET_PROCESS_SINGLE;
  }

  time_t stopTime = time( NULL );
  time_t elapsedTime = stopTime - startTime;


  // Dump stats


  fprintf( OutputFile, "\nTracing active for %lu minutes, %lu seconds\n", (elapsedTime/60), (elapsedTime%60) );

  EthernetStats::dumpAll( OutputFile );
  IpAddrTable::dumpTable( OutputFile );

  if ( receiveModeSupport ) Packet_setReceiveMode( oldReceiveMode );

  Buffer_stopReceiving( );
  Packet_release_type( );
  Buffer_stop( );


  setvect( 0x1b, oldCtrlBreakHandler);

  if ( OutputFilenameArg != NULL ) {
    fclose( OutputFile );
    printf( "Trace and statistics were written to %s\n", OutputFilenameArg );
  }

  return Packets_received ? 0 : 1;
}





int showPacketDriverStats( uint16_t softwareInt ) {

  if ( Packet_init( softwareInt ) ) {
    puts( BadPacketDriverErrMsg );
    return PKTTOOL_RC_PKTDRV_NOT_FOUND;
  }

  // From this point forward you can not do an early return because we
  // registered with the packet driver.

  int rc = -1;


  // Find out if the packet driver supports statistics
  uint8_t functionality = Packet_driver_info( );

  if ((functionality != PKTDRV_BASIC_EXTENDED) && (functionality != PKTDRV_BASIC_HIGH_PERF_EXTENDED)) {

    puts( "This packet driver doesn't support extended functions.  Statistics\n"
          "are not available." );
    rc = PKTTOOL_RC_STATS_NOT_AVAIL;

  } else {

    union REGS inregs, outregs;
    struct SREGS segregs;

    inregs.h.ah = 0x18;
    inregs.x.bx = Packet_getHandle( );

    int86x( Packet_getSoftwareInt( ), &inregs, &outregs, &segregs );

    if ( outregs.x.cflag ) {

      puts( "Unknown error reading statistics from the packet driver." );
      rc = PKTTOOL_RC_STATS_UNKNOWN_ERROR;

    } else {

      PacketStats_t far *stats = ( PacketStats_t far *)MK_FP( segregs.ds, outregs.x.si );

      printf( "All statistics are from when the packet driver was loaded.\n\n"
              "Packets in:   %10lu  Packets received\n"
              "Packets out:  %10lu  Packets sent\n"
              "Bytes in:     %10lu  Bytes received (includes Ethernet headers)\n"
              "Bytes out:    %10lu  Bytes sent (includes Ethernet headers)\n"
              "Errors in:    %10lu  Receive errors\n"
              "Errors out:   %10lu  Sending errors\n"
              "Packets lost: %10lu  Lost due to no packet handler, out of buffers, etc.\n",
              stats->packets_in, stats->packets_out, stats->bytes_in, stats->bytes_out,
              stats->errors_in, stats->errors_out, stats->packets_lost );

      rc = PKTTOOL_RC_GOOD;
    }

  }

  Packet_release_type( );

  return rc;
}





//=============================================================================
//
// Utils::dumpBytes
//
// This implementation is for dumping raw packets.  The maximum size packet we
// will ever print is around 1514 bytes.  If you assume that we can print 16
// bytes per line, that requires around 95 lines.  We print around 72 chars
// per line and you need to add a CR/LF pair to that, so just round up to 75.
// Add an extra 1K of chars for slop and we are at 8192.
//
// The normal Utils.CPP provides it's own linebuffer storage and it is only
// 160 bytes.  We have to use ::lineBuffer so that we are not ambiguous and
// cause a link error.

void Utils::dumpBytes( FILE *stream, unsigned char *buffer, unsigned int len ) {

  // Small optimization; we always start by skipping a line.

  ::lineBuffer[0] = '\n';
  char *lineBufferIndex = ::lineBuffer+1;

  // Since the buffer is pre-allocated we can compute where the ASCII part
  // of the line is going to be ahead of time and put the chars directly
  // in place.

  char *ascOffset = lineBufferIndex + 56;

  uint16_t i;
  for (i=0; i < len; i++ ) {

    // If we are at the start of a line print the offset into the buffer.

    if ( (i & 0x000f) == 0) {
      lineBufferIndex = uint16ToHexStr( i, lineBufferIndex );
      *lineBufferIndex++ = ' ';
      *lineBufferIndex++ = ' ';
    }


    // Spit out the hex value and the ASCII value.

    lineBufferIndex = uint8ToHexStr( buffer[i], lineBufferIndex );
    *lineBufferIndex++ = ' ';
    if ( buffer[i] > 31 && buffer[i] < 127 ) {
      *ascOffset++ = buffer[i];
    }
    else {
      *ascOffset++ = '.';
    }

    // If we have put 16 chars on the line then add to spaces and skip past
    // the ASCII part, which has been filled in already.  Add a new line
    // and start again.

    if ( (i & 0xf) == 0xf ) {
      *lineBufferIndex++ = ' ';
      *lineBufferIndex++ = ' ';
      lineBufferIndex += 16;
      *lineBufferIndex++ = '\n';
      ascOffset = lineBufferIndex + 56;
    }
      
  }

  if ( i & 0xf ) {
    int charsPrintedThisLine = (i & 0xf);
    int padding = 50 - ((charsPrintedThisLine<<1) + charsPrintedThisLine);
    while ( padding-- ) { *lineBufferIndex++ = ' '; }
    lineBufferIndex = ascOffset;
    *lineBufferIndex++ = '\n';
  }

  fwrite( ::lineBuffer, 1, (lineBufferIndex - ::lineBuffer), stream );

}



//=============================================================================
//
// Faster hex printing.
//
// Build a table that allows us to do a quick lookup of a single uint8 and
// convert it to two printable hexadecimal characters.  The table storage
// costs 512 bytes but it allows us to do the conversion with just a single
// two byte move.

const char hexChars[] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

char hexUintsTable[256][2];

void buildHexUints( void ) {
  for ( int i=0; i < 256; i++ ) {
    hexUintsTable[i][0] = hexChars[i / 16];
    hexUintsTable[i][1] = hexChars[i % 16];
  }
}

inline char * uint8ToHexStr( uint8_t t, char *buffer ) {
  *((uint16_t *)buffer) = *((uint16_t *)hexUintsTable[t]);
  return buffer+2;
}

inline char * uint16ToHexStr( uint16_t t, char *buffer ) {
  *((uint16_t *)buffer) = *((uint16_t *)hexUintsTable[t>>8]);
  buffer += 2;
  *((uint16_t *)buffer) = *((uint16_t *)hexUintsTable[t&0xff]);
  return buffer+2;
}



//=============================================================================
//
// Utility functions for printing nasty things.
//
// Using printIpAddr instead of sprintf with four %u's in the format string
// requires more bytes of code but it should be much faster than going through
// the overhead of sprintf.


char *printMACAddr( EthAddr_t addr ) {
  static char tmpStr[20];
  char *tmpStrPtr = tmpStr;
  int i = 6;
  while ( i ) {
    tmpStrPtr = uint8ToHexStr( *addr++, tmpStrPtr );
    *tmpStrPtr++ = ':';
    i--;
  }
  tmpStrPtr--;
  *tmpStrPtr = 0;
  return tmpStr;
}


// If you want to use this function more than once in the same sprintf call
// you need to provide each call with a separate workspace variable.
// The provided buffer must be at least 40 characters.

char *printIpv6Addr( Ipv6Addr_t addr, char *buffer ) {
  uint8_t *addrTmp = (uint8_t *)addr;
  char *tmp = buffer;
  for ( int i=0; i<8; i++ ) {
    tmp = uint8ToHexStr( *addrTmp++, tmp );
    tmp = uint8ToHexStr( *addrTmp++, tmp );
    *tmp++ = ':';
  }
  --tmp; // Get rid of the extra ':' at the end
  *tmp = 0;
  return buffer;
}


// Printing IPv4 addresses
//
// sprintf is the normal way to print integers but printing IPv4 addresses
// seems particularly wasteful because four different parameters have to be
// pushed onto the stack, and then sprintf is going to have a huge amount
// of overhead.
//
// This code turns an IPv4 address into a character string octet by octet.
// The string is built backwards in a work buffer provided by the caller
// using an inline function to make it slightly more efficient.  The inline
// function takes advantage of the idiv instruction, which can divide an
// 8 bit number, computing both the quotient and remainder at the same time.
//
// If you want to use this function more than once in the same sprintf call
// you need to provide each call with a separate workspace variable.
// The provided buffer must be at least 16 characters.

typedef union {
  struct {
    uint8_t quotient;    // AL
    uint8_t remainder;   // AH
  } parts;
  uint16_t whole;
} SmallDivide_t;

extern uint16_t smallDivide( uint8_t i );
#pragma aux smallDivide = \
  "mov dl, 10"   \
  "idiv dl"      \
  "add ah,48  "  \
  modify [ax dl] \
  parm [ax]      \
  value [ax];

char *printIpAddr( IpAddr_t addr, char *buffer ) {

  buffer[15] = 0;
  uint16_t offset = 14;

  SmallDivide_t tmp;

  for ( int i=3; i>-1; i-- ) {

    uint8_t v = addr[i];

    if ( v ) {
      while ( v ) {
        tmp.whole = smallDivide( v );
        buffer[offset--] = tmp.parts.remainder;
        v= tmp.parts.quotient;
      }
    }
    else {
      buffer[offset--] = '0';
    }

    buffer[offset--] = '.';

  }

  return (buffer + offset + 2);
}
