/*

   mTCP Ymodem.cpp
   Copyright (C) 2012-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Xmodem and Ymodem code

   Changes:

   2012-04-29: Created when xmodem and ymodem were added
   2015-01-22: Add 132 column support


*/



#include CFG_H

#ifdef FILEXFER



#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/utime.h>

#include "types.h"

#include "trace.h"
#include "utils.h"
#include "packet.h"
#include "arp.h"
#include "tcp.h"

#include "globals.h"
#include "telnetsc.h"
#include "ymodem.h"





#define TEL_IAC (0xFF)

#define EXTRA_FILE_BUFFER_SIZE (4096)



// Globals for Ymodem download

Transfer_vars transferVars;

char *ExtraFileBuffer = NULL;



const char Transfer_vars::startDownloadBytes[] = { XMODEM_NAK, 'C', 'C', 'C', 'G' };  // Need to line up with the protocol enum

const char *Transfer_vars::protocolNames[] = {
  "Xmodem Checksum",
  "Xmodem CRC",
  "Xmodem 1K/CRC",
  "Ymodem Batch",
  "Ymodem G Batch"
};





// Very basic utility functions lifted from FTP.CPP
//

static char DosChars[] = "!@#$%^&()-_{}`'~";

static uint8_t isValidDosChar( char c ) {

  if ( isalnum( c ) || (c>127) ) return 1;

  for ( uint8_t i=0; i < 16; i++ ) {
    if ( c == DosChars[i] ) return 1;
  }

  return 0;
}


static uint8_t isValidDosFilename( const char *filename ) {

  if (filename==NULL) return 0;

  uint8_t len = strlen(filename);

  if ( len == 0 ) return 0;

  if ( !isValidDosChar( filename[0] ) ) return 0;

  uint8_t i;
  for ( i=1; (i<8) && (i<len) ; i++ ) {
    if ( filename[i] == '.' ) break;
    if ( !isValidDosChar( filename[i] ) ) return 0;
  }

  if ( i == len ) return 1;

  if ( filename[i] != '.' ) return 0;

  i++;
  uint8_t j;
  for ( j=0; (j+i) < len; j++ ) {
    if ( !isValidDosChar( filename[j+i] ) ) return 0;
  }

  if ( j > 3 ) return 0;

  return 1;
}




// Checksum and CRC functions
//


// Loop unrolled four times to make it really scream.

static uint8_t xmodem_calcChecksum( uint8_t *ptr, int16_t count ) {
  uint8_t rc = 0;
  while ( count ) { rc += *ptr++; count--; rc += *ptr++; count--; rc += *ptr++; count--; rc += *ptr++; count--; }
  return rc;
}



/* My original CRC algorith - slow, but it works

static uint16_t xmodem_calcCRC(uint8_t *ptr, int16_t count) {

  uint16_t crc = 0;

  for ( uint16_t j=0; j < count; j++ ) {

    crc = crc ^ (((uint16_t)(*ptr)) << 8 );
    ptr++;

    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc = crc << 1;
    }

  }

  return (crc & 0xFFFF);
}
*/



// Table lookup version of CRC algorithms.  Adapted from
// http://www.barrgroup.com/Embedded-Systems/How-To/CRC-Calculation-C-Code 


static uint16_t crcTable[256];

void xmodem_crcInit(void) {

  uint16_t remainder;
  int      dividend;
  uint16_t  bit;

  for (dividend = 0; dividend < 256; ++dividend) {

    remainder = dividend << 8;

    for (bit = 8; bit > 0; --bit) {

      if (remainder & 0x8000) {
        remainder = (remainder << 1) ^ 0x1021;
      }
      else {
        remainder = (remainder << 1);
      }
    }

    crcTable[dividend] = remainder;
  }

}   /* crcInit() */





/* Original version of table based CRC algorithm.

uint16_t xmodem_calcCRC(uint8_t *message, uint16_t nBytes) {

  uint16_t remainder = 0;
  uint8_t data;
  int byte;

  for (byte = 0; byte < nBytes; ++byte) {

    data = message[byte] ^ (remainder >> 8);
    remainder = crcTable[data] ^ (remainder << 8);

  }

  return remainder;

}
*/



// Small tweak on above to make it more efficient for Watcom.
// Also loop unrolled once to make it slightly better.

static uint16_t xmodem_calcCRC(uint8_t *message, uint16_t nBytes) {

  union {
    uint16_t c;
    struct {
      uint8_t low;
      uint8_t high;
    };
  } r;

  r.c = 0;

  for (uint16_t byte = 0; byte < nBytes; ++byte) {
    r.c = crcTable[ message[byte] ^ (r.high) ] ^ (r.low << 8);
    ++byte;
    r.c = crcTable[ message[byte] ^ (r.high) ] ^ (r.low << 8);
  }

  return r.c;

}





void initForXmodem( void ) {
  xmodem_crcInit( );
  // It's ok to not check the return code here; if it is NULL we just won't
  // use it.
  ExtraFileBuffer = (char *)malloc( EXTRA_FILE_BUFFER_SIZE );
}




static int8_t transfer_SendByte( uint8_t data ) {

  // If there is no room in the outgoing queue then do some processing
  // to make some room.  This should not happen.

  while ( mySocket->outgoingQueueIsFull( ) ) {

    if ( mySocket->isRemoteClosed( ) ) {
      // Whoops.  We are done ...
      return 1;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );
    
  }


  DataBuf *buf;

  while ( (buf = (DataBuf *)TcpBuffer::getXmitBuf( )) == NULL ) {

    // If we can not get a transmit buffer then we better start processing
    // some packets!  This should almost never happen.

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    if ( mySocket->isRemoteClosed( ) ) {
      // Whoops.  We are done ...
      return 1;
    }

  }


  // Great.  We have an outgoing buffer and we know we can send it.
  
  buf->b.dataLen = 1;
  buf->data[0] = data;

  if ( mySocket->enqueue( &buf->b ) != 0 ) {
    // Something went wrong.  The end is near.  Put the buffer back in
    // the pool to be correct.
    TcpBuffer::returnXmitBuf( (TcpBuffer *)buf );
  }


  // Push it out!

  PACKET_PROCESS_SINGLE;
  Tcp::drivePackets( );

  transferVars.bumpTimer( );

  return 0;
}



static void recvFlush( uint32_t timeoutInMs ) {

  // Loop for designated amount of time eating any data that arrives on the
  // socket.  If the socket closes the caller needs to detect it.

  clockTicks_t start = TIMER_GET_CURRENT( );

  uint8_t remoteDone = 0;

  while ( !remoteDone ) {

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    // Keep wiping out the recv buffer in case the other side is furiously
    // sending data.
    mySocket->flushRecv( );

    if ( Timer_diff( start, TIMER_GET_CURRENT( ) ) > TIMER_MS_TO_TICKS( timeoutInMs ) ) break;

    remoteDone = mySocket->isRemoteClosed( );

  }

}







// Screen drawing and user input routines
//



void drawBox( int ul_x, int ul_y, int lr_x, int lr_y ) {

  uint16_t attr = scFileXfer << 8;
  uint16_t middleLen = lr_x - ul_x - 1;

  uint16_t far *start = (uint16_t far *)(s.Screen_base + (ul_y*s.terminalCols+ul_x)*2);

  uint16_t far *start2 = start;
  if ( s.isPreventSnowOn( ) ) { waitForCGARetraceLong( ); }
  *start2++ = (attr|0xc9);
  fillUsingWord( start2, (attr|0xcd), middleLen );
  start2 += middleLen;
  *start2 = (attr|0xbb);

  start2 = start += s.terminalCols;

  for ( uint8_t i=ul_y+1; i < lr_y; i++ ) {
    if ( s.isPreventSnowOn( ) ) { waitForCGARetraceLong( ); }
    *start2++ = (attr|0xba);
    fillUsingWord( start2, (attr|' '), middleLen );
    start2 += middleLen;
    *start2 = (attr|0xba);
    start2 = start += s.terminalCols;
  }

  if ( s.isPreventSnowOn( ) ) { waitForCGARetraceLong( ); }
  *start++ = (attr|0xc8);
  fillUsingWord( start, (attr|0xcd), middleLen );
  start += middleLen;
  *start = (attr|0xbc);


/*
  s.putch( ul_x, ul_y, scFileXfer, 0xc9 );
  s.repeatCh( ul_x+1, ul_y, scFileXfer, 0xcd, lr_x - ul_x - 1 );
  s.putch( lr_x, ul_y, scFileXfer, 0xbb );

  for ( uint8_t i=ul_y+1; i < lr_y; i++ ) {
    s.putch( ul_x, i, scFileXfer, 0xba );
    s.repeatCh( ul_x+1, i, scFileXfer, ' ', lr_x - ul_x - 1 );
    s.putch( lr_x, i, scFileXfer, 0xba );
  }

  s.putch( ul_x, lr_y, scFileXfer, 0xc8 );
  s.repeatCh( ul_x+1, lr_y, scFileXfer, 0xcd, lr_x - ul_x - 1 );
  s.putch( lr_x, lr_y, scFileXfer, 0xbc );
*/

}




typedef _Packed struct {
  uint8_t x;
  uint8_t y;
  char *string;
} ProtocolMenu_t;

ProtocolMenu_t ProtocolMenu[] = {
  {  9,  9, "\xb5 Protocol \xc6" },
  {  8, 11, "1) Xmodem" },
  {  8, 12, "2) Xmodem CRC" },
  {  8, 13, "3) Xmodem 1K" },
  {  8, 14, "4) Ymodem Batch" },
#ifdef YMODEM_G
  {  8, 15, "5) Ymodem G" },
  {  6, 17, "ESC) Cancel" },
  {  6, 19, "Protocol:" },
#else
  {  6, 16, "ESC) Cancel" },
  {  6, 18, "Protocol:" },
#endif
  { 99,  0, "" }
};


void drawProtocolMenu( void ) {

  #ifdef YMODEM_G
  drawBox( 4, 9, 24, 21 );
  #else
  drawBox( 4, 9, 24, 20 );
  #endif

  int i = 0;
  while ( ProtocolMenu[i].x != 99 ) {
    s.myCprintf( ProtocolMenu[i].x, ProtocolMenu[i].y, scFileXfer, ProtocolMenu[i].string );
    i++;
  }

  #ifdef YMODEM_G
  gotoxy( 16, 19 );
  #else
  #endif
}



void drawFilenameWindow( void ) {
  drawBox( 6, 11, 31, 16 );
  s.myCprintf( 8, 13, scBright, "Filename:" );
  gotoxy( 18, 13);
}


void drawClobberDialogWindow( void ) {
  drawBox( 9, 14, 36, 19 );
  s.myCprintf( 11, 16, scFileXfer, "Overwrite %s ?", transferVars.filename );
  s.myCprintf( 11, 17, scFileXfer, "  (Y/N):" );
  gotoxy( 20, 17 );
}



void drawFileStatusWindow( void ) {

  drawBox( 4, 9, 45, 17 );

  s.myCprintf( 7, 10, scFileXfer, "Name: %s", transferVars.filename );

  if ( transferVars.expectedFilesize == 0 ) {
    s.myCprintf( 7, 11, scFileXfer, "Size: Unknown" );
  }
  else {
    s.myCprintf( 7, 11, scFileXfer, "Size: %lu", transferVars.expectedFilesize );
  }
    
  char modDateStr[25];
  if ( transferVars.modificationDate ) {
    struct tm time_of_day;
    _localtime( &transferVars.modificationDate, &time_of_day );
    _asctime( &time_of_day, modDateStr );
    modDateStr[24] = 0; // Get rid of unwanted carriage return
  }
  else {
    strcpy( modDateStr, "(none)" );
  }
  s.myCprintf( 7, 12, scFileXfer, "Date: %s", modDateStr );

  s.myCprintf( 7, 13, scFileXfer, "Prot: %s", transferVars.protocolNames[ transferVars.fileProtocol ] );

  s.myCprintf( 6, 14, scFileXfer, "Bytes:" );
  s.myCprintf( 7, 15, scFileXfer, "Pkts:" );
  s.myCprintf( 8, 16, scFileXfer, "Msg:" );

}


void updateFileStatus( void ) {
  s.myCprintf( 13, 14, scFileXfer, "%lu", transferVars.bytesXferred );
  s.myCprintf( 13, 15, scFileXfer, "%lu", transferVars.packetsXferred );
}

void updateFileMsg( uint8_t attr, char *text ) {
  s.myCprintf( 13, 16, scFileXfer, "%-31s", " " );
  s.myCprintf( 13, 16, attr, "%s", text );
}





// Abort and cleanup

static void endTransfer( bool isErr, bool sendCancels, char *finalMsg ) {

  uint8_t attr = scFileXfer;
  if ( isErr ) attr = scErr;

  if ( sendCancels ) {
    transfer_SendByte( XMODEM_CAN );
    transfer_SendByte( XMODEM_CAN );
  }

  updateFileMsg( attr, finalMsg );

  if ( transferVars.targetFile != NULL ) {
    fclose( transferVars.targetFile );
    transferVars.targetFile = NULL;
  }

  // Setup the main program so that one key has to be hit to make the window
  // go away.

  SocketInputMode = SocketInputMode_t::Telnet;
  UserInputMode = UserInputMode_t::Help;
  s.doNotUpdateRealScreen( );

  setTelnetBinaryMode( false );
}





void Transfer_vars::checkForDownloadTimeout( void ) {

  // Assume 3 seconds is more than enough.  Do we need different timeouts for different states?

  if ( Timer_diff( getLastActivity( ), TIMER_GET_CURRENT( ) ) < TIMER_MS_TO_TICKS( 5000 ) ) return;

  TRACE(( "Download: Timeout State: %u  Retries: %u\n", packetState, retries ));

  if ( retries == 3 ) {
    endTransfer( true, true, "Too many errors: aborting" );
    return;
  }

  retries++;

  updateFileMsg( scErr, "Timeout - Retrying" );


  recvFlush( 500 );  // 500ms of flushing just to be sure

  uint8_t retryByte;


  if ( packetState == HeaderByte ) {

    // Was waiting for the start of a header.  This is the mostly likely place
    // to have a timeout.

    // We could be waiting for a header or the first data packet.  We send the
    // same start character for both conditions.

    retryByte = startDownloadBytes[fileProtocol];

  }
  else {
    // Timeout in the middle of a packet.  NAK it.
    retryByte = XMODEM_NAK;
  }

  transfer_SendByte( retryByte );

}






// Transfer_vars::parseYmodemHeader
//
// On a Ymodem download only the filename is guaranteed.  After that there
// may be an optional file size and then an option modification date.  If
// the other size sends a modification date it has to send a file size, but
// that might be zero for "don't know".
//

Transfer_vars::ParseHeaderRc_t Transfer_vars::parseYmodemHeader( void ) {

  filename[0] = 0;
  expectedFilesize = 0;
  modificationDate = 0;

  // If this is a null header then there are no more files to fetch.
  if ( ymodemPacket[0] == 0 ) return NoMoreFiles;


  uint16_t len = 0;
  char *tmp = (char *)ymodemPacket;


  while ( *tmp && len < 12 ) {
    filename[len++] = *tmp++;
  }

  if ( *tmp && len == 12 ) return BadFilename;

  // Terminate the string and move past the null in the buffer.
  filename[len++] = 0;
  tmp++;

  if ( !isValidDosFilename(filename) ) return BadFilename;




  // See if there is a file size present

  expectedFilesize = atoll( tmp );

  
  // Scan ahead for the next space for the modification date.
  // Reset len to be relative to where tmp is so we can stop
  // the scan within a reasonable time.

  len = 0;
  while ( *tmp != ' ' && (len < 10) ) {
    tmp++; len++;
  }


  if ( *tmp == ' ' ) {
    // Found a space and not a null.  Try to read the modification dae.

    // Advance past space to get to the first digit
    tmp++;

    // Set, but it is in octal.  The MAX_INT32 is only 11 octal
    // digits so cut it off at 12 if it is invalid.

    uint32_t modDate = 0;

    len = 0;
    while ( (*tmp != 0) && (*tmp != ' ') && (len < 12) ) {
      modDate = (modDate << 3) + (*tmp - '0');
      tmp++; len++;
    }

    if ( len == 12 ) {
      modDate = 0;
    }

    modificationDate = modDate;

  }



  // If the file exists and is a regular file then we can prompt the user
  // to see if they want to overwrite it.  If it exists but is something
  // else then return an error.

  struct stat stat_buf;
  int rc = stat( transferVars.filename, &stat_buf );

  if ( rc == 0 && S_ISREG(stat_buf.st_mode) ) {
    return PromptClobber;
  }
  else if ( rc == 0 ) {
    return CantClobber;
  }

  return RequestNext;
}








void processUserInput_FileProtocol( Key_t key ) {

  if ( key.specialKey != K_NormalKey ) return;

  // No matter what happens the filename is going to start fresh
  transferVars.filenameIndex = 0;

  if ( key.normalKey == '1' ) {
    transferVars.fileProtocol = Transfer_vars::Xmodem;
    UserInputMode = (UserInputMode_t::e)(UserInputMode + 2);
  }
  else if ( key.normalKey == '2' ) {
    transferVars.fileProtocol = Transfer_vars::Xmodem_CRC;
    UserInputMode = (UserInputMode_t::e)(UserInputMode + 2);
  }
  else if ( key.normalKey == '3' ) {
    transferVars.fileProtocol = Transfer_vars::Xmodem_1K;
    UserInputMode = (UserInputMode_t::e)(UserInputMode + 2);
  }
  else if ( key.normalKey == '4' ) {
    transferVars.fileProtocol = Transfer_vars::Ymodem;

    if ( UserInputMode == UserInputMode_t::ProtocolSelect_Download ) {
      transferVars.startDownload( );
    }
    else {
      UserInputMode = UserInputMode_t::FilenameSelect_Upload;
    }
  }
  #ifdef YMODEM_G
  else if ( key.normalKey == '5' ) {
    transferVars.fileProtocol = Transfer_vars::Ymodem_G;

    if ( UserInputMode == UserInputMode_t::ProtocolSelect_Download ) {
      transferVars.startDownload( );
    }
    else {
      UserInputMode = UserInputMode_t::FilenameSelect_Upload;
    }
  }
  #endif
  else if ( key.normalKey == 27 ) {
    UserInputMode = UserInputMode_t::Telnet;
    s.paint( );
  }

  if ( UserInputMode == UserInputMode_t::FilenameSelect_Download || UserInputMode == UserInputMode_t::FilenameSelect_Upload ) {
    drawFilenameWindow( );
  }


}



void processUserInput_ClobberDialog( Key_t key ) {

  if ( key.specialKey != K_NormalKey ) return;

  s.putch( 20, 17, scFileXfer, key.normalKey );  

  if ( UserInputMode == UserInputMode_t::ClobberDialog ) {
    if ( key.normalKey == 'Y' || key.normalKey == 'y' ) {
      transferVars.startDownload( );
    }
    else if ( key.normalKey == 'N' || key.normalKey == 'n' ) {
      SocketInputMode = SocketInputMode_t::Telnet;
      UserInputMode = UserInputMode_t::Help;
      s.doNotUpdateRealScreen( );
    }
  }
  else {
    if ( key.normalKey == 'Y' || key.normalKey == 'y' ) {
      UserInputMode = UserInputMode_t::TransferInProgress;
      transferVars.startNextYmodemFile( );
    }
    else if ( key.normalKey == 'N' || key.normalKey == 'n' ) {
      drawFileStatusWindow( );
      updateFileStatus( );
      endTransfer( true, true, "User said no clobber" );
    }
  }

}
    
    



// This routine processes keystrokes during a file transfer.  Right now
// the only conceived usage is to abort the current transfer.

void processUserInput_Transferring( Key_t key ) {
  if ( (key.specialKey == K_NormalKey) && (key.normalKey == 27) ) {
    endTransfer( true, true, "Aborted by user!" );
  }
}




// Read keystrokes for a filename and put them on the screen.  Provide
// some basic editing and error checking.  For use with the file download
// dialog box.

void processUserInput_Filename( Key_t key ) {

  if ( key.specialKey == K_Enter ) {

    transferVars.filename[ transferVars.filenameIndex] = 0;

    if ( isValidDosFilename( transferVars.filename ) ) {

      // Upper case it
      strupr( transferVars.filename );

      // Probably want to do some sanity checking on the filename.  Like no clobber


      if ( UserInputMode == UserInputMode_t::FilenameSelect_Download ) {

        struct stat stat_buf;
        int rc = stat( transferVars.filename, &stat_buf );

        if ( rc == -1 ) {
          // Does not exist
          transferVars.startDownload( );
        }
        else if ( rc == 0 && S_ISREG(stat_buf.st_mode)) {
          // Exists and is a regular file
          drawClobberDialogWindow( );
          UserInputMode = UserInputMode_t::ClobberDialog;
        }
        else {
          s.myCprintf( 8, 14, scErr, "Can't overwrite that!" );
          SocketInputMode = SocketInputMode_t::Telnet;
          UserInputMode = UserInputMode_t::Help;
          s.doNotUpdateRealScreen( );
        }

      }
      else {

        if ( transferVars.statFileForUpload( ) ) {
          s.myCprintf( 8, 14, scErr, "File not found!" );
          SocketInputMode = SocketInputMode_t::Telnet;
          UserInputMode = UserInputMode_t::Help;
          s.doNotUpdateRealScreen( );
        }
        else {
          transferVars.startUpload( );
        }

      }


    }
    else {

      s.myCprintf( 8, 14, scFileXfer, "Bad filename!" );
      SocketInputMode = SocketInputMode_t::Telnet;
      UserInputMode = UserInputMode_t::Help;
      s.doNotUpdateRealScreen( );

    }


  }

  if ( key.specialKey != K_NormalKey ) return;


  char ch = key.normalKey;

  if ( ch == 27 ) {
    UserInputMode = UserInputMode_t::Telnet;
    s.paint( );
  }
  else if ( ch == 8 ) {

    if ( transferVars.filenameIndex ) {
      transferVars.filenameIndex--;
      s.putch( 18 + transferVars.filenameIndex, 13, scFileXfer, ' ' );
    }

  }
  else if ( isValidDosChar( ch ) || ch == '.' ) {

    if ( transferVars.filenameIndex < 12 ) {
      s.putch( 18 + transferVars.filenameIndex, 13, scFileXfer, ch );
      transferVars.filename[transferVars.filenameIndex] = ch;
      transferVars.filenameIndex++;
    }

  }

  gotoxy( 18 + transferVars.filenameIndex, 13 );


}




// ****************************************************************************

// Functions to start the various file transfers






// Transfer_vars::startDownload
//
// By the time you get here you know the target filename (Xmodem)
// or you are going to get it in a header packet (Ymodem).
// Initialize all other variables and send the first character to
// get the transfer started.
//
// Fixme - check to see if we are in binary mode.  And at the end
// you might need to go back to non-binary mode.

void Transfer_vars::startDownload( void ) {

  // Common to all protocols

  packetState = HeaderByte;
  retries = 0;
  telnetIACSeen = 0;
  bytesXferred = 0;
  packetsXferred = 0;

  // Defaults: Assume non batch protocols (Xmodem)

  waitingForHeader = 0;
  waitingForFirstPacket = 1;
  nextExpectedPacketNum = 1;

  expectedFilesize = 0;
  modificationDate = 0;

  targetFile = NULL;


  if ( fileProtocol == Ymodem || fileProtocol == Ymodem_G ) {
    waitingForHeader = 1;
    filename[0] = 0;
    nextExpectedPacketNum = 0;  // Fix me, probably not needed
  }

  setTelnetBinaryMode( true );

  transfer_SendByte( startDownloadBytes[fileProtocol] );


  SocketInputMode = SocketInputMode_t::Download;
  UserInputMode = UserInputMode_t::TransferInProgress;

  // Repaint the original screen
  s.paint( );

  // Might want to put up a status box
  drawFileStatusWindow( );

}



// Before you call here you already have the filename, modification time
// and file size from the filesystem.  Don't wipe them out.

void Transfer_vars::startUpload( void ) {

  if (fileProtocol == Ymodem || fileProtocol == Ymodem_G ) {
    packetState = SendHeader;
  }
  else {
    packetState = StartUpload;
  }


  retries = 0;
  telnetIACSeen = 0;
  bytesXferred = 0;
  packetsXferred = 0;
  waitingForHeader = 0;
  packetNum1 = 1;

  targetFile = NULL;

  canReceived = 0;


  SocketInputMode = SocketInputMode_t::Upload;
  UserInputMode = UserInputMode_t::TransferInProgress;

  s.paint( );

  drawFileStatusWindow( );
  updateFileMsg( scFileXfer, "Waiting for start" );

  setTelnetBinaryMode( true );
}



int8_t Transfer_vars::processGoodPayload( void ) {


  if ( targetFile == NULL ) {

    // Open on first touch
    targetFile = fopen( filename, "wb" );

    if ( targetFile == NULL ) {
      TRACE_WARN(( "Download: Error opening %s, %s\n", filename, strerror(errno) ));
      return -1;
    }

    if (ExtraFileBuffer) setvbuf( targetFile, ExtraFileBuffer, _IOFBF, EXTRA_FILE_BUFFER_SIZE );

    // If we know the filesize do an fseek out to the end to improve efficiency.
    // If the server is lying and says it is larger then we'll create too large of
    // a file.  That's a server error.
    //
    // Don't bother checking for errors ...  it's just a performance thing.
    //
    // Not actually going to do this.  It works but in the case of a partial
    // transfer it misleads people into thinking the full file was transferred.
    //
    // if ( expectedFilesize ) {
    //   fseek( targetFile, expectedFilesize-1, SEEK_SET );
    //   fwrite( ymodemPacket, 1, 1, targetFile );
    //   fseek( targetFile, 0, SEEK_SET );
    // }

  }




  // Send the ACK as soon as possible to get the next packet started.

  #ifdef YMODEM_G
  if ( fileProtocol != Ymodem_G ) transfer_SendByte( XMODEM_ACK );
  #else
  transfer_SendByte( XMODEM_ACK );
  #endif



  if ( packetNum1 == (nextExpectedPacketNum-1) ) {

    // Duplicate packet.  We wrote this already so don't do it again.
    TRACE(( "Download: Ignoring duplicate packet\n" ));

    packetState = HeaderByte;
    retries = 0;

    return 0;
  }



  // Write the data to the file.  I'm hoping that with setvbuf we can survive the
  // 128 byte writes without doing our own buffering.

  uint16_t bytesToWrite = expectedPayloadSize;

  if ( expectedFilesize ) {
    if ( bytesXferred + expectedPayloadSize > expectedFilesize ) bytesToWrite = expectedFilesize - bytesXferred;
  }

  if ( fwrite( ymodemPacket, 1, bytesToWrite, targetFile ) != bytesToWrite ) {
    return -2;
  }



  bytesXferred += expectedPayloadSize;
  packetsXferred++;
  packetState = HeaderByte;
  nextExpectedPacketNum++;
  retries = 0;

  TRACE(( "Download: Received packet %lu, Bytes: %u, Total bytes: %lu\n", packetsXferred, expectedPayloadSize, bytesXferred ));

  updateFileStatus( );
  updateFileMsg( scFileXfer, "Good packet received" );

  return 0;
}





// Parse bytes received on the socket.  If we don't consume all of the bytes
// return the number left over that need to be processed the next time around.
// We are responsible for sliding any remainder to the front of recvBuffer.

uint16_t processSocket_Upload( uint8_t *recvBuffer, uint16_t len ) {

  int16_t rc = transferVars.processSocket_UploadInternal( recvBuffer, len );

  if ( (rc > 0) && (rc < len) ) {
    memmove( recvBuffer, recvBuffer + rc, (len - rc) );
  }

  transferVars.bumpTimer( );

  TRACE(( "Upload: received %u bytes, consumed %u bytes\n", len, rc ));

  return (len-rc);
}




// Just return back the number of bytes consumed.  Our caller will handle
// sliding the receive buffer around.  We normally handle only one byte
// which might seem horrible, but on the upload path we probably only ever
// get one byte at a time.

uint16_t Transfer_vars::processSocket_UploadInternal( uint8_t *recvBuffer, uint16_t len ) {

  uint8_t ch = recvBuffer[0];

  TRACE(( "Upload: Char from remote: %x, State: %u  TelIAC: %d\n", ch, packetState, telnetIACSeen ));


  // If the user has forced telnet mode off then don't look for TEL_IAC.

  if ( RawOrTelnet == 1 ) {

    if ( telnetIACSeen == 0 ) { // First TEL_IAC

      if ( ch == TEL_IAC ) {
        telnetIACSeen = 1;
        return 1;
      }

    }
    else { // Second byte after a TEL_IAC.  Determine how to handle it.

      if ( ch == 242 ) { // Data Mark
        telnetIACSeen = 0;
        endTransfer( false, false, "Telnet DataMark - done?" );
        return 1;
      } 
      else {

        int16_t rc = processTelnetCmds( recvBuffer, len );

        if ( rc > 0 ) {
          telnetIACSeen = 0;
          return rc;
        }
        else {

          // Did not have enough bytes to process the telnet command.
          // Return until we get more bytes to process.

          return 0;
        }

      }

    }

  }



  // If we get one CAN character make a note of it.  If we get a second
  // one back to back abort the transfer.  Otherwise, assume it was a
  // glitch and carry on as we were.
  //
  // Do it here instead of using the packetState variable to keep it
  // self contained.

  if ( ch == XMODEM_CAN ) {

    if ( canReceived ) {
      endTransfer( true, false, "Cancelled by remote" );
      return len; // Eat all remaining chars
    }

    // Remember that we have seen a CAN character for the next loop around.
    canReceived = 1;
    return 1;
  }

  // Was not a CAN character - safe to proceed.
  canReceived = 0;



  // Have we exceeded our retry count?
  if ( retries > 3 ) {
    endTransfer( true, true, "Too many errors: aborting" );
    return len; // Eat all remaining chars
  }


  switch ( packetState ) {

    case StartUpload: {

      // StartUpload means start sending the file data.  If we were using Ymodem
      // we already sent the header and got an ACK for it.


      // If the local user specified checksum but the remote requested CRC upgrade.
      if ( fileProtocol == Xmodem && ch == 'C' ) {
        fileProtocol = Xmodem_CRC;
      }


      if ( (ch == XMODEM_NAK) || (ch == 'C') || (ch == 'G') ) {

        packetNum1 = 1;

        // Open the file

        targetFile = fopen( filename, "rb" );

        if ( targetFile == NULL ) {
          // We stated the file already so this should not happen.
          endTransfer( true, true, "File note found: aborting" );
          return len; // Eat all remaining chars
        }

        if (ExtraFileBuffer) setvbuf( targetFile, ExtraFileBuffer, _IOFBF, EXTRA_FILE_BUFFER_SIZE );

        sendXmodemPacket( );

        packetState = Uploading;
      }


      // Waiting to start but invalid char.  Just eat the char(s) ?
      // This might happen with Xmodem and variants, but not Ymodem because
      // we've already sent the header and received an ACK.
      break;
    }


    case SendHeader: {

      if ( ch == 'C' || ch == 'G' ) {
        sendHeader( );
      }

      // Waiting to start but invalid char.  Just eat the char(s) ?
      break;
    }


    case SentHeader: {

      if ( ch == XMODEM_ACK ) {
        packetState = StartUpload;
        retries = 0;
        packetNum1 = 1;
      }
      else {
        // NAK or any other unexpected character
        retries++;
        transmitPacket( );        
      }

      break;
    }

    case Uploading: {

      if ( ch == XMODEM_ACK ) {
        // All is good - advance to the next packet.
        packetNum1++;
        bytesXferred += expectedPayloadSize;
        packetsXferred++;
        retries = 0;

        // Form and send next packet
        sendXmodemPacket( );
      }
      else {
        // NAK or any other unexpected character - resend what we already have.
        retries++;
        transmitPacket( );        
      }

      break;
    }


    case EOT: {

      if ( ch == XMODEM_ACK ) {

        if ( fileProtocol == Ymodem || fileProtocol == Ymodem_G ) {
          packetState = SendNullHeader;
        }
        else {
          // Update file Status here one more time to show a good completion
          updateFileStatus( );
          endTransfer( false, false, "Upload completed" );
        }
      }
      else {
        // NAK or any other unexpected character
        retries++;
        transfer_SendByte( XMODEM_EOT );
      }

      break;
    }

    case SendNullHeader: {

      if ( ch == 'C' || ch == 'G' ) {
        sendNullHeader( );
      }

      // Do we care if we get bad data?  For now the user can abort manually.


      #ifdef YMODEM_G
      if ( fileProtocol == Ymodem_G ) {
        // No need to wait for the return ACK.
        endTransfer( false, false, "Upload completed" );
      }
      #endif

      break;
    }


    case SentNullHeader: {

      if ( ch == XMODEM_ACK ) {
        endTransfer( false, false, "Upload completed" );
      }
      else {
        // NAK or any other unexpected character
        retries++;
        transmitPacket( );        
      }

      break;
    }

  } // end switch

  return 1;
}


#ifdef YMODEM_G
void Transfer_vars::sendForYmodemG( void ) {

  if ( packetState == Uploading ) {

    if ( mySocket->outgoing.hasRoom( ) ) {

      packetNum1++;
      bytesXferred += expectedPayloadSize;
      packetsXferred++;

      // Form and send next packet
      sendXmodemPacket( );
    }

  }

}
#endif



void Transfer_vars::sendHeader( void ) {

  ymodemPacket[0] = XMODEM_SOH;
  ymodemPacket[1] = 0;
  ymodemPacket[2] = 255;

  // Copy filename, size, etc.

  int index = 3;
  int index2 = 0;
  while ( 1 ) {
    ymodemPacket[index] = filename[index2];
    index++;
    index2++;
    if ( filename[index2] == 0 ) break;
  }

  ymodemPacket[index++] = 0;

  time_t correctedDate = modificationDate;

  index += sprintf( (char *)(ymodemPacket +index), "%lu %lo", expectedFilesize, correctedDate ) + 1;

  // From this point forward to char 128 we need to initialize to nulls.
  while ( index < 128 ) ymodemPacket[index++] = 0;

  // Need a CRC on this
  uint16_t crc = xmodem_calcCRC( ymodemPacket+3, 128 );
  ymodemPacket[131] = crc >> 8;
  ymodemPacket[132] = crc & 0xFF;

  // Push it out and wait for a response

  resendPacketSize = 133;
  transmitPacket( );

  // Will move to Uploading after an ACK is received
  packetState = SentHeader;

}



int8_t Transfer_vars::statFileForUpload( void ) {

  struct stat stat_buf;
  int rc = stat( filename, &stat_buf );

  if ( rc == -1 || !S_ISREG(stat_buf.st_mode)) {
    return -1;
  }

  expectedFilesize = stat_buf.st_size;
  modificationDate = stat_buf.st_mtime;
  
  return 0;
}






void Transfer_vars::sendXmodemPacket( void ) {

  // If we have sent everything then send an EOT char and move to the
  // next state.  We can close the file - we know they got the last
  // data packet, and even if they had not we would have the last
  // packet in the ymodemPacket buffer.

  if ( bytesXferred >= expectedFilesize ) {

    transfer_SendByte( XMODEM_EOT );
    packetState = EOT;

    fclose( targetFile );
    targetFile = NULL;

    return;
  }


  // Default packet size is 128 bytes.  If using a 1K variant and there is more
  // than 1KB of data to go then change to a 1KB packet size.  (Assuming the
  // maxEnqueue size for the socket supports it.  We require a little extra
  // room for expanding TEL_IAC.)

  expectedPayloadSize = 128;
  ymodemPacket[0] = XMODEM_SOH;

  TRACE(( "Expected: %lu   Xferred: %lu\n", expectedFilesize, bytesXferred ));

  if ( fileProtocol == Xmodem_1K || fileProtocol == Ymodem || fileProtocol == Ymodem_G ) {
    if ( ((expectedFilesize - bytesXferred) > 1024) && (mySocket->maxEnqueueSize > 1250) ) {
      expectedPayloadSize = 1024;
      ymodemPacket[0] = XMODEM_STX;
    }
  }

  ymodemPacket[1] = packetNum1;
  ymodemPacket[2] = 255 - packetNum1;



  // Read from the file.  If we don't get all of the data that we expected we
  // need to pad the packet out with CPM EOF (^Z).

  uint16_t readSize = expectedPayloadSize;
  if ( bytesXferred + expectedPayloadSize > expectedFilesize ) {
    readSize = expectedFilesize - bytesXferred;
  }

  int bytesRead = fread( ymodemPacket + 3, 1, readSize, targetFile );
  TRACE(( "Upload: Reading %d bytes, Actual read: %d\n", readSize, bytesRead ));

  if ( readSize < expectedPayloadSize ) {
    for ( int i=readSize; i < expectedPayloadSize; i++ ) ymodemPacket[i+3] = 26;
  }




  // Compute a checksum or CRC?

  if ( fileProtocol == Xmodem ) {

    ymodemPacket[131] = xmodem_calcChecksum( ymodemPacket + 3, expectedPayloadSize );

    // Remember the length in case we have to retransmit.
    resendPacketSize = 132;
    transmitPacket( );

  }
  else {

    uint16_t crc = xmodem_calcCRC( ymodemPacket+3, expectedPayloadSize );
    ymodemPacket[3 + expectedPayloadSize] = crc >> 8;
    ymodemPacket[4 + expectedPayloadSize] = crc & 0xFF;

    // Remember the length in case we have to retransmit.
    resendPacketSize =  expectedPayloadSize + 5;
    transmitPacket( );

  }

  updateFileStatus( );
  updateFileMsg( scFileXfer, "Sent a packet" );
}



// Inline because it is only called from one place

inline void Transfer_vars::sendNullHeader( ) {

  TRACE(( "Upload: Sending null hdr\n" ));

  // This actually wipes out one byte extra, but we have a few hundred extra
  // in the buffer.  It costs a few more instructions to setup but it runs
  // much faster than a loop because it uses rep stosw.

  fillUsingWord( (uint16_t *)ymodemPacket, 0, 134>>1 );


  // Our headers always have a 128 byte payload.

  ymodemPacket[0] = XMODEM_SOH;
  ymodemPacket[2] = 0xFF;


  packetState = SentNullHeader;

  resendPacketSize = 133;
  transmitPacket( );

}
  



// Transfer_vars::transmitPacket
//
// Send out the contents of an xmodem packet.  The packet is fully formed before we get here.
// In the event of a retransmit this can be called directly instead of reforming the packet
// from scratch.
//
// It is possible that our packet size will double - a goofy packet of all FFs would cause
// that.  Our caller protects us slightly by only using a 1KB packe size if the MTU size
// is at least 20% bigger than 1KB.  So at worst case we'll have to send two packets instead
// of one.  (For 128 byte packets we should fit within any reasonable MTU.)

// Fixme: add a return code for this function if we can't get a buffer

void Transfer_vars::transmitPacket( ) {

  TRACE(( "Upload: Transmit: PacketNum: %u  Len: %u\n", ymodemPacket[1], resendPacketSize ));


  // Index into the packet
  uint16_t bytesSent = 0;

  while ( bytesSent < resendPacketSize  ) {

    DataBuf *buf;

    while ( (buf = (DataBuf *)TcpBuffer::getXmitBuf( )) == NULL ) {

      // This is bad - we are out of send buffers.  This normally should not happen
      // with a protocol like Xmodem or Ymodem, but Ymodem G can cause this because
      // it does not wait for acknowledgements.

      // Keep processing packets until we get one of our outgoing buffers freed up.

      PACKET_PROCESS_SINGLE;
      Arp::driveArp( );
      Tcp::drivePackets( );

      if ( mySocket->isRemoteClosed( ) ) {
        // Whoops - we are done
        break;
      }

    }

    // Expand any 0xFF (TEL_IAC) to a second one for Telnet binary processing.

    uint16_t bufIndex = 0;

    while ( (bytesSent < resendPacketSize) && (bufIndex+1 < mySocket->maxEnqueueSize) ) {
      buf->data[bufIndex++] = ymodemPacket[bytesSent];
      if ( (RawOrTelnet == 1) && (ymodemPacket[bytesSent] == TEL_IAC) ) buf->data[bufIndex++] = TEL_IAC;
      bytesSent++;
    }

    buf->b.dataLen = bufIndex;

    if ( mySocket->enqueue( &buf->b ) ) {

      // Once again, this should never happen - it indicates that the outgoing
      // queue is full or we exceeded MTU size.  In the case of a full outgoing
      // queue we can silently drop this packet and wait for the other side
      // to complain.

      TRACE_WARN(( "Upload: enqueue failed on packet size %u\n", bufIndex ));
      TcpBuffer::returnXmitBuf( (TcpBuffer *)buf );

    }

  }


  bumpTimer( );

}








void Transfer_vars::startNextYmodemFile( void ) {

  drawFileStatusWindow( );

  #ifdef YMODEM_G

  if ( fileProtocol == Ymodem_G ) {
    transfer_SendByte( 'G' );
  }
  else {
    transfer_SendByte( 'C' );
  }

  #else

  transfer_SendByte( 'C' );

  #endif

  nextExpectedPacketNum = 1;
  retries = 0;
  updateFileStatus( );
  packetState = HeaderByte;

}





// Parse bytes received on the socket.  If we don't consume all of the bytes
// return the number left over that need to be processed the next time around.
// We are responsible for sliding any remainder to the front of recvBuffer.

uint16_t processSocket_Download( uint8_t *recvBuffer, uint16_t len ) {

  int16_t rc = transferVars.processSocket_DownloadInternal( recvBuffer, len );

  if ( (rc > 0) && (rc < len) ) {
    memmove( recvBuffer, recvBuffer + rc, (len - rc) );
  }

  transferVars.bumpTimer( );

  TRACE(( "Download: received %u bytes, consumed %u bytes\n", len, rc ));

  return (len-rc);
}






// We never know if we have enough data so use a state machine to keep track
// of our progress.  This gives us enough state so that we can call this
// once new data is received and pick up where we left off.
//
// Return the number of consumed bytes to the caller.  It is presumed that
// the caller will remove that many bytes from their buffer to make room
// for new bytes so copy and preserve what you need first.
//
// In the case of a protocol error or CRC error there are a few possible
// things to do.  A CRC error should not happen because TCP/IP is protecting
// us, but if it does we should ask for the packet again.  There should be
// nothing in the TCP buffers but clear them just in case.  A more severe
// error might be terms to kill the download immediately.


uint16_t Transfer_vars::processSocket_DownloadInternal( uint8_t *recvBuffer, uint16_t len ) {

  // Index tracks our progress through the buffer.  Normally we expect to read
  // every byte.  The only real exception is if we start processing a telnet
  // command and there are not enough bytes present.

  uint16_t index = 0;


  // If the event of a protocol error set this to one and break out of the
  // switch.  Code at the end of the loop will decide if we should retry
  // or abort

  uint8_t packetError = 0;


  while ( index < len ) {


    // We are running over TELNET so we need to be looking for TEL_IAC.
    // The first time we see a TEL_IAC make a note of it and skip over it.
    // On the second time seeing it decide to treat it as a 0xFF character
    // or to process a TELNET command.

    // The payload state has a small loop to copy the payload data
    // more efficiently than going through this loop for each byte.  In
    // the even that we see a second TEL_IAC that should be treated as a
    // 0xFF and we are in the data phase do not do anything; let the data
    // loop process the 0xFF and flip the state back to telnetIACSeen zero.


    if ( RawOrTelnet == 1 ) {

      if ( telnetIACSeen == 0 ) { // First TEL_IAC

        if ( recvBuffer[index] == TEL_IAC ) {
          telnetIACSeen = 1;
          index++;
          continue; // Do not process this char
        }

      }
      else { // Second byte after a TEL_IAC.  Determine how to handle it.

        if ( recvBuffer[index] == TEL_IAC ) {

          // Two TEL_IACs in a row.  Treat it as a single 0xFF.

          if ( packetState != Data ) {
            // If Data state then it needs to know it can read this one.
            telnetIACSeen = 0;
          }

        }
        else {

          int16_t rc = processTelnetCmds( recvBuffer+index, (len-index) );

          if ( rc > 0 ) {
            index = index + rc;
            telnetIACSeen = 0;
            continue;
          }
          else {

            // Did not have enough bytes to process the telnet command.
            // Leave us in this state until we get more bytes so we can
            // try again.

            // Our caller will slide the buffer down, making room for more
            // bytes.  The initial TEL_IAC will be gone but we will remember
            // it.

            break;
          }

        }

      }

    }




    switch ( packetState ) {

      case HeaderByte: {

        uint8_t headerByte = recvBuffer[index++];

        if ( headerByte == XMODEM_SOH ) {

          TRACE(( "Ymodem: SOH (128 byte packet)\n" ));
          packetState = PacketNum1;
          expectedPayloadSize = 128;

        }
        else if ( headerByte == XMODEM_STX ) {

          TRACE(( "Ymodem: STX (1024 byte packet)\n" ));
          packetState = PacketNum1;
          expectedPayloadSize = 1024;

        }
        else if ( headerByte == XMODEM_EOT ) {

          // Some xmodem implementations send a NAK on the first occurrence of
          // and EOT and make the sender send it again.  This is a safety
          // technique for bad modem connections.  We're protected by TCP/IP
          // so we're not going to do that here.

          TRACE(( "Transfer: EOT received, sending ACK\n" ));

          transfer_SendByte( XMODEM_ACK );


          // Close the file

          fclose( targetFile );
          targetFile = NULL;

          if ( modificationDate ) {

            struct utimbuf myTimes;
            myTimes.actime = modificationDate;
            myTimes.modtime = modificationDate;

            if ( utime( filename, &myTimes ) ) {
              TRACE_WARN(( "Download: failed to set modification time for %s\n", filename ));
            }

          }


          if ( (fileProtocol == Xmodem) ||
               (fileProtocol == Xmodem_CRC) ||
               (fileProtocol == Xmodem_1K) )
          {

            TRACE(( "Download: Xmodem transfer done\n" ));

            // Send an ACK and we are done.

            endTransfer( false, false, "Xmodem download done" );

            // If we don't return early we might keep processing bytes and sending
            // NAKS to things we don't understand.
            return 1;

          }
          else {
            TRACE(( "Transfer: Ask for next ymodem batch header\n" ));
            startDownload( );
          }

        }
        else {

          TRACE(( "Transfer: Unexpected header byte: %02x\n", headerByte ));
          packetError = 1;

        }

        break;
      }


      case PacketNum1: {
        packetState = PacketNum2;
        packetNum1 = recvBuffer[index++];
        break;
      }


      case PacketNum2: {

        // Basic sanity check
        if ( packetNum1 != (255 - recvBuffer[index++]) ) {
          packetError = 1;
          break;
        }


        // Does this match our expected packet number?
        // We expect it to match but we can tolerate being off by one.  In
        // theory that means that they lost our previous ACK, but over Telnet
        // that should never happen.

        if ( (packetNum1 != nextExpectedPacketNum) && (packetNum1 != (nextExpectedPacketNum-1)) ) {
          TRACE(( "Ymodem: Unexpected packet number: %u, should be %u\n", packetNum1, nextExpectedPacketNum ));
          packetError = 1;
          break;
        }

        TRACE(( "Ymodem: Packet number: %u\n", packetNum1 ));

        // At this point we can read the payload
        packetState = Data;
        payloadBytesRead = 0;

        break;
      }


      case Data: {

        // Copy the expected number of bytes into the local buffer.

        uint8_t earlyExit = 0;

        while ( !earlyExit && (index < len) && (payloadBytesRead < expectedPayloadSize) ) {

          if ( (RawOrTelnet == 1 ) && (recvBuffer[index] == TEL_IAC) && (telnetIACSeen == 0) ) {

            // This is the first TEL_IAC we have bumped into.  Break out of this case statement
            // without incrementing, and let the code at the top of the while loop that handles
            // telnet options deal with it.
            //
            // If this is the second TEL_IAC then we just treat it as a data byte.
            //
            // For a TEL_IAC followed by a TEL_IAC that would be heavy and we can do better,
            // but there might be telnet options present.  Optimize the code later.

            earlyExit = 1;
            break;
          }

          telnetIACSeen = 0;
          ymodemPacket[payloadBytesRead++] = recvBuffer[index++];
        }

        if ( earlyExit ) break;


        TRACE(( "Ymodem: Copied %d bytes of payload\n", payloadBytesRead ));

        if ( payloadBytesRead == expectedPayloadSize ) {

          if ( fileProtocol == Xmodem ) {
            packetState = Checksum;
          }
          else {
            packetState = CRC1;
          }

        }

        break;

      }



      case Checksum: {

        uint8_t checksumByte = recvBuffer[index++];
        uint8_t myChecksum = xmodem_calcChecksum( ymodemPacket, expectedPayloadSize );

        if ( myChecksum != checksumByte ) {
          TRACE(( "Xmodem: bad checksum, theirs: %02x  mine: %02x\n", checksumByte, myChecksum ));
          updateFileMsg( scErr, "Checksum error" );
          packetError = 1;
        }
        else {
          if ( processGoodPayload( ) ) {
            // Something wrong with the file write needs to abort the entire transfer
            endTransfer( true, true, "Filesystem error - aborting" );

            return len;
          }
        }

        break;
      }



      case CRC1: {
        crc1 = recvBuffer[index++];
        packetState = CRC2;
        break;
      }

      case CRC2: {

        uint8_t crc2 = recvBuffer[index++];

        uint16_t myCRC = xmodem_calcCRC( ymodemPacket, expectedPayloadSize );
        uint16_t theirCRC = crc1 << 8 | crc2;

        if ( myCRC != theirCRC ) {
          TRACE(( "Download: bad CRC, theirs: %04x  mine: %04x\n", theirCRC, myCRC ));
          updateFileMsg( scErr, "CRC error" );
          packetError = 1;
          break;
        }

        TRACE(( "Download: CRC good\n" ));

        if ( waitingForHeader && packetNum1 == 0 ) {

          // They sent a header.  If we were waiting for a header and they sent
          // something else, it would have been caught by the next expected
          // packet num check earlier.

          waitingForHeader = 0;

          #ifdef YMODEM_G
          if ( fileProtocol != Ymodem_G ) transfer_SendByte( XMODEM_ACK );
          #else
          transfer_SendByte( XMODEM_ACK );
          #endif

          ParseHeaderRc_t headerRc = parseYmodemHeader( );

          switch ( parseYmodemHeader( ) ) {

            case RequestNext:
              startNextYmodemFile( );
              break;

            case NoMoreFiles:
              endTransfer( false, false, "No more files" );
              break;

            case BadFilename:
              endTransfer( true, true, "Bad filename format" );
              break;

            case PromptClobber:
              drawClobberDialogWindow( );
              UserInputMode = UserInputMode_t::ClobberDialogDownloading;
              break;

            case CantClobber:
              endTransfer( true, true, "Can't create filename" );
              break;

          }

        }
        else {
          if ( processGoodPayload( ) ) {
            // Something wrong with the file write needs to abort the entire transfer
            endTransfer( true, true, "Filesystem error - aborting" );
            return len;
          }
        }

        break;
      }


    } // end switch state



    if ( packetError ) {

      TRACE(( "Download: Protocol error, PacketState: %u, Retries: %u\n",
              packetState, retries ));

      // Flush the buffer
      recvFlush( 1000 );

      retries++;

      if ( retries == 3 ) {
        endTransfer( true, true, "Too many errors: aborting" );
      }
      else {
        updateFileMsg( scErr, "Retry" );

        // Fixme: check return code
        transfer_SendByte( XMODEM_NAK );

        // Reset state to start reading a header byte
        packetState = HeaderByte;
      }

      // Return len instead of index to indicate that we ate everything.
      return len;

    }


  } // end while bytes


  return index;
}


#endif
