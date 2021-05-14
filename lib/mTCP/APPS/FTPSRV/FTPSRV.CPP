/*

   mTCP FtpSrv.cpp
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


   Description: FTP Server

   Changes:

   2011-05-27: Initial release as open source software
   2011-10-01: Major rework of directory hierarchy (move to Unix style
               paths for everything, including drive letters.);
               Scan for valid drive letters at startup; allow users
               with MKD permission to create subdirectories and upload
               to any path under their incoming directory; add MOTD
               feature; redo local console user interface (split screen
               with status line); fix MDTM bug, change to use gmtime;
               add config parameters for client TCP recv buffer size,
               client filebuffer size, and number of packets to process
               during each trip through the main loop; change compile
               options slightly to shrink exe size; enhance error
               detection on password file; add login time field to 
               client sessions; add console "show users" and "stats"
               commands; fix lost data sockets bug on error paths
   2012-12-23: Ignore UNIX style options for /bin/ls on LIST and NLST
   2013-02-11: Enable SIZE command; have it throw an error in ASCII
               mode to avoid crushing the machine.
   2013-03-30: 132 column support/awareness; minor UI changes
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.

*/



// RFC  765 - File Transfer Protocol specification
// RFC 1579 - Firewall-Friendly FTP


#include <bios.h>
#include <ctype.h>
#include <direct.h>
#include <dos.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "types.h"

#include "timer.h"
#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"
#include "tcpsockm.h"
#include "telnet.h"
#include "ftpsrv.h"
#include "ftpcl.h"





// FTP return codes and text messages
//
// If a msg string name ends in _v then it has printf specifiers in it
// and it needs to be used with addToOutput_var.

#define _NL_ "\r\n"

static char Msg_150_Send_File_List[] =       "150 Sending file list" _NL_;
static char Msg_200_Port_OK[] =              "200 PORT command successful" _NL_;
static char Msg_200_Noop_OK[] =              "200 NOOP command successful" _NL_;
static char Msg_202_No_Alloc_needed[] =      "202 No storage allocation necessary" _NL_;
static char Msg_211_End_Of_Status[] =        "211 End of status" _NL_;
static char Msg_215_System_Type[] =          "215 UNIX Type: L8" _NL_;
static char Msg_220_ServerStr[] =            "220 mTCP FTP Server" _NL_;
static char Msg_221_Closing[] =              "221 Server closing connection" _NL_;
static char Msg_226_Transfer_Complete[] =    "226 Transfer complete" _NL_;
static char Msg_226_ABOR_Complete[] =        "226 ABOR complete" _NL_;
static char Msg_230_User_Logged_In[] =       "230 User logged in" _NL_;
static char Msg_250_Cmd_Successful_v[] =     "250 %s command successful" _NL_;
static char Msg_331_UserOkSendPass[] =       "331 User OK, send Password" _NL_;
static char Msg_421_Service_Not_Avail[] =    "421 Service not available, try back later" _NL_;
static char Msg_425_Cant_Open_Conn[] =       "425 Cant open connection - please try again" _NL_;
static char Msg_425_Send_Port[] =            "425 Send PORT first or try passive mode" _NL_;
static char Msg_425_Transfer_In_Progress[] = "425 Transfer already in progress" _NL_;
static char Msg_426_Request_term[] =         "426 Request terminated" _NL_;

static char Msg_500_Parm_Missing_v[] =       "500 %s command requires a parameter" _NL_;
static char Msg_500_Syntax_Error_v[] =       "500 Syntax error: %s" _NL_;
static char Msg_501_Unknown_Option_v[] =     "501 Unrecognized option for %s: %s" _NL_;
static char Msg_501_Invalid_Num_Args[] =     "501 Invalid number of arguments" _NL_;
static char Msg_502_Not_Implemented[] =      "502 Command not implemented" _NL_;
static char Msg_503_Already_Logged_In[] =    "503 You are already logged in" _NL_;
static char Msg_503_Send_RNFR_first[] =      "503 Send RNFR first" _NL_;
static char Msg_504_Unsupp_Option_v[] =      "504 Unsupported option for %s: %s" _NL_;
static char Msg_530_Login_Incorrect[] =      "530 Login incorrect" _NL_;
static char Msg_530_Please_Login[] =         "530 Please login" _NL_;
static char Msg_550_Bad_File_v[] =           "550 %s: bad file or directory" _NL_;
static char Msg_550_Bad_Path_Or_File[] =     "550 Bad path or filename" _NL_;
static char Msg_550_Filesystem_error[] =     "550 Filesystem error" _NL_;
static char Msg_550_Bad_Drive_Letter_v[] =   "550 Invalid or inactive drive letter in path: %s" _NL_;
static char Msg_550_Permission_Denied[] =    "550 permission denied" _NL_;
static char Msg_550_Path_Too_Long[] =        "550 Path too long" _NL_;
static char Msg_550_Already_Exists_v[] =     "550 %s: already exists" _NL_;
static char Msg_550_Not_Plain_File_v[] =     "550 %s: not a plain file" _NL_;
static char Msg_550_Error_Removing_v[] =     "550 Error removing %s" _NL_;

static char *Msg_214_Help[] = {
  "214-Welcome to the mTCP FTP server, Version: " __DATE__ _NL_,
  " USER  PASS  REIN  ACCT  REST" _NL_, 
  " RNFR  RNTO  DELE" _NL_, 
  " CWD   XCWD  CDUP  XCUP  PWD   XPWD  MKD   XMKD  RMD   XRMD" _NL_, 
  " PASV  PORT  ABOR  LIST  NLST  RETR  STOR  STOU  APPE" _NL_, 
  " MODE  STRU  TYPE  HELP  ALLO  FEAT  MDTM  NOOP  STAT  SYST SITE" _NL_,
  "214 OK" _NL_,
  NULL
};


static char InternalLoggingError[] = "<INTERNAL LOGGING ERROR>";


static const char ASCII_str[] = "ASCII";
static const char BIN_str[] = "BINARY";



// Function prototypes

static void sendMotd( FtpClient *client );
static void scanValidDrives( void );      // Populate the table of valid drive letters
static int  readConfigParms( void );      // Read configuration parms for the server
static int  initSrv( void );              // Initialize the server
static void shutdown( int rc );           // Clean mTCP shutdown

static unsigned _my_dos_findfirst( const char *path, unsigned attributes, struct find_t *buffer );
static unsigned _my_dos_findnext( struct find_t *buffer );


static void processNewConnection( TcpSocket *newSocket );
static void serviceClient( FtpClient *client );

static void doAbort( FtpClient *client );
static void doCwd ( FtpClient *client, char *target );
static void doDele ( FtpClient *client, char *target );
static void doHelp ( FtpClient *client );
static void doRnfr ( FtpClient *client, char *target );
static void doRnto ( FtpClient *client, char *target );
static void doMkd ( FtpClient *client, char *target );
static void doRmd ( FtpClient *client, char *target );
static void doDataXfer( FtpClient *client, char *filespec );
static void doMode( FtpClient *client, char *nextTokenPtr );
static void doMdtm( FtpClient *client, char *nextTokenPtr );
static void doPort( FtpClient *client, char *nextTokenPtr );
static void doPasv( FtpClient *client );
static void doSize( FtpClient *client, char *nextTokenPtr );
static void doSite( FtpClient *client, char *nextTokenPtr );
static void doStat( FtpClient *client, char *nextTokenPtr );
static void doStru( FtpClient *client, char *nextTokenPtr );
static void doType( FtpClient *client, char *nextTokenPtr );
static void doXfer( FtpClient *client, char *nextTokenPtr, FtpClient::DataXferType listType );

static void doSiteStats( FtpClient *client );
static void doSiteWho( FtpClient *client );
static void doSiteDiskFree( FtpClient *client, char *nextTokenPtr );

static int formFullPath( FtpClient *client, char *outBuffer, int maxOutBufferLen, char *userPart );
static int formFullPath_DataXfer( FtpClient *client, char *outBuffer, int maxOutBufferLen, char *userPart );

static void endDataTransfers( FtpClient *client, char *msg );
static void endSession( FtpClient *client );


int normalizeDir( char *buffer, int bufferLen );
int convertToDosPath( char *buffer_p );
int convertToUserPath( const char *dosPath, char *userPath );



// Screen handling

void scrollMsgArea( int lines );
void addToScreen( int writeLog, const char *fmt, ... );
void myCprintf( uint8_t x, uint8_t y, uint8_t attr, char *fmt, ... );
void redrawStatusLine( void );
void showBeepState( void );
void showRealIpAddr( int writeLog );
void initScreen( void );

void doConsoleHelp( void );
void doConsoleShowUsers( void );
void doConsoleStats( void );


// Interrupt handlers
//



// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}




// Our DOS critical error handler

int CritErrStatus;  // Clear before using
int TestingDrive;   // Init before using

void ( __interrupt __far *oldInt24)( );

void __interrupt __far newInt24( union INTPACK r ) {

  if ( TestingDrive ) {
    // Only interested in things we deliberately tried to trigger.
    // Clear the error and report back that we saw the error.
    r.h.al = 0;
    CritErrStatus = 1;
  }
  else {
    _chain_intr( oldInt24 );
  }

}




// Globals

TcpSocket *ListeningSocket = NULL;

char PasswordFilename[DOS_MAX_PATHFILE_LENGTH];

char LogFilename[DOS_MAX_PATHFILE_LENGTH];
FILE *LogFile = NULL;

clockTicks_t FtpSrv_timeoutTicks = 3276; // 180 seconds at 18.2 ticks per second

uint16_t FtpSrv_Clients = 3;

uint16_t FtpSrv_Control_Port = 21;

IpAddr_t Pasv_IpAddr;
uint16_t Pasv_Base = 2048;
uint16_t Pasv_Ports = 1024;


// Fixme: This might roll over on us!
uint16_t Current_Year;


uint32_t SessionCounter = 0;

uint32_t Stat_SessionTimeouts = 0;

uint32_t Stat_LIST = 0, Stat_NLST = 0;
uint32_t Stat_RETR = 0, Stat_APPE = 0, Stat_STOR = 0, Stat_STOU = 0;

char StartTime[26];



// ValidDriveTable - index 0 is not used
//
// 0 = not found or not allowed (at runtime)
// 1 = found (at runtime)
// 2 = excluded (during config and scan drive phases only, changes to 0 during runtime)

char ValidDriveTable[27] = { 0 };




char *MotdBuffer = NULL;

// Toggles
//
uint8_t Sound = 1;



// Configuration file parameters

// Filebuffer size and Data receive buffer size
uint16_t Filebuffer_Size = FILEBUFFER_SIZE;
uint16_t Data_Rcv_Buf_Size = DATA_RCV_BUF_SIZE;

uint16_t PacketsPerPoll = 1;



uint8_t far *Screen_base;
int ScreenCols, ScreenRows;
  

uint8_t DOS_major, DOS_minor;



// Path and file helper functions
//
// userPath implies the input is a unix style path with / as delimeters.
// realPath implies a DOS style path with backslash delimiters.

inline int isPathAbsolute( const char *userPath ) {
  return *userPath == '/';
}

int isDrivePrefixPresent( const char *userPath ) {
  return ((strncmp("/DRIVE_", userPath, 7) == 0) && (userPath[8] == '/') && (userPath[7] >='A' && userPath[7] <='Z') );
}


int isDriveInValidTable( int driveLetter_p ) {
  if ( !isalpha(driveLetter_p) ) return 0;
  int driveLetter = toupper( driveLetter_p );
  driveLetter = driveLetter - 64;
  return ( (driveLetter > 0 && driveLetter < 27) && ValidDriveTable[driveLetter] );
}


int isDirectory( const char *realPath ) {

  if ( realPath == NULL ) return 0;

  struct stat statbuf;
  int rc = stat( realPath, &statbuf );
  if ( rc == 0 ) {
    return S_ISDIR(statbuf.st_mode);
  }
  return 0;
}

int isFile( const char *realPath ) {

  struct stat statbuf;
  int rc = stat( realPath, &statbuf );
  if ( rc == 0 ) {
    return S_ISREG(statbuf.st_mode);
  }
  return 0;
}

int doesExist( const char *realPath ) {
  struct stat statbuf;
  int rc = stat( realPath, &statbuf );
  if ( rc == 0 ) {
    return 1;
  }
  return 0;
}






static char CopyrightMsg1[] = "mTCP FtpSrv by M Brutman (mbbrutman@gmail.com) (C)opyright 2010-2020\n";
static char CopyrightMsg2[] = "Version: " __DATE__ "\n\n";

int main( int argc, char *argv[] ) {

  initScreen( );

  addToScreen( 0, "%s  %s", CopyrightMsg1, CopyrightMsg2 );

  if ( initSrv( ) ) {
    addToScreen( 1, "\nServer can not start - exiting\n" );
    scrollMsgArea( 2 );

    gotoxy( 0, ScreenRows-1 );
    exit( 1 );
  }



  // If you get to here you must use shutdown to end the program because we
  // have the timer interrupt, Ctrl-Break and Ctrl-C hooked.


  // Setup our listening socket

  ListeningSocket = TcpSocketMgr::getSocket( );
  ListeningSocket->listen( FtpSrv_Control_Port, 512 );


  clockTicks_t lastTimeoutSweep = TIMER_GET_CURRENT( );
  clockTicks_t lastKeyboardCheck = TIMER_GET_CURRENT( );


  // Main loop

  uint8_t shuttingDown = 0;

  while ( 1 ) {

    PACKET_PROCESS_MULT( PacketsPerPoll );
    Arp::driveArp( );
    Tcp::drivePackets( );


    // Check for client inactivity every 10 seconds

    if ( Timer_diff( lastTimeoutSweep, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 10000 ) ) {

      lastTimeoutSweep = TIMER_GET_CURRENT( );

      for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {

        FtpClient *client = FtpClient::activeClientsTable[i];

        if ( client->state != FtpClient::Closed ) {

          // Get the newer of the control socket last activity time and the
          // data socket last activity time, if it is in use.

          clockTicks_t last = client->cs->lastActivity;
          if ( client->ds != NULL && client->ds->lastActivity > last ) last = client->ds->lastActivity;

          clockTicks_t diff = Timer_diff( last, TIMER_GET_CURRENT( ) );

          // End them if latest activity is greater than timeout value.
          if ( diff > FtpSrv_timeoutTicks ) {
            Stat_SessionTimeouts++;
            endSession( client );
          }

        }

      } // end for

    }



    // Things to do once a second:
    //
    // - Check to see if we are finished shutting down
    // - Check for Ctrl-Break or Ctrl-C
    // - Read keyboard input
    // - Redraw the status line

    if ( Timer_diff( lastKeyboardCheck, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( 1000 ) ) {

      redrawStatusLine( );

      lastKeyboardCheck = TIMER_GET_CURRENT( );

      if ( shuttingDown ) {
        // Waiting for shutdown to complete
        if ( FtpClient::activeClients == 0 ) break;
      }
      else {

        int shutdownRequested = 0;

        // Check the keyboard

        if ( CtrlBreakDetected ) {
          shutdownRequested++;
        }
        else {

          if ( bioskey(1) ) {

            uint16_t key = bioskey(0);

            if ( (key & 0xff) == 0 ) {

              // Function key
              key = key >> 8;

              if ( key == 22 ) {        // Alt-U, Users
                doConsoleShowUsers( );
              }
              else if ( key == 31 ) {   // Alt-S, Help
                doConsoleStats( );
              }
              else if ( key == 35 ) {   // Alt-H, Help
                doConsoleHelp( );
              }
              else if ( key == 45 ) {   // Alt-X, EndProgram
                shutdownRequested++;
              }
              else if ( key == 48 ) {   // Alt-B, BeepToggle
                Sound = !Sound;
                showBeepState( );
              }

            }
            else {

              // Normal key
              key = key & 0xff;

              if ( key == 3 ) {
                shutdownRequested++;
              }

            }

          } // end if no keys

        }

        if ( shutdownRequested ) {
          addToScreen( 1, "Shutdown requested\n" );
          // Start an involuntary close on everything
          for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {
            FtpClient *client = FtpClient::activeClientsTable[i];
            endSession( client );
          }
          shuttingDown++;
        }

      } // !shutting down.

    }


    if ( !shuttingDown ) {
      // Check for new connections
      TcpSocket *tmpSocket = TcpSocketMgr::accept( );
      if ( tmpSocket != NULL ) {
        processNewConnection( tmpSocket );
      }
    }



    // Service active FTP clients

    for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {

      FtpClient *client = FtpClient::activeClientsTable[i];

      // If it is in the active list and it went to Closed, then recycle it.
      if ( client->state == FtpClient::Closed ) {

        addToScreen( 1, "(%lu) Disconnect: %d.%d.%d.%d:%u\n", client->sessionId,
          client->cs->dstHost[0], client->cs->dstHost[1],
          client->cs->dstHost[2], client->cs->dstHost[3],
          client->cs->dstPort );

        client->cleanupSession( );

        // Remove from active list and put back on free list
        FtpClient::removeFromActiveList( client );
        FtpClient::returnFreeClient( client );

        // Break the loop and start from scratch because we changed the
        // order of entries in the Client table
        break;

      } // Did client completely close?

      else {
        // Service the socket
        serviceClient( client );
      }

    } // end service active clients


  } // end while


  shutdown( 0 );

  return 0;
}







void shutdown( int rc ) {

  addToScreen( 1, "Stats: Sessions: %lu  Timeouts: %lu\n", SessionCounter, Stat_SessionTimeouts );
  addToScreen( 1, "       LIST: %lu  NLST: %lu  RETR: %lu\n", Stat_LIST, Stat_NLST, Stat_RETR );
  addToScreen( 1, "       STOR: %lu  STOU: %lu  APPE: %lu\n", Stat_STOR, Stat_STOU, Stat_APPE );
  addToScreen( 1, "=== Server shutdown === \n" );

  // Scroll one more line and position the cursor at the bottom of the screen so that
  // the stats get printed in the right spot.

  scrollMsgArea( 2 );

  gotoxy( 0, ScreenRows-1 );

  Utils::endStack( );

  if ( LogFile != NULL ) {
    fclose( LogFile );
  }

  exit( rc );
}




void processNewConnection( TcpSocket *newSocket ) {

  TRACE(( "Ftp Connect on port %u from %d.%d.%d.%d:%u\n",
          newSocket->srcPort,
          newSocket->dstHost[0], newSocket->dstHost[1],
          newSocket->dstHost[2], newSocket->dstHost[3],
          newSocket->dstPort ));

  // If this is a new connection to our control port create a new client.

  if ( newSocket->srcPort == FtpSrv_Control_Port ) {

    FtpClient *client = FtpClient::getFreeClient( ); 

    if ( client != NULL ) {
      client->startNewSession( newSocket, SessionCounter++ );
      client->addToOutput( Msg_220_ServerStr );
      newSocket = NULL;
    }
    else {
      // Could not get a new client.  Fall through to close the socket.
      // (newSocket not being NULL will trigger that.)
      newSocket->send( (uint8_t *)Msg_421_Service_Not_Avail, strlen(Msg_421_Service_Not_Avail) );
    }

  }
  else {

    // Could be a data socket.  If so, find the listening client.

    for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {

      FtpClient *client = FtpClient::activeClientsTable[i];

      // The client has to be listening and the address has to match perfectly.
      //
      // If the client does something stupid like try to connect twice we will
      // take the first one but not match on the second one because we won't
      // be listening anymore.

      if ( (client->ls != NULL) && (client->pasvPort == newSocket->srcPort) && Ip::isSame( client->cs->dstHost, newSocket->dstHost ) ) {

        // Great, it's a match.  Close the listening socket and set the
        // data socket.

        client->ls->close( );
        TcpSocketMgr::freeSocket( client->ls );
        client->ls = NULL;

        TRACE(( "Ftp (%lu) Close listening socket\n", client->sessionId ));


        // Ensure that there is not a data socket open already or we will
        // lose it.  This should only happen if the client does a PASV,
        // makes a connection, doesn't use it, and then does another PASV.
        // We'll guard against that by forcing data connections closed if
        // somebody does a PASV or PORT command while a data socket is already
        // present but a transfer is not in progress.

        if ( client->ds != NULL ) {
          // This is an error.  Force it closed before taking a new one.
          TRACE_WARN(( "Ftp (%lu) Closing data connection that was never used\n", client->sessionId ));
          client->ds->close( );
          TcpSocketMgr::freeSocket( client->ds );
          client->ds = NULL;
        }

        client->ds = newSocket;

        // Set this to NULL to signify that we used it
        newSocket = NULL;
        break;
      }

    }

  }


  // If nobody claimed it close it

  if ( newSocket ) {
    TRACE(( "Ftp Nobody claimed the new socket - closing it\n" ));
    newSocket->close( );
    TcpSocketMgr::freeSocket( newSocket );
  }

}








// - If there was an active data connection service it.
// - If there is pending output push it out
// - If we are supposed to be closing up don't process any more user input


void serviceClient( FtpClient *client ) {


  // Did they drop on us?
  if ( client->cs->isRemoteClosed( ) ) {

    switch ( client->state ) {

      case FtpClient::ClosingPushOutput: {
        // If we were trying to force final output to the user don't bother.
        client->clearOutput( );
        break;
      }

      case FtpClient::Closing: {
        // Do nothing; we are already waiting for them to close
        break;
      }

      default: {
        // Unexpected close.
        
        TRACE(( "Ftp (%lu) Control socket dropped: %d.%d.%d.%d:%u\n", client->sessionId,
          client->cs->dstHost[0], client->cs->dstHost[1],
          client->cs->dstHost[2], client->cs->dstHost[3],
          client->cs->dstPort ));

        endSession( client );
      }

    } // end switch

  }



  // Send output on control socket.
  //
  // If there was any pending output try to send it out.  If we can't send it
  // then don't bother doing anything else; we don't want to overflow the
  // buffer.

  if ( client->pendingOutput( ) ) {
    client->sendOutput( );
    return;
  }



  // Handle directory listings that are in flight
  //
  // We have to do this to detect if they were closing and to process the
  // close.  This also cleans up the data structures and sockets
  
  if ( client->dataXferState != FtpClient::DL_NotActive ) {
    doDataXfer( client, NULL );
  }



  // If we were trying to push out final output on the control socket and
  // we got here, then we suceeded.  Now we can start the close process on
  // the control socket.

  if ( client->state == FtpClient::ClosingPushOutput ) {
    TRACE(( "Ftp (%lu) Last output pushed out, moving to Closing\n", client->sessionId ));
    client->cs->closeNonblocking( );
    client->state = FtpClient::Closing;
    return;
  }



  // Were we waiting for the sockets to close?  If all of our sockets
  // have closed then we can clean up.

  if ( client->state == FtpClient::Closing ) {

    // If dataXferState is DL_NotActive then those sockets are properly closed.
    // If our control socket is closed too then we are done.

    if ( (client->dataXferState == FtpClient::DL_NotActive) && (client->cs->isCloseDone( )) ) {
      TRACE(( "Ftp (%lu) All sockets closed\n", client->sessionId ));
      client->state = FtpClient::Closed;
      // Harvesting this client will be done in the main loop.
    }

    // Whether we are done or not, return because we don't want to process
    // more user input.
    return;
  }


  if ( client->statCmdActive ) { doStat( client, NULL ); return; }


  // Check for new input on the socket

  {
    int16_t bytesToRead = INPUTBUFFER_SIZE - client->inputBufferIndex;

    int16_t bytesRead = client->cs->recv( (uint8_t *)(client->inputBuffer + client->inputBufferIndex), bytesToRead );
    if ( bytesRead < 0 ) {
      TRACE(( "Ftp (%lu) error reading socket!\n", client->sessionId ));
      return;
    }
    client->inputBufferIndex += bytesRead;


    // Did we get a full line of input?

    if ( client->inputBufferIndex < 2 ) {
      // Not even a CR/LF pair fits here
      return;
    }

    int fullLine = 0;
    for ( int i=0; i < (client->inputBufferIndex)-1; i++ ) {
      if ( client->inputBuffer[i] == '\r' && client->inputBuffer[i+1] == '\n' ) {
        fullLine = 1;
        break;
      }
    }

    if ( fullLine ) {

      if ( client->eatUntilNextCrLf ) {
        client->inputBufferIndex = 0;
        client->eatUntilNextCrLf = 0;
        return;
      }

      // Reset for the next read.  Get rid of CR/LF too.
      client->inputBuffer[client->inputBufferIndex-2] = 0;
      client->inputBufferIndex = 0;
    }
    else {

      // Need to read some more.  But before we try again, check to make
      // sure that there is room in the buffer

      if ( client->inputBufferIndex == INPUTBUFFER_SIZE ) {
        TRACE_WARN(( "Ftp (%lu) Input buffer overflow on control socket\n", client->sessionId ));
        client->addToOutput_var( Msg_500_Syntax_Error_v, "Line too long" );
        client->inputBufferIndex = 0;
        client->eatUntilNextCrLf = 1;
      }

      // Read some more, picking up where we left off
      return;
    }
  }



  // By this point we have a full line of input.

  TRACE(( "Ftp (%lu) State: %d  Input from %d.%d.%d.%d:%u: %s\n",
    client->sessionId, client->state,
    client->cs->dstHost[0], client->cs->dstHost[1],
    client->cs->dstHost[2], client->cs->dstHost[3],
    client->cs->dstPort, client->inputBuffer ));


  // If the first char is a Telnet IAC then we should interpret the sequence.
  //
  // Unix does things correctly by sending IAC before each telnet command.
  // Windows XP appears not to.  Be sloppy - if the first char is IAC then
  // assume the ABOR is coming sometime later ..

  if ( client->inputBuffer[0] == TEL_IAC ) {
    int i;
    for ( i=0; i < strlen(client->inputBuffer); i++ ) {
      if ( client->inputBuffer[i] < 128 ) break;
    }

    memmove( client->inputBuffer, client->inputBuffer+i, (strlen(client->inputBuffer)-i)+1 );

    TRACE(( "TEL_IAC detected: removed %u chars, cmd is now: %s---\n", i, client->inputBuffer ));
  }



  char command[COMMAND_MAX_LEN];
  char *nextTokenPtr = Utils::getNextToken( client->inputBuffer, command, COMMAND_MAX_LEN );

  if ( *command == 0 ) return;

  strupr( command );


  switch ( client->state ) {

    case FtpClient::UserPrompt: {

      if ( strcmp(command, "USER") == 0 ) {

        client->loginAttempts++;

        if ( client->loginAttempts > 3 ) {
          // Disconnect them for security.
          endSession( client );
          break;
        }

        char tmpUserName[USERNAME_LEN];
        Utils::getNextToken( nextTokenPtr, tmpUserName, USERNAME_LEN );
        if ( tmpUserName[0] ) {

          // Lookup in pw file
          int rc = FtpUser::getUserRec( tmpUserName, &client->user );

          if ( rc == 1 ) {

            // Send password prompt

            if ( strcmp( client->user.userPass, "[EMAIL]" ) == 0 ) {
              client->addToOutput( "331 Anonymous ok, send your email addr as the password" _NL_ );
            }
            else {
              client->addToOutput( Msg_331_UserOkSendPass );
            }

            client->state = FtpClient::PasswordPrompt;

          }
          else {
            client->addToOutput( "530 I dont like your name" _NL_ );
            addToScreen( 1, "(%lu) Bad userid: %s\n", client->sessionId, tmpUserName );
          }

        }
        else {
          // Missing parm
          client->addToOutput_var( Msg_500_Parm_Missing_v, "USER" );
        }

      }
      else {
        // Bogus command
        client->addToOutput( Msg_530_Please_Login );
      }

      break;
    }

    case FtpClient::PasswordPrompt: {

      if ( strcmp(command, "PASS") == 0 ) {

        // tmpPassword has to be long enough to accomodate reasonable
        // email addresses.

        char tmpPassword[50];
        Utils::getNextToken( nextTokenPtr, tmpPassword, 50 );

        if ( tmpPassword[0] ) {

          // Check password here

          if ( strcmp( client->user.userPass, "[EMAIL]" ) == 0 ) {

/*
            // We're not going to rigorously enforce this ...

            uint16_t formatGood = 1;

            // Email addresses start with a letter.
            if ( !isalpha( tmpPassword[0] ) ) {
              formatGood = 0;
            }
            else {

              // Find the @ sign
              char *atsign = strchr( tmpPassword, '@' );
              if ( atsign == NULL ) {
                formatGood = 0;
              }
              else {
                atsign++;
                if ( strlen(atsign) < 3 ) {
                  formatGood = 0;
                }
              }
            }

            if ( formatGood == 0 ) {
              client->addToOutput( "530 Send an email address for the password" _NL_ );
              client->state = FtpClient::UserPrompt;
              break;
            }
*/

            addToScreen( 1, "(%lu) Anon user: %s, email: %s from %d.%d.%d.%d:%u\n",
                      client->sessionId, client->user.userName, tmpPassword,
                      client->cs->dstHost[0],  client->cs->dstHost[1],
                      client->cs->dstHost[2],  client->cs->dstHost[3],
                      client->cs->dstPort );
          }
          else {

            if ( strcmp( client->user.userPass, tmpPassword ) != 0 ) {
              client->addToOutput( "530 Bad password" _NL_ );
              client->state = FtpClient::UserPrompt;
              addToScreen( 1, "(%lu) Failed password attempt user %s at %d.%d.%d.%d:%u\n",
                        client->sessionId, client->user.userName,
                        client->cs->dstHost[0],  client->cs->dstHost[1], 
                        client->cs->dstHost[2],  client->cs->dstHost[3],
                        client->cs->dstPort );
              break;
            }

            struct tm timeBuf;
            _localtime( &client->startTime, &timeBuf );

            addToScreen( 1, "(%lu) User %s signed in from %d.%d.%d.%d:%u at %04d-%02d-%02d %02d:%02d:%02d\n",
                      client->sessionId, client->user.userName,
                      client->cs->dstHost[0],  client->cs->dstHost[1],
                      client->cs->dstHost[2],  client->cs->dstHost[3],
                      client->cs->dstPort,
                      timeBuf.tm_year+1900, timeBuf.tm_mon+1, timeBuf.tm_mday,
                      timeBuf.tm_hour, timeBuf.tm_min, timeBuf.tm_sec );

          }

          // Ok, tell them connected
          if ( MotdBuffer ) { sendMotd( client ); }
          client->addToOutput( Msg_230_User_Logged_In );
          client->state = FtpClient::CommandLine;

          if ( Sound ) { sound(500); delay(100); sound(1000); delay(100); nosound( ); }

          // Per user housekeeping to do

          if ( strcmp( client->user.sandbox, "[NONE]" ) == 0 ) {
            // No sandbox - this user gets DOS style directory paths
            client->ftproot[0] = 0;
            client->cwd[0] = '/'; client->cwd[1] = 0;
          }
          else {
            // Sandbox - this user gets Unix style directory paths
            strcpy( client->ftproot, client->user.sandbox );
            client->cwd[0] = '/'; client->cwd[1] = 0;
          }

        }
        else {
          // No password
          client->addToOutput( Msg_530_Login_Incorrect );
          client->state = FtpClient::UserPrompt;

        }
      }
      else {
        // Bogus command
        client->addToOutput( Msg_530_Please_Login );
        client->state = FtpClient::UserPrompt;
      }

      break;
    }

    case FtpClient::RnfrSent: {

      // Going back to command line no matter what.
      client->state = FtpClient::CommandLine;

      if ( strcmp(command, "RNTO") == 0 ) {
        doRnto( client, nextTokenPtr );
        break;
      }

      // Fall through to CommandLine if it wasn't RNTO.  Not terribly valid
      // but we will tolerate it.
    }

    case FtpClient::CommandLine: {

      if ( strcmp(command, "QUIT") == 0 ) {

        // We really want this to make it out before the socket closes,
        // but we are not going to make an extraordinary effort to do it.
        client->addToOutput( Msg_221_Closing );
        client->sendOutput( );

        endSession( client );
      }


      // Path related
      else if ( strcmp(command, "DELE") == 0 ) { doDele( client, nextTokenPtr ); }
      else if ( strcmp(command, "RNFR") == 0 ) { doRnfr( client, nextTokenPtr ); }
      else if ( strcmp(command, "RNTO") == 0 ) { client->addToOutput( Msg_503_Send_RNFR_first ); }
      else if ( strcmp(command, "CWD" ) == 0 || strcmp(command, "XCWD") == 0 ) { doCwd( client, nextTokenPtr ); }
      else if ( strcmp(command, "CDUP") == 0 || strcmp(command, "XCUP") == 0 ) { doCwd( client, ".." ); }
      else if ( strcmp(command, "PWD" ) == 0 || strcmp(command, "XPWD") == 0 ) { client->addToOutput_var( "257 \"%s\" is current directory" _NL_, client->cwd ); }
      else if ( strcmp(command, "MKD" ) == 0 || strcmp(command, "XMKD") == 0 ) { doMkd( client, nextTokenPtr ); }
      else if ( strcmp(command, "RMD" ) == 0 || strcmp(command, "XRMD") == 0 ) { doRmd( client, nextTokenPtr ); }

      // Data transfer
      else if ( strcmp(command, "PASV") == 0 ) { doPasv( client ); }
      else if ( strcmp(command, "PORT") == 0 ) { doPort( client, nextTokenPtr ); }
      else if ( strcmp(command, "ABOR") == 0 ) { doAbort( client ); }
      else if ( strcmp(command, "LIST") == 0 ) { doXfer( client, nextTokenPtr, FtpClient::List ); }
      else if ( strcmp(command, "NLST") == 0 ) { doXfer( client, nextTokenPtr, FtpClient::Nlist ); }
      else if ( strcmp(command, "RETR") == 0 ) { doXfer( client, nextTokenPtr, FtpClient::Retr ); }
      else if ( strcmp(command, "STOR") == 0 ) { doXfer( client, nextTokenPtr, FtpClient::Stor ); }
      else if ( strcmp(command, "APPE") == 0 ) { doXfer( client, nextTokenPtr, FtpClient::StorA ); }
      else if ( strcmp(command, "STOU") == 0 ) { doXfer( client, nextTokenPtr, FtpClient::StorU ); }

      // Environment selection
      else if ( strcmp(command, "MODE") == 0 ) { doMode( client, nextTokenPtr ); }
      else if ( strcmp(command, "STRU") == 0 ) { doStru( client, nextTokenPtr ); }
      else if ( strcmp(command, "TYPE") == 0 ) { doType( client, nextTokenPtr ); }

      // Misc
      else if ( strcmp(command, "HELP") == 0 ) { doHelp( client ); }
      else if ( strcmp(command, "ALLO") == 0 ) { client->addToOutput( Msg_202_No_Alloc_needed ); }
      else if ( strcmp(command, "FEAT") == 0 ) { client->addToOutput( "211-mTCP FTP server features:" _NL_ " MDTM" _NL_ " SIZE " _NL_ "211 End" _NL_ ); }
      else if ( strcmp(command, "MDTM") == 0 ) { doMdtm( client, nextTokenPtr ); }
      else if ( strcmp(command, "SIZE") == 0 ) { doSize( client, nextTokenPtr ); }
      else if ( strcmp(command, "NOOP") == 0 ) { client->addToOutput( Msg_200_Noop_OK ); }
      else if ( strcmp(command, "STAT") == 0 ) { doStat( client, nextTokenPtr ); }
      else if ( strcmp(command, "SYST") == 0 ) { client->addToOutput( Msg_215_System_Type ); }

      else if ( strcmp(command, "SITE") == 0 ) { doSite( client, nextTokenPtr ); }

      else if ( (strcmp(command, "USER") == 0) || (strcmp(command, "PASS") == 0) ) {
        client->addToOutput( Msg_503_Already_Logged_In );
      }

      else if ( (strcmp(command, "REIN") == 0) || (strcmp(command, "ACCT") == 0) || (strcmp(command, "REST") == 0) )
      {
        client->addToOutput( Msg_502_Not_Implemented );
      }

      else {
        client->addToOutput_var( Msg_500_Syntax_Error_v, command );
        TRACE_WARN(( "Ftp: unknown command: %s\n", client->inputBuffer ));
      }

      break;
    }


  } // end switch 


}



void doHelp( FtpClient *client ) {

  int i=0;
  while ( Msg_214_Help[i] != NULL ) {
    client->addToOutput( Msg_214_Help[i] );
    i++;
  }

}



// doStat
//
// With no parameters it just returns some basic status.
//
// If given a parameter it does a directory list on the parameter.  Unlike
// the standard directory list, all output flows back over the control
// connection.  I expect this to be a fairly rare usage of the command
// so it's not optimized for speed in anyway.

void doStat( FtpClient *client, char *nextTokenPtr ) {

  // If this is the first time here parse the input.  If there is a
  // parameter setup to start sending directory entries back.

  if ( client->statCmdActive == 0 ) {

    char parm[ USR_MAX_PATHFILE_LENGTH_PADDED ];
    Utils::getNextToken( nextTokenPtr, parm, USR_MAX_PATHFILE_LENGTH_PADDED );

    if ( parm[0] == 0 ) {
      client->addToOutput_var( "211-Status of mTCP FTP Server" _NL_ " Logged in as %s" _NL_, client->user.userName );
      if ( client->ds ) {
        client->addToOutput( " Active data connection" _NL_ );
      }
      else {
        client->addToOutput( " No active data connection" _NL_ );
      }
      client->addToOutput_var( " Type: %s Structure: File, Mode: Stream" _NL_, (client->asciiMode?"ASCII":"IMAGE") );
      client->addToOutput( Msg_211_End_Of_Status );
      return;
    }


    // This is going to be longer than we thought.
    client->statCmdActive = 1;

    client->addToOutput_var( "211-Status of %s" _NL_, parm );

    char fullpath[USR_MAX_PATHFILE_LENGTH];
    int rc = formFullPath( client, fullpath, USR_MAX_PATHFILE_LENGTH, parm );
    if ( rc != 0 && rc != 2 ) {
      client->addToOutput( Msg_211_End_Of_Status );
      client->statCmdActive = 0;
      return;
    }

    // Stat it.  If it is a directory then add the *.* to the end.
    // If it's not valid don't worry about it - they will get an empty
    // listing.

    if ( strlen(fullpath) < USR_MAX_PATH_LENGTH ) {
      if ( isDirectory( fullpath ) ) {
        if ( fullpath[strlen(fullpath)-1] == '\\' ) {
          strcat( fullpath, "*.*" );
        }
        else {
          strcat( fullpath, "\\*.*" );
        }
      }
    }

    client->noMoreData = _my_dos_findfirst( fullpath, (_A_NORMAL | _A_SUBDIR), &client->fileinfo);

    if ( client->noMoreData ) {
      _dos_findclose( &client->fileinfo );
      client->addToOutput( Msg_211_End_Of_Status );
      client->statCmdActive = 0;
    }


    // Return from this function without doing any real work; not terribly
    // efficient but it cuts down on code duplication.  We will pick up where
    // we left off on the next call to this code.
  }


  else {

    // We don't get here if the client isn't done sending previously queued
    // data.  We also don't care too much about the performance of this
    // command, so keep things simple and just send one line at a time.

    // Fixme: do a small optimization by doing two lines of output at a time.

    // Format file attributes
    char attrs[] = "-rwxrwxrwx"; // Default
    if ( client->fileinfo.attrib & _A_SUBDIR ) { attrs[0] = 'd'; }
    if ( client->fileinfo.attrib & _A_RDONLY ) { attrs[2] = attrs[5] = attrs[8] = '-'; }

    ftime_t ft;
    ft.us = client->fileinfo.wr_time;

    fdate_t fd;
    fd.us = client->fileinfo.wr_date;

    if ( fd.fields.year + 1980 != Current_Year ) {
      client->addToOutput_var( " %s 1 ftp ftp %10lu %s %2d  %4d %s" _NL_,
                 attrs, client->fileinfo.size, Months[fd.fields.month-1],
                 fd.fields.day, (fd.fields.year + 1980),
                 client->fileinfo.name );
    }
    else {
      client->addToOutput_var( " %s 1 ftp ftp %10lu %s %2d %02d:%02d %s" _NL_,
                 attrs, client->fileinfo.size, Months[fd.fields.month-1],
                 fd.fields.day, ft.fields.hours, ft.fields.minutes,
                 client->fileinfo.name );
    }

    if ( (client->noMoreData = _my_dos_findnext( &client->fileinfo )) ) {
      _dos_findclose( &client->fileinfo );
      client->addToOutput( Msg_211_End_Of_Status );
      client->statCmdActive = 0;
    }

  }

}


void doSite( FtpClient *client, char *nextTokenPtr ) {

  char siteCmd[ 10 ];
  nextTokenPtr = Utils::getNextToken( nextTokenPtr, siteCmd, 10 );

  if ( stricmp( siteCmd, "stats" ) == 0 ) {
    doSiteStats( client );
  }
  else if ( stricmp( siteCmd, "who" ) == 0 ) {
    doSiteWho( client );
  }
  else if ( stricmp( siteCmd, "help" ) == 0 ) {
    client->addToOutput( "211 Site commands: HELP DISKFREE STATS WHO" _NL_ );
  }
  else if ( stricmp( siteCmd, "diskfree" ) == 0 ) {
    doSiteDiskFree( client, nextTokenPtr );
  }
  else {
    client->addToOutput( "500 Unknown SITE command" _NL_ );
  }

}



void doSiteStats( FtpClient *client ) {

  client->addToOutput_var(
    "211-Stats: Started: %s, DOS version: %d.%02d" _NL_ " Sessions: %lu  Active: %u  Timeouts: %lu" _NL_,
    StartTime, DOS_major, DOS_minor, SessionCounter, FtpClient::activeClients, Stat_SessionTimeouts
  );

  client->addToOutput_var(
    " LIST: %lu  NLST: %lu  RETR: %lu" _NL_ " STOR: %lu  STOU: %lu  APPE: %lu" _NL_,
    Stat_LIST, Stat_NLST, Stat_RETR, Stat_STOR, Stat_STOU, Stat_APPE
  );

  client->addToOutput_var( " Tcp Sockets used: %d free: %d" _NL_,
    TcpSocketMgr::getActiveSockets( ), TcpSocketMgr::getFreeSockets( )
  );

  client->addToOutput_var(
    " Tcp: Sent %lu Rcvd %lu Retrans %lu Seq/Ack errs %lu Dropped %lu" _NL_,
    Tcp::Packets_Sent, Tcp::Packets_Received, Tcp::Packets_Retransmitted,
    Tcp::Packets_SeqOrAckError, Tcp::Packets_DroppedNoSpace
  );

  client->addToOutput_var(
    " Packets: Sent: %lu Rcvd: %lu Dropped: %lu LowFreeBufCount: %u" _NL_,
    Packets_sent, Packets_received, Packets_dropped, Buffer_lowFreeCount
  );

  client->addToOutput( "211 OK" _NL_ );

}


void doSiteWho( FtpClient *client ) {

  client->addToOutput("200- Online users" _NL_ " UserId            Login time          IpAddr:port" _NL_ );

  for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {

    FtpClient *tmpClient = FtpClient::activeClientsTable[i];

    if ( tmpClient->state != FtpClient::Closed ) {

      struct tm timeBuf;
      _localtime( &client->startTime, &timeBuf );

      client->addToOutput_var( " %6ld %-10s %04d-%02d-%02d %02d:%02d:%02d %d.%d.%d.%d:%u" _NL_,
        tmpClient->sessionId, tmpClient->user.userName,
        timeBuf.tm_year+1900, timeBuf.tm_mon+1, timeBuf.tm_mday,
        timeBuf.tm_hour, timeBuf.tm_min, timeBuf.tm_sec,
        tmpClient->cs->dstHost[0], tmpClient->cs->dstHost[1],
        tmpClient->cs->dstHost[2], tmpClient->cs->dstHost[3],
        tmpClient->cs->dstPort );

    }

  }

  client->addToOutput("200 OK" _NL_ );

}

void doSiteDiskFree( FtpClient *client, char *nextTokenPtr ) {

  char driveLetter[2];
  Utils::getNextToken( nextTokenPtr, driveLetter, 2 );

  if ( driveLetter[0] == 0 ) {
    client->addToOutput( "211 Please specify a drive letter" _NL_ );
    return;
  }

  char dl = toupper(driveLetter[0]);

  if ( !isalpha( dl ) || !isDriveInValidTable( dl ) ) {
    client->addToOutput( "211 Bad or inactive drive letter" _NL_ );
    return;
  }

  // Ok, it's valid at least.

  dl = dl - 'A' + 1;

  struct diskfree_t disk_data;

  if ( _dos_getdiskfree( dl, &disk_data ) == 0 ) {
    uint32_t freeSpace = (uint32_t)disk_data.avail_clusters *
                         (uint32_t)disk_data.sectors_per_cluster *
                         (uint32_t)disk_data.bytes_per_sector;

    client->addToOutput_var( "211 Disk %s has %lu free bytes" _NL_, driveLetter, freeSpace );
  }
  else {
    client->addToOutput_var( "211 Error reading free space on Disk %s" _NL_, driveLetter );
  }

}




void doType( FtpClient *client, char *nextTokenPtr ) {

  char datatype[20];
  Utils::getNextToken( nextTokenPtr, datatype, 20 );

  if ( *datatype == 0 ) {
    client->addToOutput_var( Msg_500_Parm_Missing_v, "TYPE" );
    return;
  }

  if ( *datatype == 'a' || *datatype == 'A' ) {
    client->asciiMode = 1;
    client->addToOutput( "200 Type set to A" _NL_ );
  }
  else if ( *datatype == 'i' || *datatype == 'I' ) {
    client->asciiMode = 0;
    client->addToOutput( "200 Type set to I" _NL_ );
  }
  else {
    client->addToOutput_var( "500 TYPE %s not understood or supported" _NL_, datatype );
  }

}


void doStru( FtpClient *client, char *nextTokenPtr ) {

  char struType[20];
  Utils::getNextToken( nextTokenPtr, struType, 20 );

  if ( *struType == 0 ) {
    client->addToOutput_var( Msg_500_Parm_Missing_v, "STRU" );
    return;
  }

  if ( *struType == 'f' || *struType == 'F' ) {
    client->addToOutput( "200 STRU set to F" _NL_ );
  }
  else if ( *struType == 'r' || *struType == 'R' ||  *struType == 'p' || *struType == 'P' ) {
    client->addToOutput_var( Msg_504_Unsupp_Option_v, "STRU", struType );
  }
  else {
    client->addToOutput_var( Msg_501_Unknown_Option_v, "STRU", struType );
  }

}



void doMode( FtpClient *client, char *nextTokenPtr ) {

  char modeType[20];
  Utils::getNextToken( nextTokenPtr, modeType, 20 );

  if ( *modeType == 0 ) {
    client->addToOutput_var( Msg_500_Parm_Missing_v, "MODE" );
    return;
  }

  if ( *modeType == 's' || *modeType == 'S' ) {
    client->addToOutput( "200 MODE set to S" _NL_ );
  }
  else if ( *modeType == 'b' || *modeType == 'B' ||  *modeType == 'c' || *modeType == 'C' ) {
    client->addToOutput_var( Msg_504_Unsupp_Option_v, "MODE", modeType );
  }
  else {
    client->addToOutput_var( Msg_501_Unknown_Option_v, "MODE", modeType );
  }

}





void doPort( FtpClient *client, char *nextTokenPtr ) {

  // If we have a data transfer going already don't honor a PORT command.
  // PORT isn't much of a problem - it just caches information for the next
  // command which is probably a data transfer.  But it's possible that they
  // had done PASV even connected (but not started transfering data) and we
  // want to clean up the data socket in preparation for another transfer.

  if ( client->dataXferState != FtpClient::DL_NotActive ) {
    TRACE_WARN(( "Ftp (%lu) doPort: Transfer already in progress\n", client->sessionId ));
    client->addToOutput( Msg_425_Transfer_In_Progress );
    return;
  }


  // Ok, no transfers in progress at the moment.  If PASV had been used and
  // we had a listening socket open we need to close it.  If the user had
  // also connected the data socket (but not started a transfer), then
  // kill the data socket too.  This prevents us from losing the socket later.

  if ( client->ls != NULL ) {
    TRACE(( "Ftp (%lu) PORT command supercedes PASV, closing listening socket\n", client->sessionId ));
    client->ls->close( );
    TcpSocketMgr::freeSocket( client->ls );
    client->ls = NULL;
  }

  if ( client->ds != NULL ) {
    // This is an error.  No active data transfer, so it's safe to just hit
    // it over the head.
    TRACE_WARN(( "Ftp (%lu) doPort: Closing data connection that was never used\n", client->sessionId ));
    client->ds->close( );
    TcpSocketMgr::freeSocket( client->ds );
    client->ds = NULL;
  }

  uint16_t t0, t1, t2, t3, t4, t5;

  int rc = sscanf( nextTokenPtr, "%d,%d,%d,%d,%d,%d",  &t0, &t1, &t2, &t3, &t4, &t5 );

  if ( rc != 6 ) {
    client->addToOutput( "501 Illegal PORT command" _NL_ );
    return;
  }


  client->dataTarget[0] = t0;  client->dataTarget[1] = t1;  client->dataTarget[2] = t2;  client->dataTarget[3] = t3;
  client->dataPort = (t4<<8) + t5;

  // Ok, tell them we liked that
  client->addToOutput( Msg_200_Port_OK );
}


void doPasv( FtpClient *client ) {

  // If we have a data transfers going already don't honor a PASV command.
  // This probably never happens but we can lose sockets if we start listening
  // for a socket when one is already open.

  if ( client->dataXferState != FtpClient::DL_NotActive ) {
    client->addToOutput( Msg_425_Transfer_In_Progress );
    return;
  }


  // Ok, no data transfers were active.  If we were listening because of a
  // previous PASV command then close that socket.

  // If we have a listening socket already close it.  
  if ( client->ls != NULL ) {
    TRACE(( "Ftp (%lu) Closing previously opened listening socket\n", client->sessionId ));
    client->ls->close( );
    TcpSocketMgr::freeSocket( client->ls );
    client->ls = NULL;
  }

  if ( client->ds != NULL ) {
    // This is an error.  Force it closed before taking a new one.
    TRACE_WARN(( "Ftp (%lu) doPasv: Closing data connection that was never used\n", client->sessionId ));
    client->ds->close( );
    TcpSocketMgr::freeSocket( client->ds );
    client->ds = NULL;
  }

        
  // Open a listening socket immediately, even before pushing a response
  // on the control connection.  This prevents timing problems; the
  // client might be very fast to open the data connection.

  client->ls = TcpSocketMgr::getSocket( );
        
  if ( client->ls == NULL ) {
    TRACE_WARN(( "Ftp (%lu) Could not get listening socket for PASV\n", client->sessionId ));
    client->addToOutput( Msg_425_Cant_Open_Conn );
    return;
  }


  client->pasvPort = (rand( ) % Pasv_Ports) + Pasv_Base;

  uint16_t hiByte = client->pasvPort / 256;
  uint16_t loByte = client->pasvPort - hiByte*256;


  // Fixme: check the return code, we might have a collsion on a port.
  if ( client->ls->listen( client->pasvPort, Data_Rcv_Buf_Size ) ) {
    client->addToOutput( Msg_425_Cant_Open_Conn );
    return;
  }

  client->addToOutput_var( "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)" _NL_,
           client->pasvAddr[0], client->pasvAddr[1], client->pasvAddr[2], client->pasvAddr[3], hiByte, loByte );
        
  TRACE(( "Ftp (%lu) Waiting for data connection on %u\n", client->sessionId, client->pasvPort ));

}



// RFC 3659
//
void doMdtm( FtpClient *client, char *nextTokenPtr ) {

  // Form the full path first.
  char userPart[ USR_MAX_PATHFILE_LENGTH_PADDED ];
  Utils::getNextToken( nextTokenPtr, userPart, USR_MAX_PATHFILE_LENGTH_PADDED );

  if ( *userPart == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }

  char fullpath[USR_MAX_PATHFILE_LENGTH];
  if ( formFullPath( client, fullpath, USR_MAX_PATHFILE_LENGTH, userPart ) ) {
    client->addToOutput( Msg_550_Bad_Path_Or_File );
    return;
  }

  struct stat statbuf;
  int rc = stat( fullpath, &statbuf );

  if ( rc == 0 ) {

    // Ok, it exists.  Send out the time in Greenwich mean time

    struct tm tmbuff;
    _gmtime( &statbuf.st_mtime, &tmbuff );


    client->addToOutput_var( "213 %4d%02d%02d%02d%02d%02d" _NL_,
                             tmbuff.tm_year + 1900, tmbuff.tm_mon + 1, tmbuff.tm_mday,
                             tmbuff.tm_hour, tmbuff.tm_min, tmbuff.tm_sec );

  }
  else {
    client->addToOutput_var( Msg_550_Bad_File_v, userPart );
  }

}




void doDele( FtpClient *client, char *nextTokenPtr ) {

  // Permission check
  if ( client->user.cmd_DELE == 0 ) {
    client->addToOutput( Msg_550_Permission_Denied );
    return;
  }

  // Form the full path first.
  char userPart[USR_MAX_PATHFILE_LENGTH_PADDED];
  Utils::getNextToken( nextTokenPtr, userPart, USR_MAX_PATHFILE_LENGTH_PADDED );

  if ( *userPart == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }

  char fullpath[USR_MAX_PATHFILE_LENGTH];
  if ( formFullPath( client, fullpath, USR_MAX_PATHFILE_LENGTH, userPart ) ) {
    client->addToOutput( Msg_550_Bad_Path_Or_File );
    return;
  }

  struct stat statbuf;
  int rc = stat( fullpath, &statbuf );

  if ( rc == 0 ) {

    // Ok, it exists

    if ( S_ISREG(statbuf.st_mode) ) {
      if ( unlink( fullpath ) ) {
        client->addToOutput_var( Msg_550_Error_Removing_v, userPart );
      }
      else {
        client->addToOutput_var( Msg_250_Cmd_Successful_v, "DELE" );
        addToScreen( 1, "(%lu) DELE %s\n", client->sessionId, fullpath );
      }
    }
    else {
      client->addToOutput_var( Msg_550_Not_Plain_File_v, userPart );
    }

  }
  else {
    client->addToOutput_var( Msg_550_Bad_File_v, userPart );
  }

}




void doRmd( FtpClient *client, char *nextTokenPtr ) {

  // Permission check
  if ( client->user.cmd_RMD == 0 ) {
    client->addToOutput( Msg_550_Permission_Denied );
    return;
  }

  // Form the full path first.
  char userPart[USR_MAX_PATH_LENGTH_PADDED];
  Utils::getNextToken( nextTokenPtr, userPart, USR_MAX_PATH_LENGTH_PADDED );

  if ( *userPart == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }

  char fullpath[USR_MAX_PATH_LENGTH];
  if ( formFullPath( client, fullpath, USR_MAX_PATH_LENGTH, userPart ) ) {
    client->addToOutput( Msg_550_Bad_Path_Or_File );
    return;
  }

  struct stat statbuf;
  int rc = stat( fullpath, &statbuf );

  if ( rc == 0 ) {

    // Ok, it exists

    if ( S_ISDIR(statbuf.st_mode) ) {
      if ( rmdir( fullpath ) ) {
        client->addToOutput_var( Msg_550_Error_Removing_v, userPart );
      }
      else {
        client->addToOutput_var( Msg_250_Cmd_Successful_v, "RMD" );
        addToScreen( 1, "(%lu) RMD %s\n", client->sessionId, fullpath );
      }
    }
    else {
      client->addToOutput_var( "550 %s: not a directory" _NL_, userPart );
    }

  }
  else {
    client->addToOutput_var( Msg_550_Bad_File_v, userPart );
  }

}

void doMkd( FtpClient *client, char *nextTokenPtr ) {

  // Permission check
  if ( client->user.cmd_MKD == 0 ) {
    client->addToOutput( Msg_550_Permission_Denied );
    return;
  }

  if ( (strcmp(client->user.uploaddir, "[ANY]") != 0) && strnicmp( client->user.uploaddir, client->cwd, strlen(client->user.uploaddir) ) != 0 ) {
      client->addToOutput_var( "550 You need to be in the %s directory to create directories" _NL_, client->user.uploaddir );
      return;
  }

  // Form the full path first.
  char userPart[USR_MAX_PATH_LENGTH_PADDED];
  Utils::getNextToken( nextTokenPtr, userPart, USR_MAX_PATH_LENGTH_PADDED );

  if ( *userPart == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }


  // Reserve one char for the trailing slash that we add to paths
  char fullpath[USR_MAX_PATH_LENGTH];
  if ( formFullPath( client, fullpath, USR_MAX_PATH_LENGTH-1, userPart ) ) {
    client->addToOutput( Msg_550_Bad_Path_Or_File );
    return;
  }

  struct stat statbuf;
  int rc = stat( fullpath, &statbuf );

  int ftpRootLen = strlen(client->ftproot);

  char tmpPath[USR_MAX_PATH_LENGTH];
  convertToUserPath( fullpath, tmpPath );

  if ( rc != 0 ) {

    // Does not exist yet

    if ( mkdir( fullpath ) ) {
      client->addToOutput_var( "550 Error creating %s" _NL_, (tmpPath+ftpRootLen) );
    }
    else {
      client->addToOutput_var( "257 %s created" _NL_, (tmpPath+ftpRootLen) );
      addToScreen( 1, "(%lu) MKD %s\n", client->sessionId, tmpPath );
    }

  }
  else {
    client->addToOutput_var( Msg_550_Already_Exists_v, (tmpPath+ftpRootLen) );
  }

}




void doRnfr( FtpClient *client, char *nextTokenPtr ) {

  // Permission check
  if ( client->user.cmd_RNFR == 0 ) {
    client->addToOutput( Msg_550_Permission_Denied );
    return;
  }

  // Form the full path first.
  char userPart[USR_MAX_PATHFILE_LENGTH_PADDED];
  Utils::getNextToken( nextTokenPtr, userPart, USR_MAX_PATHFILE_LENGTH_PADDED );

  if ( *userPart == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }

  char fullpath[USR_MAX_PATHFILE_LENGTH];
  if ( formFullPath( client, fullpath, USR_MAX_PATHFILE_LENGTH, userPart ) ) {
    client->addToOutput( Msg_550_Bad_Path_Or_File );
    return;
  }

  struct stat statbuf;
  int rc = stat( fullpath, &statbuf );

  if ( rc == 0 ) {
    // Ok, it exists
    strcpy( client->filespec, fullpath );
    client->addToOutput( "350 File or directory exists, ready for destination name" _NL_ );
    client->state = FtpClient::RnfrSent;
  }
  else {
    client->addToOutput_var( Msg_550_Bad_File_v, userPart );
  }

}



void doRnto( FtpClient *client, char *nextTokenPtr ) {

  // Form the full path first.
  char userPart[USR_MAX_PATHFILE_LENGTH_PADDED];
  Utils::getNextToken( nextTokenPtr, userPart, USR_MAX_PATHFILE_LENGTH_PADDED );

  if ( *userPart == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }

  char fullpath[USR_MAX_PATHFILE_LENGTH];
  if ( formFullPath( client, fullpath, USR_MAX_PATHFILE_LENGTH, userPart ) ) {
    client->addToOutput( Msg_550_Bad_Path_Or_File );
    return;
  }

  struct stat statbuf;
  int rc = stat( fullpath, &statbuf );

  if ( rc != 0 ) {

    // Good, it does not exist yet

    if ( rename( client->filespec, fullpath ) == 0 ) {
      addToScreen( 1, "(%lu) RNTO %s to %s\n", client->sessionId, client->filespec, fullpath );
      client->addToOutput_var( Msg_250_Cmd_Successful_v, "Rename" );
    }
    else {
      client->addToOutput( "550 Rename failed" _NL_ );
    }

  }
  else {
    client->addToOutput_var( Msg_550_Already_Exists_v, userPart );
  }

}







// RFC 3659
//
// Might need to remove this because we really don't want to scan files
// to see how they are going to change when we do ASCII vs. BIN transfers.

void doSize( FtpClient *client, char *nextTokenPtr ) {

  // Form the full path first.
  char userPart[USR_MAX_PATHFILE_LENGTH_PADDED];
  Utils::getNextToken( nextTokenPtr, userPart, USR_MAX_PATHFILE_LENGTH_PADDED );

  if ( *userPart == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }

  char fullpath[USR_MAX_PATHFILE_LENGTH];
  if ( formFullPath( client, fullpath, USR_MAX_PATHFILE_LENGTH, userPart ) ) {
    client->addToOutput( Msg_550_Bad_Path_Or_File );
    return;
  }

  struct stat statbuf;
  int rc = stat( fullpath, &statbuf );

  if ( rc == 0 ) {

    // Ok, it's a file.

    if ( S_ISREG(statbuf.st_mode) ) {

      if ( client->asciiMode ) {
        client->addToOutput_var( "550 No SIZE information available in ASCII mode" _NL_ );
      }
      else {
        client->addToOutput_var( "213 %lu" _NL_, statbuf.st_size );
      }

    }                          
    else {
      client->addToOutput_var( Msg_550_Not_Plain_File_v, userPart );
    } 
    
  }
  else {
    client->addToOutput_var( Msg_550_Bad_File_v, userPart );
  }
  
}




// DOS has a maximum path length of 63 chars when setting or reading
// the current path.  Assume that includes the first '/', but not
// the drive letter and filename.  So then the full format for a
// filespec is:
//
//        1       +   1   +  63  + 1 +     12        +  1
//   drive_letter + colon + path + / + filename.ext + null
//
// We don't actually change the path while we are running; we just want
// to make sure that our file specs for opens and closes stay within
// legal limits.  So the grand total that should work for any filespec
// is 79 chars, including the trailing null.


// Fixme: comments
// If not in sandbox ..
//
// - The CWD always starts with a drive leter and / (an absolute path).
//
// - If they use a drive letter they have to use an absolute path.
//   We don't remember anything except the path for the current drive.
//
// - If they use a path starting with a / then be nice and put the
//   current drive letter in front of it.
//
// - If they give us anything else it is a relative path; it gets
//   appended to the CWD.

// If in the sandbox ..
//
// - Their CWD always starts with / (an absolute path).
//
// - Don't help them by putting a drive letter in front of a /.


void doCwd( FtpClient *client, char *nextTokenPtr ) {


  // Give them 20 extra bytes to deal with things like /.. and /.

  char parm[ USR_MAX_PATH_LENGTH + 20 ];
  Utils::getNextToken( nextTokenPtr, parm, USR_MAX_PATH_LENGTH + 20 );
  strupr( parm );

  if ( parm[0] == 0 ) {
    client->addToOutput( Msg_501_Invalid_Num_Args );
    return;
  }

  uint8_t isSandbox = ( client->ftproot[0] != 0 );


  // Length should be USR_MAX_PATH_LENGTH, but we need to leave a little
  // extra in case they have a full path and want to use a ".." to back up.
  // We will ensure it is small enough for DOS later.

  char newpath[ USR_MAX_PATH_LENGTH + 20 ];
  newpath[0] = 0;


  // If not absolute prepend the current working directory.
  if ( !isPathAbsolute( parm ) ) {
    strcat( newpath, client->cwd );
  }


  // Is there room to add the input parm to the string?  If not, error out.
  if ( strlen(newpath) + strlen(parm) > (USR_MAX_PATH_LENGTH+20-1) ) {
    client->addToOutput( Msg_550_Path_Too_Long );
    return;
  }

  strcat( newpath, parm );


  // By this point we have the full path as the user sees it.
  //
  // Now validate it and parse out any . or .. components in the path.
  // Note - normalize strips trailing slashes.

  if ( normalizeDir( newpath, USR_MAX_PATH_LENGTH ) ) {
    client->addToOutput_var( "550 \"%s\": Bad path format or too long" _NL_, parm );
    return;
  }


  // If we got through normalize it is a sane path.  If the user is in
  // a sandbox prepend their root directory.

  char fullpath[USR_MAX_PATH_LENGTH];
  fullpath[0] = 0;

  // If the user is in a sandbox prepend the sandbox directory.
  if ( isSandbox ) {
    strcpy( fullpath, client->ftproot );
  }

  // If the path is too long throw an error.  Silently truncating it is
  // going to lead to user confusion.  We need to ensure there is room
  // for a trailing '/' and a terminating null.

  if ( strlen(fullpath) + strlen(newpath) > (USR_MAX_PATH_LENGTH-2) ) {
    client->addToOutput( Msg_550_Path_Too_Long );
    return;
  }

  strcat( fullpath, newpath );



  // Now we have the full path, ready to convert to DOS form.
  //
  // For sandbox users this is ready to go because we enforce that the
  // sandbox starts with /DRIVE_X/ and they couldn't backup out of the
  // sandbox.  Non-sandbox users should have /DRIVE_X/ but it is possible
  // they backed up, in which case they have a '/' for a path.  We'll
  // accept that as a valid path, but we won't let them do any file ops
  // until they pick a drive letter.


  if ( !isSandbox ) {

    // Did they change to the root directory?
    if ( fullpath[0] == '/' && fullpath[1] == 0 ) {
      // Ok, so be it
      client->cwd[0] = '/';
      client->cwd[1] = 0;
      client->addToOutput_var( Msg_250_Cmd_Successful_v, "CWD" );
      return;
    }
      
    // Special case - they are at the root of a drive letter, but are missing
    // the trailing slash.  Add it back on.

    // Normalize would have stripped off any trailing /.  Put it back on no matter
    // what the user input actually is.  (We will scan for valid input next.)

    int tmpLen = strlen(fullpath); 
    fullpath[ tmpLen ] = '/'; fullpath[ tmpLen+1 ] = 0;


    // Now, is the input valid?  Only something of the form /DRIVE_X/ is
    // valid, and it has to be a drive in our valid table.

    if ( !isDrivePrefixPresent( fullpath ) || !isDriveInValidTable( fullpath[7] ) ) {
      client->addToOutput_var( Msg_550_Bad_Drive_Letter_v, parm );
      return;
    }

  }
  

  // By this point we have a full path, including drive letter up front.
  // And it's not the root pseudo directory, so we can test to see if it
  // actually exists.

  // Convert to a full DOS path.  Any trailing directory delimiters will
  // be removed too, except if it is at the root of a drive.
  //
  // We are not going to get a bad return code here.  We already checked for
  // non-sandbox users at the root.  And sandbox users could not have changed
  // to an invalid drive.

  convertToDosPath( fullpath );

  if ( isDirectory( fullpath ) ) {

    // Their input was good.  Remember it as the new path.

    // If there is not a trailing slash on it then add it.  (If you are
    // a non-sandbox user at the root of a drive it is there.  Otherwise
    // we are going to have to add it back.

    int tmpLen = strlen(newpath); 
    if ( newpath[ tmpLen-1 ] != '/' ) {
      newpath[ tmpLen ] = '/'; newpath[ tmpLen+1 ] = 0;
    }

    strcpy( client->cwd, newpath );

    client->addToOutput_var( Msg_250_Cmd_Successful_v, "CWD" );

  }
  else {
    client->addToOutput_var( "550 \"%s\": No such directory" _NL_, parm );
  }

}






static void cleanupDataXferStructs( FtpClient *client ) {

  switch ( client->dataXferType ) {

    case FtpClient::List:
    case FtpClient::Nlist: {
      _dos_findclose( &client->fileinfo );
      break;
    }

    case FtpClient::Retr:
    case FtpClient::Stor:
    case FtpClient::StorA:
    case FtpClient::StorU: {
      if (client->file != NULL) {
        fclose( client->file );
        client->file = NULL;
      }
      break;
    }
  }

  client->dataXferType = FtpClient::NoDataXfer;
}




// formFullPath
//
// Given a current working directory and a filespec form the full path for
// the filespec.  If the filespec was absolute or included ".." directories
// those get handled too.
// 
// Input parms
//
//   client: user data structure
//   outBuffer: where to write the results to
//   maxOutBufferLen: the length of that buffer
//   filespec: the filespect that will be normalized
//
// Returns 0 if the full path and filespec can be constructed.  This doesn't
// mean it is a valid file or directory; only that it is a legal path and
// filespec.
//
//   0 - DOS path returned
//   1 - Bad/inactive drive letter
//   2 - Unix style path returned 
//   3 - path too long
//   4 - syntax error in the path

int formFullPath( FtpClient *client, char *outBuffer, int maxOutBufferLen, char *filespec ) {

  uint8_t isSandbox = ( client->ftproot[0] != 0 );

  // Make this a little bigger than the stated max to accommodate things like /.. and /.
  // which will be compressed out during the normalize phase.

  char newpath[ USR_MAX_PATHFILE_LENGTH + 20 ];
  newpath[0] = 0;


  // If incoming filespec was not absolute prepend the current working directory.
  if ( !isPathAbsolute( filespec ) ) {
    strcat( newpath, client->cwd );
  }

  // Add the user filespec, and be careful to account for the required NULL.
  if ( strlen(newpath) + strlen(filespec) >= (USR_MAX_PATHFILE_LENGTH + 20) ) {
    // Going to be too long even for our internal buffer; return an error.
    return 3;
  }
  strcat( newpath, filespec );


  // By this point we have the full path as the user sees it.  Now compress out
  // the redundand .. and . components of it and check the validity of the
  // components of it.  Doing this before prepending the sandbox prefix ensures
  // that the user can't backup out of the sandbox.

  if ( normalizeDir( newpath, USR_MAX_PATHFILE_LENGTH + 20 ) ) {
    return 4;
  }


  // The full path as seen by the user is sane.  If the user is in a sandbox
  // we need to prepend the sandbox directory.  Now is where we start altering
  // the user's output buffer.

  if ( isSandbox ) {

    // If the user is in a sandbox prepend the sandbox directory.
    strcpy( outBuffer, client->ftproot );

  }
  else {

    // Not in the sandbox.

    outBuffer[0] = 0;

    // Special case for /DRIVE_X because normalize strips the trailing slash
    // and we want to ensure it is there before calling convertToDosPath
    // or it won't be recognized as a valid drive prefix.

    if ( (strncmp(newpath, "/DRIVE_", 7 ) == 0) && strlen(newpath) == 8 ) {
      newpath[8] = '/';
      newpath[9] = 0;
    }

  }


  // For sandbox users check if adding the userspec to the sandbox part is
  // going to exceed the target length.  (Same check is valid for non-sandbox
  // users; they just have a zero length sandbox component.)

  if ( strlen(outBuffer) + strlen(newpath) >= maxOutBufferLen ) {
    return 3;
  }

  strcat( outBuffer, newpath );


  // Have the full path now.  ConvertToDosPath will remove the trailing slash
  // from anything it gets other than a root drive letter.  If convertToDosPath
  // is given invalid input (something without a drive letter up front) it returns
  // a 1.
  //
  // The length is guaranteed to be safe; convertToDos actually takes around 6
  // bytes off the string length by converting /DRIVE_X to X: .

  // convertToDosPath returns 0 if good, 1 if bad drive letter, 2 if Unix path
  return convertToDosPath( outBuffer );

  return 0;
}



//   1 - illegal use of drive letters
//   2 - need to use absolute path with a drive letter
//   3 - path too long
//   4 - syntax error in the path

int formFullPath_DataXfer( FtpClient *client, char *outBuffer, int outBufferLen, char *filespec ) {

  int rc = formFullPath( client, outBuffer, outBufferLen, filespec );

  if ( rc == 2 && (client->dataXferType == FtpClient::List || client->dataXferType == FtpClient::Nlist) ) {
    // Allow it - harmless
    rc = 0;
  }

  switch ( rc ) {

    case 0: { break; }
    case 1:
    case 4: { endDataTransfers( client, Msg_550_Bad_Path_Or_File ); break; }
    case 2: { endDataTransfers( client, "550 No file ops supported in root directory" _NL_ ); break; }
    case 3: { endDataTransfers( client, Msg_550_Path_Too_Long ); break; }

    default: { endDataTransfers( client, "550 Unknown error" _NL_ ); break; }
  }

  return rc;
}




void doXfer( FtpClient *client, char *nextTokenPtr, FtpClient::DataXferType listType ) {

  // Permission checks

  if ( ( (listType == FtpClient::Stor)  && (client->user.cmd_STOR == 0) )
    || ( (listType == FtpClient::StorA) && (client->user.cmd_APPE == 0) )
    || ( (listType == FtpClient::StorU) && (client->user.cmd_STOU == 0) ) )
  {
    client->addToOutput( Msg_550_Permission_Denied );
    return;
  }

  if ( (listType == FtpClient::Stor) || (listType == FtpClient::StorA) || (listType == FtpClient::StorU) ) {

    if ( (strcmp(client->user.uploaddir, "[ANY]") != 0) && strnicmp( client->user.uploaddir, client->cwd, strlen(client->user.uploaddir) ) != 0 ) {
      client->addToOutput_var( "550 You need to be in the %s directory to upload" _NL_, client->user.uploaddir );
      return;
    }

  }

  if ( client->dataXferState == FtpClient::DL_NotActive ) {
    client->dataXferState = FtpClient::DL_Init;
    client->dataXferType = listType;
    doDataXfer( client, nextTokenPtr );
  }
  else {
    client->addToOutput( Msg_425_Cant_Open_Conn );
  }

}


// If there is any activity on the data socket then this code gets run.
//
// - If the data sockets are being closed for either natural or unnatural
//   reasons we will detect that just wait for them to go closed.
// - If this is the first time in we'll initialize everything.
// - If they closed their side we'll start the close process
// - If we had unsent output, we'll send it.
// - If we need to generate more output and send it, we'll do that too.
// - If we run out of output, we'll start the close process.


void doDataXfer( FtpClient *client, char *parms ) {

  if ( client->dataXferState == FtpClient::DL_Closing ) {

    // We are supposed to be cleaning up.  Wait for both sockets to close.
    // After they close cleanup the fileinfo structure or open file pointer.
    // If a data transfer ever got past the Init stage then is should come
    // through here to clean up.

    // If the client had sent a PORT command wipe out the port so that we
    // know // they are forced to set it again for the next transfer.  We
    // don't need to wipe out the whole address - having a port of zero is
    // enough of an indicator.

    client->dataPort = 0;

    if ( client->ds != NULL ) {

      if ( client->ds->isCloseDone( ) ) {

        // Great, return the socket and close up.
        TcpSocketMgr::freeSocket( client->ds );
        client->ds = NULL;

        cleanupDataXferStructs( client );
        client->dataXferState = FtpClient::DL_NotActive;
      }

    }
    else {
      // Socket was never allocated so we don't wait for it to close.
      cleanupDataXferStructs( client );
      client->dataXferState = FtpClient::DL_NotActive;
    }

  } // end if DL_Closing



  else if ( client->dataXferState == FtpClient::DL_Init ) {

    client->connectStarted = time( NULL );
    char *nextTokenPtr = Utils::getNextToken( parms, client->filespec, USR_MAX_PATHFILE_LENGTH );


    if ( client->dataXferType == FtpClient::List || client->dataXferType == FtpClient::Nlist ) {

      // If the client sends options for /bin/ls ignore them.  This might be an error
      // if the client was looking for all files starting with what looks like /bin/ls
      // options, but I don't know of a better way to deal with these options.  (A sane
      // client should not send them.)

      if ( *client->filespec == '-' ) {
        Utils::getNextToken( nextTokenPtr, client->filespec, USR_MAX_PATHFILE_LENGTH );
      }

    }



    // If it is a RETR and the file doesn't exist, cut them off early.
    // If it is a STOR and the file does exist, cut them off early.
    // We don't check APPE because it doesn't matter if the file exists or
    // not.  We don't check STOU either because it doesn't look at the
    // filename.

    char fullpath[USR_MAX_PATHFILE_LENGTH_PADDED];

    if ( client->dataXferType == FtpClient::Retr ) {
      if ( formFullPath_DataXfer( client, fullpath, USR_MAX_PATHFILE_LENGTH, client->filespec ) ) {
        return;
      }
      if ( !isFile( fullpath ) ) {
        endDataTransfers( client, Msg_550_Bad_Path_Or_File );
        return;
      }
    }
    else if ( client->dataXferType == FtpClient::Stor ) {
      if ( formFullPath_DataXfer( client, fullpath, USR_MAX_PATHFILE_LENGTH, client->filespec ) ) {
        return;
      }
      if ( doesExist( fullpath ) ) {
        endDataTransfers( client, "553 File exists already" _NL_ );
        return;
      }
    }
      

    // If we have a listening socket open already then we must be in PASV
    // mode and waiting for a client to connect.  Move to the next state
    // and continue to wait for the connection.
    //
    // If it was PASV and they connected already do the same thing.

    if ( client->ls != NULL ) {
      // Listening socket is open, still waiting for a data connection.
      client->activeConnect = 0;
      client->dataXferState = FtpClient::DL_Connecting;
    }
    else if ( client->ds != NULL ) {
      // Listening socket is not open, but we have a data socket already.
      client->activeConnect = 0;
      client->dataXferState = FtpClient::DL_Connected;
    }
    else {

      // Not listening and not connected yet.  That means we should try
      // to connect.

      if ( client->dataPort == 0 ) {
        endDataTransfers( client, Msg_425_Send_Port );
        return;
      }

      client->ds = TcpSocketMgr::getSocket( );
      if ( client->ds == NULL ) {
        TRACE_WARN(( "Ftp (%lu) Could not allocate a data socket\n", client->sessionId ));
        endDataTransfers( client, Msg_425_Cant_Open_Conn );
        return;
      }

      if ( client->dataXferType == FtpClient::Stor ||
           client->dataXferType == FtpClient::StorA ||
           client->dataXferType == FtpClient::StorU )
      {

        // Setup receive buffer for the socket.  Fixme: This is a waste
        // for dir listings and file sends; 
        if ( client->ds->setRecvBuffer( Data_Rcv_Buf_Size ) ) {
          TRACE_WARN(( "Ftp (%lu) Could not allocate data socket receive buffer\n", client->sessionId ));
          endDataTransfers( client, Msg_425_Cant_Open_Conn );
          return;
        }

      }

      // Start a non-blocking connect
      int16_t rc = client->ds->connectNonBlocking( (FtpSrv_Control_Port-1), client->dataTarget, client->dataPort );
      if ( rc ) {
        TRACE(( "Ftp (%lu) Initial connect call on data socket failed\n", client->sessionId ));
        endDataTransfers( client, Msg_425_Cant_Open_Conn );
        return;
      }

      client->activeConnect = 1;

      // Have to wait for the connect to complete now.
      client->dataXferState = FtpClient::DL_Connecting;
    }

  }




  else if ( client->dataXferState == FtpClient::DL_Connecting ) {

    // We are waiting for the data connection.  Check for timeout.

    if ( client->activeConnect == 0 ) {
      if ( client->ls != NULL ) {
        if ( time( NULL ) - client->connectStarted > 10 ) {
          TRACE(( "Ftp (%lu) Passive data connection timed out\n", client->sessionId ));
          endDataTransfers( client, Msg_425_Cant_Open_Conn );
        }
        // If you made it here you are still waiting and not timed out yet.
      }
      else {
        // We have our data connection - move to next state.
        client->dataXferState = FtpClient::DL_Connected;
      }

    }
    else {
      // This was a nonblocking connect that we started.
      if ( client->ds->isConnectComplete( ) ) {
        client->dataXferState = FtpClient::DL_Connected;
      }
      else {
        if ( time( NULL ) - client->connectStarted > 10 ) {
          TRACE(( "Ftp (%lu) Nonblocking connected for data socket timed out\n", client->sessionId ));
          endDataTransfers( client, Msg_425_Cant_Open_Conn );
        }
        // If you made it here you are still waiting and not timed out yet.
      }

    }

  }




  else if ( client->dataXferState == FtpClient::DL_Connected ) {

    // Ok, we have a data connection now.  Setup to actually start
    // transferring data.

    const char *dataTypeStr = BIN_str;
    if ( client->asciiMode ) {
      dataTypeStr = ASCII_str;
    }

    switch ( client->dataXferType ) {

      case FtpClient::List:
      case FtpClient::Nlist: {

        client->addToOutput( Msg_150_Send_File_List );

        char fullpath[USR_MAX_PATHFILE_LENGTH_PADDED];
        if ( formFullPath_DataXfer( client, fullpath, USR_MAX_PATHFILE_LENGTH_PADDED, client->filespec ) ) {
          return;
        }

        // Stat it.  If it is a directory then add the *.* to the end.
        // If it's not valid don't worry about it - they will get an empty
        // listing.

        if ( strlen(fullpath) < USR_MAX_PATH_LENGTH ) {
          if ( isDirectory( fullpath ) ) {
            if ( fullpath[strlen(fullpath)-1] == '\\' ) {
              strcat( fullpath, "*.*" );
            }
            else {
              strcat( fullpath, "\\*.*" );
            }
          }
        }

        client->noMoreData = _my_dos_findfirst( fullpath, (_A_NORMAL | _A_SUBDIR), &client->fileinfo);

        break;
      }

      case FtpClient::Retr: {

        // Fix me at some point.  We moved this code earlier, but have no way
        // pass the full filename here.  Just do it again, and it should not
        // fail.

        char fullpath[USR_MAX_PATHFILE_LENGTH_PADDED];
        if ( formFullPath_DataXfer( client, fullpath, USR_MAX_PATHFILE_LENGTH_PADDED, client->filespec ) ) {
          return;
        }

        if ( isFile( fullpath ) ) {
          client->addToOutput_var( "150 %s type File RETR started" _NL_, dataTypeStr );
        }
        else {
          endDataTransfers( client, Msg_550_Bad_Path_Or_File );
          return;
        }

        if ( client->asciiMode ) {
          client->file = fopen( fullpath, "r" );
        }
        else {
          client->file = fopen( fullpath, "rb" );
        }

        if ( client->file == NULL ) {
          endDataTransfers( client, Msg_550_Filesystem_error );
          return;
        }

        addToScreen( 1, "(%lu) %s RETR started for %s\n", client->sessionId, dataTypeStr, fullpath );

        client->noMoreData = 0;
        break;
      }


      case FtpClient::Stor:
      case FtpClient::StorA: {

        char fullpath[USR_MAX_PATHFILE_LENGTH_PADDED];
        if ( formFullPath_DataXfer( client, fullpath, USR_MAX_PATHFILE_LENGTH_PADDED, client->filespec ) ) {
          return;
        }

        if ( client->dataXferType == FtpClient::Stor ) {

          if ( doesExist( fullpath ) ) {
            endDataTransfers( client, "550 File exists already" _NL_ );
            return;
          }

          client->addToOutput_var( "150 %s type File STOR started" _NL_, dataTypeStr );

        }
        else {

          // If it exists, then it must be a file.  If it doesn't exist that is ok too.
          // I assume that STAT will pick up the special filenames; if not, this doesn't
          // work.

          if ( doesExist(fullpath) && !isFile(fullpath) ) {
            endDataTransfers( client, "550 Target exists but is not a normal file" _NL_ );
            return;
          }

          client->addToOutput_var( "150 %s type File APPE started" _NL_, dataTypeStr );
        }

        char filemode[3];
        if ( client->dataXferType == FtpClient::Stor ) { filemode[0] = 'w'; } else { filemode[0] = 'a'; }
        if ( client->asciiMode ) { filemode[1] = 't'; } else { filemode[1] = 'b'; }
        filemode[2] = 0;

        client->file = fopen( fullpath, filemode );

        if ( client->file == NULL ) {
          endDataTransfers( client, Msg_550_Filesystem_error );
          return;
        }

        addToScreen( 1, "(%lu) %s STOR or APPE started for %s\n", client->sessionId, dataTypeStr, fullpath );

        client->noMoreData = 0;

        break;
      }


      case FtpClient::StorU: {

        // Need to create a unique filename in the selected directory.

        char filename[13] = "U0000000.QUE";
        char fullpath[USR_MAX_PATHFILE_LENGTH];

        int attempts = 0;

        while ( attempts < 5 ) {

          // Generate a semi-random filename
          for ( int i=1; i < 8; i++ ) { filename[i] = (rand( ) % 10) + 48; }

          // Should not fail
          if ( formFullPath_DataXfer( client, fullpath, USR_MAX_PATHFILE_LENGTH, filename ) ) {
            return;
          }

          // Stat it to see if it is unique
          struct stat statbuf;
          int rc = stat( fullpath, &statbuf );
          if ( rc ) break;

          attempts++;
        }

        if ( attempts == 5 ) {
          endDataTransfers( client, "550 Cant generate a unique name" _NL_ );
          return;
        }

        char filemode[3];
        filemode[0] = 'w';
        if ( client->asciiMode ) { filemode[1] = 't'; } else { filemode[1] = 'b'; }
        filemode[2] = 0;

        client->file = fopen( fullpath, filemode );

        if ( client->file == NULL ) {
          endDataTransfers( client, Msg_550_Filesystem_error );
          return;
        }

        client->addToOutput_var( "150 %s type STOU started, Filename is %s%s" _NL_, dataTypeStr, client->cwd, filename );

        client->noMoreData = 0;

        addToScreen( 1, "(%lu) %s STOU started for %s\n", client->sessionId, dataTypeStr, fullpath );

        break;
      }

    } // end switch


    // Common initialization for all of the data transfer types

    client->dataXferState = FtpClient::DL_Active;
    client->bytesSent = 0;
    client->fileBufferIndex = 0;

    // Used only by receive path
    client->bytesRead = 0;
    client->bytesToRead = Filebuffer_Size;

  }



  else if ( client->dataXferState == FtpClient::DL_Active ) {

    // Did the data socket close on us?
    //
    // If we were sending data (LIST, NLST or RETR) and they closed the
    // connection it is an error and there is no need to wait.  If we
    // were receiving data (STOR) then this means end of file, but we
    // have to wait until all data is read from the socket.

    if ( client->ds->isRemoteClosed( ) ) {

      if ( (client->dataXferType != FtpClient::Stor) && (client->dataXferType != FtpClient::StorA) && (client->dataXferType != FtpClient::StorU) ) {
        TRACE(( "(%lu) Data socket closed on us\n", client->sessionId ));
        endDataTransfers( client, Msg_426_Request_term );
      }
      else {
        client->noMoreData = 1;
      }

    }



    // We are primed and ready to read our first directory entries, or we have
    // re-entered because we gave up control to give somebody else a chance.


    if ( (client->dataXferType != FtpClient::Stor) && (client->dataXferType != FtpClient::StorA) && (client->dataXferType != FtpClient::StorU) ) {

      // Are there leftover bytes to send from the last time we were called?
      if ( client->fileBufferIndex ) {
        client->bytesSent += client->ds->send( (uint8_t *)(client->fileBuffer) + client->bytesSent, client->fileBufferIndex - client->bytesSent );
        // client->bytesSent += client->ds->send( (uint8_t *)(client->fileBuffer) + client->bytesSent, 1 );
        if ( client->bytesSent == client->fileBufferIndex ) {
          // Good - we cleared out the previous data.
          client->bytesSent = 0;
          client->fileBufferIndex = 0;
        }
        else {
          // Still blocked.  Give somebody else a chance.
          return;
        }
      }


      // By this point any previous data that needed to be sent has been sent.
      // Build up a new string to send.


      switch ( client->dataXferType ) {

        case FtpClient::Nlist:
        case FtpClient::List: {

          if ( !client->noMoreData ) {

            // Loop until we fill the buffer or there are no more directory entries.
            while ( 1 ) {

                if ( strcmp( client->fileinfo.name, "." ) != 0 && strcmp( client->fileinfo.name, ".." ) != 0 ) {

                  // Format file attributes
                  char attrs[] = "-rwxrwxrwx"; // Default
                  if ( client->fileinfo.attrib & _A_SUBDIR ) { attrs[0] = 'd'; }
                  if ( client->fileinfo.attrib & _A_RDONLY ) { attrs[2] = attrs[5] = attrs[8] = '-'; }

                  ftime_t ft;
                  ft.us = client->fileinfo.wr_time;

                  fdate_t fd;
                  fd.us = client->fileinfo.wr_date;

                  uint16_t rc = 0;;
                  if ( client->dataXferType == FtpClient::List ) {
                    if ( fd.fields.year + 1980 != Current_Year ) {
                      rc = sprintf( (char *)(client->fileBuffer + client->fileBufferIndex), "%s 1 ftp ftp %10lu %s %2d  %4d %s" _NL_,
                             attrs, client->fileinfo.size, Months[fd.fields.month-1], fd.fields.day, (fd.fields.year + 1980),
                             client->fileinfo.name );
                    }
                    else {
                      rc = sprintf( (char *)(client->fileBuffer + client->fileBufferIndex), "%s 1 ftp ftp %10lu %s %2d %02d:%02d %s" _NL_,
                             attrs, client->fileinfo.size, Months[fd.fields.month-1], fd.fields.day, ft.fields.hours, ft.fields.minutes,
                             client->fileinfo.name );
                    }
                  }
                  else {
                    rc = sprintf( (char *)(client->fileBuffer + client->fileBufferIndex), "%s" _NL_, client->fileinfo.name );
                  }

                  client->fileBufferIndex += rc;

                } // end if . or ..


              if ( (client->noMoreData = _my_dos_findnext( &client->fileinfo )) ) {
                _dos_findclose( &client->fileinfo );
                break;
              }

              if ( (Filebuffer_Size - client->fileBufferIndex) < 80 ) {
                break;
              }

            } // end build string up

          } // end no more dir entries

          break;
        }

        case FtpClient::Retr: {

          int rc = fread( client->fileBuffer, 1, Filebuffer_Size, client->file );
          if ( rc ) {
            client->fileBufferIndex = rc;
          }
          if ( feof( client->file ) ) client->noMoreData = 1;

          break;
        }


      } // end switch on data xfer type

      // Send the bytes out?
      if ( client->fileBufferIndex ) {
        client->bytesSent = client->ds->send( (uint8_t *)(client->fileBuffer), client->fileBufferIndex );
        // client->bytesSent = client->ds->send( (uint8_t *)(client->fileBuffer), 1 );
        if ( client->bytesSent == client->fileBufferIndex ) {
          // Good - we cleared out the previous data.
          client->bytesSent = 0;
          client->fileBufferIndex = 0;
        }
        else {
          // Still blocked.  Give somebody else a chance.
          return;
        }
      }

    } // end if sending data

    else {  // Receiving

      int16_t recvRc;

      while ( recvRc = client->ds->recv( client->fileBuffer+client->bytesRead, client->bytesToRead ) ) {

        if ( recvRc > 0 ) {

          client->bytesRead += recvRc;
          client->bytesToRead -= recvRc;

          if ( client->bytesToRead == 0 ) {
            // Buffered writing - just filled our buffer to write it now.  Hopefully it is a nice
            // multiple of 4K so that it writes quickly.
            size_t rc = fwrite( client->fileBuffer, 1, client->bytesRead, client->file );
            if ( rc != client->bytesRead ) {
              endDataTransfers( client, Msg_550_Filesystem_error );
              return;
            }
            client->bytesToRead = Filebuffer_Size;
            client->bytesRead = 0;

          }

        }
        else if ( recvRc < 0 ) {
          endDataTransfers( client, Msg_550_Filesystem_error );
          return;
        }

      } // end while

      // Flush remaining bytes
      if ( client->noMoreData && recvRc == 0 ) {
        int rc = fwrite( client->fileBuffer, 1, client->bytesRead, client->file );
        if (rc != client->bytesRead ) {
          endDataTransfers( client, Msg_550_Filesystem_error );
          return;
        }

      }

    }


    // If you got here there is no leftover data to send.  If there are no more
    // dir entries, pack up and go home.

    if ( client->noMoreData ) {
      client->ds->closeNonblocking( );

      switch ( client->dataXferType ) {
        case FtpClient::List: { Stat_LIST++; break; }
        case FtpClient::Nlist: { Stat_NLST++; break; }
        case FtpClient::Retr:  { Stat_RETR++; addToScreen( 1, "(%lu) RETR completed\n", client->sessionId ); break; }
        case FtpClient::Stor:  { Stat_STOR++; addToScreen( 1, "(%lu) STOR completed\n", client->sessionId ); break; }
        case FtpClient::StorA: { Stat_APPE++; addToScreen( 1, "(%lu) APPE completed\n", client->sessionId ); break; }
        case FtpClient::StorU: { Stat_STOU++; addToScreen( 1, "(%lu) STOU completed\n", client->sessionId ); break; }
      }

      client->dataXferState = FtpClient::DL_Closing;
      client->addToOutput( Msg_226_Transfer_Complete );
    }

  }

}




// doAbort needs to terminate any current data transfer, including the
// listening socket if it is in use.

void doAbort( FtpClient *client ) {

  TRACE(( "Ftp (%lu) doAbort\n" ));
  endDataTransfers( client, Msg_426_Request_term );
  client->addToOutput( Msg_226_ABOR_Complete );

}



// endDataTransfers
//
// If a data transfer was active this will start the close process and
// send a message to the control connection.
//
// If we were only in PASV state listening for a connection then it goes
// away without a message.

void endDataTransfers( FtpClient *client, char *msg ) {

  TRACE(( "Ftp (%lu) endDataTransfers  cs: (%08lx)  ds: (%08lx)  ls: (%08lx)\n",
          client->sessionId, client->cs, client->ds, client->ls ));

  // If there was a socket open for listening close it and return it.
  if ( client->ls != NULL ) {
    client->ls->close( ); // Should be immediate; nonBlocking is not needed.
    TcpSocketMgr::freeSocket( client->ls );
    client->ls = NULL;
  }


  // There might be a data connection even if there is no transfer in progress.
  // This happens when the client has sent PASV and has made the data
  // connection, but has not send a command that uses the data socket.

  if ( client->ds != NULL ) {
    // Throw away any data that might come in from this point forward
    client->ds->shutdown( TCP_SHUT_RD );
    client->ds->closeNonblocking( );

    // Ensure the state goes to closing so that we will drive it to completion.
    // This is done down below.
  }


  // If the user had started a data transfer send them the cancelled msg.

  if ( client->dataXferState != FtpClient::DL_NotActive ) {
    client->addToOutput( msg );
  }

  client->dataXferState = FtpClient::DL_Closing;
}




// endSession
//
// Mark the session as ending.  Besides ending the session we need to
// end any background processing like file transfers and directory
// listings.
//
// The main loop will wait for all of the client sockets to close and then
// recycle the client data structure.

void endSession( FtpClient *client ) {

  TRACE(( "Ftp (%lu) endSession\n", client->sessionId ));
  endDataTransfers( client, Msg_426_Request_term );

  // Mark the client as closing.
  client->state = FtpClient::ClosingPushOutput;

  // Now we just need to wait for everything to close
}





static char DosChars[] = "!@#$%^&()-_{}`'~*?";

static int isValidDosChar( char c ) {
  if ( isalnum( c ) || (c>127) ) return 1;
  for ( int i=0; i < 18; i++ ) {
    if ( c == DosChars[i] ) return 1;
  }
  return 0;
}


static int isValidDosFilename( const char *filename ) {

  if (filename==NULL) return 0;

  // Special case - check for . and ..
  if ( strcmp( filename, "." ) == 0 || strcmp( filename, ".." ) == 0 ) return 1;

  int len = strlen(filename);

  if ( len == 0 ) return 0;

  if ( !isValidDosChar( filename[0] ) ) return 0;

  int i;
  for ( i=1; (i<8) && (i<len) ; i++ ) {
    if ( filename[i] == '.' ) break;
    if ( !isValidDosChar( filename[i] ) ) return 0;
  }

  if ( i == len ) return 1;

  if ( filename[i] != '.' ) return 0;

  i++;
  int j;
  for ( j=0; (j+i) < len; j++ ) {
    if ( !isValidDosChar( filename[j+i] ) ) return 0;
  }

  if ( j > 3 ) return 0;

  return 1;
}





// Normalize takes a user path and breaks it up into components.  Along the
// way it checks each component for validity.  At the end if everything
// is valid it rewrites the normalized path.
//
// A user path looks like a Unix path - there are no DOS drive letters.
//
// The output usually does not have a trailing / as we don't know if it is
// a directory or a filename at the end.  The exception is the root directory.
// The caller should add a '/' to the end if they want to denote it is a directory.

static char component[32][13];

int normalizeDir( char *buffer, int bufferLen ) {

  int top = 0;

  char tmp[13];

  int bufferIndex = 0;

  // Enforce a leading slash
  if ( buffer[bufferIndex] != '/' ) return 1;

  bufferIndex++;

  while ( 1 ) {

    if ( top == 20 ) return 1;

    // Read next component from the path

    char tmp[13];
    int tmpIndex = 0;
    while ( 1 ) {

      if ( buffer[bufferIndex] == 0 ) {
        // Out of data
        break;
      }

      if ( buffer[bufferIndex] == '/' ) {
        bufferIndex++;
        break;
      }

      if ( tmpIndex > 12 ) {
        return 1;
      }

      tmp[tmpIndex++] = buffer[bufferIndex++];

    }
    tmp[tmpIndex] = 0;

    if ( tmp[0] == 0 ) {
      if ( buffer[bufferIndex] == 0 ) {
        // Empty component and end of input ..  end main loop.
        break; 
      }
      else {
        // Empty component, but not end of input.  Must have been
        // back to back slashes.  We don't tolerate this.
        return 1;
      }
    }

    if ( !isValidDosFilename( tmp ) ) {
      return 1;
    }

    if ( strcmp( tmp, ".." ) == 0 ) {
      if ( top > 0 ) {
        top--;
      }
    }
    else if ( strcmp( tmp, "." ) == 0 ) {
      // Do nothing
    }
    else if ( tmp[0] ) {
      // Add it to the stack
      strcpy(component[top], tmp );
      top++;
    }

  }


  buffer[0] = 0;

  // If we wind up with no components at all then we are at the root.
  // Otherwise, construct the normalized path.

  if ( top == 0 ) {
    buffer[0] = '/';
    buffer[1] = 0;
  }
  else {
    for ( int i=0; i < top; i++ ) {

      // If we are going to overflow the buffer by adding another
      // delimiter and component then return an error.

      if ( strlen(buffer) + 1 + strlen(component[i]) >= bufferLen ) return 1;

      strcat( buffer, "/" );
      strcat( buffer, component[i] );
    }
  }

  // Uppercase the output
  strupr( buffer );

  return 0;
}




// Take a full user path (including the prefix for the sandbox) and convert it
// to a full DOS path.  The output will always be shorter than the input so
// don't worry about buffer length checking.
//
// If the full user path does not start with /DRIVE_X/ then this is not going
// to be a valid DOS path no matter what we do.  It can only be something in
// the root path, which is a pseudo directory.  If that is the case leave it
// unchanged and return something to the user to indicate the potential problem.
// The only code that can deal with that is doCwd and the file listing routines;
// nothing else can work there.
//
// Returns:
//   0 if no problems
//   1 if bad drive letter
//   2 if Unix style path was passed in


int convertToDosPath ( char *buffer_p ) {

  if ( !isDrivePrefixPresent( buffer_p ) ) {
    buffer_p[0] = '/'; buffer_p[1] = 0;
    return 2;
  }

  if ( !isDriveInValidTable( buffer_p[7] ) ) {
    return 1;
  }

  char rc[80];
  rc[0] = buffer_p[7];
  rc[1] = ':';
  rc[2] = '\\';

  int i=3, j=9;

  // Fix an obscure problem when somebody uses a sandbox but sets it to the
  // root of a drive.  We wind up with a double slash which DOS gets upset
  // about.

  bool lastCharWasSlash = true;
  do {
    if ( buffer_p[j] == '/' ) {
      if (!lastCharWasSlash) rc[i++] = '\\';
    }
    else {
      rc[i++] = buffer_p[j];
      lastCharWasSlash = false;
    }
  }
  while ( buffer_p[j++] );

  if ( i > 4 ) {
    // A drive letter and something else; removing any trailing backslash.
    // (But if it is just a drive letter, we keep the trailing backslash.)
    if ( rc[i-2] == '\\' ) rc[i-2] = 0;
  }

  strcpy( buffer_p, rc );

  return 0;
}


int convertToUserPath( const char *dosPath, char *userPath ) {

  strcpy( userPath, "/DRIVE_X" );
  userPath[7] = dosPath[0];
  strcat( userPath+8, dosPath+2 );
  
  int i=8;
  while ( userPath[i] ) {
    if ( userPath[i] == '\\' ) userPath[i] = '/';
    i++;
  }
  userPath[i] = 0;

  return 0;
}



static void sendMotd( FtpClient *client ) {

  char tmpLine[100];
  char *motdIndex = MotdBuffer;

  while ( *motdIndex ) {

    tmpLine[0] = '2'; tmpLine[1] = '3'; tmpLine[2] = '0'; tmpLine[3] = '-';
    int tmpLineIndex = 4;

    // Scan until we find the line feed
    while ( *motdIndex != 10 ) {

      // If we run out of chars in the motd buffer or the output line is
      // getting too long, punt.  This isn't worth real error handling.
      // They can fix the file and restart.

      if ( *motdIndex == 0 || tmpLineIndex == 95 ) return;
      tmpLine[tmpLineIndex++] = *motdIndex++;
    }

    // Must send a CR/LF pair or some clients get hung up.  It's the law.
    tmpLine[tmpLineIndex++] = 13;
    tmpLine[tmpLineIndex++] = 10;
    tmpLine[tmpLineIndex] = 0;
    motdIndex++;

    client->addToOutput( tmpLine );

  }

}






static void readMotdFile( const char *motdFilename ) {

  struct stat statbuf;
  int rc = stat( motdFilename, &statbuf );

  uint16_t fsize = statbuf.st_size;

  if ( fsize > 0 && fsize < MOTD_MAX_SIZE ) {

    MotdBuffer = (char *)malloc( fsize + 1 );
    if ( MotdBuffer == NULL ) return;

    FILE *tmp = fopen( motdFilename, "rt" );
    if ( tmp == NULL ) {
      free( MotdBuffer);
      return;
    }

    rc = fread( MotdBuffer, 1, fsize, tmp );
    fclose( tmp );

    MotdBuffer[rc] = 0;

  }

}
  


static int readConfigParms( void ) {

  Utils::openCfgFile( );


  // Password file is required.

  if ( Utils::getAppValue( "FTPSRV_PASSWORD_FILE", PasswordFilename, DOS_MAX_PATHFILE_LENGTH ) ) {
    addToScreen( 1, "Need to specify FTPSRV_PASSWORD_FILE in mTCP config file\n" );
    return 1;
  }


  // Logfile is optional

  if ( Utils::getAppValue( "FTPSRV_LOG_FILE", LogFilename, DOS_MAX_PATHFILE_LENGTH ) ) {
    addToScreen( 1, "Warning: A log file is not being used.\n\n" );
  }


  char tmpBuffer[30];
  uint16_t tmpVal;

  if ( Utils::getAppValue( "FTPSRV_SESSION_TIMEOUT", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( (tmpVal > 59) && tmpVal < 7201 ) {
      FtpSrv_timeoutTicks = tmpVal;
      FtpSrv_timeoutTicks = FtpSrv_timeoutTicks * 18ul;
    }
    else {
      addToScreen( 1, "FTPSRV_SESSION_TIMEOUT must be between 60 and 7200 seconds\n" );
      return 1;
    }
  }

  if ( Utils::getAppValue( "FTPSRV_CONTROL_PORT", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( tmpVal > 0 ) {
      FtpSrv_Control_Port = tmpVal;
    }
    else {
      addToScreen( 1, "FTPSRV_CONTROL_PORT must be greater than 0\n" );
      return 1;
    }
  }


  if ( Utils::getAppValue( "FTPSRV_EXT_IPADDR", tmpBuffer, 20 ) == 0 ) {
    uint16_t tmp1, tmp2, tmp3, tmp4;
    int rc = sscanf( tmpBuffer, "%d.%d.%d.%d\n", &tmp1, &tmp2, &tmp3, &tmp4 );
    if ( rc != 4 ) {
      addToScreen( 1, "Bad IP address format on FTPSRV_EXT_IPADDR\n" );
      return 1;
    }
    Pasv_IpAddr[0] = tmp1; Pasv_IpAddr[1] = tmp2;
    Pasv_IpAddr[2] = tmp3; Pasv_IpAddr[3] = tmp4;
  }

  if ( Utils::getAppValue( "FTPSRV_PASV_BASE", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( tmpVal > 1023 && tmpVal < 32768u ) {
      Pasv_Base = tmpVal;
    }
    else {
      addToScreen( 1, "FTPSRV_PASV_BASE must be between 1024 and 32768\n" );
      return 1;
    }
  }

  if ( Utils::getAppValue( "FTPSRV_PASV_PORTS", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( tmpVal > 255 && tmpVal < 10241 ) {
      Pasv_Ports = tmpVal;
    }
    else {
      addToScreen( 1, "FTPSRV_PASV_PORTS must be between 256 and 10240\n" );
      return 1;
    }
  }

  if ( Utils::getAppValue( "FTPSRV_CLIENTS", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( tmpVal > 0 && tmpVal <= FTP_MAX_CLIENTS ) {
      FtpSrv_Clients = tmpVal;
    }
    else {
      addToScreen( 1, "FTPSRV_CLIENTS must be between 1 and %u\n", FTP_MAX_CLIENTS );
      return 1;
    }
  }

  if ( Utils::getAppValue( "FTPSRV_FILEBUFFER_SIZE", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( tmpVal >= 4 && tmpVal <= 16 ) {
      Filebuffer_Size = tmpVal * 1024;
    }
    else {
      addToScreen( 1, "FTPSRV_FILEBUFFER_SIZE must be between 4 and 16 KB units\n" );
      return 1;
    }
  }

  if ( Utils::getAppValue( "FTPSRV_TCPBUFFER_SIZE", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( tmpVal >= 4 && tmpVal <= 16 ) {
      Data_Rcv_Buf_Size = tmpVal * 1024;
    }
    else {
      addToScreen( 1, "FTPSRV_TCPBUFFER_SIZE must be between 4 and 16 KB units\n" );
      return 1;
    }
  }


  if ( Utils::getAppValue( "FTPSRV_PACKETS_PER_POLL", tmpBuffer, 10 ) == 0 ) {
    tmpVal = atoi( tmpBuffer );
    if ( tmpVal >= 1 && tmpVal <= 10 ) {
      PacketsPerPoll = tmpVal;
    }
    else {
      addToScreen( 1, "FTPSRV_PACKETS_PER_POLL must be between 1 and 10\n" );
      return 1;
    }
  }


  if ( Utils::getAppValue( "FTPSRV_EXCLUDE_DRIVES", tmpBuffer, 27 ) == 0 ) {
    tmpVal = strlen(tmpBuffer);
    for ( int i=0; i < tmpVal; i++ ) {
      char dl = toupper(tmpBuffer[i]);
      if ( !isalpha(dl) ) {
        addToScreen( 1, "FTPSRV_EXCLUDE_DRIVES bad input: use drive letters" );
        return 1;
      }
      addToScreen( 1, "Excluding drive letter %c\n", dl );
      ValidDriveTable[dl-64] = 2;
    }
  }


  char motdFilename[DOS_MAX_PATHFILE_LENGTH];
  if ( Utils::getAppValue( "FTPSRV_MOTD_FILE", motdFilename, DOS_MAX_PATHFILE_LENGTH ) == 0 ) {
    readMotdFile( motdFilename );
  }


  Utils::closeCfgFile( );

  return 0;
}




// scanValidDrives
//
// Query DOS to find the number of drive letters on the system.  Then walk
// down the list finding each drive parameter table using undocumented DOS
// function 32h.  (It is undocumented but available and stable since DOS 2.0.)
//
// In the case of floppy drives, if BIOS says there is only one drive then
// detect which drive letter it is set to at the moment and skip the drive
// letter that is not active.
//
// While we are doing this we have the DOS critical error handler hooked.
// We set a flag so that the new handler knows that we are purposely
// tripping any errors, and that they can be ignored.

static void scanValidDrives( void ) {

  unsigned int equipWord = *((unsigned int far *)MK_FP(0,0x410));
  int numFloppyDrives = (equipWord & 0x1) ? ((equipWord & 0x00C0) >> 6) + 1 : 0;

  // Flag for single drive: 0=a, 1=b
  unsigned char far *pfloppy = (unsigned char far *)MK_FP(0,0x504);

  unsigned curdrive, lastdrive;

  _dos_getdrive( &curdrive );
  _dos_setdrive( curdrive, &lastdrive );

  // union REGS regs;

  TestingDrive = 1;

  char testPath[4];
  testPath[1] = ':'; testPath[2] = '\\'; testPath[3] = 0;

  for ( int i=1; i <= lastdrive; i++ ) {

    if ( ValidDriveTable[i] == 2 ) {
      // We were told to skip this
      ValidDriveTable[i] = 0;
      continue;
    }

    // Assume not valid
    ValidDriveTable[i] = 0;

    if ( numFloppyDrives == 1 ) {
      if ( (i==1) && (*pfloppy == 1) ) {
        // Looking at Drive A but BIOS has it as drive B.
        continue;
      }
      else if ( (i==2) && (*pfloppy == 0) ) {
        // Looking at Drive B and BIOS has it as drive A.
        continue;
      }
    }

    CritErrStatus = 0;

    /*
       Bad code!  Ignores network drives, including CD-ROM!
       Replace with generic 'stat' that is also more
       portable.  Mike: 2011-10-04

    regs.h.ah = 0x32;
    regs.h.dl = i;
    int86( 0x21, &regs, &regs );

    if ( regs.h.al != 0xff ) {
      ValidDriveTable[i] = 1;
    }
    */


    testPath[0] = 64 + i;
    if ( isDirectory( testPath ) ) {
      if ( CritErrStatus == 0 ) {
        ValidDriveTable[i] = 1;
      }
    }

  }

  TestingDrive = 0;

}



// findNextValidDrive - scans the valid drive table from a starting index
// and returns the first valid drive number that it finds or 0 if one is
// not found.

int findNextValidDrive( int start ) {
  for ( int i=start; i < 27; i++ ) {
    if ( ValidDriveTable[i] ) return i;
  }
  return 0;
}




// initSrv
//
// Returns 0 on succesful startup
// Returns 1 on failure

static int initSrv( void ) {

  // Read parameters and initialize
  if ( Utils::parseEnv( ) ) {
    return 1;
  }

  // Turn on mTCP tracing (if requested) as soon as possible.  This is normally
  // done but Utils::initStack but that does not happen for a while.
  Trace_beginTracing( );


  // Once our IP address is known set our default address that will
  // be advertised on the PASV command.  This might be overridden
  // when we read our application specific config parms.
  Ip::copy( Pasv_IpAddr, MyIpAddr );



  // Hook the DOS critical error handler
  oldInt24 = _dos_getvect( 0x24 );
  _dos_setvect(0x24, (void (__interrupt __far *)())newInt24);


  // Read the configuration parameters, scan for active drive letters
  // and then read and sanity check the password file.

  if ( readConfigParms( ) ) return 1;

  scanValidDrives( );

  if ( FtpUser::init( PasswordFilename ) )  return 1;


  // See if we can open the log file for append.  Before this there is no
  // logging to the FTP server log.

  if ( *LogFilename ) {
    LogFile = fopen( LogFilename, "ac" );
    if ( LogFile == NULL ) {
      addToScreen( 1, "\nCan't open logfile for writing.\n" );
      return 1;
    }
  }

  addToScreen( 1, "mTCP FtpSrv version (" __DATE__ ") starting\n\n" );

  if ( FtpClient::initClients( FtpSrv_Clients ) ) {
    addToScreen( 1, "\nFailed to initialize clients\n" );
    return 1;
  }


  // For small numbers of clients (5 and under) allocate 3 sockets per client
  // plus one more for a listening socket.  After five clients start to only
  // give one sockets per client.

  uint16_t requestedSockets;
  uint16_t requestedTcpBuffers;

  if ( FtpSrv_Clients < 6 ) {
    requestedSockets = FtpSrv_Clients * 3 + 1;
    requestedTcpBuffers = FtpSrv_Clients * 5;
  }
  else {
    requestedSockets = 16 + (FtpSrv_Clients-5);
    requestedTcpBuffers = TCP_MAX_XMIT_BUFS;
  }


  if ( Utils::initStack( requestedSockets, requestedTcpBuffers, ctrlBreakHandler, ctrlBreakHandler ) ) {
    addToScreen( 1, "\nFailed to initialize TCP/IP - exiting\n" );
    return 1;
  }


  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.


  uint16_t dosv = dosVersion( );
  DOS_major = dosv & 0xff;
  DOS_minor = dosv >> 8;


  // Note our starting time
  struct tm time_of_day;
  time_t tmpTime;
  time( &tmpTime );
  _localtime( &tmpTime, &time_of_day );
  _asctime( &time_of_day, StartTime );
  StartTime[24] = 0; // Get rid of unwanted carriage return

  // Make a note of the current year - we use this for directory listings.
  DosDate_t currentDate;
  getdate( &currentDate );
  Current_Year = currentDate.year;



  char lineBuffer[80];

  addToScreen( 1, "Clients: %u, Client file buffer size: %u, TCP buffer size: %u\n",
               FtpSrv_Clients, Filebuffer_Size, Data_Rcv_Buf_Size );
  addToScreen( 1, "Packets per poll: %u, TCP sockets: %u, Send buffers: %u, Recv buffers: %d\n",
               PacketsPerPoll, requestedSockets, requestedTcpBuffers, PACKET_BUFFERS );
  addToScreen( 1, "Client session timeout: %lu seconds\n", (FtpSrv_timeoutTicks / 18ul) );
  addToScreen( 1, "Control port: %u, Pasv ports: %u-%u\n", FtpSrv_Control_Port, Pasv_Base, (Pasv_Base+Pasv_Ports-1) );
  showRealIpAddr( 1 );

  addToScreen( 0, "\nPress [Ctrl-C] or [Alt-X] to end the server\n\n" );

  return 0;

}

// Used by the directory listing code.  If the incoming path starts with a '/'
// (which is completely invalid for DOS) then we know they are at the root.
// In that case we will substitute our own directory listing in, which consists
// of the valid drives they can choose from.

static unsigned _my_dos_findfirst( const char *path, unsigned attributes, struct find_t *buffer ) {

  if ( path[0] == '/' ) {

    // A Unix style path was passed in, so they are at the root of the
    // filesystem.  Start sending them our list of active drives.

    // Put an eye-catcher in the reserved area so that we can tell that we are
    // processing our pseudo-directory when somebody calls findnext.
    buffer->reserved[0] = 'M'; buffer->reserved[1] = 'B'; buffer->reserved[2] = 'B';

    // Start the drive search from Drive A.  Store in fourth byte for findnext.
    int nextDrive = findNextValidDrive( 1 );

    buffer->reserved[3] = nextDrive;
    buffer->attrib = _A_SUBDIR;        // Subdirectory
    buffer->wr_time = 0;               // Midnight
    buffer->wr_date = 0x19C;              // January 1 1980
    buffer->size = 0;
    strcpy( buffer->name, "DRIVE_X" );
    buffer->name[6] = nextDrive + 64;

    return 0;                          // Was not end of search
  }
  else {
    // Not for us - let DOS handle it.
    return _dos_findfirst( path, attributes, buffer );
  }

}


static unsigned _my_dos_findnext( struct find_t *buffer ) {

  if ( buffer->reserved[0] == 'M' && buffer->reserved[1] == 'B' && buffer->reserved[2] == 'B' ) {

    // Our eye-catcher was in the reserved area.

    // Look for the next drive
    int nextDrive = findNextValidDrive( buffer->reserved[3] + 1 );

    if ( nextDrive == 0 ) {
      // Blot out our eye catcher in case DOS does not use it; we don't want
      // to be confused and come into the wrong area by accident.
      buffer->reserved[2] = buffer->reserved[1] = buffer->reserved[0] = 0;
      return 1;
    }

    buffer->reserved[3] = nextDrive;
    buffer->attrib = _A_SUBDIR;        // Subdirectory
    buffer->wr_time = 0;               // Midnight
    buffer->wr_date = 0x19C;              // January 1 1980
    buffer->size = 0;
    strcpy( buffer->name, "DRIVE_X" );
    buffer->name[6] = nextDrive + 64;

    return 0;                          // Was not end of search
  }
  else {
    // Not for us - let DOS handle it.
    return _dos_findnext( buffer );
  }
}



void scrollMsgArea( int lines ) {
  memmove( (void *)(Screen_base + 2*(ScreenCols*2)), (void *)(Screen_base + (2+lines)*(ScreenCols*2)), ((ScreenRows-2)-lines)*(ScreenCols*2) );
  uint16_t far *start = (uint16_t far *)(Screen_base + (ScreenRows-lines)*(ScreenCols*2));
  fillUsingWord( start, (7<<8), lines*ScreenCols);
}






// addToScreen
//
// Writes everything it gets to the screen and to the log file, and to the
// mTCP trace if that is active too.  Log file entries get timestamped.
//
// Up to 512 bytes at a time can be written, which is more than enough for
// anything we are going to write.

char logLineBuffer[512];

void addToScreen( int writeLog, const char *fmt, ... ) {

  va_list ap;
  va_start( ap, fmt );
  int bytesOut = vsnprintf( logLineBuffer, 512, fmt, ap );
  va_end( ap );

  if ( bytesOut > 511 ) {
    // Truncated output - better than nothing
    logLineBuffer[511] = 0;
  }
  else if ( bytesOut < 0 ) {
    // Internal error - find and fix this!
    strcpy( logLineBuffer, InternalLoggingError );
    bytesOut = 24;
  }


  // Get current date and time for the FTP log

  DosTime_t currentTime;
  gettime( &currentTime );

  DosDate_t currentDate;
  getdate( &currentDate );


  if ( writeLog && (LogFile != NULL) ) {
    // Write FTP log
    fprintf( LogFile, "%04d-%02d-%02d %02d:%02d:%02d.%02d %s",
             currentDate.year, currentDate.month, currentDate.day,
             currentTime.hour, currentTime.minute, currentTime.second,
             currentTime.hsecond, logLineBuffer );
    // Add to mTCP log (if it is active).
    TRACE(( "Ftp %s", logLineBuffer ));
    fflush( LogFile );
  }


  // Now write it onto the screen in the message area.


  // Scan the output to see how many lines we are going to scroll.  As
  // expensive as it seems to prescan this output before writing it
  // byte-by-byte it is cheaper than scrolling close to 2000 bytes each
  // time we find a newline.


  uint8_t x = 0;
  uint8_t linesToScroll = 0;
  for ( int i=0; i < bytesOut; i++ ) {
    if ( logLineBuffer[i] == 10 ) {
      x = 0;
      linesToScroll++;
    }
    else {
      x++;
      if ( x == ScreenCols ) {
        x = 0;
        linesToScroll++;
      }
    }
  }


  // Scroll the screen upward.
  scrollMsgArea( linesToScroll );


  // Now write the buffer
  uint16_t far *start = (uint16_t far *)(Screen_base + ((ScreenRows-linesToScroll)*(ScreenCols*2)));


  // If this is not going to the logfile highlight it on the screen.
  uint16_t attr = 7;
  if (writeLog == 0 ) attr = 0xF;
  attr = attr << 8;


  x = 0;
  for ( uint16_t i = 0; i < bytesOut; i++ ) {
    if ( logLineBuffer[i] == 10 ) {
      start += (ScreenCols-x);
      x = 0;
    }
    else {
      *start++ = ( attr | logLineBuffer[i] );
      x++;
      if ( x == ScreenCols ) {
        x = 0;
      }
    }
  }

}


// This is not much of a cprintf - it doesn't handle new lines.  The primary
// use is for updating the status line.  It uses the same formatting buffer
// as the logger does, so don't let the logger use this.

void myCprintf( uint8_t x, uint8_t y, uint8_t attr, char *fmt, ... ) {

  va_list ap;
  va_start( ap, fmt );
  int bytesOut = vsnprintf( logLineBuffer, 512, fmt, ap );
  va_end( ap );

  if ( bytesOut > 511 ) {
    // Truncated output - better than nothing
    logLineBuffer[511] = 0;
  }
  else if ( bytesOut < 0 ) {
    // Internal error - find and fix this!
    strcpy( logLineBuffer, InternalLoggingError );
    bytesOut = 24;
  }

  uint16_t far *start = (uint16_t far *)(Screen_base + (y*ScreenCols+x)*2);

  for ( uint16_t i = 0; i < bytesOut; i++ ) {
    *start++ = ( attr << 8 | logLineBuffer[i] );
  }

}



/*
          1         2         3         4         5         6         7
01234567890123456789012345678901234567890123456789012345678901234567890123456789
mTCP FTPSrv: Total Connections: 00000  Active Sessions: 00
*/

void redrawStatusLine( void ) {

  myCprintf(  0, 0, 0x1F, "mTCP FTPSrv:" );
  myCprintf( 14, 0, 0x0F, "Total Sessions:" );
  myCprintf( 30, 0, 0x07, "%5d", SessionCounter );
  myCprintf( 37, 0, 0x0F, "Active Sessions:" );
  myCprintf( 54, 0, 0x07, "%2d", FtpClient::activeClients );

  myCprintf( 60, 0, 0x0F, "Use Alt-H for Help" );

}


void initScreen( void ) {

  // This always works:
  unsigned char screenMode = *((unsigned char far *)MK_FP( 0x40, 0x49 ));

  if ( screenMode == 7 ) {
    Screen_base = (uint8_t far *)MK_FP( 0xb000, 0 );
  }
  else {
    Screen_base = (uint8_t far *)MK_FP( 0xb800, 0 );
  }

  // Call int 10, ah=12 for EGA/VGA config

  if ( getEgaMemSize( ) == 0x10 ) {
    // Failed.  Must be MDA or CGA
    ScreenCols = 80;
    ScreenRows = 25;
  }
  else {
    ScreenCols = *((unsigned char far *)MK_FP( 0x40, 0x4A ));
    ScreenRows = *((unsigned char far *)MK_FP( 0x40, 0x84 )) + 1;
  }



  // Clear screen
  fillUsingWord( (uint16_t far *)Screen_base, (7<<8|32), ScreenRows*ScreenCols );

  // Draw separator line
  fillUsingWord( (uint16_t far *)(Screen_base + (ScreenCols*2)), (7<<8|196), ScreenCols );

  redrawStatusLine( );
  showBeepState( );

}



void showRealIpAddr( int writeLog ) {
  addToScreen( writeLog, "Real IP address: %d.%d.%d.%d, Pasv response IP addr: %d.%d.%d.%d\n",
               MyIpAddr[0], MyIpAddr[1], MyIpAddr[2], MyIpAddr[3],
               Pasv_IpAddr[0], Pasv_IpAddr[1], Pasv_IpAddr[2], Pasv_IpAddr[3] );
}

void doConsoleHelp( void ) {
  
  addToScreen( 0, "\n%s  %s", CopyrightMsg1, CopyrightMsg2 );
  addToScreen( 0, "Alt B: Toggle beeper   Alt-S: Stats   Alt-U: Users   Alt-X: Exit\n" );
  showRealIpAddr( 0 );
  addToScreen( 0, "\n" );
}


void showBeepState( void ) {
  if ( Sound ) {
    myCprintf( 4, 1, 0x7, "[ Beep on ]\xC4" );
  }
  else {
    myCprintf( 4, 1, 0x7, "[ Beep off ]" );
  }
}

void doConsoleShowUsers( void ) {


  if ( FtpClient::activeClients == 0 ) {
    addToScreen( 0, "No active users!\n" );
    return;
  }
  
  addToScreen( 0, "  Sess Name       Login time          IpAddr:port\n" );

  for ( uint16_t i=0; i < FtpClient::activeClients; i++ ) {

    FtpClient *client = FtpClient::activeClientsTable[i];

    struct tm timeBuf;
    _localtime( &client->startTime, &timeBuf );

    addToScreen( 0, "%6ld %-10s %04d-%02d-%02d %02d:%02d:%02d %d.%d.%d.%d:%u\n",
                 client->sessionId, client->user.userName,
                 timeBuf.tm_year+1900, timeBuf.tm_mon+1, timeBuf.tm_mday,
                 timeBuf.tm_hour, timeBuf.tm_min, timeBuf.tm_sec,
                 client->cs->dstHost[0], client->cs->dstHost[1],
                 client->cs->dstHost[2], client->cs->dstHost[3],
                 client->cs->dstPort );

  }

}

void doConsoleStats( void ) {

  addToScreen( 0, "\nStarted: %s\nSessions: %lu  Active: %u  Timeouts: %lu\n",
    StartTime, SessionCounter, FtpClient::activeClients, Stat_SessionTimeouts
  );

  addToScreen( 0, "LIST: %lu  NLST: %lu  RETR: %lu\nSTOR: %lu  STOU: %lu  APPE: %lu\n",
    Stat_LIST, Stat_NLST, Stat_RETR, Stat_STOR, Stat_STOU, Stat_APPE
  );

  addToScreen( 0, "Tcp Sockets used: %d free: %d\n",
    TcpSocketMgr::getActiveSockets( ), TcpSocketMgr::getFreeSockets( )
  );

  addToScreen( 0, "Tcp: Sent %lu Rcvd %lu Retrans %lu Seq/Ack errs %lu Dropped %lu\n",
    Tcp::Packets_Sent, Tcp::Packets_Received, Tcp::Packets_Retransmitted,
    Tcp::Packets_SeqOrAckError, Tcp::Packets_DroppedNoSpace
  );

  addToScreen( 0, "Packets: Sent: %lu Rcvd: %lu Dropped: %lu LowFreeBufCount: %u\n\n",
    Packets_sent, Packets_received, Packets_dropped, Buffer_lowFreeCount
  );

}
