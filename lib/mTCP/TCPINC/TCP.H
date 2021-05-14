/*

   mTCP Tcp.H
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


   Description: TCP data structures and functions

   Changes:

   2012-04-29: Add flushRecv inline
   2011-05-27: Initial release as open source software
   2013-02-17: Improved timeout and retransmit support
   2013-03-28: Add a method to see if a socket has waiting data
   2014-05-18: Code cleanup: get rid of socket level forcePureAck and
               forceProbe flags and replace with TcpBuffer level
               versions; remove dead inUse flag; add two more counters;
               add static asserts for configuration options

*/



#ifndef _TCP_H
#define _TCP_H



// Load and check the configuration options

#include CFG_H
#include "types.h"

static_assert( TCP_MAX_SOCKETS > 0 );
static_assert( TCP_MAX_SOCKETS <= 64 );        // Sockets are around 210 bytes
static_assert( TCP_MAX_XMIT_BUFS > 0 );
static_assert( TCP_MAX_XMIT_BUFS <= 40 );      // Limited by malloc
static_assert( TCP_CLOSE_TIMEOUT >= 5000ul );



// Continue with other includes

#include "eth.h"
#include "ip.h"
#include "ringbuf.h"



// Configuration items that applications really should not be setting

#define TCP_MAX_SRTT           (181)     // 10 seconds in clock ticks
#define TCP_RETRANS_COUNT       (10)     // How many attempts per packet
#define TCP_PA_TIMEOUT       (10000ul)   // Pending accept timeout
#define TCP_PROBE_INTERVAL    (1000ul)   // Time between zero window probes




// TCP return codes

#define TCP_RC_GOOD             (  0 )
#define TCP_RC_BAD              ( -1 )
#define TCP_RC_NO_XMIT_BUFFERS  ( -2 )
#define TCP_RC_TIMEOUT          ( -3 )
#define TCP_RC_PORT_IN_USE      ( -4 )
#define TCP_RC_TOO_MUCH_DATA    ( -5 )
#define TCP_RC_RECV_BAD_STATE   ( -6 )
#define TCP_RC_NOT_SUPPORTED    ( -7 )


// TcpHeader
//
// TcpHeader is the actual packet header layout of a Tcp packet.
// This rides in an Ip packet.


// TcpHeader defines
//
#define TCP_CODEBITS_URG 0x20
#define TCP_CODEBITS_ACK 0x10
#define TCP_CODEBITS_PSH 0x08
#define TCP_CODEBITS_RST 0x04
#define TCP_CODEBITS_SYN 0x02
#define TCP_CODEBITS_FIN 0x01


class TcpHeader {

  public:

    uint16_t src;
    uint16_t dst;
    uint32_t seqnum;
    uint32_t acknum;
    uint8_t  hlenBits;  // Use the methods!
    uint8_t  codeBits;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;

    inline void setTcpHlen( uint16_t bytes ) {
      hlenBits = (bytes>>2) << 4;
    };

    inline uint8_t getTcpHlen( void ) const {
      return (hlenBits>>2);
    };

    static uint16_t near readMSS( TcpHeader *tcp );

};



// TcpPacket
//
// TcpPacket defines a minimal sized Tcp/Ip packet including room
// for the Ethernet header.
//
// The EthHeader field is a fixed length.  The IpHeader and TcpHeader
// fields can vary, so you can't use this data structure unless you know
// you are sending with no Ip or Tcp options.  (You can wiggle a bit on
// the Tcp options by treating them as user data, but not the Ip options.)
//
// User data follows after TcpHeader.

typedef struct {
  EthHeader eh;
  IpHeader  ip;
  TcpHeader tcp;
} TcpPacket_t;



// TcpBuffer
//
// This data structure consists of a TcpPacket and book-keeping
// fields.  The book keeping fields allow us to manage things like
// retransmits, detecting when the packet was acked, etc.
//
// A pool of TcpBuffers is allocated at initialization time.  It is
// expected that users will use the pool buffers, which get automatically
// put back into the pool when the protocol stack is done with them.  The
// pool buffers are all maximum size.
//
// It is possible for a user to allocate their own buffers and use them
// along with these buffers.  Bizarre, but possible.  (Maybe you don't
// want maximum sized packets.)  In that case go ahead, but leave the
// bufferPool flag set at zero so that we don't try to manage them with
// the free list.
//
// Idle buffers live on the free list.  A user gets a buffer by calling
// the getXmitBuf method, which takes it off the free list.  The user
// can then fill in the data and enqueue the buffer.  When the stack is
// done with the buffer, it will put the buffer back on the free list.
// If for some reason the user does not enqueue the buffer, they need to
// use returnXmitBuf to put it back on the free list.

class TcpBuffer {

  public:

    // The user only needs to fill in dataLen.  Everything else is managed
    // by the code.

    uint32_t     seqNum;      // SeqNum+Len-1, determines when to free pkt
    uint16_t     dataLen;     // User data length: Filled in by user
    uint16_t     packetLen;   // Packet length, including headers and pad
    clockTicks_t timeSent;    // Last time that we tried to send.
    clockTicks_t overdueAt;   // Timestamp after which we need to try again
    uint8_t      attempts;    // Number of send attempts after ARP is resolved
    uint8_t      pendingArp;  // Are we just waiting for arp?
    uint8_t      rc;          // Final result code
    uint8_t      bufferPool;  // Is this part of the pool for a socket?
    uint16_t     flags;       // Misc flags; see getters/setters below
    TcpPacket_t  headers;     // Start of the actual packet data.


    // If a packet was sent and it had no data and only an ACK flag, it is
    // an ACK only or pure ACK packet.  We need to know when this happens
    // so that we don't put it on the sent queue for a socket and wait for
    // the other side to ack it.  (It won't.)

    inline bool wasAckOnly( void )        { return ((flags & 0x01) == 0x01); }
    inline void setWasAckOnly( void )     { flags = flags | 0x01; }


    // These two flags are only set inside sendPureAck.  ForceAckOnly will
    // ensure that no other flag bits get set on the packet.  This is used
    // when we get a packet with a bad sequence or acknowledgement number.
    // ForceProbe will cause us to set a slightly invalid sequence number
    // which should elicit an ACK packet from the other side.  We do not need
    // clear methods because these flags get reset every time this particular
    // magic packet is used.

    inline bool isForceAckOnly( void )    { return ((flags & 0x80) == 0x80); }
    inline void setForceAckOnly( void )   { flags = flags | 0x80; }

    inline bool isForceProbe( void )      { return ((flags & 0x40) == 0x40); }
    inline void setForceProbe( void )     { flags = flags | 0x40; }


    // Buffer pool management

    static int8_t init( uint8_t xmitBufs_p );
    static void   stop( void );

    static TcpBuffer *xmitBuffers[TCP_MAX_XMIT_BUFS];
    static uint8_t    freeXmitBuffers;       // Number of buffers in the list
    static uint8_t    allocatedXmitBuffers;  // Total number of buffers allocated
    static void      *xmitBuffersMemPtr;     // Use this to deallocate the memory


    static inline TcpBuffer *getXmitBuf( void ) {
      if (freeXmitBuffers) {
        freeXmitBuffers--;
        return xmitBuffers[freeXmitBuffers];
      }
      else { return NULL; };
    }


    static inline void returnXmitBuf( TcpBuffer *tmp ) {
      // Defend against returning a buffer that doesn't belong to the pool.
      if ( tmp->bufferPool ) {
        xmitBuffers[freeXmitBuffers] = tmp;
        freeXmitBuffers++;
      }
    }

};




// TcpSocket states
//
// Note that we treat TIME_WAIT as CLOSED.  We do this because we don't
// implement the 2MSL wait time.  If we ever choose to implement it later
// we can fix the constant.

#define TCP_STATE_CLOSED      1
#define TCP_STATE_LISTEN      2
#define TCP_STATE_SYN_SENT    3
#define TCP_STATE_SYN_RECVED  4
#define TCP_STATE_ESTABLISHED 5
#define TCP_STATE_CLOSE_WAIT  6
#define TCP_STATE_LAST_ACK    7
#define TCP_STATE_FIN_WAIT_1  8
#define TCP_STATE_FIN_WAIT_2  9
#define TCP_STATE_CLOSING    10
#define TCP_STATE_TIME_WAIT  11

// Not offical states, but needed.  These tell us we need to send
// a FIN packet, then transition to the next state.
#define TCP_STATE_SEND_FIN1  12  // ESTABLISHED -> FIN_WAIT_1
#define TCP_STATE_SEND_FIN2  13  // CLOSE_WAIT  -> LAST_ACK
#define TCP_STATE_SEND_FIN3  14  // SYN_RECVD   -> FIN_WAIT_1



// Defines for use with the socket shutdown() call
//
#define TCP_SHUT_RD   0
#define TCP_SHUT_WR   1
#define TCP_SHUT_RDWR 2




// TcpSocket
//
// This class keeps track of the state of one socket connection.

class TcpSocket {

  friend class Tcp;
  friend class TcpSocketMgr;

  public:

    uint16_t srcPort;          // Local port number
    IpAddr_t dstHost;          // Destination IP address
    uint16_t dstPort;          // Destination port number

    uint32_t seqNum;           // Next sequence number to send.
    uint32_t ackNum;           // Other guys' ack number

    uint32_t oldestUnackedSeq; // Used for knowing when to clear Sent ringbuf

    clockTicks_t lastActivity; // Last time we sent or received something (in ticks)

    clockTicks_t lastAckRcvd;  // Last time we received a good ACK packet.
                               // Used to determine if we need to send a
                               // probe packet.

    clockTicks_t closeStarted; // Used to tell when a close has gone too long.
                               // Protect against multiple closes by initing to 0
                               // and only setting to a value if it is 0.

    uint8_t  state;            // One of the TCP Socket states above
    uint8_t  disableReads;     // Set if user calls shutdown( shut_rd )
    uint8_t  pendingAccept;    // Is this created by Listen and still pending accept?

    uint8_t  closeReason;      // Under what circumstances did this close
                               //  0 is normal close
                               //  1 is RST recvd
                               //  2 is forced
                               //  3 is fail setrecvbuf after listen
                               //  4 is forced after retry failures

    uint16_t remoteMSS;        // MSS for the remote end
    uint16_t maxEnqueueSize;   // Maximum user data len that can be enqueued
    uint16_t remoteWindow;     // Last reported remote window size

    RingBuffer outgoing;       // TCPBuffers enqueued for send
    RingBuffer sent;           // TCPBuffers sent and awaiting ACKs
    RingBuffer incoming;       // Raw incoming packets from the wire

    // Minimal packet used for initial connections, sending ACKs
    // and sending the final FIN packet.
    //
    // This is so much easier than trying to deal with the buffer pool
    // when it runs out of buffers.
    struct {
      TcpBuffer pkt;
      uint8_t   data[4];
    } connectPacket;

    // Receive buffer management
    uint8_t  *rcvBuffer;
    uint16_t  rcvBufFirst;
    uint16_t  rcvBufLast;
    uint16_t  rcvBufEntries;

    // For normal sockets this is the receive buffer size.
    // For listening sockets this is the size to create new
    // sockets with.
    uint16_t  rcvBufSize;


    // Performance hack - cache the MAC address of our target so that we
    // don't have to look it up on every packet send.
    //
    // Not quite sure what to do in case of an error.  If the other side
    // changes it's MAC addr the connection is probably dead anyway, so
    // don't worry about it.

    EthAddr_t cachedMacAddr; // If this is equal to broadcast it is not set.


    // Retransmit data

    uint16_t SRTT;           // Smoothed round trip time ( units are clock ticks )
    uint16_t RTT_deviation;  // Deviation ( units are clock ticks )


    // Experimental: used to shrink the receive window on bad connections.
    uint8_t  consecutiveGoodPackets;
    uint8_t  consecutiveSeqErrs;
    bool     reportSmallWindow;
    bool     padding01;

  public:

    // User Interface

    TcpSocket( );

    int8_t setRecvBuffer( uint16_t recvBufferSize );

    int8_t connect( uint16_t srcPort_p, IpAddr_t host_p, uint16_t dstPort_p, uint32_t timeoutMs_p );
    int8_t connectNonBlocking( uint16_t srcPort_p, IpAddr_t host_p, uint16_t dstPort_p );
    int8_t isConnectComplete( void ) { return ( (state == TCP_STATE_ESTABLISHED) || (state == TCP_STATE_CLOSE_WAIT) ); };

    #ifdef TCP_LISTEN_CODE
    int8_t listen( uint16_t srcPort_p, uint16_t recvBufferSize );
    #endif

    int8_t shutdown( uint8_t how );

    void   close( void );
    void   closeNonblocking( void );
    int8_t isCloseDone( void );


    inline int8_t isEstablished( void ) { return state == TCP_STATE_ESTABLISHED; };
    inline int8_t isClosed( void ) { return state == TCP_STATE_CLOSED; }


    // This returns true whenever we are in a state where the other side
    // has sent a FIN or we are already closd.
    inline int8_t isRemoteClosed( void ) {
      return state == TCP_STATE_CLOSED     ||
             state == TCP_STATE_CLOSE_WAIT ||
             state == TCP_STATE_LAST_ACK   ||
             state == TCP_STATE_SEND_FIN2  || // SEND_FIN2 equiv to CLOSE_WAIT
             state == TCP_STATE_CLOSING    ||
             state == TCP_STATE_TIME_WAIT;
    }

    inline uint8_t getCloseReason( void ) { return closeReason; }

    // Negative return codes are bad.
    int16_t recv( uint8_t *userBuf, uint16_t userBufLen );
    int16_t send( uint8_t *userBuf, uint16_t userBufLen );

    inline void flushRecv( void ) { rcvBufFirst = rcvBufLast = rcvBufEntries = 0; }


    inline bool recvDataWaiting( void ) {
      if ( (incoming.entries > 0) || (rcvBufEntries > 0) ) return true; else return false;
    }

    inline bool outgoingQueueIsFull( void ) { return !outgoing.hasRoom( ); }

    // Almost should be private: alternative to send
    int16_t enqueue( TcpBuffer *buf );    // Send data

    void   reinit( );


  private:

    void   near clearQueues( void );

    // Called from TcpSockM - might want to leave this as not 'near'
    void        destroy( void );

    int8_t near connect2( uint16_t srcPort_p, IpAddr_t host_p, uint16_t dstPort_p );

    int8_t near closeLocal( void );

    void   near setMaxEnqueueSize( TcpHeader *tcp );

    int8_t near sendPacket( TcpBuffer *buf );
    void   near resendPacket( TcpBuffer *buf );
    void   near sendPureAck( bool forceProbe = false );

    void   near processSyn( IpHeader *ip, TcpHeader *tcp, uint32_t incomingSeqNum );

    void   near removeSentPackets( uint32_t targetSeqNum );
    int8_t near addToRcvBuf( uint8_t *data, uint16_t dataLen );


    static void near sendResetPacket( IpHeader *ip, TcpHeader *tcp, uint16_t incomingDataLen );


  public:

    static char *StateDesc[];

};




class Tcp {

  public:

    static void process( uint8_t *packet, IpHeader *ip );
    static void drivePackets2( void );

    static inline void drivePackets( void ) {
      if ( Pending_Sent || Pending_Outgoing ) { drivePackets2( ); }
    }

    static void dumpStats( FILE *stream );

    static uint32_t Packets_Sent;
    static uint32_t Packets_Received;
    static uint32_t Packets_Retransmitted;
    static uint32_t Packets_SeqOrAckError;
    static uint32_t Packets_DroppedNoSpace;
    static uint32_t ChecksumErrors;
    static uint32_t OurWindowReopened;
    static uint32_t SentZeroWindowProbe;

    static uint16_t Pending_Sent;
    static uint16_t Pending_Outgoing;


  private:

    static void near process2( uint8_t *packet, IpHeader *ip, TcpHeader *tcp, TcpSocket *socket );
    static int processPacketData( TcpSocket *socket, uint16_t incomingDataLen, uint8_t *packet, IpHeader *ip, TcpHeader *tcp );


};



#endif



