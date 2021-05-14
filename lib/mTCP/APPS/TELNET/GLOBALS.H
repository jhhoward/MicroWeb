/*

   mTCP Globals.h
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


   Description: Data structures shared between the telnet and ymodem
                modules.

   Changes:

   2012-04-29: Created when xmodem and ymodem were added

*/


#ifndef _GLOBALS_H
#define _GLOBALS_H 


#include "telnetsc.h"

// Definitions of enums and global vars.  All of these live in TELNET.CPP
// but might be used by file transfer.



// Socket handling mode

class SocketInputMode_t {
  public: enum e {
    Telnet,
    Download,
    Upload
  };
};

extern SocketInputMode_t::e SocketInputMode;



// User input mode.  Normally Telnet unless we are doing something special.

class UserInputMode_t {
  public: enum e {
    Telnet,
    Help,
    ProtocolSelect_Download,
    ProtocolSelect_Upload,
    FilenameSelect_Download,
    FilenameSelect_Upload,
    ClobberDialog,
    ClobberDialogDownloading,
    TransferInProgress,
  };
};

extern UserInputMode_t::e UserInputMode;



// Definition of a buffer used for outgoing data.  The real MTU size might
// be smaller than what is assumed here; the caller is responsible for
// using this correctly.  For just a few bytes this is always safe.

typedef struct {
  TcpBuffer b;
  uint8_t data[1460];
} DataBuf;




// The socket and screen

extern TcpSocket *mySocket;
extern Screen s;
extern uint8_t scFileXfer;
extern uint8_t scErr;
extern uint8_t scNormal;
extern uint8_t scBright;



extern int16_t processTelnetCmds( uint8_t *cmdStr, uint8_t cmdSize );
extern void setTelnetBinaryMode( bool binaryMode );

extern bool RawOrTelnet;


#endif
