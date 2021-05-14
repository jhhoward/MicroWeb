/*

   mTCP IpType.cpp
   Copyright (C) 2015-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: An incomplete list of IP protocol descriptions

   Changes:

   2015-03-31: New file for use with pkttool.cpp.

*/


#include "iptype.h"


// IP Protocol descriptions
//
// This table is an array that has a description for each IP protocol up
// to around index 64.  After that I got tired of typing and gave up.
// If you want to add more you can, just don't skip any.  (If you want
// to be able to skip around you will need to add type numbers for each
// description and use a search algorithm, not a simple table lookup.)


const char *IP_Protocol_Strings[] = {
  "HOPOPT",
  "ICMP",
  "IGMP",
  "GGP",
  "IPv4 enc",
  "ST",
  "TCP",
  "CBT",
  "EGP",
  "IGP",
  "BBN-RCC-MON",
  "NVP-II",
  "PUP",
  "ARGUS",
  "EMCON",
  "XNET",
  "CHAOS",
  "UDP",
  "MUX",
  "DCN-MEAS",
  "HMP",
  "PRM",
  "XNS-IDP",
  "TRUNK-1",
  "TRUNK-2",
  "LEAF-1",
  "LEAF-2",
  "RDP",
  "IRTP",
  "ISO-TP4",
  "NETBLT",
  "MSE-NSP",
  "MERIT-INP",
  "DCCP",
  "3PC",
  "IDPR",
  "XTP",
  "DDP",
  "IDPR-CMTP",
  "TP++",
  "IL",
  "IPv6 enc",
  "SDRP",
  "IPv6-Route",
  "IPv6-Frag",
  "IDRP",
  "RSVP",
  "GRE",
  "DSR",
  "BNA",
  "ESP",
  "AH",
  "I-NLSP",
  "SWIPE",
  "NARP",
  "MOBILE",
  "TLSP",
  "SKIP",
  "IPv6 ICMP",
  "IPv6-NoNxt",
  "IPv6-Opts",
  "(host internal)",
  "CFTP",
  "(local network)"
};

const char *UnknownIpType = "(no description)";

const char *IpType_findDescription( uint8_t target) {

  if ( target < 64 ) {
    return IP_Protocol_Strings[target];
  }

  return UnknownIpType;
}

