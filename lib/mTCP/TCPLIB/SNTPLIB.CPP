/*

   mTCP SNTPLIB.CPP
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


#include <string.h>
#include <time.h>

#include "sntplib.h"

#include "timer.h"
#include "trace.h"
#include "utils.h"
#include "arp.h"
#include "packet.h"



// Class variables

static IpAddr_t SntpLib::SNTPServerAddr = { 0, 0, 0, 0 };  // Fixme.
static uint16_t SntpLib::SNTPServerPort;
static uint16_t SntpLib::UdpCallbackPort;

static void (*SntpLib::CallbackFunc)(Callback_data_t d);

// Set by the NTP UDP handler when a response is received.
static time_t   SntpLib::TargetTimeSecs = 0;   // Seconds, Coordinated Universal Time
static uint32_t SntpLib::TargetTimeFrac = 0;   // Fractional seconds, NTP format




int SntpLib::init( IpAddr_t sntpServerAddr, uint16_t sntpServerPort,
                   void (*f)(Callback_data_t d)) {

  Ip::copy( SNTPServerAddr, sntpServerAddr );
  SNTPServerPort = sntpServerPort;

  UdpCallbackPort = Udp::getUnusedCallbackPort( );
  Udp::registerCallback( UdpCallbackPort, ntpUdpHandler );

  CallbackFunc = f;

  return 0;
}



#ifdef SNTPLIB_TIMESTAMP_FUNC
static char printTimeStampBuffer[40];

char * SntpLib::printTimeStamp( uint32_t ts_p, uint32_t frac, bool localTime ) {

  // This uses NTP format, so if you print a DOS timestamp you have to
  // convert it to the NTP format first.

  time_t ts = ts_p;

  struct tm tmbuf;

  if ( localTime ) {
    _localtime( &ts, &tmbuf );
  }
  else {
    _gmtime( &ts, &tmbuf );
  }

  sprintf( printTimeStampBuffer, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
          tmbuf.tm_year+1900, tmbuf.tm_mon+1, tmbuf.tm_mday,
          tmbuf.tm_hour, tmbuf.tm_min, tmbuf.tm_sec, (frac / 1048) >> 12 );

  return printTimeStampBuffer;

}
#endif


int SntpLib::setDosDateTime( void ) {

  struct dosdate_t dos_date;
  struct dostime_t dos_time;


  time_t ts1 = TargetTimeSecs;

  struct tm tmbuf;
  _localtime( &ts1, &tmbuf );

  dos_date.year = tmbuf.tm_year + 1900;
  dos_date.month = tmbuf.tm_mon+1;
  dos_date.day = tmbuf.tm_mday;

  int rc1 = _dos_setdate( &dos_date );

  dos_time.hour = tmbuf.tm_hour;
  dos_time.minute = tmbuf.tm_min;
  dos_time.second = tmbuf.tm_sec;
  dos_time.hsecond = uint32_t((TargetTimeFrac / 10480)) >> 12; // NTP fractional to 1/100ths

  int rc2 = _dos_settime( &dos_time );

  if (rc1 || rc2 ) return 1;

  return 0;
}




// sendSNTPRequest
//
// Returns  0 if the request was sent,
//         -1 if UDP has a hard error
//         -2 if ARP times out.

static NTP_UDP_packet_t OutgoingRequest;

int SntpLib::sendSNTPRequest( bool blocking, uint32_t *outTime, uint32_t *outTimeFrac ) {

  // Setup outgoing packet
  memset( &OutgoingRequest, 0, sizeof(OutgoingRequest) );

  // Wipe out TargetTimeSecs and TargetTimeFrac to indicate that
  // we are waiting for a reply.

  TargetTimeSecs = TargetTimeFrac = 0;

  // Get the current time.  We use time() to get Coordinated Universal Time
  // and _dos_gettime to get the fraction of a second.  They are both based
  // on the same time, just the timezone offset is different between them.
  // (_dos_gettime is local time.)

  struct dostime_t dos_time;
  _dos_gettime( &dos_time );

  OutgoingRequest.ntp.leapIndicator = 3;  // Unknown or unset
  OutgoingRequest.ntp.version = 3;        // Version = 3
  OutgoingRequest.ntp.mode = 3;           // Mode = Client

  OutgoingRequest.ntp.transTimeSecs = time( NULL );
  OutgoingRequest.ntp.transTimeFrac = (uint32_t(dos_time.hsecond) * 10480) << 12;

  *outTime = OutgoingRequest.ntp.transTimeSecs;
  *outTimeFrac = OutgoingRequest.ntp.transTimeFrac;

  OutgoingRequest.ntp.transTimeSecs = htonl( OutgoingRequest.ntp.transTimeSecs +  NTP_OFFSET );
  OutgoingRequest.ntp.transTimeFrac = htonl( OutgoingRequest.ntp.transTimeFrac );

  uint16_t reqLen = sizeof( OutgoingRequest.ntp );

  int rc = Udp::sendUdp( SNTPServerAddr, UdpCallbackPort, SNTPServerPort, reqLen,
                         (uint8_t *)&OutgoingRequest, 1 );

  // Hard error from UDP.
  if ( rc == -1 ) return -1;

  if ( blocking == false ) return 0;

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  // Spin and process packets until we can resolve ARP and send our request
  while ( rc == 1 ) {

    if ( Timer_diff( startTime, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 2000 ) ) {
      TRACE_WARN(( "Sntp: Arp timeout sending request\n" ));
      return -2;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );

    rc = Udp::sendUdp( SNTPServerAddr, UdpCallbackPort, SNTPServerPort, reqLen,
                       (uint8_t *)&OutgoingRequest, 1 );

    // Hard error from UDP
    if ( rc == -1 ) return -1;

  }

  return 0;

}



void SntpLib::ntpUdpHandler( const unsigned char *packet, const UdpHeader *udp ) {

  NTP_packet_t *ntp = &(((NTP_UDP_packet_t *)(packet))->ntp);

  // Quick sanity check

  if ( (ntohs(udp->src) == SNTPServerPort) && (ntohs(udp->dst) == UdpCallbackPort) && ( (ntp->mode & 0x7) == 4 ) ) {

    // What we are doing here is taking the transmit timestamp from the
    // server and using it directly, which is wrong.  We are supposed
    // to generate a new local timestamp (Destination Timestamp) and then
    // compute a few deltas to arrive at the adjustment that we need.
    //
    // This is DOS. We are going to use what they gave us directly since
    // we know the standard timer resolution on the machine is at best
    // 55ms.

    TargetTimeSecs = ntohl( ntp->transTimeSecs ) - NTP_OFFSET;
    TargetTimeFrac = ntohl( ntp->transTimeFrac );


    // Calculate the difference.  As a failsafe do not update the time if
    // there is more than a 10 minute difference.

    // Difference calculation. 

    // Convert NTP fraction to milliseconds
    uint16_t targetFrac = uint16_t((TargetTimeFrac / 1048) >> 12);


    struct dostime_t dos_time;
    _dos_gettime( &dos_time );

    time_t   currentTime = time( NULL );
    uint16_t currentFrac = dos_time.hsecond * 10;

    time_t   diffSecs;
    uint16_t diffFrac = 0;

    if ( currentTime > TargetTimeSecs ) {

      diffSecs = currentTime - TargetTimeSecs;

      if ( currentFrac > targetFrac ) {
        diffFrac = currentFrac - targetFrac;
      } else {
        diffSecs--;
        diffFrac = (1000 + currentFrac) - targetFrac;
      }

    } else if ( TargetTimeSecs > currentTime ) {

      diffSecs = TargetTimeSecs - currentTime;

      if ( targetFrac > currentFrac ) {
        diffFrac = targetFrac - currentFrac;
      } else {
        diffSecs--;
        diffFrac = (1000 + targetFrac) - currentFrac;
      }

    } else {

      diffSecs = 0;

      if ( currentFrac > targetFrac ) {
        diffFrac = currentFrac - targetFrac;
      } else {
        diffFrac = targetFrac - currentFrac;
      }

    }

    TRACE(("SNTPLib: Reponse from server, difference is %lu.%03u seconds\n", diffSecs, diffFrac ));

    if ( CallbackFunc != NULL ) {

      Callback_data_t d;
      d.ntp = ntp;
      d.currentTime = currentTime;
      d.currentTimeFrac = (uint32_t(currentFrac) * 1048) << 12; // Convert MS to NTP format.
      d.targetTime = TargetTimeSecs;
      d.targetTimeFrac = TargetTimeFrac;
      d.diffSecs = diffSecs;
      d.diffMs = diffFrac;

      CallbackFunc(d);
    }

  }

  // We are done processing this packet.  Remove it from the front of
  // the queue and put it back on the free list.
  Buffer_free( packet );

}
