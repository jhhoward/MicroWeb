/*

   mTCP Irc.cpp
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


   Description: IRCjr IRC client!

   Changes:

   2011-05-27: Initial release as open source software
   2011-10-01: Add user input editing; add DOS version to
               CTCP VERSION response
   2012-11-23: Add real help window
               Change to make user type /ctcp for ctcp commands
               Add ctcp time command
   2013-02-15: 132 column awareness; use halloc to enable larger backscroll
               buffers; add some more CTCP commands
   2013-02-22: Add PASS command
   2013-03-19: Add parameters for default logging mode and how to
               handle nick changes and quit msgs.
   2013-03-23: Combine printing and logging in one call
   2013-03-24: Add options for specifying a directory for the logs
   2013-03-30: Add more options for where to put nick changes and quit msgs
   2013-04-10: Redo the help screen to save space by embedding the
               color codes right in the text.  (Saves almost 1kb)
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.

*/



#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "types.h"
#include "session.h"
#include "screen.h"


#include "timer.h"
#include "trace.h"
#include "utils.h"

#include "packet.h"
#include "arp.h"
#include "udp.h"
#include "dns.h"
#include "tcp.h"
#include "tcpsockm.h"

#include "screen.h"
#include "session.h"
#include "irc.h"



enum IrcState_t {
  NotConnected = 0,
  SentNickAndUser,
  WelcomeReceived,
  Connected,
  ErrorReceived
};



// IRC user information

char IRCNick[ IRCNICK_MAX_LEN ];
char IRCUser[ IRCUSER_MAX_LEN ];
char IRCRealName[ IRCREALNAME_MAX_LEN ];
char IRCPass[ IRCPASS_MAX_LEN ];


// Server connection information

char IRCServer[ IRCHOSTNAME_MAX_LEN ];


char InitialChan[ IRCCHANNEL_MAX_LEN ] = "";
char ServerPrefix[ 80 ] = "";

IrcState_t SessionState = NotConnected;
uint16_t LastServerError = 0;

uint32_t ConnectTimeout;
uint32_t RegisterTimeout;


uint32_t PingResponses = 0;

time_t StartTime = 0;



uint32_t UserPingTime;


// TCP options

uint16_t ServerPort = 6667;
TcpSocket *s;




// User interface options

bool Beeper = false;
bool Timestamp = false;
bool ShowRaw = false;
bool LoggingDefault = false;


enum InfoMessagesTarget { None, Server, Current, All };

InfoMessagesTarget NickUpdates = None;
InfoMessagesTarget QuitUpdates = None;


// DOS limits:  drive letter + colon + path + null = 67
char LogDirectory[67] = { 0 };


// User input buffers/data areas

char inputBuffer[ SCBUFFER_MAX_INPUT_LEN ];
uint8_t switchToSession;




// Screen and session handling

uint16_t SessionCounter = 0;

Session *ServerSession = NULL;
Session *CS = NULL;

bool HelpIsActive = false;


uint8_t ColorScheme = 0;  // 0 default, 1 = CGA_MONO

uint8_t scErr;          // Error messages
uint8_t scNormal;       // Normal text
uint8_t scBright;       // Bright/Bold
uint8_t scReverse;      // Black on white
uint8_t scServerMsg;    // Message from the IRC server
uint8_t scUserMsg;      // Input from the local user
uint8_t scOtherUserMsg; // Message from an IRC user
uint8_t scActionMsg;    // Used for CTCP ACTION
uint8_t scTitle;        // Title - used only at startup
uint8_t scLocalMsg;     // For locally injected messages (like help, stats)

uint8_t scBorder;       // Border lines on help window
uint8_t scCommandKey;   // Used in help menu



uint16_t BsLinesChannel = 150;
uint16_t BsLinesServer = 50;
uint16_t BsLinesChat = 75;




// Input buffer read from the TCP socket
#define INBUFSIZE (4096)

uint8_t  *inBuf;                // Input buffer
uint16_t  inBufIndex=0;          // Index to next char to fill
uint16_t  inBufSearchIndex=0;    // Where to continue searching for \r\n

uint16_t getLineFromInBuf( char *target ); // Read full lines from inBuf

char *getNextParm( char *input, char *target, uint16_t bufLen );



// General purpose work area.
#define OUTBUF_LEN (512)
char outBuf[ OUTBUF_LEN ];


// DOS Version
uint8_t DOS_major;
uint8_t DOS_minor;




// Function prototypes

void shutdown( int rc );
void parseArgs( int argc, char *argv[] );
void getCfgOpts( void );

// User commands
void processUserInput( void );
void processBackScroll( void );
void processForwardScroll( void );
void processCloseWindow( void );
void processBeepToggle( void );
void processHelp( void );
void processStats( void );
void processTimestampToggle( void );
void processShowRawToggle( void );
void processLoggingToggle( void );
void processSessionSwitch( void );


// Socket handling
bool processSocketInput( void );
void pollSocket( uint32_t timeout, uint16_t batching );
int16_t registerWithServer( void );
int16_t waitForStateChange( uint16_t seconds );
int16_t getLimitedInput( void );



// Utils
void initScreen( void );

char *getTimeStr( void );


// Screen handling functions
void updateIndicatorLine( uint16_t x, uint8_t attr, char *msg );
void updateIndicatorChannel( void );
void updateIndicatorLogState( void );



void switchSession( Session *newSession );

Session *getTargetSession( char *targetSessionName, bool flipOnCreate );




// Ctrl-Break and Ctrl-C handlers.

// Check this flag once in a while to see if the user wants out.
volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}

void __interrupt __far ctrlCHandler( ) {
  // Do Nothing - Ctrl-C is a legal character
}

const char CtrlBreakMsg[] = "\nCtrl-Break detected: exiting\n";




bool checkUserWantsOut( void ) {

  if ( bioskey(1) ) {
    char c = getch( );
    if ( c == 27 ) {
      CS->puts( scErr, "[Esc] pressed - quitting.\n" );
      CS->draw( );
      return true;
    }
  }

  if ( CtrlBreakDetected ) {
    CS->puts( scErr, CtrlBreakMsg );
    CS->draw( );
    return true;
  }

  return false;
}


static char OutOfSessionWarning[] =
  "Warning: New window needed for \"%s\" but out of sessions\n"
  "or memory!  Use the /privmsg command to respond from this\n"
  "window and read the docs to learn how to avoid this.\n";

static char QuitMsg[] = "QUIT :IRCjr DOS Client (Yes - DOS!)\r\n";

static char Err_MustSupplyChannel[] = "Error: Must supply a channel name\n";
static char Err_MustSupplyUserForMsg[] = "Error: Must supply a user to send the msg to\n";
static char Err_MustSupplyAMsg[] = "Error: must supply a msg!\n";
static char Err_RemoteSideClosed[] = "\nRemote side closed connection!\n";

static char Msg_HasParted[] = "%s has parted %s\n";
static char Msg_HasJoined[] = "%s joined channel %s\n";
static char Msg_SetsMode[] = "%s sets mode%s\n";
static char Msg_JoinedChannel[] = "You joined channel %s\n";





static char CopyrightMsg1[] = "mTCP IRCjr by M Brutman (mbbrutman@gmail.com) (C)opyright 2008-2020\n";
static char CopyrightMsg2[] = "Version: " __DATE__ "\n\n";




int main( int argc, char *argv[] ) {

  printf( "%s  %s", CopyrightMsg1, CopyrightMsg2 );

  parseArgs( argc, argv );

  inBuf = (uint8_t *)malloc( INBUFSIZE );
  if ( inBuf == NULL ) {
    puts( "IRCjr: Could not allocate memory" );
    exit(1);
  }

  if ( Utils::parseEnv( ) != 0 ) {
    exit(1);
  }

  getCfgOpts( );

  // One socket, 5 outgoing TCP buffers
  if ( Utils::initStack( 1, 5, ctrlBreakHandler, ctrlCHandler ) ) {
    puts( "IRCjr: could not init TCP/IP" );
    exit(1);
  }

  uint16_t dosv = dosVersion( );
  DOS_major = dosv & 0xff;
  DOS_minor = dosv >> 8;



  // From this point forward you have to call the shutdown( ) routine to
  // exit because we have the timer interrupt hooked.


  Screen::init( inputBuffer, &switchToSession );

  // Create main session
  ServerSession = Session::createAndMakeActive( "Server", Screen::getOutputRows() + BsLinesServer, LoggingDefault );
  if ( ServerSession == NULL ) {
    puts( "Failed to create server session" );
    shutdown(1);
  }

  CS = ServerSession;

  initScreen( );

  StartTime = time( NULL );



  CS->printf( Session::PrintOpts_none, scNormal, "Connect timeout: %lu  Registration timeout: %lu\n",
                          ConnectTimeout/1000, RegisterTimeout/1000 );

  CS->puts( scNormal, "Press [Esc] during the connection process to quit\n\n"
                      "Resolving IRC server name: " );
  CS->printf( Session::PrintOpts_none, scBright, "%s\n", IRCServer );


  // Draw it!
  updateIndicatorChannel( );
  updateIndicatorLogState( );
  CS->draw( );


  // Resolve DNS
  //

  IpAddr_t newAddr;
  int8_t rc = Dns::resolve( IRCServer, newAddr, 1 );
  if ( rc < 0 ) {
    CS->puts( scErr, "Error resolving IRC server" );
    CS->draw( );
    shutdown(1);
  }

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  bool done = false;

  while ( !done ) {
    if ( checkUserWantsOut( ) ) break;
    if ( !Dns::isQueryPending( ) ) break;
    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );
    Dns::drivePendingQuery( );

  }

  // Query is no longer pending or we bailed out of the loop.
  rc = Dns::resolve( IRCServer, newAddr, 0 );

  if ( rc != 0 ) {
    CS->puts( scErr, "Failed to resolve IRC server.\n" );
    CS->draw( );
    shutdown(1);
  }

  uint32_t t = Timer_diff( startTime, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;


  CS->printf( Session::PrintOpts_none, scNormal, "IRC server resolved in %ld.%02ld seconds\n\n", (t/1000), (t%1000) );


  // Open socket to the server

  CS->printf( Session::PrintOpts_none, scNormal, "Opening socket to IRC server on port %u\n", ServerPort );
  CS->draw( );

  uint16_t localPort = 4096 + ( rand( ) % 1024 );

  s = TcpSocketMgr::getSocket( );

  if ( s->setRecvBuffer( 4096 ) ) {
    CS->puts( scErr, "Failed to allocate socket\n" );
    CS->draw( );
    shutdown(1);
  }

  rc = s->connectNonBlocking( localPort, newAddr, ServerPort );

  if ( rc == 0 ) {

    clockTicks_t start = TIMER_GET_CURRENT( );

    while ( 1 ) {

      PACKET_PROCESS_SINGLE;
      Tcp::drivePackets( );
      Arp::driveArp( );

      if ( s->isConnectComplete( ) ) { break; }

      if ( s->isClosed( ) || checkUserWantsOut( ) || (Timer_diff( start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( ConnectTimeout )) ) {
        rc = -1;
        break;
      }

      // Sleep for 50 ms just in case we are cutting TRACE records at
      // a furious pace.
      delay(50);
    }

  }

  if ( rc != 0 ) {
    CS->puts( scErr, "Failed to connect to IRC server\n" );
    CS->draw( );
    shutdown(1);
  }


  CS->puts( scNormal, "Connected - registering with server\n\n" );
  CS->draw( );

  int16_t registerRc = registerWithServer( );
  CS->draw( );

  if ( registerRc ) {
    CS->draw( );
    s->shutdown( TCP_SHUT_RD );
    s->close( );
    TcpSocketMgr::freeSocket( s );
    shutdown(0);
  }




  // Join a channel if one was specified
  if ( *InitialChan ) {
    int16_t bufLen = sprintf( outBuf, "JOIN %s\r\n", InitialChan );
    s->send( (uint8_t *)outBuf, bufLen );
  }



  // Main loop

  clockTicks_t lastTimestampUpdate = startTime;

  done = false;
  while ( !done ) {

    clockTicks_t currentTicks = TIMER_GET_CURRENT( );
    if ( Timer_diff( lastTimestampUpdate, currentTicks ) > 18 ) {
      lastTimestampUpdate = currentTicks;
      updateIndicatorLine( 72, scErr, getTimeStr( ) );
      updateIndicatorChannel( );
      Screen::updateCursor( );
    }

    if ( CtrlBreakDetected ) {
      s->send( (uint8_t *)QuitMsg, strlen(QuitMsg) );
      done = true;
    }


    Screen::InputActions newAction;
    for ( uint16_t i=0; i < 5; i++ ) {
      newAction = Screen::getInput( );
      if ( newAction != Screen::NoAction ) break;
    }

    switch ( newAction ) {
      case Screen::BackScroll:      { processBackScroll( ); break; }
      case Screen::ForwardScroll:   { processForwardScroll( ); break; }
      case Screen::CloseWindow:     { processCloseWindow( ); break; }
      case Screen::InputReady:      { processUserInput( ); break; };
      case Screen::BeepToggle:      { processBeepToggle( ); break; }
      case Screen::Help:            { processHelp( ); break; }
      case Screen::Stats:           { processStats( ); break; }
      case Screen::ShowRawToggle:   { processShowRawToggle( ); break; }
      case Screen::TimestampToggle: { processTimestampToggle( ); break; }
      case Screen::LoggingToggle:   { processLoggingToggle( ); break; }
      case Screen::SwitchSession:   { processSessionSwitch( ); break; }
      case Screen::AteOneKeypress:  { HelpIsActive = false; CS->draw( ); break; }
      case Screen::Redraw:          { CS->draw( ); break; }
      case Screen::EndProgram: {
        s->send( (uint8_t *)QuitMsg, strlen(QuitMsg) );
        done = true;
        break;
      }
    }

    // Check the socket, if nothing is available come back right
    // away, and if something is available limit ourselves to
    // processing five lines.
    pollSocket( 0, 5 );

    if ( (CS->getBackScrollOffset() == 0) && (HelpIsActive == false) ) {
      CS->drawIfUpdated( );
    }

    if ( s->isRemoteClosed( ) ) {
      CS->puts( scErr, Err_RemoteSideClosed );
      CS->draw( );
      done = true;
    }

  } // end main while loop


  // Give the other side a few seconds to shut down cleanly.
  startTime = TIMER_GET_CURRENT( );

  while ( 1 ) {
    pollSocket( 100, 5 );
    CS->drawIfUpdated( );
    uint32_t t = Timer_diff( startTime, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;
    if ( (t > 4000) || s->isRemoteClosed( ) ) break;
  }

  s->shutdown( TCP_SHUT_RD );
  s->close( );

  TcpSocketMgr::freeSocket( s );

  time_t EndTime = time( NULL );
  time_t diffTime = EndTime - StartTime;

  Screen::clearInputArea( );

  // printf( "Irc Stats: Ping Responses: %ld, Active %02ld:%02ld\n",
  //        PingResponses, (diffTime/60), (diffTime%60)  );

  shutdown(0);

  // Never make it here - just supress the stupid warning.
  return 0;
}



int16_t registerWithServer( void ) {

  // Send PASS if one was specified

  uint16_t bufLen;

  if ( IRCPass[0] ) {
    bufLen = snprintf( outBuf, OUTBUF_LEN, "PASS %s\r\n", IRCPass );
    outBuf[OUTBUF_LEN-1] = 0;
    s->send( (uint8_t *)outBuf, bufLen );
  }


  // Send initial registration message which is NICK and USER.

  bufLen = snprintf( outBuf, OUTBUF_LEN, "NICK %s\r\nUSER %s 0 * :%s\r\n", IRCNick, IRCUser, IRCRealName );
  outBuf[OUTBUF_LEN-1] = 0;
  s->send( (uint8_t *)outBuf, bufLen );

  SessionState = SentNickAndUser;


  // Go around until we get something

  while ( 1 ) {

    if ( waitForStateChange( RegisterTimeout ) != 0 ) {
      // Timeout
      CS->puts( scErr, "Timeout registering with server\n" );
      return -1;
    }

    // State change occurred, but might not be what we want.

    if ( SessionState == WelcomeReceived ) {
      SessionState = Connected;
      // CS->puts( scLocalMsg, "Registered!\n" );
      return 0;
    }
    else if ( SessionState == ErrorReceived ) {

      if ( LastServerError == IRC_ERR_NO_NICKNAME_GIVEN ||
           LastServerError == IRC_ERR_ERRONEOUS_NICKNAME ||
           LastServerError == IRC_ERR_NICKNAME_IN_USE ) {

        // Prompt for a new nickname
        CS->puts( scLocalMsg, "\nThe server didn't like your nickname.  Enter another nickname:\n" );

        int16_t rc = getLimitedInput( );

        if ( rc == 0 ) {

          strncpy( IRCNick, inputBuffer, IRCNICK_MAX_LEN );
          IRCNick[ IRCNICK_MAX_LEN - 1 ] = 0;

          CS->puts( scLocalMsg, "Sending new nickname: " );
          CS->puts( scBright, IRCNick );
          CS->puts( scNormal, "\n" );
        }
        else if ( rc == -1 ) {
          CS->puts( scErr, "\nEnding program at your request\n" );
          return -1;
        }
        else if ( rc == -2 ) {
          CS->puts( scErr, Err_RemoteSideClosed );
          return -1;
        }

        // Just send NICK this time
        uint16_t bufLen = snprintf( outBuf, OUTBUF_LEN, "NICK %s\r\n", IRCNick );
        outBuf[OUTBUF_LEN-1] = 0;
        s->send( (uint8_t *)outBuf, bufLen );

        SessionState = SentNickAndUser;

      }
      else if ( LastServerError == IRC_ERR_WAITASEC ) {

        // Not really an error ..
        SessionState = SentNickAndUser;

      }
      else if ( LastServerError == IRC_ERR_PASSWD_MISMATCH ) {
        CS->puts( scErr, "\nThe server rejected your password.  Please fix it.\n\n" );
        return -1;
      }
      else {
        // Unknown error response from server
        CS->puts( scErr, "\nUnknown response from server - giving up.\n" );
        return -1;
      }

    }
    else {
      // Unknown State
      return -1;
    }

  } // end while 1


  return 0;
}



// Returns:
//
//   0 - state change within specified time
//  -1 - timeout

int16_t waitForStateChange( uint16_t seconds ) {

  IrcState_t startState = SessionState;

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( 1 ) {

    if ( checkUserWantsOut( ) ) {
      shutdown(1);
    }

    uint32_t t = Timer_diff( startTime, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;

    if ( t > seconds ) return -1;

    pollSocket( 0, 1 );
    CS->drawIfUpdated( );

    if ( startState != SessionState ) break;

  }

  return 0;
}



// Returns:
//
//   0 - Input received
//  -1 - Program exit by user
//  -2 - Server closed our socket

int16_t getLimitedInput( void ) {

  Screen::updateCursor( );

  while ( 1 ) {

    if ( CtrlBreakDetected ) {
      CS->puts( scErr, CtrlBreakMsg );
      CS->draw( );
      shutdown(1);
    }

    Screen::InputActions newAction;
    for ( uint16_t i=0; i < 3; i++ ) {
      newAction = Screen::getInput( );
      if ( newAction != Screen::NoAction ) break;
    }

    switch ( newAction ) {

      case Screen::InputReady: {
        return 0;
      }
      case Screen::EndProgram: {
        return -1;
      }

    }

    pollSocket( 0, 1 );
    if ( CS->getBackScrollOffset() == 0 ) {
      CS->drawIfUpdated( );
    }

    if ( s->isRemoteClosed( ) ) {
      CS->draw( );
      return -2;
    }

  }

  // Not reached
  return 0;
}




// Screen updating functions

void updateIndicatorLine( uint16_t x, uint8_t attr, char *msg ) {
  Screen::printf( x, Screen::getSeparatorRow(), attr, "%s", msg );
}


/*
01234567890123456789012345678901234567890123456789
          1         2         3         4

-0123456789012345678901234-0123456789-
*/

// Call this to update the active channel and channel indicators.  This
// happens when the window switches, when we add channels, and when a
// channel gets something written to it.

void updateIndicatorChannel( void ) {

  // Blot out the old contents
  if ( Screen::isPreventSnowOn( ) ) { waitForCGARetraceLong( ); }
  fillUsingWord( Screen::getSeparatorRowAddress(), (scNormal<<8)|196, 37 );

  // Write the name

  char tmpName[26];
  strncpy(tmpName, CS->getName(), 25);
  tmpName[25] = 0;
  Screen::printf( 1, Screen::getSeparatorRow(), scErr, "%s", tmpName );

  // Update indicators

  uint16_t far *indicatorPtr = (uint16_t far *)Screen::getSeparatorRowAddress() + 27;

  for ( int i=0; i < Session::activeSessions; i++ ) {
    uint16_t tmp;
    if ( CS == Session::activeSessionList[i] ) {
      // Current session - make it bright
      tmp = scBright;
    }
    else {
      // Other sessions
      if ( Session::activeSessionList[i]->wasSessionUpdated( ) ) {
        // Updated - reverse
        tmp = scReverse;
      }
      else {
        // Background and not updated
        tmp = scNormal;
      }
    }

    tmp = (tmp<<8) | (i+'0');

    if ( Screen::isPreventSnowOn( ) ) {
      writeCharWithoutSnow( Screen::getScreenBaseSeg( ), FP_OFF(indicatorPtr), tmp );
    } else {
      *indicatorPtr = tmp;
    }

    indicatorPtr++;
  }
      
}



void updateIndicatorLogState( void ) {
    
  if ( CS->isLoggingOn( ) ) {
    updateIndicatorLine( 66, scErr, "[Log]" );
  }
  else {
    updateIndicatorLine( 66, scNormal, "\xC4\xC4\xC4\xC4\xC4" );
  }

}



// Wipes out the backscroll indicator and brings the current screen to the
// forefront.

void restoreNormalScreen( void ) {
  if ( CS->getBackScrollOffset( ) ) {
    updateIndicatorLine( 46, scNormal, "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4" );
    CS->resetBackScrollOffset( );
  }
  CS->draw( );
}



// switchSession
//

void switchSession( Session *newSession ) {

  CS = newSession;
  CS->draw( );
  updateIndicatorChannel( );

  // Need to fix backscroll indicator status.  Other indicators are ok.
  if ( CS->getBackScrollOffset( ) ) {
    updateIndicatorLine( 46, scErr, "[Backscroll]" );
  }
  else {
    updateIndicatorLine( 46, scNormal, "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4" );
  }

  updateIndicatorLogState( );

}



void closeSession( Session *target ) {
  if ( target != ServerSession ) {
    Session::removeActiveSession( target );
    if ( CS == target ) {
      switchSession( ServerSession );
    }
  }
  updateIndicatorChannel( );
}





void processUserInput( void ) {

  outBuf[0] = 0;

  bool echoUserInput = true;

  if ( inputBuffer[0] == '/' ) { // Command

    char target[ IRCNICK_MAX_LEN ];

    // The first token is a command.  Commands are generally small.
    char token[20];
    char *pos = inputBuffer;
    pos = Utils::getNextToken( pos, token, 20 );



    // For JOIN and PART, don't bother parsing the rest of the input - just
    // pass it straight to the server.  This allows the user to specify
    // multiple channels, passwords, or whatever ..

    if ( stricmp( "/join", token ) == 0 ) {

      if ( pos ) {
        sprintf( outBuf, "join%s\r\n", pos );
      }
      else {
        CS->puts( scErr, Err_MustSupplyChannel );
      }

      // If there is no error from the server we will get a response
      // that will cause the new session to open.
    }

    else if ( stricmp( "/part", token ) == 0 ) {

      if ( pos ) {
        sprintf( outBuf, "part%s\r\n", pos );
      }
      else {
        CS->puts( scErr, Err_MustSupplyChannel );
      }

      // If there is no error from the server we will get a response
      // that will close the session, if applicable.
    }

    else if ( stricmp( "/nick", token ) == 0 ) {

       // Next token is the nick to change to.  This protects us from the
       // user setting a long nick, having the server accept it, and then
       // not being able to process it.

       Utils::getNextToken( pos, target,  IRCNICK_MAX_LEN );

       if ( *target ) {
         sprintf( outBuf, "NICK %s\r\n", target );
       }
      else {
        CS->puts( scErr, "Error: Must supply a new nick!\n" );
      }

    }

    // MSG sends a private message but does not open a new session.

    else if ( stricmp( "/msg", token ) == 0 ) {

      // Next token is the target of the message

      pos = Utils::getNextToken( pos, target, IRCNICK_MAX_LEN );

      if ( *target ) {

        if ( pos != NULL ) {
          // Send the original command to the server
          sprintf( outBuf, "PRIVMSG %s :%s\r\n", target, pos+1 );
        }
        else {
          CS->puts( scErr, Err_MustSupplyAMsg );
        }
      }
      else {
        CS->puts( scErr, Err_MustSupplyUserForMsg );
      }

    }

    // QUERY sends a private message and opens a new session.

    else if ( stricmp( "/query", token ) == 0 ) {

      // Next token is the target of the message

      pos = Utils::getNextToken( pos, target, IRCNICK_MAX_LEN );

      if ( *target ) {

        if ( pos != NULL ) {

          Session *tmp = getTargetSession( target, true );

          // Do not need to put user input on the new session here ...
          // It will get done because we are switching sessions on the
          // next statement.

          // The user kind of asked for a new window to the target, so even
          // if it is not new flip over to it.
          if ( CS != tmp ) switchSession( tmp );

          // Send the original command to the server
          sprintf( outBuf, "PRIVMSG %s :%s\r\n", target, pos+1 );
        }
        else {
          CS->puts( scErr, Err_MustSupplyAMsg );
        }
      }
      else {
        CS->puts( scErr, Err_MustSupplyUserForMsg );
      }

    }

    // CTCP commands

    else if ( stricmp( "/me", token ) == 0 ) {

      // Find the start of the next parameter.

      if ( pos != NULL ) while ( *pos && isspace(*pos) ) { pos++; } 

      if ( pos != NULL && *pos ) {
        sprintf( outBuf, "PRIVMSG %s :\001ACTION %s\001\r\n", CS->getName( ), pos );
        echoUserInput = false;
        CS->printf( Session::PrintOpts_Part1, scActionMsg, "* %s %s\n", IRCNick, pos );
      }
      else {
        CS->puts( scErr, "Error: /me requires some text\n" );
      }

    }

    else if ( stricmp( "/ctcp", token ) == 0 ) {

      char ctcpCmd[20];

      pos = Utils::getNextToken( pos, ctcpCmd, 20 );

      if ( *pos == 0 ) {
        CS->puts( scErr, "Error: Need to specify a CTCP command to use\n" );
      }
      else {

        strupr( ctcpCmd );

        pos = Utils::getNextToken( pos, target, IRCNICK_MAX_LEN );

        if ( *target == 0 ) {
          CS->puts( scErr, "Error: syntax for ctcp commands is /ctcp <command> <nick>\nwhere <command> is the command "
                           "and nick is the user/channel to send it to.\n" );
        }
        else {

          if ( stricmp( "ping", ctcpCmd ) == 0 ) {
            UserPingTime = time( NULL );
            sprintf( outBuf, "PRIVMSG %s :\001PING %lu\001\r\n", target, UserPingTime );
          }
          else {
            // Default - just pass it through
            strupr( ctcpCmd );
            sprintf( outBuf, "PRIVMSG %s :\001%s\001\r\n", target, ctcpCmd );
          }

        }

      }

    }

    else {
      sprintf( outBuf, "%s\r\n", &inputBuffer[1] );
    }

  }
  else {

    // If it is not a command assume it is a msg to the current channel

    if ( stricmp( CS->getName( ), "Server" ) == 0 ) {
      CS->puts( scErr, "Sorry, sending messages to the server doesn't make sense.\n" );
      echoUserInput = false;
    }
    else {
      if ( *inputBuffer ) {
        // Don't send null messages
        sprintf( outBuf, "PRIVMSG %s :%s\r\n", CS->getName( ), inputBuffer );
      }
    }

  }


  if ( echoUserInput ) {
    CS->printf( Session::PrintOpts_Part1, scUserMsg, "<%s> ", IRCNick );
    CS->printf( Session::PrintOpts_Part2, scNormal, "%s\n", inputBuffer );

  }

  if ( outBuf[0] ) {
    s->send( (uint8_t *)outBuf, strlen(outBuf) );
  }

  restoreNormalScreen( );
}







void processBackScroll( void ) {

  if ( CS->isBackScrollAvailable( ) == false ) return;

  CS->adjustBackScrollOffset( Screen::getOutputRows() );

  CS->draw( );
  updateIndicatorLine( 46, scErr, "[Backscroll]" );
}



void processForwardScroll( void ) {

  if ( CS->isBackScrollAvailable( ) == false ) return;

  CS->adjustBackScrollOffset( - Screen::getOutputRows() );

  if ( CS->isBackScrollAtHome( ) ) {
    updateIndicatorLine( 46, scNormal, "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4" );
  }

  CS->draw( );
}





void processCloseWindow( void ) {

  // If it is a channel then send the command to the server.  Otherwise it was
  // just a private message window so no part command is appropriate.

  if ( CS->isChannel( ) ) {
    int rc = sprintf( outBuf, "part %s\r\n", CS->getName( ) );
    s->send( (uint8_t *)outBuf, rc );
  }

  closeSession( CS );
}



void processBeepToggle( void ) {

  Beeper = !Beeper;

  if ( Beeper ) {
    updateIndicatorLine( 59, scErr, "[Beep]" );
  }
  else {
    updateIndicatorLine( 59, scNormal, "\xC4\xC4\xC4\xC4\xC4\xC4" );
  }

}





// Encoded help screen
//
// This is ugly but it cuts down on the code space when compared to doing
// a large number of printfs, which was required for each color change.
// Now the Screen::printf routine is mildy hacked to recognize these four
// escape sequences so that we can embed color codes in with the text.
//
// The #defines look like this to tell the compiler where the hex constant ends.

#define ESC_NORMAL  "\x1b" "1"
#define ESC_CMDKEY  "\x1b" "2"
#define ESC_LOCAL   "\x1b" "3"
#define ESC_BRIGHT  "\x1b" "4"

char *EncodedHelp[] = {

"Commands: " ESC_CMDKEY "Alt-H" ESC_NORMAL " Help   " ESC_CMDKEY "Alt-C" ESC_NORMAL " Close Session   " ESC_CMDKEY "Alt-X" ESC_NORMAL " Exit",
"Toggles:  " ESC_CMDKEY "Alt-B" ESC_NORMAL " Beep   " ESC_CMDKEY "Alt-L" ESC_NORMAL " Log to file     " ESC_CMDKEY "Alt-T" ESC_NORMAL " Timestamps",
"",
"Navigation: " ESC_CMDKEY "PgUp" ESC_NORMAL " Go backwards to older messages   " ESC_CMDKEY "Alt-0" ESC_NORMAL " Go to server window",
"            " ESC_CMDKEY "PgDn" ESC_NORMAL " Go forwards to newer messages    " ESC_CMDKEY "Alt-n" ESC_NORMAL " Go to window n (1-9)",
"",
"IRC cmds:  /join, /part, /msg, /query, /nick, /away, /list, /quit, etc ...",
"CTCP cmds: /me, /ctcp ping, /ctcp time, /ctcp version, etc ...",
"",
"Colors: " ESC_CMDKEY "Ctrl-B" ESC_NORMAL " Bold      " ESC_CMDKEY "Ctrl-K" ESC_NORMAL " mIRC Colors   " ESC_CMDKEY "Ctrl-O" ESC_NORMAL " Reset   " ESC_CMDKEY "Ctrl-R" ESC_NORMAL " Reverse",
"        " ESC_CMDKEY "Ctrl-I" ESC_NORMAL " Italics   " ESC_CMDKEY "Ctrl-U" ESC_NORMAL " Underline " ESC_LOCAL "(Both can be sent but not displayed)",
"",
ESC_BRIGHT "Press a key to go back to your session ...",

NULL

};


void processHelp( void ) {

  Screen::clearRows( 2, 17 ); 

  Screen::repeatCh( 0, 1, scBorder, 205, Screen::getScreenCols() );

  Screen::printf( 0, 3, scTitle, "%s", CopyrightMsg1 );
  Screen::printf( 2, 4, scTitle, "%s", CopyrightMsg2 );

  gotoxy( 0, 6 );

  int i = 0;
  while ( EncodedHelp[i] != NULL ) {
    Screen::printf( scNormal, "%s\r\n", EncodedHelp[i] );
    i++;
  }

  Screen::repeatCh( 0, 19, scBorder, 205, Screen::getScreenCols() );

  Screen::eatOneKeypress( );
  HelpIsActive = true;

}



void processStats( void ) {

  time_t EndTime = time( NULL );
  time_t diffTime = EndTime - StartTime;

  CS->printf( Session::PrintOpts_none, scLocalMsg, "\nIRCjr Statistics: Active %02ld:%02ld, Server pings: %ld\n",
              (diffTime/60), (diffTime%60), PingResponses );

  CS->printf( Session::PrintOpts_none, scLocalMsg, "Tcp packets: Sent %lu Rcvd %lu Retrans %lu Seq/Ack errs %lu Dropped %lu\n",
           Tcp::Packets_Sent, Tcp::Packets_Received, Tcp::Packets_Retransmitted,
           Tcp::Packets_SeqOrAckError, Tcp::Packets_DroppedNoSpace );

  CS->printf( Session::PrintOpts_none, scLocalMsg, "IP packets: Udp Rcvd: %lu  Icmp Rcvd: %lu  Frags: %lu  ChksumErr: %lu\n",
           Udp::Packets_Received, Ip::icmpRecvPackets, Ip::fragsReceived, Ip::badChecksum );

  CS->printf( Session::PrintOpts_none, scLocalMsg, "Packets: Sent: %lu Rcvd: %lu Dropped: %lu SndErrs: %lu LowFreeBufCount: %u\n\n",
          Packets_sent, Packets_received, Packets_dropped, Packets_send_errs, Buffer_lowFreeCount );

  restoreNormalScreen( );
}


void processTimestampToggle( void ) {

  if ( Timestamp ) {
    CS->puts( scLocalMsg, "Timestamps turned off\n" );
  }
  else {
    CS->puts( scLocalMsg, "Timestamps turned on\n" );
  }

  Timestamp = !Timestamp;
}


void processShowRawToggle( void ) {
  ShowRaw = !ShowRaw;
  ServerSession->printf( Session::PrintOpts_none, scLocalMsg, "Showing raw messages from server: %s\n", (ShowRaw?"yes":"no") );
}


void processLoggingToggle( void ) {

  CS->loggingToggle( );

  updateIndicatorLogState( );

}


void processSessionSwitch( void ) {

  if ( switchToSession < Session::activeSessions ) {
    switchSession( Session::activeSessionList[switchToSession] );
  }
  else {
    ERRBEEP( );
  }

}


void shutdown( int rc ) {

  for ( uint16_t i = Session::activeSessions ; i > 0 ; i-- ) {
    Session::removeActiveSession( Session::activeSessionList[i-1] );
  }

  Utils::endStack( );

  puts( "\nIRCjr - Get your daily dose of DOS!" );
  puts( "Please send comments and bug reports to mbbrutman@gmail.com");

  exit( rc );
}





// Write a message to all open windows

void broadcastMsg( uint8_t color, char *msg ) {
  for ( int i=0; i < Session::activeSessions; i++ ) {
    Session::activeSessionList[i]->printf( Session::PrintOpts_Part1, color, "%s", msg );
  }
}




// This parm might or might not be a trailer.  If it has a leading ":"
// then it is a trailer, so skip that char.

void printTrailer( bool part1, Session *session, uint8_t attr, char *possibleTrailer ) {

  // Just being safe
  if ( possibleTrailer == NULL ) return;

  if ( *possibleTrailer == ':' ) possibleTrailer++;

  if ( *possibleTrailer ) {
    if ( part1 == true ) {
      session->printf( Session::PrintOpts_Part1, attr, "%s\n", possibleTrailer );
    }
    else {
      session->printf( Session::PrintOpts_Part2, attr, "%s\n", possibleTrailer );
    }
  }

}



// Processes numeric return codes from the server.  Messages go to the
// current session which should usually be correct.  In the off chance
// that it is not those will have to be handed separately.

void processServerResp( char *msgNick, char *command, char *restOfCommand ) {

  char *nextTok = restOfCommand;


  // Parse the line a little more

  int cmdOpcode = atoi( command );

  char replyTarget[IRCNICK_MAX_LEN];
  nextTok = Utils::getNextToken( nextTok, replyTarget, IRCNICK_MAX_LEN );

  char parm1[40];
  char *parm1_ptr = nextTok + 1;
  nextTok = Utils::getNextToken( nextTok, parm1, 40 );


  // If this is an error message prior to connection then it is probably
  // fatal.  But after connection there are lots of possible error messages.

  if ( cmdOpcode >= 400 && cmdOpcode <= 599 ) {

    if ( SessionState != Connected ) {
      SessionState = ErrorReceived;
    }

    LastServerError = cmdOpcode;
    CS->printf( Session::PrintOpts_Part1, scErr, "<%s> %s%s\n", msgNick, command, restOfCommand );
    return;
  }


  switch ( cmdOpcode ) {

    case IRC_RPL_WELCOME:
    case IRC_RPL_YOURHOST:
    case IRC_RPL_CREATED:
    case IRC_RPL_LUSERCLIENT:
    case IRC_RPL_LUSERME:
    case IRC_RPL_LOCALUSERS:
    case IRC_RPL_GLOBALUSERS:
    case IRC_STATSDLINE:
    case IRC_RPL_INFO:
    case IRC_RPL_MOTD:
    case IRC_RPL_ENDOFINFO:
    case IRC_RPL_ENDOFMOTD:
    case IRC_RPL_ISUPPORT:
    case IRC_RPL_INFOSTART:
    case IRC_RPL_MOTDSTART:
    {
      printTrailer( true, CS, scServerMsg, parm1_ptr );
      break;
    }

    case IRC_RPL_LUSEROP: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "Operators online: %s\n", parm1 );
      break;
    }

    case IRC_RPL_LUSERUNKNOWN: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "Unknown connections: %s\n", parm1 );
      break;
    }
      
    case IRC_RPL_LUSERCHANNELS: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "Channels formed: %s\n", parm1 );
      break;
    }

    case IRC_RPL_MYINFO: {
      SessionState = WelcomeReceived;
      Utils::getNextToken( nextTok, ServerPrefix, 80 );
      printTrailer( true, CS, scServerMsg, parm1_ptr );
      break;
    }

    case IRC_RPL_NAMREPLY: {

      char channel[IRCCHANNEL_MAX_LEN];
      nextTok = Utils::getNextToken( nextTok, channel, 40 );

      CS->printf( Session::PrintOpts_Part1, scServerMsg, "Names in %s: ", channel );
      printTrailer( false, CS, scServerMsg, nextTok+1 );

      break;
    }

    case IRC_RPL_NOTOPIC: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "Topic is not set\n" );
      break;
    }

    case IRC_RPL_TOPIC: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "Topic for %s is: ", parm1 );
      printTrailer( false, CS, scServerMsg, nextTok+1 );
      break;
    }

    case IRC_RPL_TOPICWHOTIME: {
      char setBy[ IRCNICK_MAX_LEN ];
      Utils::getNextToken( nextTok, setBy, IRCNICK_MAX_LEN );
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "Topic set by: %s\n", setBy );
      break;
    }


    case IRC_RPL_ENDOFNAMES: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "End of names\n" );
      break;
    }

    case IRC_RPL_AWAY: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "%s is away\n", parm1 );
      break;
    }

    case IRC_RPL_UMODEIS: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, Msg_SetsMode, replyTarget, parm1 );
      break;
    }

    default: {
      CS->printf( Session::PrintOpts_Part1, scServerMsg, "<%s> %s%s\n", msgNick, command, restOfCommand );
      break;
    }



  } // end switch


}




// Find the write session, presumably for a message we are about to write.
// If the session doesn't exist, try to create it.  If that fails, we will
// return the ServerSession.

Session *getTargetSession( char *targetSessionName, bool flipOnCreate ) {

  Session *tmp;
  int16_t i = Session::getSessionIndex( targetSessionName );

  if ( i == -1 ) {

    // Session does not exist yet

    if ( SessionState != Connected ) {
      // Special handling for AUTH - use the ServerSession until connected
      tmp = ServerSession;
    }
    else {

      // Try to create a new session

      uint16_t bsLines = BsLinesChat;
      if ( *targetSessionName == '#' ) bsLines = BsLinesChannel;

      // Caution - this can fail!  Check the return code!
      tmp = Session::createAndMakeActive( targetSessionName, Screen::getOutputRows() + bsLines, LoggingDefault );

      if ( tmp == NULL ) {
        ServerSession->printf( Session::PrintOpts_none, scErr, OutOfSessionWarning, targetSessionName );
        tmp = ServerSession;
      }

    }

    // Whether we created a new session or are using the ServerSession, this
    // is a new message that came in - flip to it.
    if ( flipOnCreate ) switchSession( tmp );

  }
  else {
    tmp = Session::activeSessionList[i];
  }

  return tmp;
}




// CTCP handling
//
// After reading the original spec and a few other sources, I decided to punt
// and do a very cut-rate implementation.  This code:
//
// - does not do any quoting or unquoting
// - only handles one CTCP request or response per message
// - only handles a small subset of CTCP commands
//
// This gives us most of the function that we want without the ridiculous
// overhead that the quoting adds.
//
// None of this code is terribly performance sensitive, so please excuse
// the extra strlen operations.

void handleCtcp( char *src, char *target, char *msg, bool request ) {

  if ( *msg == 0 ) return; // Protect against malformed requests

  // Brutish but effective - find the last 0x1 and NULL it out.
  // That means we only handle one CTCP per message.  We're not going to ever
  // try to process this buffer again after this, so truncating it is
  // acceptable.

  char *tmp = msg;
  while ( *tmp ) {
    if ( *tmp == 0x1 ) {
      *tmp = 0;
      break;
    }
    tmp++;
  }


  char cmd[20];
  char *pos = Utils::getNextToken( msg, cmd, 20 );

  // More protection against bad input
  if ( cmd[0] == 0 ) return;

  strupr( cmd );

  if ( request ) {

    // This CTCP command was in a PRIVMSG


    if ( stricmp( cmd, "ACTION" ) == 0 ) {

      if ( pos != NULL ) {

        // Advance us past one space.  If somebody is playing games we might
        // be sitting on a NULL character now, but that wont kill us.
        pos++;

        char *targetSessionName = target;
        if ( stricmp( target, IRCNick ) == 0 ) {
          // We are the target; this is a private message
          targetSessionName = src;
        }

        Session *tmp = getTargetSession( targetSessionName, true );
        tmp->printf( Session::PrintOpts_Part1, scActionMsg, "* %s %s\n", src, pos );

      } // end if msg parameter is present

    } // end ACTION

    else {

      // All other CTCP commands

      ServerSession->printf( Session::PrintOpts_Part1, scLocalMsg, "CTCP: %s request from %s\n", cmd, src );

      outBuf[0] = 0;

      if ( strcmp( cmd, "PING" ) == 0 ) {

        if ( pos != NULL ) {

          // It looks like there is a space missing between PING and %s, but
          // pos points at the next place to start parsing, which should be
          // a space character.

          snprintf( outBuf, OUTBUF_LEN, "NOTICE %s :\001PING%s\001\r\n", src, pos );
          outBuf[OUTBUF_LEN-1] = 0;
        }

      }

      else if ( strcmp( cmd, "VERSION" ) == 0 ) {
        snprintf( outBuf, OUTBUF_LEN, "NOTICE %s :\001VERSION mTCP IRCjr for DOS version " __DATE__ " running under DOS %d.%02d\001\r\n",
                  src, DOS_major, DOS_minor );
        outBuf[OUTBUF_LEN-1] = 0;
      }

      else if ( strcmp( cmd, "CLIENTINFO" ) == 0 ) {
        snprintf( outBuf, OUTBUF_LEN, "NOTICE %s :\001CLIENTINFO PING VERSION TIME SOURCE USERINFO CLIENTINFO\001\r\n", src );
        outBuf[OUTBUF_LEN-1] = 0;
      }

      else if ( strcmp( cmd, "TIME" ) == 0 ) {
        char timeStr[26];
        time_t localTime = time( NULL );
        _ctime( &localTime, timeStr );
        timeStr[24] = 0;
        snprintf( outBuf, OUTBUF_LEN, "NOTICE %s :\001TIME %s\001\r\n", src, timeStr );
        outBuf[OUTBUF_LEN-1] = 0;
      }

      else if ( strcmp( cmd, "SOURCE" ) == 0 ) {
        snprintf( outBuf, OUTBUF_LEN, "NOTICE %s :\001SOURCE http://www.brutman.com/mTCP/mTCP.html\001\r\n", src );
        outBuf[OUTBUF_LEN-1] = 0;
      }

      else if ( strcmp( cmd, "USERINFO" ) == 0 ) {
        snprintf( outBuf, OUTBUF_LEN, "NOTICE %s :\001USERINFO %s\001\r\n", src, IRCRealName );
        outBuf[OUTBUF_LEN-1] = 0;
      }

      if ( outBuf[0] != 0 ) {
        s->send( (uint8_t *)outBuf, strlen(outBuf) );
      }

    } // end other CTCP commands


  }

  else {

    if ( stricmp( cmd, "PING" ) == 0 ) {

      char textTime[20];
      pos = Utils::getNextToken( pos, textTime, 20 );

      if ( *textTime ) {
        uint32_t t = atol( textTime );
        if ( t == UserPingTime ) {
          uint32_t diff = time( NULL ) - t;
          CS->printf( Session::PrintOpts_Part1, scLocalMsg, "CTCP: PING response from %s in %u seconds\n", src, diff );
        }
      }

    } // end if PING response

    // Default: Just print the command and text without interpretation.

    else {

      CS->printf( Session::PrintOpts_Part1, scLocalMsg, "CTCP: %s response from %s:%s\n", cmd, src, pos ); 

    }

  }
  
}



void handlePrivMsg( char *src, char *target, char *msg, bool privMsg ) {

  if ( *msg == 0 ) return;

  if ( *msg == 0x1 ) {
    handleCtcp( src, target, msg+1, privMsg );
    return;
  }


  // If we are the target then this is a private message from another user.
  // Open a new session for that user if we don't have one.  If we are not
  // the target then this was a message to a channel; open a new session
  // for that channel if we don't have one.

  char *targetSessionName = target;

  if ( stricmp( target, IRCNick ) == 0 ) {
    // We are the target; this is a private message
    targetSessionName = src;
  }

  // NOTICE goes to the current screen, PRIVMSG gets a new one
  Session *tmp = CS;
  if ( privMsg ) { 
    tmp = getTargetSession( targetSessionName, true );
  }


  if ( tmp != ServerSession ) {
    tmp->printf( Session::PrintOpts_Part1, scOtherUserMsg, "<%s> ", src );
    tmp->printf( Session::PrintOpts_Part2, scNormal, "%s\n", msg );
  }
  else {
    // If we ran out of sessions the message format is slightly more verbose
    tmp->printf( Session::PrintOpts_Part1, scOtherUserMsg, "<%s to %s>", src, target );
    tmp->printf( Session::PrintOpts_Part2, scNormal, " %s\n", msg );
  }

  if ( Beeper ) {
    sound(500); delay(20); nosound( );
  }
}



void pollSocket( uint32_t timeout, uint16_t batching ) {

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( 1 ) {

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    int rc = s->recv( inBuf + inBufIndex, (INBUFSIZE - inBufIndex) );
    if ( rc > -1 ) inBufIndex += rc;


    for ( uint8_t i=0; i < batching; i++ ) {
      if ( processSocketInput( ) == false ) break;
    }

    uint32_t t_ms = Timer_diff( startTime, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;

    // Timeout?
    if ( t_ms >= timeout ) break;

  }

}


// If there is a full line of input in the input buffer:
//
// - return a copy of the line in target
// - adjust the input buffer to remove the line
//
// Removing a full line of input and sliding the remaining buffer down
// is slow, but makes the buffer code easier.
//
// Side effects: to keep from redundantly searching, store the index
// of the last char searched in inBufSearchIndex.

uint16_t getLineFromInBuf( char *target ) {

  if ( inBufIndex == 0 ) return 0;

  int i;
  for ( i=inBufSearchIndex; (i < (inBufIndex-1)) && (i < IRC_MSG_MAX_LEN); i++ ) {

    if ( inBuf[i] == '\r' && inBuf[i+1] == '\n' ) {

      // Found delimiter

      // We should only copy i-1, not i, but this is safe and marginally
      // faster.
      memcpy( target, inBuf, i );
      target[i] = 0;

      memmove( inBuf, inBuf+i+2, (inBufIndex - (i+2)) );
      inBufIndex = inBufIndex - (i+2);
      inBufSearchIndex = 0;
      return 1;
    }

  }

  // Remember position for next time
  inBufSearchIndex = i;

  // Not yet
  return 0;
}



bool processSocketInput( void ) {

  char tmpBuffer[ IRC_MSG_MAX_LEN ];
  char *pos;

  if ( !getLineFromInBuf( tmpBuffer ) ) return false;


  char token2[100];


  if ( ShowRaw ) ServerSession->printf( Session::PrintOpts_none, scBright, "%s\n", tmpBuffer );


  int i=0;

  char msgNick[IRCHOSTNAME_MAX_LEN];  // Larger of server name or nick length

  // A prefix is optional.  To ensure that we have a msgNick set it
  // to a default value up front.
  strcpy( msgNick, "Server" );


  // Is there a prefix?
  if ( tmpBuffer[0] == ':' ) {

    // Find out who the sender is.  This is either the server name or a user
    // in usernick!user@server format.  If it is the latter we are only interested
    // in the nick portion.  (See RFC 2812 for the grammar.)


    int j=0;
    int tmpBufferLen = strlen(tmpBuffer);

    for ( i=1; i < tmpBufferLen; i++ ) {
      if ( (tmpBuffer[i]==' ') || (tmpBuffer[i]=='!') || (tmpBuffer[i]=='@') ) {
        msgNick[j] = 0;
        break;
      }
      else {
        if ( j < IRCHOSTNAME_MAX_LEN-1 ) {
          msgNick[j++] = tmpBuffer[i];
        }
      }
    }

    // Check for protocol errors:
    //
    // - hit a space or a '!' or a '@' immediately
    // - ran to the end of tmpBuffer without hitting a space, '!' or '@'

    if ( (j == 0) || (msgNick[j] != 0) ) {
      ServerSession->printf( Session::PrintOpts_Part1, scErr, "Parse error! %s\n", tmpBuffer );
      return false;
    }


    // We either have the server name or the nick.  If it was a nick
    // we still need to get to the first space that delimits the prefix
    // and the command or return code.

    for ( ; i < tmpBufferLen; i++ ) {
      if ( tmpBuffer[i]==' ' ) break;
    }

    // Another protocol check - we had better have found the space without
    // exhausting the buffer.

    if ( i == tmpBufferLen ) {
      ServerSession->printf( Session::PrintOpts_Part1, scErr, "Parse error! %s\n", tmpBuffer );
      return false;
    }

    i++;
  }


  // We now have the nick or server that this message was sent from.

  pos = &tmpBuffer[i];
  char command[20];
  pos = Utils::getNextToken( pos, command, 20 );


  if ( isdigit(command[0]) && isdigit(command[1]) && isdigit(command[2]) && (command[3]==0) ) {
    processServerResp( msgNick, command, pos );
    return true;
  }


  // Put the most used commands up front for best performance.

  // PrivMsg or Notce?
  if ( stricmp( "PRIVMSG", command ) == 0 ) {

    // Get the target
    char token2[100];
    pos = Utils::getNextToken( pos, token2, 100 );

    // +2 to skip the : of the trailing parm
    handlePrivMsg( msgNick, token2, pos+2, true );

  }
  else if ( stricmp( "NOTICE", command ) == 0 ) {

    // Get the target
    char token2[100];
    pos = Utils::getNextToken( pos, token2, 100 );

    // +2 to skip the : of the trailing parm
    handlePrivMsg( msgNick, token2, pos+2, false );

  }

  else if ( stricmp( "JOIN", command ) == 0 ) {

    // Get the new channel name
    pos = getNextParm( pos, token2, 100 );
    char *newChannel = token2;

    if ( stricmp( msgNick, IRCNick ) == 0 ) {

      // Find which session to send it to.  If there is not an existing session
      // then try to create a new one.  If we can't create a new one, it goes
      // to the server session.

      Session *tmp;
      int16_t i = Session::getSessionIndex( newChannel );
      if ( i == -1 ) {
        tmp = Session::createAndMakeActive( newChannel, Screen::getOutputRows() + BsLinesChannel, LoggingDefault );
      }
      else {
        tmp = Session::activeSessionList[i];
      }


      // tmp either points to the session if we just created it (or already
      // it), or to null if we couldn't create it.

      if ( tmp == NULL ) {
        ServerSession->printf( Session::PrintOpts_none, scErr, OutOfSessionWarning, newChannel );
        tmp = ServerSession;
      }

      switchSession( tmp );
      CS->printf( Session::PrintOpts_Part1, scLocalMsg, Msg_JoinedChannel, newChannel );
      CS->draw( );

    }

    else {

      // Somebody else joined a session.  Figure out where to write the message to.

      int16_t i = Session::getSessionIndex( token2 );
      if ( i == -1 ) {
        ServerSession->printf( Session::PrintOpts_Part1, scLocalMsg, Msg_HasJoined, msgNick, newChannel );
      }
      else {
        Session::activeSessionList[i]->printf( Session::PrintOpts_Part1, scLocalMsg, Msg_HasJoined, msgNick, newChannel );
      }

    }

  }
  else if ( stricmp( "PART", command ) == 0 ) {

    // Get the channel name
    pos = getNextParm( pos, token2, 100 );
    char *channel = token2;

    // See if we have a session for this channel

    Session *target = NULL;
    int16_t i = Session::getSessionIndex( channel );
    if ( i != -1 ) {
      target = Session::activeSessionList[i];
    }

    // Are we the sender?  If so, we requested the PART
    if ( stricmp(msgNick, IRCNick) == 0 ) {

      if ( target ) {
        // We had an open session.  Remove it.
        closeSession( target );
      }
      else {
        // We were interested, but did not have an open session.
        // The msg goes to the server window
        ServerSession->printf( Session::PrintOpts_Part1, scLocalMsg, "Parted %s\n", channel );
      }

    }
    else {

      // Somebody else was parting - note it in the appropriate session.

      if ( target ) {
        target->printf( Session::PrintOpts_Part1, scLocalMsg, Msg_HasParted, msgNick, channel );
      }
      else {
        ServerSession->printf( Session::PrintOpts_Part1, scLocalMsg, Msg_HasParted, msgNick, channel );
      }

    }

  }

  else if ( stricmp( "NICK", command ) == 0 ) {

    // Somebody changed their nick

    char newNick[ IRCNICK_MAX_LEN ];
    pos = getNextParm( pos, newNick, IRCNICK_MAX_LEN );

    // Did the user change their own nick?
    if ( stricmp( msgNick, IRCNick ) == 0 ) {

      // Being overly protective here; we are also guarding when they send
      // the NICK command to make sure it wasn't too long.  But just in
      // case they get around that check we'll avoid a buffer overrun.
      // (But they are probably hosed because they have a truncated nick.)

      strncpy( IRCNick, newNick, IRCNICK_MAX_LEN );
      IRCNick[ IRCNICK_MAX_LEN - 1 ] = 0;

      CS->printf( Session::PrintOpts_Part1, scServerMsg, "You changed your nickname to %s\n", IRCNick );

    }
    else {

      if ( NickUpdates != None ) {

        sprintf( outBuf, "%s changed their nickname to %s\n", msgNick, newNick );

        if ( NickUpdates == Server ) {
          ServerSession->printf( Session::PrintOpts_Part1, scServerMsg, "%s", outBuf );
        }
        else if ( NickUpdates == Current ) {
          CS->printf( Session::PrintOpts_Part1, scServerMsg, "%s", outBuf );
        }
        else if ( NickUpdates == All ) {
          broadcastMsg( scServerMsg, outBuf );
        }

      }

    }

  }

  else if ( stricmp( "QUIT", command ) == 0 ) {

    // Somebody quit

    if ( QuitUpdates != None ) {

      getNextParm( pos, token2, 100 );
      sprintf( outBuf, "%s has quit: %s\n", msgNick, token2 );

      if ( QuitUpdates == Server ) {
        ServerSession->printf( Session::PrintOpts_Part1, scServerMsg, "%s", outBuf );
      }
      else if ( QuitUpdates == Current ) {
        CS->printf( Session::PrintOpts_Part1, scServerMsg, "%s", outBuf );
      }
      else if ( QuitUpdates == All ) {
        broadcastMsg( scServerMsg, outBuf );
      }

    }

  }

  else if ( stricmp( "PING", command ) == 0 ) {

    char hostname[ IRCHOSTNAME_MAX_LEN ];
    getNextParm( pos, hostname, IRCHOSTNAME_MAX_LEN );

    uint16_t bytes = sprintf( outBuf, "PONG %s\r\n", hostname );
    s->send( (uint8_t *)outBuf, bytes );

    PingResponses++;
    return false;
  }

  else if ( stricmp( "MODE", command ) == 0 ) {

    // If a channel is the subject of the mode command try to find the
    // session.  Otherwise, use the server session.  (This happens
    // if a user modifies their own mode.)

    Session *tmp = ServerSession;

    pos = getNextParm( pos, token2, IRCNICK_MAX_LEN );

    // Pos is pointing at the last parm.  Skip the ':' that should be on the last parm.
    if ( pos != NULL && *pos == ':' ) pos++;

    if ( *token2 ) {
      if ( *token2 == '#' ) {
        tmp = getTargetSession( token2, false );
      }

      tmp->printf( Session::PrintOpts_Part1, scServerMsg, Msg_SetsMode, msgNick, pos );
    }

  }

  else {

    // Not quite sure what it is ...  Some servers send messages after
    // the user sends a QUIT command that will get processed here.

    CS->printf( Session::PrintOpts_Part1, scErr, "%s\n", tmpBuffer );
  }

  return true;
}






static char HelpText[] =
  "\nIRCjr [options] irc_server [#channel]\n\n"
  "Options:\n"
  "  -help        (Shows this help)\n"
  "  -port <n>    (Specify server port)\n";

static void usage( void ) {
  puts( HelpText );
  exit( 1 );
}


void parseArgs( int argc, char *argv[] ) {

  if ( argc < 2 ) usage( );

  int i=1;
  for ( ; i<argc; i++ ) {

    if ( stricmp( argv[i], "-port" ) == 0 ) {
      i++;
      if ( i == argc ) {
        puts( "Need to provide a port number with the -port option" );
        usage( );
      }
      ServerPort = atoi( argv[i] );
      if ( ServerPort == 0 ) {
        puts( "Check the port number you specified!" );
        usage( );
      }
    }
    else if ( stricmp( argv[i], "-help" ) == 0 ) {
      puts( "Options and usage ..." );
      usage( );
    }
    else if ( argv[i][0] != '-' ) {
      // End of options
      break;
    }
    else {
      printf( "Unknown option: %s\n", argv[i] );
      usage( );
    }

  }


  if ( i == argc ) {
    puts( "Need to provide a server name to connect to" );
    usage( );
  }

  // Next argument is always the server name
  strncpy( IRCServer, argv[i], IRCHOSTNAME_MAX_LEN );
  IRCServer[ IRCHOSTNAME_MAX_LEN - 1 ] = 0;

  i++;
  if ( i == argc ) return;

  // Optional channel was provided
  strncpy( InitialChan, argv[i], IRCCHANNEL_MAX_LEN );
  InitialChan[ IRCCHANNEL_MAX_LEN ] = 0;


}








static char *ParmNames[]  = { "IRCJR_NICK", "IRCJR_USER", "IRCJR_NAME" };
static char *ParmStrs[]   = { IRCNick, IRCUser, IRCRealName };
static uint8_t ParmLens[] = { IRCNICK_MAX_LEN, IRCUSER_MAX_LEN, IRCREALNAME_MAX_LEN };

void getCfgOpts( void ) {

  Utils::openCfgFile( );

  for ( uint16_t i=0; i < 3; i++ ) {
    Utils::getAppValue( ParmNames[i], ParmStrs[i], ParmLens[i] );
    if ( *ParmStrs[i] == 0 ) {
      printf( "Need to set %s in the config file\n", ParmNames[i] );
      exit(1);
    }
  }

  char tmp[50];

  Utils::getAppValue( "IRCJR_PASS", tmp, 50 );
  if ( *tmp != 0 ) {
    strcpy( IRCPass, tmp );
  }

  Utils::getAppValue( "IRCJR_BACKSCROLL", tmp, 10 );
  if ( *tmp != 0 ) {
    BsLinesChannel = atoi( tmp );
  }

  Utils::getAppValue( "IRCJR_BACKSCROLL_CHAT", tmp, 10 );
  if ( *tmp != 0 ) {
    BsLinesChat = atoi( tmp );
  }

  Utils::getAppValue( "IRCJR_BACKSCROLL_SERVER", tmp, 10 );
  if ( *tmp != 0 ) {
    BsLinesServer = atoi( tmp );
  }

  Utils::getAppValue( "IRCJR_COLOR_SCHEME", tmp, 10 );
  if ( stricmp( tmp, "CGA_MONO" ) == 0 ) {
    ColorScheme = 1;
  }

  Utils::getAppValue( "IRCJR_CONNECT_TIMEOUT", tmp, 10 );
  ConnectTimeout = (uint32_t)(atoi(tmp)) * 1000ul;
  if ( ConnectTimeout == 0 ) {
    ConnectTimeout = 30000ul;
  }

  // Read registration timeout value
  Utils::getAppValue( "IRCJR_REGISTER_TIMEOUT", tmp, 10 );
  RegisterTimeout = atoi(tmp) * 1000ul;
  if ( RegisterTimeout == 0 ) {
    RegisterTimeout = 30000; // Thirty seconds in milliseconds
  }

  Utils::getAppValue( "IRCJR_TIMESTAMPS", tmp, 10 );
  if ( stricmp( tmp, "on" ) == 0 ) {
    Timestamp = true;
  }

  if ( Utils::getAppValue( "IRCJR_NICK_UPDATES", tmp, 10 ) == 0 ) {

    if ( stricmp( tmp, "none" ) == 0 ) {
      NickUpdates = None;
    }
    else if ( stricmp( tmp, "server" ) == 0 ) {
      NickUpdates = Server;
    }
    else if ( stricmp( tmp, "current" ) == 0 ) {
      NickUpdates = Current;
    }
    else if ( stricmp( tmp, "all" ) == 0 ) {
      NickUpdates = All;
    }

  }

  if ( Utils::getAppValue( "IRCJR_QUIT_UPDATES", tmp, 10 ) == 0 ) {

    if ( stricmp( tmp, "none" ) == 0 ) {
      QuitUpdates = None;
    }
    else if ( stricmp( tmp, "server" ) == 0 ) {
      QuitUpdates = Server;
    }
    else if ( stricmp( tmp, "current" ) == 0 ) {
      QuitUpdates = Current;
    }
    else if ( stricmp( tmp, "all" ) == 0 ) {
      QuitUpdates = All;
    }

  }

  Utils::getAppValue( "IRCJR_LOGGING_DEFAULT", tmp, 10 );
  if ( stricmp( tmp, "on" ) == 0 ) {
    LoggingDefault = true;
  }

  if ( Utils::getAppValue( "IRCJR_LOG_DIR", LogDirectory, 67 ) == 0 ) {

    uint16_t len = strlen( LogDirectory );

    if ( len > 0 ) {

      // Insist on a backslash at the end to be explicit that it is a directory.

      if ( LogDirectory[len-1] != '\\' ) {
        printf( "Log directory '%s' needs to end in a backslash\n", LogDirectory );
        exit(1);
      }

      // Stat the input to see if it is a directory.  But some versions of DOS don't
      // like the trailing backslash, so strip it temporarily.

      LogDirectory[len-1] = 0;

      uint16_t attrs;

      if ( (getFileAttributes( LogDirectory, &attrs ) != 0) || ((attrs & 0x10) != 0x10)  ) {
        printf( "Log directory '%s\\' is not a directory (%x)\n", LogDirectory, attrs );
        exit(1);
      }

      // Be sure to put it back!
      LogDirectory[len-1] = '\\';

    }

  }

      

  Utils::closeCfgFile( );

}




// This is crazy, but it was fun.

extern uint16_t smallDivide( unsigned char i );
#pragma aux smallDivide = \
  "mov dl, 10" \
  "idiv dl" \
  "add al,48" \
  "add ah,48" \
  modify [ax dl] \
  parm [ax] \
  value [ax];


static char CurrentTimeStr[9] = "00:00:00";

char *getTimeStr( void ) {

  DosTime_t current;
  gettime( &current );

  uint8_t *tmp = (uint8_t *)CurrentTimeStr;

  uint16_t t = smallDivide( current.hour );
  *(uint16_t *)tmp = t;
  tmp += 3;

  t = smallDivide( current.minute );
  *(uint16_t *)tmp = t;
  tmp += 3;

  t = smallDivide( current.second );
  *(uint16_t *)tmp = t;

/*
  CurrentTimeStr[0] = (current.hour / 10) + 48;
  CurrentTimeStr[1] = (current.hour % 10) + 48;

  CurrentTimeStr[3] = (current.minute / 10) + 48;
  CurrentTimeStr[4] = (current.minute % 10) + 48;

  CurrentTimeStr[6] = (current.second / 10) + 48;
  CurrentTimeStr[7] = (current.second % 10) + 48;
*/

  return CurrentTimeStr;
}






static uint8_t logoBitmap[] = {
  0xF3, 0xF0, 0x78, 0x0C, 0x00,
  0x61, 0x98, 0xCC, 0x00, 0x00,
  0x61, 0x99, 0x80, 0x0C, 0xDC,
  0x61, 0xE1, 0x80, 0x0C, 0x76,
  0x61, 0xB1, 0x80, 0x0C, 0x66,
  0x61, 0x98, 0xCC, 0xCC, 0x60,
  0xF3, 0x98, 0x78, 0xCC, 0xF0,
  0x00, 0x00, 0x00, 0x78, 0x00
};



void initScreen( void ) {

  // Setup the color palette.  There are three options:
  //
  //   Text mode with color (CGA to VGA)
  //   Text mode with no color (CGA, but on a monochrome screen)
  //   Text mode for monochrome (MDA only)

  if ( Screen::isColorCard() ) {

    if ( ColorScheme == 0 ) {
      scErr          = 0x40; //        Black   on red
      scNormal       = 0x07; //        White   on black
      scBright       = 0x0F; // Bright White   on black
      scReverse      = 0x70; //        Black   on white
      scServerMsg    = 0x0E; //        Yellow  on black
      scUserMsg      = 0x0F; // Bright White   on black
      scTitle        = 0x1F; // Bright White   on blue
      scOtherUserMsg = 0x02; //        Green   on black
      scActionMsg    = 0x05; //        Magenta on black
      scLocalMsg     = 0x03; //        Cyan    on black
      scBorder       = 0x0C; // Bright Red     on black
      scCommandKey   = 0x09; // Bright Blue    on black
    }
    else { // CGA_MONO
      scErr          = 0x70;
      scNormal       = 0x07;
      scBright       = 0x0F;
      scReverse      = 0x70;
      scServerMsg    = 0x0F;
      scUserMsg      = 0x0F;
      scTitle        = 0x0F;
      scOtherUserMsg = 0x07;
      scActionMsg    = 0x0F;
      scLocalMsg     = 0x0F;
      scBorder       = 0x0F;
      scCommandKey   = 0x0F;
    }
  }
  else {
    scErr          = 0x70; // Reverse
    scNormal       = 0x07; // Normal
    scBright       = 0x0F; // Bright
    scReverse      = 0x70; // Black  on white
    scServerMsg    = 0x01; // Underline
    scUserMsg      = 0x0F; // Bright
    scTitle        = 0x0F; // Bright
    scOtherUserMsg = 0x07; // Normal
    scActionMsg    = 0x0F; // Bright
    scLocalMsg     = 0x0F; // Bright
    scBorder       = 0x0F;
    scCommandKey   = 0x01; // Underline
  }


  for ( uint16_t i=0; i < 8; i++ ) {
    for ( uint16_t j=0; j < 5; j++ ) {
      for ( int16_t k=7; k > -1; k-- ) {
        if ( logoBitmap[ i*5+j ] & (1<<k) ) {
          CS->puts( scTitle, "\xB0" );
        }
        else {
          CS->puts( scNormal, " " );
        }
      }
    }
    CS->puts( scNormal, "\n" );
  }


  CS->puts( scNormal, "\n" );
  CS->puts( scTitle, CopyrightMsg1 );
  CS->puts( scNormal, "  " );
  CS->puts( scTitle, CopyrightMsg2 );

  CS->printf( Session::PrintOpts_none, scNormal, "IP Address:  %d.%d.%d.%d\n",
           MyIpAddr[0], MyIpAddr[1], MyIpAddr[2], MyIpAddr[3] );

  CS->printf( Session::PrintOpts_none, scNormal, "MAC Address: %02X.%02X.%02X.%02X.%02X.%02X\n",
           MyEthAddr[0], MyEthAddr[1], MyEthAddr[2],
           MyEthAddr[3], MyEthAddr[4], MyEthAddr[5] );

  CS->printf( Session::PrintOpts_none, scNormal, "Packet interrupt: 0x%02X\n\n", Packet_getSoftwareInt() );

  CS->draw( );
}



// We should be using this for parsing IRC messages.  It is just like the
// normal Utils::getNextToken, but it recognizes the special trailing
// parameter and returns it as a single token (even with embedded
// white spaces) if it is found.

char * getNextParm( char *input, char *target, uint16_t bufLen ) {

  // Be as compatible with Utils::getNextToken as possible, including
  // defending against bad input.

  if ( input == NULL ) {
    *target = 0;
    return NULL;
  }


  // Skip leading whitespace

  int i=0;
  while ( (input[i]) && isspace(input[i]) ) {
    i++;
  }

  if ( input[i] == 0 ) {
    *target = 0;
    return NULL;
  }


  // If we made it here we have a non-whitespace character.

  if ( input[i] == ':' ) {

    // This is a trailing parameter.  Return the rest of the string
    // as the token

    i++;

    int j = 0;
    while ( input[i] && (j < bufLen-1) ) {
      target[j] = input[i];
      i++;
      j++;
    }

    target[j] = 0;
    return NULL;

  }

  // Normal parameter.  Use the standard parser.
  return Utils::getNextToken( input+i, target, bufLen );

}



void ERRBEEP( void ) {
  sound(1000); delay(250); nosound( );
}
