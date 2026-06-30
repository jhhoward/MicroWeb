/*

   mTCP NetDrive (CHKSUM_TEST.CPP)
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


   Description: IP and UDP checksuming test code for NetDrive.

   To do checksum testing you need this program and a special version of
   the device driver that has some extra hooks for this program to use.
   Build the chksum_test target, install it, and then run this program.
   To keep things simple remove NETDRIVE.SYS from the test system
   temporarily and only plan on doing checksum testing; even thoough
   all of the code is present the extra hooks move things around and make
   this incompatible with the command line program.

   Why go through all of this?  Because you should not need to do checksum
   testing often.

   What kind of testing is it doing?  Basically it's just fuzz testing.  It
   creates a sample UDP packet of mostly random data, then computes the
   IP and UDP checksums using the device driver code, the mTCP library code,
   and a common implementation of the checksum routines in C.  If any of the
   code doesn't agree there is an error that needs to be investigated.

*/


#include <dos.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "Arp.h"
#include "Ip.h"
#include "Udp.h"


#define CHKSUM_TEST
#include "SHARED.H"



uint8_t  TargetDriveLetter = 0;                      // A=1, B=2, C=3, ... (0 is invalid)

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

#endif





Net_data_t far *net_data;


void doTest( void );


// Signature to verify we have found the NetData structure in memory.
// The last two bytes are the major and minor revision numbers.
static const char Signature[] = {'N', 'E', 'T', 'D', 'R', 'I', 'V', 'E', 0x00, 0x03};


int main( int argc, char *argv[] ) {

  puts( "mTCP Netdrive Checksum routine test code." );

  TargetDriveLetter = argv[1][0];
  if ( TargetDriveLetter >= 'A' && TargetDriveLetter <= 'Z') {
    TargetDriveLetter = TargetDriveLetter - 'A' + 1;
  } else if ((TargetDriveLetter >= 'a' && TargetDriveLetter <= 'z')) {
    TargetDriveLetter = TargetDriveLetter - 'a' + 1;
  } else {
    puts( "Bad drive letter" );
    return 1;
  }


  typedef struct {
    Net_data_t far *net_data;
    void       far *unit;
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

  printf( "NetDrive device opened, IOCTL_read return code: %x %04X:%04X\n\n",
      rc, FP_SEG(net_data), FP_OFF(net_data) );


  doTest( );

  return 0;
}


uint8_t tp[1400];



void far *testIpFuncPtr;

extern uint16_t testIpFunc(uint8_t far *);
#pragma aux testIpFunc = \
  "push ss"              \
  "pop ds"               \
  "call testIpFuncPtr"   \
  modify [bx cx]      \
  value [ax]             \
  parm [si];


void far *testUdpFuncPtr;

extern uint16_t testUdpFunc(uint8_t far *);
#pragma aux testUdpFunc = \
  "push ss"              \
  "pop ds"               \
  "call testUdpFuncPtr"   \
  modify [bx cx]      \
  value [ax]             \
  parm [si];


uint16_t pseudoChecksum( const IpAddr_t src, const IpAddr_t target,
         uint16_t *data_p, uint8_t protocol, uint16_t len );


void doTest( void ) {

  srand(time(NULL));

  testIpFuncPtr = MK_FP(net_data->testIp_seg, net_data->testIp_off);
  testUdpFuncPtr = MK_FP(net_data->testUdp_seg, net_data->testUdp_off);

  printf( "testIpFuncPtr: %04X:%04X, testUdpFuncPtr: %04X:%04X\n",
    net_data->testIp_seg, net_data->testIp_off, net_data->testUdp_seg, net_data->testUdp_off );
  printf( "Fake packet address: %04X:%04X\n", FP_SEG(&tp), FP_OFF(&tp) );

  puts( "\nPress a key to stop." );

  uint32_t tests = 0;
  while ( 1 ) {

    if ( biosIsKeyReady( ) ) {
      break;
    }

    tests++;

    // Generate random crap in the packet

    int ip_options_len = (rand( ) % 5) * 4;
    int udp_payload_len = rand( ) % 1024;
    int total_len = 20 + 8 + ip_options_len + udp_payload_len;

    // printf( "IP options len: %d, UDP len: %d\n", ip_options_len, udp_payload_len );

    uint8_t far * g = (uint8_t far *)(tp+14);

    // Write garbage everywhere, and then fixup specific fields to make them legal.
    for ( int i=0; i < total_len; i++ ) { g[i] = rand( ); }

    // Fixup the parts of the packet that are important for the checksum protocol:
    // * IP Header length, protocol, total length and checksum
    // * UDP Length and checksum

    IpHeader far * ip_header = (IpHeader far *)(tp+14);
    IpHeader * ip_header_local = (IpHeader *)(tp+14);
    UdpHeader far * udp_header = (UdpHeader far *)(tp+14+20+ip_options_len);

    ip_header->versHlen = 0x40 | ((20+ip_options_len)/4);
    ip_header->total_length = htons(total_len);
    ip_header->protocol = 17;
    ip_header->chksum = 0;
    udp_header->len = htons(8+udp_payload_len);
    udp_header->chksum = 0;

    // Get the IP checksum and UDP checksum from the device driver
    uint16_t ip_1 = testIpFunc(tp);
    uint16_t udp_1 = testUdpFunc(tp);

    // This routine is used for incoming and outgoing checksum calculations
    // for both UDP and TCP so it doesn't fix up the UDP checksum if it is
    // 0x0000.  Do that here if needed.
    if ( udp_1 == 0x0000 ) { udp_1 = 0xFFFF; }

    // Randomly corrupt a byte
    // if ( rand() % 10000 == 0 ) {
    //   g[28 + (rand() % 1024)] = rand();
    // }

    // Now compare to the existing mTCP IP and UDP checksum implementations.
    uint16_t ip_2 = ipchksum( (uint16_t *)ip_header, 20 + ip_options_len );

    uint16_t udp_2 = ip_p_chksum(
      ip_header->ip_src,
      ip_header->ip_dest,
      (uint16_t *)udp_header,
      IP_PROTOCOL_UDP, 8+udp_payload_len);

    // This routine is used for incoming and outgoing checksum calculations
    // for both UDP and TCP so it doesn't fix up the UDP checksum if it is
    // 0x0000.  Do that here if needed.
    if ( udp_2 == 0x0000 ) { udp_2 = 0xFFFF; }


    // Do it again with a C implementation
    uint16_t udp_3 = pseudoChecksum(
      ip_header_local->ip_src,
      ip_header_local->ip_dest,
      (uint16_t *)udp_header,
      IP_PROTOCOL_UDP, 8+udp_payload_len);

    // This routine is used for incoming and outgoing checksum calculations
    // for both UDP and TCP so it doesn't fix up the UDP checksum if it is
    // 0x0000.  Do that here if needed.
    if ( udp_3 == 0x0000 ) { udp_3 = 0xFFFF; }

    if ( ip_1 != ip_2 ) {
      printf("IP checksum failed: %04X %04X\n", ip_1, ip_2);
      break;
    }

    if ( (udp_1 != udp_2) || (udp_1 != udp_3) || (udp_2 != udp_3) ) {
      printf("UDP checksum failed: %04X %04X %04X\n", udp_1, udp_2, udp_3 );
      break;
    }

    if ( udp_1 == 0 ) {
      puts("The UDP checksum is 0 - that is not allowed.");
      break;
    }

  }

  printf("Tests executed: %ld\n", tests);

}



uint16_t pseudoChecksum( const IpAddr_t src, const IpAddr_t target,
         uint16_t *data_p, uint8_t protocol, uint16_t len ) {

  struct {
    IpAddr_t src;
    IpAddr_t target;
    uint8_t zero;
    uint8_t protocol;
    uint16_t len;
  } psheader;

  Ip::copy( psheader.src, src );
  Ip::copy( psheader.target, target );
  psheader.zero = 0;
  psheader.protocol = protocol;
  psheader.len = htons(len);

  // Checksum the pseudo header

  uint32_t sum = 0;

  uint16_t *tmpData = (uint16_t *)&psheader;
  uint16_t tmpLen = sizeof( psheader );

  while ( tmpLen > 1 ) {

    sum += *tmpData;
    tmpData++;

    if ( sum & 0x80000000) sum = (sum & 0xffff) + (sum>>16);

    tmpLen = tmpLen -2;
  }

  while ( sum >> 16 ) {
    sum = (sum & 0xffff) + (sum>>16);
  }


  // Now do the main part

  while ( len > 1 ) {

    sum += *data_p;
    data_p++;

    if ( sum & 0x80000000) sum = (sum & 0xffff) + (sum>>16);

    len = len -2;
  }

  if ( len ) sum += (unsigned short)*(unsigned char *)data_p;

  while ( sum >> 16 ) {
    sum = (sum & 0xffff) + (sum>>16);
  }

  uint16_t rc = (~sum) & 0xFFFF;

  return rc;
}

