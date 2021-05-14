
/*

   mTCP FtpCl.cpp
   Copyright (C) 2009-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Ftp server connected client code

   Changes:

   2011-05-27: Initial release as open source software

*/




#include <stdarg.h>
#include <time.h>

#include "ftpcl.h"
#include "ftpusr.h"
#include "trace.h"

FtpClient *FtpClient::activeClientsTable[ FTP_MAX_CLIENTS ];
FtpClient *FtpClient::freeClientsTable[ FTP_MAX_CLIENTS ];

uint16_t FtpClient::activeClients = 0;
uint16_t FtpClient::freeClients = 0;
uint16_t FtpClient::allocatedClients = 0;


extern IpAddr_t Pasv_IpAddr;
extern uint16_t Filebuffer_Size;



static char ErrMsg_ClientOutputBufferOverflow[] = "Ftp (%lu) Client output buffer overflowed\n";


// Allocate and initialize Ftp clients.  By the end of this we will have
// all clients roughly initialized and on the free list.  At the session
// start there will be further initialize to be done.
//
// If this fails the program should not be allowed to continue.

int FtpClient::initClients( uint16_t clients_p ) {

  if ( clients_p > FTP_MAX_CLIENTS ) return -1;

  allocatedClients = clients_p;

  for ( uint16_t i=0; i < clients_p; i++ ) {

    // Allocate
    FtpClient *newClient = (FtpClient *)malloc( sizeof(FtpClient) );
    if ( newClient == NULL ) {
      return -1;
    }

    memset( newClient, 0, sizeof(FtpClient) );

    // Initialize
    strcpy( newClient->eyeCatcher, "FtpClient" );

    newClient->fileBuffer = (uint8_t *)malloc( Filebuffer_Size );
    if ( newClient->fileBuffer == NULL ) {
      return -1;
    }

    newClient->outputBuffer = (char *)malloc( OUTPUTBUFFER_SIZE );
    if ( newClient->outputBuffer == NULL ) {
      return -1;
    }

    // Add to free list
    returnFreeClient( newClient );
  }

  return 0;
}


FtpClient *FtpClient::getFreeClient( void ) {

  if ( freeClients == 0 ) {
    TRACE_WARN(( "getFreeClient: no free client available\n" ));
    return NULL;
  }

  freeClients--;
  return freeClientsTable[ freeClients ];

}

void FtpClient::returnFreeClient( FtpClient *client ) {

  if ( freeClients >= allocatedClients ) {
    TRACE_WARN(( "getFreeClient: tried to return too many clients to free list\n" ));
    return;
  }

  freeClientsTable[ freeClients ] = client;
  freeClients++;
}



void FtpClient::addToActiveList( FtpClient *client ) {

  if ( activeClients >= allocatedClients ) {
    TRACE_WARN(( "addToActiveList: tried to add too many clients to active list\n" ));
    return;
  }

  activeClientsTable[ activeClients ] = client;
  activeClients++;
}

void FtpClient::removeFromActiveList( FtpClient *client ) {

  uint16_t index;
  for ( index = 0; index < activeClients; index++ ) {
    if ( activeClientsTable[ index ] == client ) {
      break;
    }
  }

  if ( index == activeClients ) {
    TRACE_WARN(( "removeFromActiveList: tried to remove a client from the active list that wasnt there\n" ));
    return;
  }

  // index points to the one to remove.  The caller needs to
  // move it to the free list.
  activeClients--;
  activeClientsTable[ index ] = activeClientsTable[ activeClients ];
}


// FtpClient::startNewSession can assume the following:
//
// - The filebuffer is already created.
//
// Everything else should be explicitly set.


void FtpClient::startNewSession( TcpSocket *newSocket, uint32_t sessionId_p ) {

  #ifdef CCC
    checkClients( );

    // If we did everything right during cleanup cs, ds, and ls are NULL
    if ( cs != NULL || ds != NULL || ls != NULL ) {
      TRACE_WARN(( "Ftp Allocating new client: Expected nulls, found cs=%p, ds=%p, ls=%p\n", cs, ds, ls ));
    }
  #endif


  state = UserPrompt;
  sessionId = sessionId_p;

  cs = newSocket;

  user.userName[0] = 0;

  loginAttempts = 0;

  cwd[0] = 0;
  dataTarget[0] = 0; dataTarget[1] = 0; dataTarget[2] = 0; dataTarget[3] = 0;
  dataPort = 0;
  pasvPort = 0;

  // Too earlier to know what the real FTPROOT is for this user.  They are not
  // logged in yet.  It will be set at login.
  ftproot[0] = 0;
  cwd[0]='/'; cwd[1] = 0;

  inputBufferIndex = 0;
  eatUntilNextCrLf = 0;

  outputBufferLen = 0;
  outputBufferIndex = 0;

  dataXferState = DL_NotActive;
  dataXferType = NoDataXfer;

  asciiMode = 0;
  fileBufferIndex = 0;
  bytesRead = bytesToRead = 0;

  statCmdActive = 0;


  // Is this client on our same subnet?
  //
  // If local, PASV responses always use our IP address.  If not local and
  // the admin specified a different addresses for PASV responses, set that
  // up.

  uint32_t clientIpAddr_u = *(uint32_t *)(&cs->dstHost);

  if ( (MyIpAddr_u & Netmask_u) == (clientIpAddr_u & Netmask_u) ) {
    isLocalSubnet = 1;
    Ip::copy(pasvAddr, MyIpAddr);
  }
  else {
    isLocalSubnet = 0;
    Ip::copy(pasvAddr, Pasv_IpAddr );
  }

    
  time( &startTime );

  TRACE(( "Ftp (%lu) New connection from %d.%d.%d.%d:%u, cs=%p\n", sessionId,
    cs->dstHost[0], cs->dstHost[1],
    cs->dstHost[2], cs->dstHost[3],
    cs->dstPort, cs ));


  // Add to active list now that it is initialized
  addToActiveList( this );

  return;
}



// cleanupSession gets called when all of the sockets are closed.  It
// needs to cleanup the data structures and return the sockets to
// the free pool.
//
// outputBuffer and fileBuffer get reused.

void FtpClient::cleanupSession( void ) {

  // Dirlist cleanup (just in case)
  _dos_findclose( &fileinfo );

  if ( file != NULL ) {
    fclose( file );
    file = NULL;
  }

  state = Closed;

  // Return all of the sockets to the pool
  if ( cs ) { TcpSocketMgr::freeSocket( cs ); cs = NULL; }
  if ( ds ) { TcpSocketMgr::freeSocket( ds ); ds = NULL; }
  if ( ls ) { TcpSocketMgr::freeSocket( ls ); ls = NULL; }

}



void FtpClient::addToOutput_var( char *fmt, ... ) {

  int bytesAvail = OUTPUTBUFFER_SIZE - outputBufferLen;

  va_list ap;
  va_start( ap, fmt );
  int rc = vsnprintf( outputBuffer + outputBufferLen, bytesAvail, fmt, ap );
  va_end( ap );

  if ( (rc<0) || (rc>=bytesAvail) ) {
    outputBuffer[OUTPUTBUFFER_SIZE-1] = 0;
    TRACE_WARN(( ErrMsg_ClientOutputBufferOverflow, sessionId ));
    outputBufferLen = OUTPUTBUFFER_SIZE;
  }
  else {
    outputBufferLen += rc;
  }


}

void FtpClient::addToOutput( char *str ) {

  int bytesAvail = (OUTPUTBUFFER_SIZE - outputBufferLen) - 1;

  int bytesToCopy = strlen(str);
  if ( bytesToCopy > bytesAvail ) {
    bytesToCopy = bytesAvail;
    TRACE_WARN(( ErrMsg_ClientOutputBufferOverflow, sessionId ));
  }

  memcpy( outputBuffer + outputBufferLen, str, bytesToCopy );
  outputBufferLen += bytesToCopy;

  outputBuffer[outputBufferLen] = 0;
}



void FtpClient::sendOutput( void ) {

  int bytesToSend = outputBufferLen - outputBufferIndex;

  // Fixme: return code!
   int bytesSent = cs->send( (uint8_t *)(outputBuffer + outputBufferIndex), bytesToSend );
  // int bytesSent = cs->send( (uint8_t *)(outputBuffer + outputBufferIndex), 1 );

  if ( bytesSent >= 0 ) {
    outputBufferIndex += bytesSent;

    if ( bytesSent == bytesToSend ) {
      // Great, everything is pushed out
      outputBufferLen = 0;
      outputBufferIndex = 0;
    }

  }
  else {
    TRACE_WARN(( "Ftp (%lu) Error sending on cs\n", sessionId ));
    // Error on the send ..  probably need to kill the socket
  }

}






#ifdef CCC

static void FtpClient::checkClients( void ) {

  if ( _heapchk( ) != _HEAPOK ) {
    TRACE_WARN(( "checkClient: heap is corrupted\n" ));
  }

  if ( (activeClients + freeClients) != allocatedClients ) {
    TRACE_WARN(( "checkClient: Number of active and free clients doesn't add up: Active: %u  Free: %u\n",
                 activeClients, freeClients ));
  }

  // Check the active clients
  for ( uint16_t i = 0; i < activeClients; i++ ) {

    FtpClient *client = activeClientsTable[ i ];

    if ( strcmp( client->eyeCatcher, "FtpClient_t" ) != 0 ) {
      TRACE_WARN(( "checkClient: Slot(%u) eyeCatcher corrupted\n", i ));
    }

    if ( client->state > Closing ) {
      TRACE_WARN(( "checkClient: Slot(%u) state is inconsistent\n", i ));
    }

    if ( client->fileBuffer == NULL ) {
      TRACE_WARN(( "checkClient: Slot(%u) fileBuffer is NULL\n", i ));
    }

    if ( client->fileBufferIndex > Filebuffer_Size ) {
      TRACE_WARN(( "checkClient: Slot(%u) fileBufferIndex too big: %u\n", i, client->fileBufferIndex ));
    }

    if ( client->dataXferState > DL_Closing ) {
      TRACE_WARN(( "checkClient: Slot(%u) dataXferState is inconsistent\n", i ));
    }

  }

}
#endif

