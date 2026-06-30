/*

   mTCP NetDrive (SHARED.H)
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


   Description: C version of the data structure shared with the device driver.

*/


#include "types.h"



// The device driver side doesn't hold as much but it's not needed.
#define DRIVE_NAME_MAXLEN_DRVSIDE    (40)


typedef struct {
  uint8_t   media_check_value;                     // 0 (unknown), 1 (no change), or 0xff (changed)
  uint8_t   readonly;                              // Is this unit marked read-only?
  IpAddr_t  server_ip;                             // Server IP addr.
  uint16_t  server_port;                           // Server UDP port.
  EthAddr_t next_hop_mac;                          // Next hop MAC
  uint16_t  session;                               // Set by the server at connect time. 0 means not connected.
  uint16_t  sequence;                              // Sequence number for dupe/lost detection.
  char      remoteName[DRIVE_NAME_MAXLEN_DRVSIDE]; // Might want to drop this.
  uint8_t   BPB[25];                               // BPB of the connected image.
  char      padding2;
  uint32_t  blocks_read;
  uint32_t  blocks_written;
  uint32_t  retries;
} Unit_t;



typedef struct {
  EthAddr_t dest;
  EthAddr_t src;
  uint16_t  ethType;
  ArpHeader arpHeader;
  char      padding[18];
} Prebuilt_ArpResponse_t;


// NetData structure - shared with the device driver so the layout has to match exactly.
//
// This is in a separate file so it can be used by CHKSUM_T, which adds some
// function poiinters.

typedef struct {

  // Initialized by the device driver code.  (Read only)

  uint16_t  segment;                 // Device driver code segment
  uint16_t  receiverFuncOffset;      // packet receiver function offset
  uint16_t  shimOffset;              // packet driver shim offset
  uint16_t  intInstOffset;           // Interrupt instruction offset
  uint16_t  unit_start_offset;       // Offset from the driver start to the unit array
  char      firstDriveLetter;        // First drive letter, 0 if unknown (DOS 2.x)


  // Shared fields

  uint8_t   connectedCount;          // Driver inits this, this program maintains it.

  uint16_t  pkt_drv_handle;          // Not actually used by the driver, just stashed.
  uint8_t   pkt_drv_int;             // Used by the driver to send packets.
  uint8_t   pkt_drv_int2;            // Not used by the driver, but makes doStatus easier.
  uint8_t   padding;
  uint8_t   pkt_recv_on;             // Used to gate processing of arriving packets.

  uint16_t  timeoutThreshold;        // Could be a uint8_t ...
  uint16_t  retries;                 // Needs to be uint16_t to keep things simple

  IpAddr_t  my_ip;                   // This machine's IP, UDP port and MAC.
  uint16_t  my_port;
  EthAddr_t my_mac;


  // Global stats

  uint32_t  count_r_total;
  uint32_t  count_r_our_mac;
  uint32_t  count_r_our_udp;
  uint32_t  count_arp_resp;
  uint32_t  count_pkt_send_errors;

  char      arpRespEyeCatcher[8];
  Prebuilt_ArpResponse_t prebuiltArpResponse;

#ifdef CHKSUM_TEST
  uint16_t  testIp_off;
  uint16_t  testIp_seg;
  uint16_t  testUdp_off;
  uint16_t  testUdp_seg;
#endif

} Net_data_t;


