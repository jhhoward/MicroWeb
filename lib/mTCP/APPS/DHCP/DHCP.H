
/*

   mTCP Dhcp.h
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


   Description: Data structures for Dhcp

   Changes:

   2011-05-27: Initial release as open source software

*/




#ifndef _DHCP_H
#define _DHCP_H


#include "types.h"



#define DHCP_REQUEST_PORT ( 67 )
#define DHCP_REPLY_PORT   ( 68 )

typedef struct {

  public:

    UdpPacket_t udpHdr; // Space for Ethernet, IP and UDP headers

    uint8_t  operation;         // 1 is a req, 2 is a reply
    uint8_t  hardwareType;      // Set 1
    uint8_t  hardwareAddrLen;   // 6
    uint8_t  hops;              // 0
    uint32_t transactionId;     // Whatever you like
    uint16_t seconds;           // 0
    uint16_t flags;             // Broadcast - not used
    IpAddr_t clientIpAddr;      // set to 0 to indicate we don't know it yet
    IpAddr_t yourIpAddr;        // what is being assigned
    IpAddr_t serverIpAddr;      // not used
    IpAddr_t gatewayIpAddr;     // not used (bootp gateways only)
    uint8_t  clientHdwAddr[16]; // our MAC addr
    uint8_t  serverName[64];    // Server name or DHCP options
    uint8_t  file[128];         // boot file or DHCP options
    uint8_t  optionsCookie[4];
    uint8_t  options[512];

} Dhcp_t;


enum DhcpStatus_t {
  Dhcp_Start,
  Dhcp_Offer,
  Dhcp_Declined,
  Dhcp_Ack,
  Dhcp_Nack,
  Dhcp_Timeout,
  Dhcp_UserAbort
};



#endif
