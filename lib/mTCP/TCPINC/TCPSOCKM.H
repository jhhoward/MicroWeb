/*

   mTCP TcpSockM.H
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


   Description: TCP socket manager

   Changes:

   2011-05-27: Initial release as open source software

*/




#ifndef _TCPSOCKM_H
#define _TCPSOCKM_H


#include CFG_H
#include "types.h"



// TcpSocketMgr
//
// Dedicated class to manage active and free sockets.  If we handle the
// management of sockets we should be able to save the user from errors.
// Also, by managing a pool ourselves we avoid repeated calls to
// malloc/free which can fragment the DOS heap.
//
// There is only one instance of this so all methods and vars are static.


class TcpSocketMgr {

  public:

    static int8_t init( uint8_t maxSockets );
    static void   stop( void );

    static TcpSocket *getSocket( void );
    static int8_t freeSocket( TcpSocket *target );

    static TcpSocket *accept( void );


    static inline int8_t getSocketsPendingAccept( void ) { return pendingAccepts; }

    // Used by TcpSocket

    static inline void incPendingAccepts( void ) { pendingAccepts++; }

    static inline uint8_t getActiveSockets( void ) { return activeSockets; }
    static inline uint8_t getFreeSockets( void ) { return availSockets; }

    // Put a socket in the Active table.  (Not for end users.)
    static void makeActive( TcpSocket *target );

    // Remove a socket from the Active table.  (Not for end users.)
    // This changes the order of the Active table!
    static void makeInactive( TcpSocket *target );


    // socketTable keeps track of the currently open sockets.
    // We need this so that we can track down an interested
    // socket for an incoming packet.

    static TcpSocket *socketTable[TCP_MAX_SOCKETS];


  private:

    static TcpSocket *availSocketTable[TCP_MAX_SOCKETS];
    static TcpSocket *socketsMemPtr;

    static uint8_t    allocatedSockets;  // Number of sockets created (total)
    static uint8_t    activeSockets;     // Number in active table
    static uint8_t    availSockets;      // Number in free table

    // Number of sockets ready for the user to accept.
    static uint8_t    pendingAccepts;


  public:

    // Does not really belong here.
    static uint16_t   MSS_to_advertise;  // MSS size to advertise to other side.

};


#endif
