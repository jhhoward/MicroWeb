
/*

   mTCP TcpSockM.cpp
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


   Description: TCP socket manager code

   Changes:

   2011-05-27: Initial release as open source software

*/




// TcpSocketManager
//
// Routines to manage the active list and free list of sockets.  Routines
// include:
//
// - list setup
// - allocate and deallocate from the free list
// - 'accept' syscall to formally give a new socket created by a listening
//   socket to the user.




#if defined ( __WATCOMC__ ) || defined ( __WATCOM_CPLUSPLUS__ )
#include <malloc.h>
#else
#include <alloc.h>
#endif
#include <string.h>

#include "timer.h"
#include "trace.h"
#include "utils.h"

#include "tcp.h"
#include "tcpsockm.h"




// Tables
//
// Do not write code that depends on the position of a socket
// in these arrays.  When a socket is removed the last socket on
// the list gets moved down to take its place so there are no
// holes in the table.
//
// If you are processing the table and you remove an entry, give
// up and start again because you changed the order of things.
//
// Todo: Add a high water mark for the number of active sockets?

TcpSocket *TcpSocketMgr::socketTable[TCP_MAX_SOCKETS];
TcpSocket *TcpSocketMgr::availSocketTable[TCP_MAX_SOCKETS];

// Pointer to the original chunk of memory that we allocated.
// We need to keep it around for when we eventually free the memory.
TcpSocket *TcpSocketMgr::socketsMemPtr = NULL;

uint8_t    TcpSocketMgr::activeSockets;
uint8_t    TcpSocketMgr::availSockets;
uint8_t    TcpSocketMgr::allocatedSockets;
uint8_t    TcpSocketMgr::pendingAccepts;

// Unrelated, but here for lack of a TcpSocket::init method.
uint16_t   TcpSocketMgr::MSS_to_advertise;



int8_t TcpSocketMgr::init( uint8_t maxSockets ) {

  if ( (maxSockets == 0) || (maxSockets > TCP_MAX_SOCKETS) ) {
    TRACE_TCP_WARN(( "Tcp: Bad maxSocket parm on init: %u\n", maxSockets ));
    return TCP_RC_BAD;
  }

  // Allocate memory for socket data structures
  TcpSocket *socketsMemPtr = (TcpSocket *)malloc( maxSockets * sizeof( TcpSocket ) );
  if ( socketsMemPtr == NULL ) {
    allocatedSockets = 0;
    TRACE_TCP_WARN(( "Tcp: Mem alloc err creating socket pool\n" ));
    return TCP_RC_BAD;
  }

  allocatedSockets = maxSockets;

  for ( uint8_t i=0; i < allocatedSockets; i++ ) {
    availSocketTable[i] = &socketsMemPtr[i];
  }

  availSockets   = allocatedSockets;
  activeSockets  = 0;
  pendingAccepts = 0;

  MSS_to_advertise = MyMTU - (sizeof(IpHeader) + sizeof(TcpHeader));

  TRACE_TCP(( "Tcp: Allocated %u sockets, MTU is %u, My MSS is %u\n",
	  allocatedSockets, MyMTU, MSS_to_advertise ));

  return TCP_RC_GOOD;
};




void TcpSocketMgr::stop( void ) {
  // The user is responsible for closing and draining sockets properly.
  // We are just here to deallocate the memory that we used.
  if ( socketsMemPtr != NULL ) { free( socketsMemPtr ); }
}



TcpSocket *TcpSocketMgr::getSocket( void ) {

  if ( availSockets == 0 ) {
    TRACE_TCP_WARN(( "Tcp: No free sockets\n" ));
    return NULL;
  }

  availSockets--;
  TcpSocket *rc = availSocketTable[availSockets];
  rc->reinit( );

  TRACE_TCP(( "Tcp: (%08lx) Socket from free list\n", rc ));

  return rc;
}

int8_t TcpSocketMgr::freeSocket( TcpSocket *target ) {

  // Todo: Consistency check to ensure they didn't return more than once.

  if ( availSockets == allocatedSockets ) {
    TRACE_TCP_WARN(( "Tcp: Really bad - Too many sockets on the free list.\n" ));
    return TCP_RC_BAD;
  }

  TRACE_TCP(( "Tcp: (%08lx) Socket returned to free list\n", target ));

  availSocketTable[availSockets] = target;
  availSockets++;

  return TCP_RC_GOOD;
}


// accept
//
// If a listening socket resulted in a new socket you need to 'accept'
// the new socket.
//
// The socket is already in established state - the user just doesn't
// have a pointer to it.  Calling this routine gives you the first
// socket in the active list that was created as the result of a listening
// socket, which is not necessarily the order it was established in.
// You may have to call this a few times to get all of the new sockets.
//
// Before the accept is done the socket is live, and will receive data.
// But eventually it will have to drop packets when it runs out of buffer
// space.  So don't wait too long between checking for new sockets.
//
// Call this periodically to see if new sockets are available and to
// clear out ones that didn't quite make it.

TcpSocket *TcpSocketMgr::accept( void ) {

  // Do some maintenance while we are here

  // If the socket is pendingAccept and is in anything other than
  // established state for longer than a few seconds, wipe it out.
  //
  // Ha!  They could have sent something small and a FIN bit, putting
  // us on the way to closing before we got accepted!  Modify this
  // code to only cleanup sockets that have not REACHED established.

  for ( uint8_t i=0; i < activeSockets; i++ ) {

    if ( (socketTable[i]->pendingAccept) && (socketTable[i]->state < TCP_STATE_ESTABLISHED) ) {

      if ( Timer_diff( socketTable[i]->lastActivity, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS(TCP_PA_TIMEOUT) ) {

	TRACE_TCP_WARN(( "Tcp: (%08lx) Was pending accept, timed out\n", socketTable[i] ));

	// Probably should attempt a close first, then a destroy, but
	// I'm not wasting the extra code on it.

	socketTable[i]->destroy( );

	// Just changed the order of the socket table ..  break out.
	break;
      }

    }

  }

  TcpSocket *rc = NULL;

  // Force them to do maintenance, but save them a little time here.  We
  // really don't need a pendingAccepts count, but it might save the table
  // scan.
  if ( pendingAccepts ) {

    for ( uint8_t i=0; i < activeSockets; i++ ) {
      if ( (socketTable[i]->pendingAccept) && (socketTable[i]->state >= TCP_STATE_ESTABLISHED) ) {
	rc = socketTable[i];
	socketTable[i]->pendingAccept = 0;
	pendingAccepts--;

        rc->lastActivity = TIMER_GET_CURRENT( );

	TRACE_TCP(( "Tcp: (%08lx) Accepted new socket, pendingAccepts=%u\n",
		    rc, pendingAccepts ));
	break;
      }
    }

  }

  return rc;
}





// Put this socket in the active list.  The caller already allocated the
// data structure - we are not just indicating that we are open for
// business.

void TcpSocketMgr::makeActive( TcpSocket *target ) {

  // Make sure we don't have it already.

  uint8_t found = 0;

  for ( uint8_t i=0; i < getActiveSockets( ); i++ ) {
    if ( socketTable[i] == target ) {
      found = 1;
      break;
    }
  }

  if ( found == 0 ) {
    socketTable[activeSockets] = target;
    activeSockets++;
  }
  else {
    TRACE_TCP_WARN(( "Tcp: (%08lx) Tried to make a socket active twice\n", target ));
  }
}


// Remove this socket from the active list.  The caller still has to
// return the socket data structure to the free list when done.  This
// just keeps the TCP code from trying to work with it.

void TcpSocketMgr::makeInactive( TcpSocket *target ) {

  uint8_t found = 0;

  uint8_t i;
  for ( i=0; i < getActiveSockets( ); i++ ) {
    if ( socketTable[i] == target ) {
      found = 1;
      break;
    }
  }

  /*
     Faster, but does not preserve ordering.
     Apparently ordering is important for web clients.

  if ( found ) {
    activeSockets--;
    socketTable[i] = socketTable[activeSockets];
  }
  */

  // Slide the remaining sockets down to fill the hole.

  if ( found ) {
    activeSockets--;
    for ( uint8_t j = i; j < getActiveSockets( ); j++ ) {
      socketTable[j] = socketTable[j+1];
    }
  }

}




