/*

   mTCP SNTPLIB.H
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


   Description: A simple class for sending and receiving SNTP messages.
   Most of this was originally part of the SNTP client; I've made a
   stand-alone library to allow other programs to use it.

*/


#ifndef _SNTPLIB_H
#define _SNTPLIB_H


#include CFG_H
#include "ip.h"
#include "udp.h"
#include "sntp.h"



typedef struct {
  UdpPacket_t udpHdr;   // Space for Ethernet, IP and UDP headers.
  NTP_packet_t ntp;     // Actual NTP data.
} NTP_UDP_packet_t;



class SntpLib {

  public:

    typedef struct {
      NTP_packet_t *ntp;
      time_t   currentTime;      // Seconds in time_t format.
      uint32_t currentTimeFrac;  // Fractional seconds in NTP format.
      time_t   targetTime;       // Seconds in time_t format.
      uint32_t targetTimeFrac;   // Fractional seconds in NTP format.
      time_t   diffSecs;         // Difference in seconds
      uint16_t diffMs;           // Difference in milliseconds
    } Callback_data_t;
      

    static int  init( IpAddr_t sntpServerAddr, uint16_t sntpServerPort, void (*f)(Callback_data_t d) );
    static int  sendSNTPRequest( bool blocking, uint32_t *outTime, uint32_t *outTimeFrac );
    static void ntpUdpHandler( const unsigned char *packet, const UdpHeader *udp );

    static char *printTimeStamp( uint32_t ts_p, uint32_t frac, bool localTime );
    static int   setDosDateTime( void );

    static bool replyReceived( void ) { return TargetTimeSecs != 0; }


  private:

    static IpAddr_t SNTPServerAddr;
    static uint16_t SNTPServerPort;
    static uint16_t UdpCallbackPort;
    static void     (*CallbackFunc)(Callback_data_t d);


    // Set by the NTP UDP handler when a response is received.
    static time_t   TargetTimeSecs;   // Seconds, Coordinated Universal Time
    static uint32_t TargetTimeFrac;   // Fractional seconds, NTP format

};


#endif
