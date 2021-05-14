/*

   mTCP Utils.H
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


   Description: Data structures for utility functions common to all
     of the applications.

   Changes:

   2011-04-29: Add sleep calls to packet processing macros
   2011-05-27: Initial release as open source software
   2013-03-24: Add inline function for getting file attributes
   2013-04-10: Move getEgaMemSize here (several programs are using it)
   2014-05-18: Add static assert macro
   2015-01-17: Break out the inline utils and tracing into new files
   2015-01-18: Move Ctrl-Break and Ctrl-C initialization to initStack

*/


#ifndef _UTILS_H
#define _UTILS_H


#include <dos.h>
#include <stdio.h>

#include CFG_H
#include "types.h"
#include "inlines.h"





//-----------------------------------------------------------------------------
//

#ifdef SLEEP_CALLS

// On ancient hardware without power management or multitasking there is no
// point to making a sleep call.  But on newer hardware or hardware that is
// virtual/emulated it makes sense.
//
// Int 28 is the DOS "idle" interrupt.  DOS uses this to signal TSRs that a
// user is probably pondering their next keystroke at the keyboard, and that
// there is plenty of time to do background tasks.  This also works with
// FDAPM.
//
// Int 2F is the multiplex interrupt.  Function 1680 says that the application
// is willingly giving up the time slice.  If you call this the first time and
// you get a zero back, then the function is supported and should be used.
// This works under WinXP with SwsVpkt.
//
// If sleeping is compiled in and enabled we always call int 28.  If int 2F
// function 1680 is available we do that too.


extern void dosIdleCall( void );
#pragma aux dosIdleCall =      \
  "int 0x28";


extern uint8_t releaseTimeslice( void );
#pragma aux releaseTimeslice = \
  "mov ax,0x1680"              \
  "int 0x2f"                   \
  modify [ax]                  \
  value  [al];


// Globals that tell us if we should be making the sleep calls.

extern uint8_t mTCP_sleepCallEnabled;
extern uint8_t mTCP_releaseTimesliceEnabled;


// The sleep macro that gets called during idle periods.

#define SLEEP( ) { if ( mTCP_sleepCallEnabled ) { dosIdleCall( ); if ( mTCP_releaseTimesliceEnabled ) releaseTimeslice( ); } }

#else

#define SLEEP( )

#endif





//-----------------------------------------------------------------------------
//

// Packet driving macros
//
// You have to use one of these to check for and process incoming packets.
// Ideally you do this when your application is sitting around doing nothing
// else, or when you are waiting for network traffic.
//
// Note that IP_FRAGS_CHECK_OVERDUE and SLEEP might not produce any code;
// it depends on your compile options.
//
// This is structured so that SLEEP is only called if a packet is not
// processed.  (If there was a packet to process, you probably don't
// want to give up the CPU right then.)


#ifdef IP_FRAGMENTS_ON
#define IP_FRAGS_CHECK_OVERDUE( ) if ( Ip::fragsInReassembly ) Ip::purgeOverdue( );
#else
#define IP_FRAGS_CHECK_OVERDUE( )
#endif


#define PACKET_PROCESS_SINGLE                                     \
{                                                                 \
  if ( Buffer_first != Buffer_next ) {                            \
    Packet_process_internal( );                                   \
  }                                                               \
  else {                                                          \
    SLEEP( );                                                     \
  }                                                               \
  IP_FRAGS_CHECK_OVERDUE( );                                      \
}



// Use this one when dealing with lots of small packets and the receive
// buffer.

#define PACKET_PROCESS_MULT( n )                                  \
{                                                                 \
  uint8_t i=0;                                                    \
  while ( i < n ) {                                               \
    if ( Buffer_first != Buffer_next ) {                          \
      Packet_process_internal( );                                 \
    }                                                             \
    else {                                                        \
      SLEEP( );                                                   \
      break;                                                      \
    }                                                             \
    i++;                                                          \
  }                                                               \
  IP_FRAGS_CHECK_OVERDUE( );                                      \
}




//-----------------------------------------------------------------------------
//
// Utils class
//
// In general, a zero return code means that things worked.  A non-zero
// return code is an error; see the code for possible error values.


#define UTILS_LINEBUFFER_LEN (160)
#define UTILS_PARAMETER_LEN (40)

class Utils {

  public:

    // Read the critical TCP/IP values from the mTCP configuration file.
    static int8_t   parseEnv( void );

    // Parse the TCP/IP environment variables only; this is used by DHCP
    // because it can not use parseEnv.  (parseEnv calls it under the covers
    // so the code is not duplicated.
    //
    static void     parseOptionalEnvVars( void );


    // If you have more configuration settings to read, use these.
    //
    static FILE    *openCfgFile( void );
    static void     closeCfgFile( void );
    static int8_t   getAppValue( const char *target, char *val, uint16_t valBufLen );

    // Configuration file related, but basically not for end-user usage.
    static int      getLine( FILE *inputFile, bool removeNewline, char *buffer, int bufferLen, int lineNumber );


    // initStack sets up all of the layers of the stack.
    //
    // If you are not using TCP then the first two parameters can be zero.
    // But the Ctrl-Break and Ctrl-C handlers must be provided.  (They can be
    // the same function if the desired action is the same.)  These must be real
    // functions, not NULL, or you will crash and burn horribly.
    //
    static int8_t   initStack( uint8_t tcpSockets, uint8_t tcpXmitBuffers,
                               void __interrupt __far (*newCtrlBreakHandler)(),
                               void __interrupt __far (*newCtrlCHandler)() );


    // endStack shuts everything down.  Ideally you should make sure that all
    // of the sockets are closed and all buffers are returned before calling
    // this.  But it should be safe even if you did not.
    //
    static void     endStack( void );


    // Dump stats for TCP, IP, and the Packet layer
    static void     dumpStats( FILE *stream );


    // Generic utility functions
    //
    static void      dumpBytes( FILE *stream, unsigned char *, unsigned int );
    static uint32_t  timeDiff( DosTime_t startTime, DosTime_t endTime );
    static char     *getNextToken( char *input, char *target, uint16_t bufLen );
    static bool      rtrim( char *nullTerminatedString );


    // Used by the DHCP client to set the desired packet software interrupt
    // because the DHCP client does not use Utils::parseEnv which would set it.
    //
    static void      setPacketInt( uint8_t packetInt_p ) { packetInt = packetInt_p; }


    // Temporary buffers - make them public so DHCP can use them too.
    static char lineBuffer[UTILS_LINEBUFFER_LEN];
    static char parmName[UTILS_PARAMETER_LEN];

  private:

    // The packet interrupt to use specified in the configuration file.
    static uint8_t packetInt;

    static FILE    *CfgFile;
    static char    *CfgFilenamePtr;  // This points into our environment!

    // The old Ctrl-Break handler so we can restore it in endStack.
    static void ( __interrupt __far *oldCtrlBreakHandler)( );


};



// Parameter Names

extern const char Parm_PacketInt[];
extern const char Parm_Hostname[];
extern const char Parm_IpAddr[];
extern const char Parm_Gateway[];
extern const char Parm_Netmask[];
extern const char Parm_Nameserver[];
extern const char Parm_Nameserver_preferred[];
extern const char Parm_Mtu[];


#endif
