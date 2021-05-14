
/*

   mTCP FtpCl.h
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


   Description: Ftp server connected client data structures

   Changes:

   2011-05-27: Initial release as open source software

*/




#ifndef _FTP_CLIENT_H
#define _FTP_CLIENT_H


#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "tcp.h"
#include "tcpsockm.h"


#include "ftpusr.h"


class FtpClient {

  public:

    enum FtpClientState { Closed = 0, UserPrompt, PasswordPrompt, CommandLine, RnfrSent, ClosingPushOutput, Closing };
    enum DataXferState  { DL_NotActive = 0, DL_Init, DL_Connecting, DL_Connected, DL_Active, DL_Closing };
    enum DataXferType   { NoDataXfer = 0, List, Nlist, Retr, Stor, StorA, StorU };

    char             eyeCatcher[16];

    FtpClientState   state;
    uint32_t         sessionId;

    time_t           startTime;

    TcpSocket       *cs;
    TcpSocket       *ds;
    TcpSocket       *ls;

    FtpUser          user;

    uint8_t          loginAttempts;
    uint8_t          isLocalSubnet;

    char             ftproot[USR_MAX_PATH_LENGTH];  // Optional; may be null for this user
    char             cwd[USR_MAX_PATH_LENGTH];
    char             padding3;

    IpAddr_t         dataTarget;
    uint16_t         dataPort;

    IpAddr_t         pasvAddr;

    uint16_t         pasvPort;

    char             inputBuffer[INPUTBUFFER_SIZE];
    uint16_t         inputBufferIndex;
    uint8_t          eatUntilNextCrLf;
    uint8_t          padding;

    char            *outputBuffer;
    uint16_t         outputBufferLen;
    uint16_t         outputBufferIndex;

    uint8_t          *fileBuffer;


    DataXferState    dataXferState;
    DataXferType     dataXferType;
    uint8_t          asciiMode;

    uint8_t          statCmdActive;

    time_t           connectStarted;
    uint8_t          activeConnect;

    uint8_t          noMoreData;
    uint16_t         bytesSent;
    uint16_t         fileBufferIndex;
    uint16_t         bytesRead;
    uint16_t         bytesToRead;

    char             filespec[DOS_MAX_PATHFILE_LENGTH];

    struct find_t    fileinfo;
    uint8_t          padding2;

    FILE            *file;



    void startNewSession( TcpSocket *newSocket, uint32_t newSessionId );
    void cleanupSession( void );

    int pendingOutput( void ) { return outputBufferLen!=0; }
    void addToOutput( char *str );
    void addToOutput_var( char *fmt, ... );
    void sendOutput( void );
    void clearOutput( void ) { outputBufferIndex = outputBufferLen = 0; }


    // Class variables and methods

    static FtpClient *activeClientsTable[ FTP_MAX_CLIENTS ];
    static FtpClient *freeClientsTable[ FTP_MAX_CLIENTS ];

    static uint16_t activeClients;
    static uint16_t freeClients;
    static uint16_t allocatedClients;


    static int        initClients( uint16_t clients_p );

    static FtpClient *getFreeClient( void );
    static void       returnFreeClient( FtpClient *client );
    static void       addToActiveList(  FtpClient *client );
    static void       removeFromActiveList( FtpClient *client );


    #ifdef CCC
    static void checkClients( void );
    #endif

};





#endif
