/*

   mTCP EthType.cpp
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


   Description: An incomplete list of EtherTypes and descriptions

   Changes:

   2015-03-31: New for use with pkttool.cpp

*/


#include <stdlib.h>
#include "ethtype.h"



static const char Str_3Com[] = "3Com Corporation";
static const char Str_Wellfleet[] = "Wellfleet Communications";
static const char Str_Xyplex[] = "Xyplex";
static const char Str_Symbolics[] = "Symbolics private";


// EtherTypes and descriptions
//
// This is a table of EtherTypes and descriptions.  Most of these are not in
// use anymore but just in case somebody has a really cool network I've
// included them.  Most of this list came from www.iana.org; IEEE has the
// official regristry but it does not seem as detailed.
//
// Also note that this list is incomplete.
//
// Entries in the list need to be sorted by EtherType or the search
// routine will not work.  (The search routine does a binary search.)
//
// There is one "pseudo" EtherType that is not an IEEE assigned EtherType;
// I use 0x0000 to represent packets that are 802.3 packets.  Those packets
// use the field for a length and not a type.  Ethernet v2 frames use a
// type field instead, which is what mTCP expects.


typedef struct {
  EtherType etherType;
  const char *desc;
} EtherTypeDesc_t;

EtherTypeDesc_t EtherTypeTable[] = {

  { 0x0000, "(IEEE 802.3 packet)" },   // pseudo value - not actually valid.

  { 0x0600, "XEROX NS IDP" },
  { 0x0660, "DLOG" },
  { 0x0661, "DLOG" },
  { 0x0800, "IPv4" },
  { 0x0801, "X.75 Internet" },
  { 0x0802, "NBS Internet" },
  { 0x0803, "ECMA Internet" },
  { 0x0804, "Chaosnet" },
  { 0x0805, "X.25 Level 3" },
  { 0x0806, "ARP" },
  { 0x0807, "XNS compatibility" },
  { 0x0808, "Frame Relay ARP" },
  { 0x081C, Str_Symbolics },
  { 0x0842, "Wake-on-LAN" },
  { 0x0888, Str_Xyplex },
  { 0x0889, Str_Xyplex },
  { 0x088A, Str_Xyplex },
  { 0x0900, "Ungermann-Bass debugger" },
  { 0x0A00, "Xerox IEEE802.3 PUP" },
  { 0x0A01, "PUP address trans" },
  { 0x0BAD, "Banyan VINES" },
  { 0x0BAE, "VINES loopback" },
  { 0x0BAF, "VINES echo" },
  { 0x22F0, "IEEE 1722-2011" },
  { 0x6000, "DEC (unassigned)" },
  { 0x6001, "DEC MOP Dump/Load" },
  { 0x6002, "DEC MOP Remote Console" },
  { 0x6003, "DECnet Phase IV" },
  { 0x6004, "DEC LAT" },
  { 0x6005, "DEC Diag Protocol" },
  { 0x6006, "DEC Customer Protocol" },
  { 0x6007, "DEC LAVC, SCA" },
  { 0x6008, "DEC (unassigned)" },
  { 0x6009, "DEC (unassigned)" },
  { 0x6010, Str_3Com },
  { 0x6011, Str_3Com },
  { 0x6012, Str_3Com },
  { 0x6013, Str_3Com },
  { 0x6014, Str_3Com },
  { 0x8035, "RARP" },
  { 0x809B, "AppleTalk/EtherTalk" },
  { 0x80C4, "Banyan Systems" },
  { 0x80C5, "Banyan Systems" },
  { 0x80D5, "IBM SNA" },
  { 0x80F3, "AppleTalk AARP" },
  { 0x80F7, "Apollo Computer" },
  { 0x80FF, Str_Wellfleet },
  { 0x8100, "VLAN tagged" },
  { 0x8101, Str_Wellfleet },
  { 0x8102, Str_Wellfleet },
  { 0x8103, Str_Wellfleet },
  { 0x8107, Str_Symbolics },
  { 0x8108, Str_Symbolics },
  { 0x8109, Str_Symbolics },
  { 0x8130, "Hayes Microcomputers" },
  { 0x8137, "Novell IPX" },
  { 0x8138, "Novell IPX" },
  { 0x814C, "SNMP over Ethernet" },
  { 0x8191, "PowerLAN" },
  { 0x81B7, Str_Xyplex },
  { 0x81B8, Str_Xyplex },
  { 0x81B9, Str_Xyplex },
  { 0x81D6, "Artisoft Lantastic" },
  { 0x81D7, "Artisoft Lantastic" },
  { 0x8204, "QNX Qnet" },
  { 0x86DD, "IPv6" },
  { 0x880B, "Point-To-Point protocol" },
  { 0x880C, "GSMP" },
  { 0x8847, "MPLS" },
  { 0x8848, "MPLS with upstream label" },
  { 0x8861, "MCAP" },
  { 0x8863, "PPPoE Discovery Stage" },
  { 0x8864, "PPPoE Session Stage" },
  { 0x8870, "Jumbo Frames" },
  { 0x888E, "EAP over LAN" },
  { 0x889A, "HyperSCSI" },
  { 0x88A2, "ATA over Ethernet" },
  { 0x88CC, "Link Layer Discover Protocol" },
  { 0x88E3, "Media Redundancy Protocol" },
  { 0x88E5, "MAC security (802.1AE)" },
  { 0x88F7, "Precision Time Protocol" },
  { 0x0000, NULL }
};

const char *UnknownEtherType = "(no description)";


// Initialized on first touch
static uint16_t lastIndex = 0;


const char * EtherType_findDescription( EtherType target) {

  // Find where the last index of the table is so that we can use it in
  // the binary search.  This happens once per run of the program.
  // lastIndex will be left sitting at the last index of the array, which
  // is one past the last valid value.

  if ( lastIndex == 0 ) {
    while ( EtherTypeTable[lastIndex].desc != NULL ) {
      lastIndex++;
    }
  }


  // Binary search

  uint16_t a = 0, b = lastIndex;

  while ( b >= a ) {

    uint16_t mid = ((b - a) / 2) + a;

    if ( EtherTypeTable[mid].etherType == target ) {
      return  EtherTypeTable[mid].desc;
    }
    else if ( EtherTypeTable[mid].etherType < target ) {
      a = mid + 1;
    }
    else {
      b = mid - 1;
    }

  }
  
  return UnknownEtherType;
}

