
/*

   mTCP Tcp.cpp
   Copyright (C) 2006-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: TCP buffer and TCP protocol handling code

   Changes:

   2011-05-27: Initial release as open source software
   2013-02-17: Improved timeout and retransmit support
   2014-05-18: Code cleanup: get rid of socket level forcePureAck and
               forceProbe flags and replace with TcpBuffer level
               versions; remove dead inUse flag; add two more counters;
               fix off-by-one error on TCP retransmit count

*/




// Tcp buffer management, socket code, packet routing and packet sending
// routines.  This is the heart of the project ... be very very careful.
//
// The consistency checking code is obsolete - I'll redo it another time.




#ifdef __TURBOC__
#include <alloc.h>
#else
#include <malloc.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>

#include "timer.h"
#include "trace.h"
#include "utils.h"

#include "packet.h"
#include "arp.h"
#include "eth.h"
#include "ip.h"

#include "tcp.h"
#include "tcpsockm.h"





// Tcp static vars
//
uint32_t Tcp::Packets_Sent = 0;
uint32_t Tcp::Packets_Received = 0;
uint32_t Tcp::Packets_Retransmitted = 0;
uint32_t Tcp::Packets_SeqOrAckError = 0;
uint32_t Tcp::Packets_DroppedNoSpace = 0;
uint32_t Tcp::OurWindowReopened = 0;
uint32_t Tcp::SentZeroWindowProbe = 0;
uint32_t Tcp::ChecksumErrors = 0;

uint16_t Tcp::Pending_Sent = 0;
uint16_t Tcp::Pending_Outgoing = 0;




void Tcp::dumpStats( FILE *stream ) {
  fprintf( stream, "Tcp: Sent %lu Rcvd %lu Retrans %lu Seq/Ack errs %lu Dropped %lu\n"
		   "     Checksum errs %lu\n",
           Packets_Sent, Packets_Received, Packets_Retransmitted,
           Packets_SeqOrAckError, Packets_DroppedNoSpace, ChecksumErrors );
}



// TcpBuffer static vars
//
TcpBuffer *TcpBuffer::xmitBuffers[TCP_MAX_XMIT_BUFS];
uint8_t    TcpBuffer::freeXmitBuffers;          // Number of buffers in the list
uint8_t    TcpBuffer::allocatedXmitBuffers;     // Total number of buffers allocated
void      *TcpBuffer::xmitBuffersMemPtr = NULL; // Use this to deallocate the memory



int8_t TcpBuffer::init( uint8_t xmitBufs_p ) {

  allocatedXmitBuffers = freeXmitBuffers = 0;

  if ( xmitBufs_p > TCP_MAX_XMIT_BUFS ) {
    TRACE_TCP_WARN(( "Tcp: TcpBuffers parm (%u) too big, limit=%u:\n", xmitBufs_p, TCP_MAX_XMIT_BUFS ));
    return TCP_RC_BAD;
  }

  uint16_t bufSize = sizeof(TcpBuffer)+TcpSocketMgr::MSS_to_advertise;


  // Caution - malloc handles up to 64K at max.  Really should change the
  // tmpSize data type and check for overflow.

  uint16_t tmpSize = xmitBufs_p * bufSize;
  uint8_t *tmp = (uint8_t *)malloc( tmpSize );

  if ( tmp == NULL ) {
    TRACE_TCP_WARN(( "Tcp: Mem alloc err on TcpBuffers\n" ));
    return TCP_RC_BAD;
  }

  xmitBuffersMemPtr = tmp;
  freeXmitBuffers = allocatedXmitBuffers = xmitBufs_p;


  for ( uint8_t i=0; i < allocatedXmitBuffers; i++ ) {

    #if defined(__TINY__) || defined(__SMALL__) || defined(__MEDIUM__)

      // In these memory models the stack and help share the same segment
      // and we only have an offset to data.  It would be nice to normalize
      // our pointers, but since we have only offsets we can't do that.
      // Leave our pointers as is, and let the routines that care worry
      // about it.
      //
      // We could force the use of far pointers, but that has a lot of
      // ripple effect.  Malloc starts toward the low end of the data
      // segment so the chances on a pointer being near the end of the
      // segment are pretty slim anyway.

      xmitBuffers[i] = (TcpBuffer *)(tmp+(i*bufSize));

    #else

      // Normalize each pointer to make the offset as small as possible
      // here so that we don't run into segment wrap problems with
      // LODS, LODSW or related instructions later on as we are
      // computing checksums in IP.CPP.
      //
      // This is slightly expensive, but we only do it once for the life
      // of a buffer pointer.

      uint8_t *t = tmp+(i*bufSize);
      uint16_t seg = FP_SEG( t );
      uint16_t off = FP_OFF( t );
      seg = seg + (off/16);
      off = off & 0x000F;
      xmitBuffers[i] = (TcpBuffer *)MK_FP( seg, off );

    #endif

    xmitBuffers[i]->bufferPool = 1; // Indicate this buffer is pool managed.
  }

  return TCP_RC_GOOD;
}


void TcpBuffer::stop( void ) {
  if ( xmitBuffersMemPtr != NULL ) free( xmitBuffersMemPtr );
}





char *TcpSocket::StateDesc[] = {
  "NA",
  "CLOSED",
  "LISTEN",
  "SYN_SENT",
  "SYN_RECVED",
  "ESTABLISHED",
  "CLOSE_WAIT",
  "LAST_ACK",
  "FIN_WAIT_1",
  "FIN_WAIT_2",
  "CLOSING",
  "TIME_WAIT",
  "FIN_WAIT_1a",
  "CLOSE_WAIT_a",
  "FIN_WAIT_1b"
};




// Socket free list consistency check.
//
// Run through both the active and free lists, and make sure that all
// of our original socket pointers are accounted for.
//
// If a socket doesn't appear at all, the user is holding it or lost it.
// If a socket appears more than once anywhere, you are in deep trouble.

#ifdef CONSISTENCY_CHK
void TcpSocket::cc( void ) {

  if ( allocatedSockets > TCP_MAX_SOCKETS ) {
    TRACE_TCP_WARN(( "Tcp: -CC- allocatedSockets is too big: %d\n",
                 allocatedSockets ));
  }

  if ( (availSockets + activeSockets) > allocatedSockets ) {
    TRACE_TCP_WARN(( "Tcp: -CC- availSockets+activeSockets %d, should be %d\n",
                 (availSockets + activeSockets), allocatedSockets ));
  }

  uint16_t counts[ TCP_MAX_SOCKETS ];
  TcpSocket *tmpTable[ TCP_MAX_SOCKETS ];

  for ( uint8_t i=0; i < allocatedSockets; i++ ) {
    counts[i] = 0;
    tmpTable[i] = &((TcpSocket *)socketsMemPtr)[i];
  }

  for ( i=0; i < availSockets; i++ ) {
    for ( uint8_t j=0; j < allocatedSockets; j++ ) {
      if ( tmpTable[j] == availSocketTable[i] ) {
        counts[j]++;
      }
    }
  }

  uint16_t socketsInPendingAccept = 0;

  for ( i=0; i < activeSockets; i++ ) {
    for ( uint8_t j=0; j < allocatedSockets; j++ ) {
      if ( tmpTable[j] == socketTable[i] ) {
        counts[j]++;
      }
    }
    if ( (socketTable[i]->pendingAccept) && (socketTable[i]->state == TCP_STATE_ESTABLISHED) ) {
      socketsInPendingAccept++;
    }
  }

  if ( socketsInPendingAccept != pendingAccepts ) {
    TRACE_TCP_WARN(( "Tcp: -CC- sockets in pending accept %d > pendingAccepts %d\n",
                 socketsInPendingAccept, pendingAccepts ));
  }


  for ( i=0; i < allocatedSockets; i++ ) {
    if ( counts[i] != 1 ) {
      TRACE_TCP_WARN(( "Tcp: -CC- Socket (%08lx) found %d in lists\n",
                   tmpTable[i], counts[i] ));
    }
  }

  if ( farheapcheck( ) < 0 ) {
    TRACE_TCP_WARN(( "Tcp: -CC- Sockets: heap is corrupted\n" ));
  }

}
#endif






// TcpSocket
//
// The user can always create a socket.  Don't try to add it
// to the active socket table until they try to connect or listen.
// This keeps us from having to report an error from the constructor.
//
// We don't have new with placement but we need it.  Simulate it by
// providing a reinit method, which also has to init any contained
// objects.

TcpSocket::TcpSocket( ) {
  reinit( );
}

void TcpSocket::reinit( ) {

  TRACE_TCP(( "Tcp: (%08lx) Re-init\n", this ));

  // Brutal, but effective.
  memset( this, 0, sizeof( TcpSocket ) );


  // Generate a 32 random number for seqNum.  The random number generator
  // only gives us 16 bits and the high bit is never on, but it is good
  // enough.

  union {
    uint32_t big;
    struct {
      int16_t hi;
      int16_t lo;
    } parts;
  } tmp;

  tmp.parts.hi = rand( );
  tmp.parts.lo = rand( );

  oldestUnackedSeq = seqNum = tmp.big;

  ackNum = 0;
  state = TCP_STATE_CLOSED;
  disableReads = 0;
  pendingAccept = 0;

  lastActivity = TIMER_GET_CURRENT( );
  lastAckRcvd = lastActivity;

  closeReason = 0;

  closeStarted = 0;

  outgoing.init( );
  sent.init( );
  incoming.init( );

  rcvBuffer = NULL;
  rcvBufSize = rcvBufFirst = rcvBufLast = rcvBufEntries = 0;

  // Set to unitialized state
  Eth::copy( cachedMacAddr, Eth::Eth_Broadcast );


  // Retransmit timer data

  SRTT = TCP_MAX_SRTT; // Initial smoothed RTT ( units are clock ticks )
  RTT_deviation = 0;   // Start with no deviation ( units are clock ticks )


  // Experimental: Used to shrink the receive window on bad connections.

  consecutiveGoodPackets = 0;
  consecutiveSeqErrs = 0;
  reportSmallWindow = false;
}


// TcpSocket::setRecvBuffer
//
// The default for a socket is not to have a receive buffer.  If you
// want to use a recv buffer, call this.  Valid buffer sizes are from
// 512 to 16KB.
//
// Call this at most once after creating a socket.  Don't call it
// again, or on a socket created as the result of a listen.  (If you
// needed it set on those, you should have set the parm on the listen
// call.)

int8_t TcpSocket::setRecvBuffer( uint16_t recvBufferSize_p ) {

  if ( recvBufferSize_p == 0 ) {
    // Don't make a recv buffer.
    return TCP_RC_GOOD;
  }

  if ( (recvBufferSize_p < 512) || (recvBufferSize_p > 16384) ) {
    TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Bad recvBufferSize specified: %u\n",
                     this,
                     dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort,
                     recvBufferSize_p ));
    return TCP_RC_BAD;
  }


  rcvBufSize = recvBufferSize_p;

  // The receive buffer is a ring buffer.  Allocate one extra byte
  // so that we don't have to worry about boundary conditions.

  rcvBuffer = (uint8_t *)malloc( rcvBufSize + 1 );
  if ( rcvBuffer == NULL ) {
    // This is kind of bad, but not fatal.  Woe to the user who does
    // not check return codes.
    TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Failed to allocate rcvbuf\n",
                     this, dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort ));
    rcvBufSize = 0;
    return TCP_RC_BAD;
  }

  TRACE_TCP(( "Tcp: (%08lx) Recv buffer set to %u\n", this, rcvBufSize ));


  return TCP_RC_GOOD;
}





// Connect2
//
// For local use only - starts the connection process.
// Other users should use connect or connectNonBlocking

int8_t near TcpSocket::connect2( uint16_t srcPort_p, IpAddr_t host_p, uint16_t dstPort_p ) {

  if ( state != TCP_STATE_CLOSED ) {
    TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Tried to connect with a non CLOSED socket (%s)\n",
                     this,
                     dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort,
                     TcpSocket::StateDesc[state] ));
    return TCP_RC_BAD;
  }

  srcPort = srcPort_p;
  Ip::copy( dstHost, host_p );
  dstPort = dstPort_p;

  TcpSocketMgr::makeActive( this );

  TRACE_TCP(( "Tcp: (%08lx) Connecting to %d.%d.%d.%d:%u from port %u\n",
              this,
              dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort,
              srcPort
  ));


  // First packet is the SYN packet.  Data length of the packet is
  // zero, but 1 will get added to the sequence number in the send code.
  state = TCP_STATE_SYN_SENT;

  connectPacket.pkt.dataLen = 0;


  // No need to check the return code because this socket has no traffic yet
  // and we are well below any MSS limits.
  enqueue( &connectPacket.pkt );


  // Somebody has to get in a loop now to process TCP and ARP packets

  return 0;
}





// Blocking connect.
//
// Use this when you don't care about other sockets blocking while you
// wait for a connection.  If you do care you need to write your own loop.
//
// I'm going to do something gross here.  On the blocking connect if
// ARP resolution isn't complete we'll keep pounding sendPacket, and
// if TRACE is on that creates a flood of bogus trace entries.  If
// we are in that condition throttle ourselves here.  The correct place
// to do it would be on the sendPacket, but I don't want to slow down
// the main path for this nit.

int8_t TcpSocket::connect( uint16_t srcPort_p, IpAddr_t host_p, uint16_t dstPort_p, uint32_t timeoutMs_p ) {

  int rc = connect2( srcPort_p, host_p, dstPort_p );

  if ( rc ) return rc;


  clockTicks_t start;
  clockTicks_t lastCheck;
  start = lastCheck = TIMER_GET_CURRENT( );

  while ( 1 ) {

    PACKET_PROCESS_SINGLE;
    Tcp::drivePackets( );
    Arp::driveArp( );


    // Established is obvious, but Close_Wait is not so obvious.  There is
    // a timing window where they could send data and a FIN bit, pushing us
    // to CLOSE_WAIT very quickly.

    if ( isConnectComplete( ) ) {
      TRACE_TCP(( "Tcp: (%08lx) Connected\n", this ));
      return TCP_RC_GOOD;
    }

    if ( Timer_diff( start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( timeoutMs_p ) ) {
      break;
    }

    // Sleep a little so that we are not spewing TRACE records.
    while ( lastCheck == TIMER_GET_CURRENT( ) ) { };
    lastCheck = TIMER_GET_CURRENT( );
  }

  TRACE_TCP(( "Tcp: (%08lx) Timeout\n", this ));

  // Should we close the socket?

  return TCP_RC_TIMEOUT;
}



// connectNonBlocking
//
// After calling this you are responsible for driving TCP and ARP traffic
// and recogizing when the socket has connected.
//
// Returns  0: Good so far
//         -1: Initial error; don't bother with it

int8_t TcpSocket::connectNonBlocking( uint16_t srcPort_p, IpAddr_t host_p, uint16_t dstPort_p ) {
  return connect2( srcPort_p, host_p, dstPort_p );
}




// Listen
//
// Listen sets up a special socket that listens on a port.
// If a packet comes in on the port that no other socket owns
// the listening socket gets to try to handle it.
//
// If it is a SYN packet, the code creates a new socket to handle
// the hand shaking for the new connection.  Otherwise, it gets
// dropped.
//
// The recvBufferSize parameter is for the new sockets that are created
// as the result of the listen, not for the listening socket.
//
// Note: We do not support listening bound to a specific machine and port!
// Part of the reason is that is rarely used.  If we need it then somebody
// has to fix the recvBufferSize handling ... (It would actually have to
// be allocated.)
//
// Fixme: Cleanup Listening - ensure that we are correct in the paragraph
// above.


#ifdef TCP_LISTEN_CODE

int8_t TcpSocket::listen( uint16_t srcPort_p, uint16_t recvBufferSize ) {

  if ( state != TCP_STATE_CLOSED ) {
    TRACE_TCP_WARN(( "Tcp: (%08lx) Tried to listen on a socket that was in state %s\n",
                     this, TcpSocket::StateDesc[ state ] ));
    return TCP_RC_BAD;
  }

  // Are we listening on this already?
  for ( uint8_t i=0; i < TcpSocketMgr::getActiveSockets( ); i++ ) {
    if ( (TcpSocketMgr::socketTable[i]->state == TCP_STATE_LISTEN) &&
         (TcpSocketMgr::socketTable[i]->srcPort == srcPort_p) ) {
      return TCP_RC_PORT_IN_USE;
    }
  }

  // FIXME: Should this be a consistency check?
  if ( TcpSocketMgr::getActiveSockets( ) == TCP_MAX_SOCKETS ) {
    // Active socket table is full
    return TCP_RC_BAD;
  }

  srcPort = srcPort_p;
  dstHost[0] = dstHost[1] = dstHost[2] = dstHost[3] = 0;
  dstPort = 0;

  TcpSocketMgr::makeActive( this );

  TRACE_TCP(( "Tcp: (%08lx) Listening on port %u\n", this, srcPort ));

  state = TCP_STATE_LISTEN;


  // Make sure this socket doesn't try to read any user data.
  // It is only for handshaking.
  shutdown( TCP_SHUT_RD );

  rcvBufSize = recvBufferSize;

  return TCP_RC_GOOD;
}

#endif



// Fixme: Why have a return code here?
int8_t TcpSocket::shutdown( uint8_t how ) {

  TRACE_TCP(( "Tcp: (%08lx) Shutdown=%d\n", this, how ));

  switch ( how ) {

    case TCP_SHUT_RD: {
      // User is making sure the app doesn't read any more data.
      // Just set a flag to toss data away if it comes in.
      disableReads = 1;
      break;
    }

    case TCP_SHUT_WR: {
      // User is making sure the app doesn't write any more data.
      // This causes a FIN to be sent.
      closeLocal( );
      break;
    }

    case TCP_SHUT_RDWR: {
      // The best of both worlds ..  disallow reads and send a FIN.
      // might as well just close the socket.
      disableReads = 1;
      closeLocal( );
      break;
    }

  }

  return TCP_RC_GOOD;
}




// closeLocal
//
// This initiates a close message, either by sending a new packet with
// the FIN or by piggybacking the FIN on an existing packet.
//
// This should always be safe to call.  At worst you will get a warning
// message that a close has already been initiated.
//
// Note: If you close a socket in LISTEN there might be new sockets that
// are already ESTABLISHED and waiting for you to accept them.  Closing
// the listening socket has no affect on those - don't orphan them!
//
// Note: This does not start the actual close process!  You need to use
// one of the close methods for that.

int8_t near TcpSocket::closeLocal( void ) {

  // Not every state change requires a FIN to be sent.
  uint8_t sendPacket = 0;

  TRACE_TCP(( "Tcp: (%08lx) closeLocal: State was %s\n",
          this, TcpSocket::StateDesc[state] ));

  switch ( state ) {

    case TCP_STATE_CLOSED:
    case TCP_STATE_TIME_WAIT:
    case TCP_STATE_LISTEN:
    case TCP_STATE_SYN_SENT: {
      state = TCP_STATE_CLOSED;
      break;
    }

    case TCP_STATE_SYN_RECVED: {
      state = TCP_STATE_SEND_FIN3; // state = TCP_STATE_FIN_WAIT_1;
      sendPacket = 1;
      break;
    }

    case TCP_STATE_ESTABLISHED: {
      state = TCP_STATE_SEND_FIN1; // state = TCP_STATE_FIN_WAIT_1;
      sendPacket = 1;
      break;
    }

    case TCP_STATE_CLOSE_WAIT: {
      state = TCP_STATE_SEND_FIN2; // state = TCP_STATE_LAST_ACK;
      sendPacket = 1;
      break;
    }

    // By this point we have either acknowledged an incoming FIN or we
    // initiated it and are in the process of shutting down.  Closing
    // again is a user error.

    default: {
      TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Should not close a socket in state %s\n",
                       this,
                       dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort,
                       TcpSocket::StateDesc[state] ));
      break;
    }

  }

  TRACE_TCP(( "     closeLocal: State is now %s\n", TcpSocket::StateDesc[state] ));


  if ( sendPacket ) {

    if ( outgoing.entries == 0 ) {

      TRACE_TCP(( "     Enqueuing standalone FIN pkt\n" ));
      connectPacket.pkt.dataLen = 0;

      // No need to check the return code because we know we have room (outgoing
      // entries is zero) and we are enqueuing a packet with no data.
      enqueue( &connectPacket.pkt );
    }
    else {
      TRACE_TCP(( "     Piggybacking FIN (%d outgoing pkts)\n", outgoing.entries ));
    }
  }

  return TCP_RC_GOOD;
}




// Blocking close.  We'll keep processing packets while waiting, but you
// might cause problems if it takes a long time for the close to timeout.
// Use the nonblocking version if there is a potential problem.

void TcpSocket::close( void ) {

  // Only set closeStarted if it has not been set before.
  // This protects us against a user error where they keep trying to
  // close the socket (and thus reseting the time each time they do).

  if ( closeStarted == 0 ) closeStarted = TIMER_GET_CURRENT( );

  TRACE_TCP(( "Tcp: (%08lx) Close (blocking)\n", this ));

  // Start the process
  closeLocal( );

  // Loop until it goes away

  while ( !isCloseDone( ) ) {

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

  }

}

// closeNonblocking
//
// Starts the close process, but it is up to the caller to ensure
// that the socket actually closes after a reasonable period.
//
// The end user has to:
// - Keep track of when the close was initiated
// - Check to see if the socket goes into TIME_WAIT or CLOSED
// - call destroy if it doesn't after a reasonable time

void TcpSocket::closeNonblocking( void ) {

  // Only set closeStarted if it has not been set before.
  // This protects us against a user error where they keep trying to
  // close the socket (and thus reseting the time each time they do).

  if ( closeStarted == 0 ) closeStarted = TIMER_GET_CURRENT( );

  TRACE_TCP(( "Tcp: (%08lx) Close (nonblocking)\n", this ));

  // Start the process
  closeLocal( );

}


// isCloseDone
//
// Use with closeNonblocking.  Returns
//
// - 0 if the close has not completed
// - 1 if the close has completed

int8_t TcpSocket::isCloseDone( void ) {

  // If it closed naturally, call destroy to cleanup and we are done.
  if ( (state == TCP_STATE_TIME_WAIT) || (state == TCP_STATE_CLOSED) ) {
    destroy( );
    closeReason = 0; // It went peacefully
    return 1;
  }

  // If it has not closed and we are not timed out yet, do nothing.
  if ( Timer_diff( closeStarted, TIMER_GET_CURRENT( ) ) < TIMER_MS_TO_TICKS(TCP_CLOSE_TIMEOUT) ) {
    return 0;
  }

  // It has not closed on its own.  Cut a warning message and destroy it.

  TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Timeout waiting for close, State = %s\n",
                   this, dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort, TcpSocket::StateDesc[state] ));

  destroy( );
  closeReason = 2; // It was forced
  return 1;
}



void near TcpSocket::clearQueues( void ) {

  TcpBuffer *xmitBuffer;

  int tmp = outgoing.entries;
  while ( outgoing.entries ) {
    xmitBuffer = ((TcpBuffer *)outgoing.dequeue( ));
    TcpBuffer::returnXmitBuf( xmitBuffer );
  }
  Tcp::Pending_Outgoing = Tcp::Pending_Outgoing - tmp;

  tmp = sent.entries;
  while ( sent.entries ) {
    xmitBuffer = ((TcpBuffer *)sent.dequeue( ));
    TcpBuffer::returnXmitBuf( xmitBuffer );
  }
  Tcp::Pending_Sent = Tcp::Pending_Sent - tmp;

  while ( incoming.entries ) {
    uint8_t *packet = ((uint8_t *)incoming.dequeue( ));
    Buffer_free( packet );
  }

}



// Method of last resort.  Cleans the queues, deallocates the memory,
// and sets the state to closed.  Calling this should be safe and
// deallocate anything related to this socket.

void TcpSocket::destroy( void ) {

  TRACE_TCP(( "Tcp: (%08lx) Destroy   Final SRTT: (%u, %u)\n", this, SRTT, RTT_deviation ));

  // Clear the queues to a known good state.
  clearQueues( );

  // Move straight to closed state.  One day implement TIME_WAIT.
  state = TCP_STATE_CLOSED;


  // Now would be a good time to deallocate anything related to this
  // socket, including removing from the active table.
  //
  // The user is responsible for putting the socket back on the free list.
  // They might be referencing it, so having it on free list without them
  // doing it explicitly might be bad.

  // Remove from active table
  TcpSocketMgr::makeInactive( this );

  // If a receive buffer was allocated then free it.
  if ( rcvBuffer != NULL ) {
    free( rcvBuffer );
    rcvBuffer = NULL;  // Do this to cover ourselves from double deletes.
  }

  // If this was created by listen and not yet accepted by the user,
  // return it to the free list.
  if ( pendingAccept ) {
    TcpSocketMgr::freeSocket( this );
    pendingAccept = 0; // Do this to cover ourselves.
  }

}



// When we send a SYN we always advertise our MSS.  When we receive
// a SYN we need to note the sender's MSS.  If it is not specified
// use a defalt of 536, which is enough for 536 bytes of data plus
// 20 bytes of TCP header and 20 bytes of IP header.  (Assuming a
// router safe MTU of 576.)
//
// Of course they are always free to send us something up to our
// MSS size, but if they are on a narrow pipe they probably wont.

void near TcpSocket::setMaxEnqueueSize( TcpHeader *tcp ) {

  // Will return the actual MSS sent or 536 if none was sent.
  remoteMSS = TcpHeader::readMSS( tcp );

  // Even if the other side says they can handle more we can't send
  // more because our TcpBuffers are only allocated big enough to
  // handle MSS_to_advertise.

  maxEnqueueSize = remoteMSS;
  if ( TcpSocketMgr::MSS_to_advertise < maxEnqueueSize ) {
    maxEnqueueSize = TcpSocketMgr::MSS_to_advertise;
  }

  TRACE_TCP(( "Tcp: (%08lx) Remote MSS=%u\n", this, remoteMSS ));
}




// Users don't send packets, they enqueue them.
//
// Returns TCP_RC_GOOD if successful
// Returns TCP_RC_BAD if the enqueue fails
// Returns -2 if the packet is too big.

int16_t TcpSocket::enqueue( TcpBuffer *buf ) {

  if ( (state == TCP_STATE_CLOSED || state == TCP_STATE_LISTEN || state == TCP_STATE_CLOSING ) ||
       ((state == TCP_STATE_TIME_WAIT) && (buf != &connectPacket.pkt) ) ) {

    TRACE_TCP_WARN(( "Tcp: (%08lx) Tried to enqueue a packet while in state %s\n",
                 this, TcpSocket::StateDesc[state] ));
    return TCP_RC_BAD;
  }

  if ( buf->dataLen > maxEnqueueSize ) {
    TRACE_TCP_WARN(( "Tcp: (%08lx) Tried to enqueue oversized segment, len=%d\n",
                 this, buf->dataLen ));
    return TCP_RC_TOO_MUCH_DATA;
  }



  #ifdef TCP_OPT_ENQUEUED_ACKS

  // If there is just one packet enqueued so far and it is an empty packet
  // with just ACK set, then discard it and use this packet instead.  This
  // cuts down on the case where we are ACKing a keystroke and then generating
  // the echo.
  //
  // If we are in ESTABLISHED then we know that the packet is purely for ACK
  // purposes and can be replaced.

  if ( outgoing.entries == 1 && state == TCP_STATE_ESTABLISHED ) {

    TcpBuffer *first = (TcpBuffer *)outgoing.peek( );

    if ( first->dataLen == 0 ) {
      TRACE_TCP(( "Tcp: Enqueue: Piggybacking on existing ACK\n" ));
      outgoing.dequeue( );
      TcpBuffer::returnXmitBuf( first );
    }

  }

  #endif
    


  // Update last activity time on the socket
  lastActivity = TIMER_GET_CURRENT( );

  // Do accounting for the buffer
  buf->timeSent = 0;
  buf->attempts = 0;
  buf->pendingArp = 0;
  buf->flags = 0;
  buf->rc = 0;

  // Ringbuffer enqueue returns 0 if good, -1 if bad
  //return outgoing.enqueue( buf );

  int16_t rc = outgoing.enqueue( buf );
  if ( rc == 0 ) {
    Tcp::Pending_Outgoing++;
  };

  return rc;
}




// resendPacket
//
// Assumes ARP resolution is done already, and that we are resending
// because of a dropped packet.
//
// We don't ARP to ensure that the target is still at the same MAC addr.
//

void near TcpSocket::resendPacket( TcpBuffer *buf ) {

  TcpPacket_t* packetPtr = &buf->headers;

  TRACE_TCP(( "Tcp: (%08lx) Resend: Buf=%08lx Seq=%08lx Tries=%u\n",
          this, buf, ntohl( packetPtr->tcp.seqnum ), buf->attempts ));


  // We do redo the IP header because the IP ident field should be changed
  // on a resent packet, which requires a new checksum.  We are definitely
  // not on a performance sensitive path when this happens.

  uint16_t tcpLen = buf->dataLen + packetPtr->tcp.getTcpHlen( );
  packetPtr->ip.set( IP_PROTOCOL_TCP, packetPtr->ip.ip_dest, tcpLen, 0, 0 );

  Packet_send_pkt( packetPtr, buf->packetLen );

  buf->attempts++;
}



// sendPacket
//
// Returns 0 if the packet was sent (no ARP resolution pending)
// Returns 1 if ARP resolution is pending


int8_t near TcpSocket::sendPacket( TcpBuffer *buf ) {

  TRACE_TCP(( "Tcp: (%08lx) Send: State=%s  Buf=%08lx DataLen=%u  RmtWin=%u\n",
              this, TcpSocket::StateDesc[state], buf, buf->dataLen, remoteWindow ));

  // If we have filled this packet in before and it is just pending
  // Arp resolution then retry just that part.

  if ( buf->pendingArp ) {

    buf->pendingArp = buf->headers.ip.setDestEth( buf->headers.eh.dest );

    if ( buf->pendingArp == 0 ) {
      TRACE_TCP(( "     Arp satisfied\n" ));
      Eth::copy( cachedMacAddr, buf->headers.eh.dest );
      Packet_send_pkt( &buf->headers, buf->packetLen );
    }

    return buf->pendingArp;
  }

  Tcp::Packets_Sent++;

  buf->packetLen = sizeof(TcpPacket_t) + buf->dataLen;

  TcpPacket_t* packetPtr = &buf->headers;

  // Fill in the TCP header.

  packetPtr->tcp.src = htons( srcPort );
  packetPtr->tcp.dst = htons( dstPort );


  packetPtr->tcp.seqnum = htonl( seqNum );
  seqNum += buf->dataLen;

  // Another global variable kludge. Instead of sending a pure ack packet
  // which won't get acknowledged, we need to put the sequence number just
  // out of window. That will force an ACK to come back to us.
  if ( buf->isForceProbe( ) ) {
    uint32_t tmp = seqNum-1;
    packetPtr->tcp.seqnum = htonl( tmp );
  }


  packetPtr->tcp.setTcpHlen( 20 );


  packetPtr->tcp.codeBits = TCP_CODEBITS_ACK; // Default is always send ACK


  // Performance: Our normal path is ESTABLISHED so skip the switch
  if ( state != TCP_STATE_ESTABLISHED ) {


  switch ( state ) {

    case TCP_STATE_SYN_RECVED:
    case TCP_STATE_SYN_SENT: {

      if ( state == TCP_STATE_SYN_SENT ) {
        packetPtr->tcp.codeBits = TCP_CODEBITS_SYN;
        packetPtr->tcp.acknum = 0;
      }
      else {
        packetPtr->tcp.codeBits = TCP_CODEBITS_SYN | TCP_CODEBITS_ACK;
      }

      seqNum++;

      // MSS Option
      //
      // This is ugly, but good enough. We are putting the extra TCP option
      // that we need in the data area, but the data area is right after the
      // header so we can claim no data but a longer header.  Which we will
      // do when we send the packet.

      // Bug fix: If the user set a RCVBUFSIZE that is less than the MSS
      // use that instead.

      uint8_t *dataStart = ((uint8_t *)packetPtr) + sizeof( TcpPacket_t );

      uint16_t mss = TcpSocketMgr::MSS_to_advertise;

      // If we are using the RECV buffer interface then MSS should not
      // be bigger than rcvBufSize.
      if ( rcvBufSize ) {
        if ( rcvBufSize < mss ) mss = rcvBufSize;
      }

      dataStart[0] = 0x2; // Option type = MSS
      dataStart[1] = 0x4; // Option len including type and len byte
      dataStart[2] = (mss >> 8 );
      dataStart[3] = (mss & 0xff );

      packetPtr->tcp.setTcpHlen( 24 );
      buf->packetLen += 4;
      break;
    }


    case TCP_STATE_SEND_FIN1:   // ESTABLISHED -> FIN_WAIT_1
    case TCP_STATE_SEND_FIN2:   // CLOSE_WAIT  -> LAST_ACK
    case TCP_STATE_SEND_FIN3: { // SYN_RECVD   -> FIN_WAIT_1

      // Kind of an icky place to stick a check for ForceAckOnly, but here is
      // the reasoning.  We'll never send an ack only packet before we are
      // established, so we don't need to check early or exclude the switch
      // statement entirely.  If we try to force an ack only packet, a FIN bit
      // would be the only thing that could be added that might be undesirable.
      // So in the interest of performance, to the check here where it has
      // the least impact to mainline code.

      if ( (buf->isForceAckOnly( ) == false) && (outgoing.entries == 1) ) {

        TRACE_TCP(( "     Set FIN bit on last packet\n" ));
        packetPtr->tcp.codeBits |= TCP_CODEBITS_FIN;
        seqNum++;

        if ( state == TCP_STATE_SEND_FIN2 ) {
          state = TCP_STATE_LAST_ACK;
        }
        else {
          state = TCP_STATE_FIN_WAIT_1;
        }

      }
      break;

    }

  }

  } // end if !ESTABLISHED
  else {
    if ( buf->dataLen ) packetPtr->tcp.codeBits |= TCP_CODEBITS_PSH;
  }

  // Cache this away for later - makes it easier to determine
  // when to remove the packet from the sent queue.
  buf->seqNum = seqNum-1;


  packetPtr->tcp.acknum = htonl( ackNum );
  if ( (buf->dataLen == 0) && (packetPtr->tcp.codeBits == TCP_CODEBITS_ACK ) ) {
    buf->setWasAckOnly( );
  }

  // Available window size
  uint16_t winSize;
  if ( rcvBufSize ) {
    winSize = rcvBufSize - rcvBufEntries;
  }
  else {
    winSize = (TcpSocketMgr::MSS_to_advertise<<2);
  }


  // Experimental: If this socket is repeatedly stumbling it might be
  // because the other side only sends the packet reported lost, not the
  // next packets after it.  Selective ACK would help that but for now
  // just reduce the window size.

  if ( reportSmallWindow ) {
    winSize = TcpSocketMgr::MSS_to_advertise;
  }


  // Adjust what we think is left on their window
  if ( buf->dataLen <= remoteWindow ) {
    remoteWindow -= buf->dataLen;
  }
  else {
    // Should not have gotten here.  If we did it means that somehow we
    // violated their remote window.  Put a warning or consistency check
    // here.
    TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Sent %u bytes when window was %u\n",
                     this,
                     dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort,
                     buf->dataLen, remoteWindow ));
    remoteWindow = 0;
  }


  packetPtr->tcp.window = htons( winSize );
  packetPtr->tcp.checksum = 0;
  packetPtr->tcp.urgent = 0;


  TRACE_TCP(( "     Seq=%08lx  Ack=%08lx  MyWin=%u\n",
              ntohl(packetPtr->tcp.seqnum), ntohl(packetPtr->tcp.acknum),
              winSize ));

  uint16_t tcpLen = buf->dataLen + packetPtr->tcp.getTcpHlen( );

  // packetPtr->tcp.checksum = Ip::pseudoChecksum( MyIpAddr, dstHost,
  //                             ((uint16_t *)&(packetPtr->tcp)), 6, tcpLen );

  packetPtr->tcp.checksum = ip_p_chksum( MyIpAddr, dstHost,
                                         ((uint16_t *)&(packetPtr->tcp)),
                                         IP_PROTOCOL_TCP, tcpLen );


  // Fill in the IP header
  packetPtr->ip.set( IP_PROTOCOL_TCP, dstHost, tcpLen, 0, 0 );


  // Fill in the Eth header
  packetPtr->eh.setSrc( MyEthAddr );
  packetPtr->eh.setType( 0x0800 );


  if ( !Eth::isSame( cachedMacAddr, Eth::Eth_Broadcast ) ) {
    Eth::copy( buf->headers.eh.dest, cachedMacAddr );
  }
  else {
    buf->pendingArp = buf->headers.ip.setDestEth( buf->headers.eh.dest );
  }

  if ( buf->pendingArp == 0 ) {
    Packet_send_pkt( &buf->headers, buf->packetLen );
  }

  return buf->pendingArp;
}



// sendResetPacket
//
// This sends a one-off reset packet.  The packet may not be associated
// with a socket, so pass in all of the stuff needed to fill in the header.
// We don't attempt to retransmit on an ARP failure; since we received
// the offending packet recently, we should have the hardware address.
// If we don't, too bad.  We don't expect an ACK for a reset packet, so
// we don't have to put it on a retransmit queue.
//
// In short, sending a reset packet is simple.

void near TcpSocket::sendResetPacket( IpHeader *ip, TcpHeader *tcp, uint16_t incomingDataLen ) {

  // Do not respond to reset packets
  if ( tcp->codeBits & TCP_CODEBITS_RST ) return;


  uint32_t seqNum;

  if ( tcp->codeBits & TCP_CODEBITS_ACK ) {
    seqNum = ntohl( tcp->acknum );
  }
  else {
    seqNum = 0;
  }

  uint32_t ackNum = ntohl(tcp->seqnum) + incomingDataLen;

  // If it was a SYN packet, bump the ACK count by 1 just like
  // on a normal SYN packet.
  if ( tcp->codeBits & TCP_CODEBITS_SYN ) {
    ackNum++;
  }


  Tcp::Packets_Sent++;

  TcpPacket_t rp;


  // Fill in the TCP header.
  rp.tcp.src = tcp->dst;
  rp.tcp.dst = tcp->src;

  rp.tcp.seqnum = htonl( seqNum );
  rp.tcp.setTcpHlen( 20 );

  // ACK is on because they sent us the packet first, and it had a seq num.
  rp.tcp.codeBits = TCP_CODEBITS_RST | TCP_CODEBITS_ACK;

  rp.tcp.acknum = htonl( ackNum );

  rp.tcp.window = 0;
  rp.tcp.checksum = 0;
  rp.tcp.urgent = 0;

  // rp.tcp.checksum = Ip::pseudoChecksum( MyIpAddr, ip->ip_src,
  //                     ((uint16_t *)&(rp.tcp)), 6, 20 );

  rp.tcp.checksum = ip_p_chksum( MyIpAddr, ip->ip_src,
                                 ((uint16_t *)&(rp.tcp)),
                                 IP_PROTOCOL_TCP, 20 );


  // Fill in the IP header
  rp.ip.set( IP_PROTOCOL_TCP, ip->ip_src, 20, 0, 0 );


  // Fill in the Eth header
  rp.eh.setSrc( MyEthAddr );
  rp.eh.setType( 0x0800 );

  int8_t pendingArp = rp.ip.setDestEth( rp.eh.dest );
  if ( pendingArp == 0 ) {
    Packet_send_pkt( &rp, sizeof(TcpPacket_t) );
  }

}



// TcpSocket::sendPureAck
//
// Every once in a while we need to blast an empty packet out without queuing
// it.  When we do that we want to make sure that only the ACK flag is set in
// the packet.  We might also want to fudge the sequence number to elicit a
// response from the other side.
//
// This function handles the setup of such a packet.  This is slightly
// dangerous because this packet is allocated on the stack.  We need to ensure
// that it gets sent immediately and not enqueued anywhere because that would
// leave a dangling pointer into the stack.  Calling sendPacket directly
// takes care of that for us.
//
// We're not going to worry about ARP - we should have a hardware address
// already.  It is remotely possible that the target address is not in the ARP
// cache anymore.  In that case we won't be able to send, but the other side
// might timeout and try again.  We will have already sent out an ARP request
// so it might make it next time.
//
// It is possible that we are in a state transition to send a FIN
// bit, so calling sendPacket would cause a FIN bit to go out,
// not just the required ack.  setForceAckOnly takes care of that for us.

void near TcpSocket::sendPureAck( bool forceProbe ) {

  TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Sending %s\n",
                   this,
                   dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort,
                   ( forceProbe ? "probe" : "empty ack") ));

  if ( state != TCP_STATE_ESTABLISHED ) {
    TRACE_TCP_WARN(( "Tcp: (%08lx) Badness: tried to sendPureAck in non-EST state\n", this ));
    return;
  }

  // Allocated on the stack: make sure this does not get queued up
  // anywhere, or we will have a dangling pointer into the stack.

  TcpBuffer ackPacket;

  // Do accounting for the buffer - adapted from enqueue
  ackPacket.timeSent = 0;
  ackPacket.attempts = 0;
  ackPacket.pendingArp = 0;
  ackPacket.flags = 0;
  ackPacket.rc = 0;

  ackPacket.setForceAckOnly( );
  if ( forceProbe ) {
    ackPacket.setForceProbe( );
    Tcp::SentZeroWindowProbe++;
  }

  ackPacket.dataLen = 0;
  sendPacket( &ackPacket );

}




void Tcp::process( uint8_t *packet, IpHeader *ip ) {

  TcpHeader *tcp = (TcpHeader *)(ip->payloadPtr( ));

  uint16_t tcpSrcPort = ntohs(tcp->src);
  uint16_t tcpDstPort = ntohs(tcp->dst);
  uint16_t tcpHdrLen  = tcp->getTcpHlen( );

  uint16_t incomingDataLen = ip->payloadLen( ) - tcpHdrLen;

  #ifndef NOTRACE

  if ( TRACE_ON_TCP ) {

    Trace_tprintf( "Tcp: Src: %d.%d.%d.%d:%u  Dst: %u  Payload Len: %d\n",
                   ip->ip_src[0], ip->ip_src[1], ip->ip_src[2], ip->ip_src[3],
                   tcpSrcPort, tcpDstPort, incomingDataLen );

    if ( TRACE_ON_DUMP ) {
      Utils::dumpBytes( Trace_Stream, (unsigned char *)tcp, tcpHdrLen );
    }

  }

  #endif


  // Check the incoming chksum.

  uint16_t myChksum = ip_p_chksum( ip->ip_src, MyIpAddr,
                                   ((uint16_t *)tcp),
                                   IP_PROTOCOL_TCP,
                                   (incomingDataLen + tcpHdrLen) );

  if ( myChksum ) {
    TRACE_TCP_WARN(( "Tcp: Bad chksum from %d.%d.%d.%d:%u to port %u len: %u\n",
                     ip->ip_src[0], ip->ip_src[1], ip->ip_src[2], ip->ip_src[3],
                     tcpSrcPort, tcpDstPort, incomingDataLen ));
    Tcp::ChecksumErrors++;
    Buffer_free( packet );
    return;
  }



  Packets_Received++;



  // Find the socket this packet belongs to.
  // First scan for active sockets.  Then scan for listening sockets.

  TcpSocket *owningSocket = NULL;


  for ( uint8_t i=0; i < TcpSocketMgr::getActiveSockets( ); i++ ) {

    TcpSocket *tmp = TcpSocketMgr::socketTable[i];

    if ( tmp->state == TCP_STATE_CLOSED ||
         tmp->state == TCP_STATE_TIME_WAIT ) {
      // Optimize this
      continue;
    }
    else {

      if ( (Ip::isSame(ip->ip_src, tmp->dstHost)) &&
           (tcpSrcPort == tmp->dstPort) &&
           (tcpDstPort == tmp->srcPort) )
      {
        owningSocket = tmp;
        break;
      }

    }

  }


  // No match to an existing connected socket.  Scan sockets listening
  // on a port.

  #ifdef TCP_LISTEN_CODE
  if ( owningSocket == NULL ) {

    for ( uint8_t i=0; i < TcpSocketMgr::getActiveSockets( ); i++ ) {
      TcpSocket *tmp = TcpSocketMgr::socketTable[i];
      if ( tmp->state == TCP_STATE_LISTEN ) {
        if ( tcpDstPort == tmp->srcPort ) {
          owningSocket = tmp;
          break;
        }
      }
    }

  }
  #endif


  if ( owningSocket ) {
    process2( packet, ip, tcp, owningSocket );
  }
  else {
    // No owner for this.  Send a reset packet. [Page 36]
    TcpSocket::sendResetPacket( ip, tcp, incomingDataLen );
    TRACE_TCP(( "Tcp: No socket for packet, sent reset\n" ));
    Buffer_free( packet );
  }


}



void near Tcp::process2( uint8_t *packet, IpHeader *ip, TcpHeader *tcp, TcpSocket *socket ) {

  socket->lastActivity = TIMER_GET_CURRENT( );

  uint8_t freePacket = 1;
  uint8_t generatePkt = 0;

  uint16_t incomingDataLen = ip->payloadLen( ) - tcp->getTcpHlen( );

  // What bits are set?  Flags are set to 0 or non-zero.
  uint8_t isUrgSet = tcp->codeBits & TCP_CODEBITS_URG;
  uint8_t isAckSet = tcp->codeBits & TCP_CODEBITS_ACK;
  uint8_t isPshSet = tcp->codeBits & TCP_CODEBITS_PSH;
  uint8_t isRstSet = tcp->codeBits & TCP_CODEBITS_RST;
  uint8_t isSynSet = tcp->codeBits & TCP_CODEBITS_SYN;
  uint8_t isFinSet = tcp->codeBits & TCP_CODEBITS_FIN;

  uint32_t incomingSeqNum = ntohl(tcp->seqnum);
  uint32_t incomingAckNum = ntohl(tcp->acknum);

  uint8_t  incomingAckNumIsCurrent = 0;
  if ( isAckSet ) {
    incomingAckNumIsCurrent = (incomingAckNum == socket->seqNum);
  }

  uint16_t remoteWindow = ntohs( tcp->window );


  #ifndef NOTRACE
  if ( TRACE_ON_TCP || (TRACE_ON_WARN && isRstSet) ) {

    char bits[] = "uaprsf";
    if ( isUrgSet ) bits[0]='U';
    if ( isAckSet ) bits[1]='A';
    if ( isPshSet ) bits[2]='P';
    if ( isRstSet ) bits[3]='R';
    if ( isSynSet ) bits[4]='S';
    if ( isFinSet ) bits[5]='F';

    Trace_tprintf( "Tcp: (%08lx) Src: %d.%d.%d.%d:%u  Dst: %u  Payload Len: %d\n",
                   socket,
                   ip->ip_src[0], ip->ip_src[1], ip->ip_src[2], ip->ip_src[3],
                   ntohs( tcp->src ), ntohs( tcp->dst ), incomingDataLen );

    Trace_tprintf( "     Pkt bits=%s seq=%lx ack=%lx dlen=%u win=%u\n",
                   bits, incomingSeqNum, incomingAckNum,
                   incomingDataLen, remoteWindow );
    Trace_tprintf( "     State=%d, seq=%lx, ack=%lx\n",
                   socket->state, socket->seqNum, socket->ackNum );
  }
  #endif



  // For LISTEN and SYN_SENT the incoming seqNum is undefined, and
  // we are waiting for an incoming SYN to define it.

  #ifdef TCP_LISTEN_CODE
  if ( socket->state == TCP_STATE_LISTEN ) {

    // The only allowable codebit is SYN.  Send RESET on anything else.
    if ( tcp->codeBits == TCP_CODEBITS_SYN ) {
      socket->processSyn( ip, tcp, incomingSeqNum );
    }
    else {
      TcpSocket::sendResetPacket( ip, tcp, incomingDataLen );
      TRACE_TCP(( "     Bad flags for listen; sent reset\n" ));
    }

  } else

  #endif

  // We sent a SYN packet to initiate a connection.
  if ( socket->state == TCP_STATE_SYN_SENT ) {

    if ( isRstSet ) {
      TRACE_TCP(( "     RST received, going to CLOSED\n" ));
      // Page 37: Acceptable if ACK matches the SYN
      if ( incomingAckNumIsCurrent ) {
        // Page 37: Abort and go to closed.
        socket->destroy( );
        socket->closeReason = 1;
      }
    }
    else {

      if ( isSynSet ) {

        if ( isAckSet ) {

          if ( incomingAckNumIsCurrent ) {
            socket->removeSentPackets( incomingAckNum );
            socket->state = TCP_STATE_ESTABLISHED;
            socket->ackNum = incomingSeqNum + 1;
            generatePkt = 1;

            // What was their MSS?
            socket->setMaxEnqueueSize( tcp );

            // New connection - keep track of their window size
            socket->remoteWindow = remoteWindow;

          }
          else {
            // Page 36: Sent reset, stay in the same state.
            TRACE_TCP(( "     Bad ACK for socket in SYN_SENT; sent reset\n" ));
            TcpSocket::sendResetPacket( ip, tcp, incomingDataLen );
          }

        }
        else { // !isAckSet

          TRACE_TCP(( "     Simultaneous active opens; going to SYN_RECVED\n" ));

          // Each side is doing active opens
          //
          // Our original SYN (pun intended) has not been ACKed yet.

          socket->state = TCP_STATE_SYN_RECVED;
          socket->ackNum = incomingSeqNum + 1;

          socket->setMaxEnqueueSize( tcp );

          // We are going to send a new SYN packet with an ACK this time.
          // We want the SEQ num to match the original.  (The send code bumped it.)
          socket->seqNum--;

          // Clear the queues so that we don't try to retransmit the
          // original SYN.
          socket->clearQueues( );

          // Put a new SYN/ACK packet on the outbound queue.  No need to check
          // enqueue return code; it can't fail.
          socket->connectPacket.pkt.dataLen = 0;
          socket->enqueue( &socket->connectPacket.pkt );
        }

      } // isSynSet
      else {
        // SYN is not set.  Send a reset packet, stay in same state [page 36]
        TcpSocket::sendResetPacket( ip, tcp, incomingDataLen );
      }

    } // end else

  }

  else {

    // From this point forward the incoming seqNum has to match
    // what we are expecting.  If it is old, it is a dupe and
    // it should be ignored.  If it is newer than we expect there
    // was a lost packet and they will retransmit eventually.

    uint8_t isIncomingAckProper = 0;
    if ( isAckSet ) {
      TRACE_TCP(( "     Oldest unacked seq=%08lx\n", socket->oldestUnackedSeq ));
      if ( socket->oldestUnackedSeq < socket->seqNum ) {
        // We have not wrapped the 32 bit counter.
        if ( (incomingAckNum >= socket->oldestUnackedSeq) &&
             (incomingAckNum <= socket->seqNum ) )
        {
          isIncomingAckProper = 1;
        }
      }
      else {
        // We have wrapped the 32 bit counter.
        if ( (incomingAckNum >= socket->oldestUnackedSeq) ||
             (incomingAckNum <= socket->seqNum ) )
        {
          isIncomingAckProper = 1;
        }
      }
    }



    uint8_t isIncomingSeqProper = (incomingSeqNum == socket->ackNum);

    if ( socket->state == TCP_STATE_SYN_RECVED ) {
      if ( incomingSeqNum == socket->ackNum || (incomingSeqNum == socket->ackNum-1) ) {
        isIncomingSeqProper = 1;
      }
    }


    if ( isRstSet ) {

      // 2008-10-21: From page 37 of RFC:
      //
      // In all states except SYN-SENT, all reset (RST) segments are
      // validated by checking their SEQ fields.  A reset is valid if
      // its sequence number is in the window. In the SYN-SENT state
      // (a RST received in response to an initial SYN), the RST is
      // acceptable if the ACK field acknowledges the SYN.
      //
      // We are going to simplify and improve security by making
      // the incoming SEQ match exactly what we were expecting.

      if ( isIncomingSeqProper ) {

        // They hit dead on.
        TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Socket received reset in state: %s\n",
                         socket,
                         socket->dstHost[0], socket->dstHost[1],
                         socket->dstHost[2], socket->dstHost[3],
                         socket->dstPort, socket->srcPort,
                         TcpSocket::StateDesc[socket->state] ));

        // Page 37: If is in SYN_RECVED and was in LISTEN prior, go back
        // to LISTEN.  Otherwise, go to closed.
        //
        // Listening sockets (those that use the listen call) can never
        // get into this state, as they always create new sockets before
        // they change to SYN_RECVED.  Therefore, don't bother tracking
        // the original state.

        socket->destroy( );
        socket->closeReason = 1;

      }

    }

    else if ( isIncomingAckProper && isIncomingSeqProper ) {

      // Keep track of the number of good packets we've received.
      // If we are on a good streak then ensure the receive window is
      // not being constricted.

      if ( socket->consecutiveGoodPackets < 255 ) socket->consecutiveGoodPackets++;
      socket->consecutiveSeqErrs = 0;

      if ( socket->consecutiveGoodPackets > 50 ) {
        socket->reportSmallWindow = false;
      }


      // We received something recently so keep us from sending
      // zero window probe packets unnecessarily.
      socket->lastAckRcvd = TIMER_GET_CURRENT( );


      // We can safely remove packets from the sent queue.
      if ( socket->sent.entries ) {
        // Small optimization - don't call unless we know there are
        // packets on the queue.
        socket->removeSentPackets( incomingAckNum );

      }

      // Are all sent packets acked?  If so, then set the remoteWindow
      // size to whatever was in this packet because it is the most
      // up to date.
      //
      // It might also be a pure ack in response to a probe that we
      // sent, so don't assume that we have removed sent packets from
      // the queue.  (ie: don't put this in the if check above.
      if ( socket->sent.entries == 0 ) {
        socket->remoteWindow = remoteWindow;
      }



      // Process data first before possible state changes.  If we get into
      // buffer space trouble we can 'forget' that we saw this packet and
      // play dead, hoping that the other side will retransmit.

      if ( incomingDataLen ) {

        int rc = processPacketData( socket, incomingDataLen, packet, ip, tcp );

        freePacket = (rc & 0x1); // Free packet is the first bit

        if ( (rc & 0x2) == 0 ) {

          // Data was added to the user incoming queue or receive buffer.
          // We need to generate an outgoing ACK packet.

          if ( socket->outgoing.entries == 0 ) {
            // Nothing else to piggyback on, so generate one
            generatePkt = 1;
          }
        }
        else {
          // They want us to play dead!

          // This is ugly; I prefer to fall through, but in this case an
          // early exit is warranted.

          // We are not generating an ACK packet, and we are not preserving
          // this incoming packet.  Just free it and let them retransmitt.
          Buffer_free( packet );
          return;
        }

      }


      // Process state changes

      switch ( socket->state ) {

        case TCP_STATE_ESTABLISHED: {
          if ( isFinSet ) {
            socket->state = TCP_STATE_CLOSE_WAIT;
            socket->ackNum++;
            generatePkt = 1;
          }
          break;
        }


        // TCP_STATE_SEND_FIN1 means we did a local close but have not
        // pushed that packet out yet.  If we get a FIN it looks more like
        // the FIN came before the close, and we still have a FIN packet
        // enqueued to go out.  Move to state TCP_STATE_SEND_FIN2 but don't
        // generate another FIN packet.

        case TCP_STATE_SEND_FIN1: {
          if ( isFinSet ) {
            socket->state = TCP_STATE_SEND_FIN2;
            socket->ackNum++;
          }
          break;
        }



        case TCP_STATE_SYN_RECVED: {
          if ( incomingAckNumIsCurrent ) {
            socket->state = TCP_STATE_ESTABLISHED;
            if ( socket->pendingAccept ) {
              TcpSocketMgr::incPendingAccepts( );
            }
            // New connection; keep track of their window size
            socket->remoteWindow = remoteWindow;

            TRACE_TCP(( "     Socket moved to ESTAB from SYN_RECVED\n" ));
          }
          else {
            // Blow the queues and send a reset
            socket->clearQueues( );
            TcpSocket::sendResetPacket( ip, tcp, incomingDataLen );
          }
          break;
        }

        case TCP_STATE_SEND_FIN3: {
          // User requested a close while we were in SYN_RECVED.
          // When the FIN packet gets driven we're going to FIN_WAIT_1
          break;
        }


        case TCP_STATE_LAST_ACK: {
          socket->state = TCP_STATE_CLOSED;
          break;
        }


        case TCP_STATE_FIN_WAIT_1: {

          if ( isFinSet ) {

            socket->ackNum++;
            generatePkt = 1;

            if ( incomingAckNumIsCurrent ) {
              // Our FIN was acked, and they sent a FIN
              socket->state = TCP_STATE_TIME_WAIT;
            }
            else {
              // Our FIN is not acked yet and they sent a FIN
              socket->state = TCP_STATE_CLOSING;
            }

          }
          else {

            // If they acked our FIN then we can move state.
            if ( incomingAckNumIsCurrent ) {
              socket->state = TCP_STATE_FIN_WAIT_2;
            }

          }

          break;
        }

        case TCP_STATE_FIN_WAIT_2: {
          if ( isFinSet ) {
            socket->state = TCP_STATE_TIME_WAIT;
            socket->ackNum++;
            generatePkt = 1;
          }
          break;
        }

        case TCP_STATE_CLOSING: {
          if ( incomingAckNumIsCurrent ) {
            socket->state = TCP_STATE_TIME_WAIT;
          }
          break;
        }


      } // end Switch

    } // end if incoming seq and ack nums are acceptable.

    else {
      // Error path.
      socket->sendPureAck( );
      Tcp::Packets_SeqOrAckError++;

      // Whoops - sequence error.  If we have had too many then restrict
      // the receiving window.

      socket->consecutiveGoodPackets = 0;
      if ( socket->consecutiveSeqErrs < 255 ) socket->consecutiveSeqErrs++;

      if ( socket->consecutiveSeqErrs > 4 ) {
        socket->reportSmallWindow = true;
        TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Restricting window size\n",
                         socket,
                         socket->dstHost[0], socket->dstHost[1],
                         socket->dstHost[2], socket->dstHost[3],
                         socket->dstPort, socket->srcPort ));

      }

    }

  } // end if states other than LISTEN and SYN_SENT



  if ( generatePkt ) {
    socket->connectPacket.pkt.dataLen = 0;

    // Fixme: check return code because we might not have room.
    // Might be a good place for a warning too.  Not sending
    // Acks is anti-social.
    socket->enqueue( &socket->connectPacket.pkt );
  }


  if ( freePacket ) {
    Buffer_free( packet );
  }

}



// Return code is ugly.  Bit 0 is 'free the incoming packet'.  Bit 1 is
// 'play dead'.  If we don't play dead we want to generate an outgoing
// ACK packet because we need to acknowledge the data that they sent.
// But if we do need to play dead, then make sure nothing gets generated.

int Tcp::processPacketData( TcpSocket *socket, uint16_t incomingDataLen, uint8_t *packet, IpHeader *ip, TcpHeader *tcp ) {

  uint8_t freePacket = 1;
  uint8_t playDead = 0;


  if ( socket->disableReads ) {

    // User called shutdown( SHUT_RD ) so they don't want any
    // more incoming data.  We need to ack the packet, but don't
    // bother trying to deliver it.

    socket->ackNum += incomingDataLen;

    TRACE_TCP(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) State: %s SHUT_RD set, tossing incoming data len %u\n",
                socket,
                socket->dstHost[0], socket->dstHost[1],
                socket->dstHost[2], socket->dstHost[3],
                socket->dstPort, socket->srcPort,
                TcpSocket::StateDesc[socket->state],
                incomingDataLen ));

  } else {

    // Two ways to handle incoming data.
    //
    // Method1: The user gets access to the raw packet and is
    //          responsible for freeing the packet when done.  If the
    //          user is not responsive it will cause the packet driver
    //          to start dropping incoming packets due to lack of
    //          buffers.
    //
    // Method2: The user allocated a ring buffer.  The incoming data
    //          gets copied to the ring buffer and the packet goes
    //          back to the free pool immediately.  This is better
    //          for the packet driver, but costs an extra memcpy.
    //
    // If when using either method there is no room to store the
    // incoming data do not update the ackNum and drop the packet
    // on the floor.  Hopefully the other side will retransmit when
    // there is room on our side.

    // Fixme: If the socket is closing and in a state where we should
    // not be accepting received data, then don't try to receive data.
    // Just drop the packet.

    if ( socket->rcvBuffer == NULL ) {

      // Torture test code - simulate a full buffer 20% of the time
      // if ( socket->incoming.hasRoom( ) && ( (rand( ) % 5) != 0) ) {

      if ( socket->incoming.hasRoom( ) ) {

        socket->ackNum += incomingDataLen;

        // No need to check return code; we know there is room.
        socket->incoming.enqueue( (void *)packet );
        freePacket = 0;

      }
      else {
        TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) State: %s Dropped pkt: no space in ring buf\n",
                         socket,
                         socket->dstHost[0], socket->dstHost[1],
                         socket->dstHost[2], socket->dstHost[3],
                         socket->dstPort, socket->srcPort,
                         TcpSocket::StateDesc[socket->state] ));
        Packets_DroppedNoSpace++;
        playDead = 1;
      }

    }
    else {

      uint8_t *userData = ((uint8_t *)tcp)+tcp->getTcpHlen( );
      int16_t userDataLen = ip->payloadLen( ) - tcp->getTcpHlen( );

      int8_t rc = socket->addToRcvBuf( userData, userDataLen );

      if ( rc == 0 ) {
        socket->ackNum += incomingDataLen;
      }
      else {
        TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) State: %s Dropped pkt: recvBuffer full\n",
                         socket,
                         socket->dstHost[0], socket->dstHost[1],
                         socket->dstHost[2], socket->dstHost[3],
                         socket->dstPort, socket->srcPort,
                         TcpSocket::StateDesc[socket->state] ));

        Packets_DroppedNoSpace++;
        playDead = 1;
      }

    }

  } // end if reads disabled


  return ( freePacket | (playDead<<1) );
}







// Process an incoming SYN packet for a listening socket.
// We assume that all listening sockets are bound only to a port and
// not a specific machine.
//
// In case of failure do nothing.

#ifdef TCP_LISTEN_CODE
void near TcpSocket::processSyn( IpHeader *ip, TcpHeader *tcp, uint32_t incomingSeqNum ) {

  // Allocate a new socket to use.
  TcpSocket *newSocket = TcpSocketMgr::getSocket( );
  if ( newSocket == NULL ) {
    TRACE_TCP_WARN(( "Tcp: (%08lx) Could not allocate socket for incoming SYN\n", this ));
    return;
  }

  // Allocate a recvBuffer.  The recvBuffer size comes from whatever
  // the listening socket has it set for.
  int8_t rc = newSocket->setRecvBuffer( this->rcvBufSize );
  if ( rc ) {
    // Dang again.
    newSocket->destroy( );
    // Setting close reason here, but they'll never see this socket.
    newSocket->closeReason = 3;
    TcpSocketMgr::freeSocket( newSocket );
    TRACE_TCP_WARN(( "Tcp: (%08lx) Failed to alloc recv buf on new socket\n", this ));
    return;
  }


  // Everything is good.  Setup the new socket and send a packet out.
  // Fixme: Good place to add a consistency check

  TcpSocketMgr::makeActive( newSocket );

  newSocket->pendingAccept = 1; // Set only for sockets created here.

  newSocket->srcPort = this->srcPort;
  Ip::copy( newSocket->dstHost, ip->ip_src );
  newSocket->dstPort = ntohs( tcp->src );

  newSocket->state = TCP_STATE_SYN_RECVED;
  newSocket->ackNum = incomingSeqNum + 1;

  newSocket->setMaxEnqueueSize( tcp );

  TRACE_TCP(( "Tcp: (%08lx) New socket for %d.%d.%d.%d:%u, local port: %u\n",
              newSocket, newSocket->dstHost[0], newSocket->dstHost[1],
              newSocket->dstHost[2], newSocket->dstHost[3], newSocket->dstPort,
              newSocket->srcPort ));

  connectPacket.pkt.dataLen = 0;

  // No need to check the return code; it won't fail.
  newSocket->enqueue( &connectPacket.pkt );

}
#endif





// Packets get sent in order.  If we want to remove packets that
// have been acked start at the beginning of the outgoing packet
// queue.  If the packet seqNum + dataLen < targetSeqNum then
// it can be dequeued and thrown away.

// If you are only receiving data and not sending data, there is
// no point to calling this.  The oldestUnackedSeq should be equal
// to the current seqnum.

// If you are sending data the oldestUnackedSeq will be behind the
// seqNum until ACKS come on.  When the last ACK comes in it will
// be set to our current seqNum.

void near TcpSocket::removeSentPackets( uint32_t targetSeqNum ) {

  TRACE_TCP(( "Tcp: (%08lx) Removing sent pkts w/ seqnum < %08lx (%d in queue)\n",
          this, targetSeqNum, sent.entries ));

  while (1) {

    TcpBuffer *p = (TcpBuffer *)sent.peek( );
    if ( p == NULL ) {
      // No packets in the sent queue, so everything has been ACKed.
      oldestUnackedSeq = seqNum;
      break;
    }

    clockTicks_t currentTime = TIMER_GET_CURRENT( );


    // Need to be careful because of wrapping situations
    //
    // 1. No wrap recently - any buffer on the sent queue with a
    //    seqNum < this incoming ACK can be removed.
    //
    // 2. Wrap recently.  Some packets might have very high seqNums
    //    and some will have very low seqnums.
    //
    // |-----------------------------------------------------------|
    //       ^                                ^       ^
    //       |                                |       |
    //     target                          oldest     p-seq (somewhere in this range)
    //                                                ( <= oldest and < target)

    // p->seqNum is the first seqNum of the packet
    // oldestUnackedSeq num is the seqNum in the oldest packet
    //
    // Conditions for removing the packet
    //
    //   seqNum < target
    //   seqNum > target, target < oldest, seq > oldest

    if ( (p->seqNum < targetSeqNum) ||
        ((p->seqNum > targetSeqNum) && (targetSeqNum < oldestUnackedSeq) && (p->seqNum >= oldestUnackedSeq)) )
    {

      // This packet was acked, but possibly combined with the ack of another later packet.
      // Update the RTT and deviation times, and only if there was no retransmit.

      // Avoid using floating point.

      if ( p->attempts == 1 ) {

        uint16_t RTT = Timer_diff( p->timeSent, currentTime );          // Compute RTT for this packet
        SRTT = ((SRTT << 3) + (RTT << 2)) / 10;                         // Compute Smoothed RTT
        uint16_t delta = (SRTT > RTT) ? (SRTT - RTT) : (RTT - SRTT);    // Compute deviation for this packet
        RTT_deviation = ((RTT_deviation << 3) + (delta << 2)) / 10;     // Compute smoothed deviation

        // In a perfect world we are doing this at millisecond resolution.  In our DOS world
        // our normal timer tick is 55ms and our machines might be very slow.  In this world
        // both of these calculations might come out to be zero.  Set a minimum SRTT time of
        // 1 clock tick so that we don't instantly time out packets.

        if ( SRTT == 0 ) SRTT = 1; else if ( SRTT > TCP_MAX_SRTT ) SRTT = TCP_MAX_SRTT;

        TRACE_TCP(( "RTT Stats: (%08lx) RTT: %5u   newSRTT: %5u    Dev: %5u\n", this, RTT, SRTT, RTT_deviation ));
      }

      TRACE_TCP(( "     Removing pkt with seq num + len %08lx, len %u\n", p->seqNum, p->dataLen));

      sent.dequeue( );
      Tcp::Pending_Sent--;
      TcpBuffer::returnXmitBuf( p );
    }
    else {
      // Found first packet still waiting for an ACK.
      oldestUnackedSeq = ntohl(p->headers.tcp.seqnum);
      break;
    }
  }

}




// Used when receiving packets from the network interface and a ring
// buffer is in use on the socket.

int8_t near TcpSocket::addToRcvBuf( uint8_t *data, uint16_t dataLen ) {

  // Enough room?
  if ( dataLen > (rcvBufSize - rcvBufEntries) ) {
    return TCP_RC_BAD;
  }

  TRACE_TCP(( "Tcp: (%08lx) Add: RcvBufEntries=%u, Adding %u\n",
          this, rcvBufEntries, dataLen ));

  rcvBufEntries += dataLen;


  if ( (dataLen + rcvBufLast) < rcvBufSize ) {

    uint8_t *target = rcvBuffer+rcvBufLast;
    // trixterCpy( target, data, dataLen );

    memcpy( target, data, dataLen );


    // One contiguous copy
    // memcpy( rcvBuffer+rcvBufLast, data, dataLen );

    rcvBufLast += dataLen;
  }
  else {
    // Two copies because we wrapped over the end
    uint16_t firstCpyLen = rcvBufSize - rcvBufLast;

    uint8_t *target = rcvBuffer+rcvBufLast;
    // trixterCpy( target, data, firstCpyLen );

    memcpy( target, data, firstCpyLen );

    //memcpy( rcvBuffer+rcvBufLast, data, firstCpyLen );

    uint16_t secondCpyLen = dataLen - firstCpyLen;

    uint8_t *src = data+firstCpyLen;
    target = rcvBuffer;

    // trixterCpy( target, src, secondCpyLen );
    memcpy( target, src, secondCpyLen );

    // memcpy( rcvBuffer, data+firstCpyLen, secondCpyLen );
    rcvBufLast = secondCpyLen;
  }

  return TCP_RC_GOOD;

}



// recv
//
// Returns the number of bytes read or an error code.
// Errors are negative numbers.

int16_t TcpSocket::recv( uint8_t *userBuf, uint16_t userBufLen ) {

  uint16_t origWin = rcvBufSize - rcvBufEntries;


  // This used to be more restrictive, but it's possible to have data queued up
  // that we never processed even after a connection has moved to closed.  So
  // only balk if a connection is truly closed or not established.

  if ( state < TCP_STATE_ESTABLISHED ) {

    TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Tried recv in state %s\n",
                     this,
                     dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort,
                     TcpSocket::StateDesc[state] ));
    return TCP_RC_RECV_BAD_STATE;
  }


  if ( (rcvBufEntries == 0) || (userBufLen == 0) ) return 0;

  uint16_t cpyLen = userBufLen;
  if ( rcvBufEntries < cpyLen ) {
    cpyLen = rcvBufEntries;
  }

  TRACE_TCP(( "Tcp: (%08lx) Recv: RcvBufEntries=%u, removing %u\n",
          this, rcvBufEntries, cpyLen ));


  rcvBufEntries -= cpyLen;

  if ( (cpyLen + rcvBufFirst) < rcvBufSize ) {
    memcpy( userBuf, rcvBuffer + rcvBufFirst, cpyLen );
    rcvBufFirst += cpyLen;
  } else {
    uint16_t firstCpyLen = rcvBufSize - rcvBufFirst;
    memcpy( userBuf, rcvBuffer + rcvBufFirst, firstCpyLen );
    uint16_t secondCpyLen = cpyLen - firstCpyLen;
    memcpy( userBuf+firstCpyLen, rcvBuffer, secondCpyLen );
    rcvBufFirst = secondCpyLen;
  }


  // Zero window processing
  //
  // If our TCP receive window was closed before consuming this data then
  // send an ACK packet to the other side to let them know we are open for
  // business again.

  if ( origWin == 0 ) {
    sendPureAck( );
    Tcp::OurWindowReopened++;
  }

  return cpyLen;

}



// send
//
// This is safe to call even with no data.



int16_t TcpSocket::send( uint8_t *userBuf, uint16_t userBufLen ) {

  if ( state != TCP_STATE_ESTABLISHED ) {
    TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) Tried to send a packet while in %s\n",
                     this,
                     dstHost[0], dstHost[1], dstHost[2], dstHost[3], dstPort, srcPort,
                     TcpSocket::StateDesc[state] ));
    return TCP_RC_BAD;
  }


  uint16_t bytesSent = 0;

  while ( bytesSent < userBufLen ) {

    if ( !outgoing.hasRoom( ) ) break;

    TcpBuffer *tmp = TcpBuffer::getXmitBuf( );
    if ( tmp == NULL ) break;

    uint16_t cpyLen = maxEnqueueSize;
    if ( userBufLen - bytesSent < cpyLen ) cpyLen = userBufLen - bytesSent;

    uint8_t *dataStart = ((uint8_t *)tmp) + sizeof( TcpBuffer );
    memcpy( dataStart, userBuf + bytesSent, cpyLen );


    tmp->dataLen = cpyLen;

    // No need to check the return code.  We know there is room and we
    // are not adding more than the MSS.
    enqueue( tmp );

    bytesSent += cpyLen;
  }

  return bytesSent;
}




void Tcp::drivePackets2( void ) {

  clockTicks_t currentTicks, elapsedTicks;

  for ( uint8_t i = 0; i < TcpSocketMgr::getActiveSockets( ); i++ ) {

    TcpSocket *socket = TcpSocketMgr::socketTable[i];

    if ( socket->sent.entries ) {

      // Check the oldest packet.  If it is not overdue, then none of the other
      // sent packets are overdue yet either.

      TcpBuffer *sentPacket = (TcpBuffer *)socket->sent.peek( );

      currentTicks = TIMER_GET_CURRENT( );

      if ( currentTicks > sentPacket->overdueAt ) {

        if ( sentPacket->attempts > TCP_RETRANS_COUNT ) {

          TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) State: %s Too many retries (%u) on packet (SEQ=%08lx, ACK=%08lx)\n",
                           socket,
                           socket->dstHost[0], socket->dstHost[1],
                           socket->dstHost[2], socket->dstHost[3],
                           socket->dstPort, socket->srcPort,
                           TcpSocket::StateDesc[socket->state],
                           sentPacket->attempts,
                           ntohl(sentPacket->headers.tcp.seqnum),
                           ntohl(sentPacket->headers.tcp.acknum) ));

          socket->destroy( );
          socket->closeReason = 4;

          // Destroy might have rearranged the order of sockets in the
          // table.  Just exit to avoid problems.  Unfinished work will
          // be picked up the next time this is called.
          return;

        }


        // We are going to retransmit.  Double our SRTT value (up to a reasonable point.)
        // This has the effect of doubling our timeout for the next packet.
        //
        // Notice the + 2 on the overdueAt calculation?  Without it, the PCjr was sending
        // out duplicate packets agressively.  Adding 1 tick helped, and adding 2 ticks
        // made that problem go away.  A timer tick is 55ms which is pretty gross.  We
        // were probably right at the edge of a tick, saw the new time, and decided that
        // packets were overdue then they really were not.
        //
        // A more elegant solution would be to have a higher resolution timer and a bigger
        // constant, which would make things more granular.
        //
        // This only affected SPDTEST when sending.  In the real world it was probably not
        // causing any problems.

        socket->SRTT = socket->SRTT << 1;
        if ( socket->SRTT > TCP_MAX_SRTT ) socket->SRTT = TCP_MAX_SRTT;

        sentPacket->timeSent = currentTicks;
        sentPacket->overdueAt = currentTicks + (socket->SRTT + (socket->RTT_deviation<<2)) + 2;

        Packets_Retransmitted++;

        TRACE_TCP_WARN(( "Tcp: (%08lx) (%d.%d.%d.%d:%u %u) State: %s Retrans: Tries: %u  SEQ=%08lx  ACK=%08lx  SRTT (%u, %u)\n",
                         socket,
                         socket->dstHost[0], socket->dstHost[1],
                         socket->dstHost[2], socket->dstHost[3],
                         socket->dstPort, socket->srcPort,
                         TcpSocket::StateDesc[socket->state],
                         sentPacket->attempts,
                         ntohl(sentPacket->headers.tcp.seqnum),
                         ntohl(sentPacket->headers.tcp.acknum),
                         socket->SRTT, socket->RTT_deviation ));

        // Resend packet just blasts the packet out. If there was a MAC
        // addr change we won't pick it up.  Fix this.
        socket->resendPacket( sentPacket );


        // Retransmitted a packet ... no point in doing anything else
        // on this socket.
        continue;

      }

    } // end if there are packets waiting for ACKs


    // If there are packets waiting to be sent then try to send them.

    while ( socket->outgoing.entries && socket->sent.hasRoom( ) ) {

      TcpBuffer *pendingPacket = (TcpBuffer *)socket->outgoing.peek( );


      // Is the remote window big enough to send a packet?

      if ( pendingPacket->dataLen > socket->remoteWindow ) {

        // Remote window is not big enough.  Send a probe.  The probe will
        // have a purposefully wrong sequence number that will elicit an
        // ACK packet.
        //
        // This is probably a bug - we should not be using lastAckRcvd for this.

        currentTicks = TIMER_GET_CURRENT( );
        clockTicks_t elapsedTicks = Timer_diff( socket->lastAckRcvd, currentTicks );

        if ( elapsedTicks > TIMER_MS_TO_TICKS(TCP_PROBE_INTERVAL) ) {
          socket->lastAckRcvd = currentTicks;
          socket->sendPureAck( true );
        }

        // Whether we sent a probe or not, don't send any more data.
        break;
      }



      int8_t rc = socket->sendPacket( pendingPacket );

      if ( rc == 0 ) {

        // This is the first sending of this packet

        pendingPacket->attempts++;
        pendingPacket->timeSent = TIMER_GET_CURRENT( );
        pendingPacket->overdueAt = pendingPacket->timeSent + (socket->SRTT + (socket->RTT_deviation<<2)) + 2;
        socket->outgoing.dequeue( );
        Pending_Outgoing--;

        // Only put real packets on the sent queue.  We don't care if
        // a packet sent purely for Acking gets acked.
        if ( pendingPacket->wasAckOnly( ) == false ) {
          // No need to check the return code; we know it has room.
          socket->sent.enqueue( pendingPacket );
          Pending_Sent++;
        }
        else {
          TcpBuffer::returnXmitBuf( pendingPacket );
        }
      }
      else {
        // Pending Arp resolution.  If this is stuck everything
        // behind it is stuck as well.
        break;
      }

    } // end while drive outgoing packets

  }


}



uint16_t near TcpHeader::readMSS( TcpHeader *tcp ) {

  uint16_t rc = 536;

  uint8_t hlen = tcp->getTcpHlen( );

  if ( hlen != sizeof( TcpHeader ) ) {

    uint8_t *userData = ((uint8_t *)tcp)+tcp->getTcpHlen( );

    // Find our option.
    uint8_t *optionsStart = ((uint8_t *)tcp)+sizeof( TcpHeader );

    while ( 1 ) {

      if (optionsStart == userData ) break;

      if ( *optionsStart == 0 ) {
        // End of list
        break;
      }
      else if ( *optionsStart == 1 ) {
        // No-op
        optionsStart++;
      }
      else if ( *optionsStart == 2 ) {
        // MSS.  Len byte is always 4
        rc = *(optionsStart+2);
        rc = (rc<<8) + *(optionsStart+3);
        optionsStart +=4;
      }
      else {
        // Unknown or don't care
        optionsStart += *(optionsStart+1);
      }
    }

  }

  return rc;
}
