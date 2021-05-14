/*

   mTCP Ymodem.h
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


   Description: Data structures and function prototypes for xmodem/ymodem
                support.

   Changes:

   2012-04-29: Created when xmodem and ymodem were added

*/


#ifdef FILEXFER


#ifndef _YMODEM_H
#define _YMODEM_H


#include <time.h>

#include "types.h"
#include "timer.h"
#include "keys.h"


#define XMODEM_SOH ( 0x01 )
#define XMODEM_STX ( 0x02 )

#define XMODEM_ACK ( 0x06 )
#define XMODEM_NAK ( 0x15 )
#define XMODEM_EOT ( 0x04 )
#define XMODEM_CAN ( 0x18 )





// Function prototypes

void initForXmodem( void );


// Process user input specific to file transfers

void processUserInput_FileProtocol( Key_t key );
void processUserInput_Filename( Key_t key );
void processUserInput_ClobberDialog( Key_t key );
void processUserInput_Transferring( Key_t key );


// Drawing on the screen specific to file transfers

void drawProtocolMenu( void );
void drawFileStatusWindow( void );
void drawFilenameWindow( void );

void updateFileStatus( void );
void updateFileMsg( char *text );


// File transfer functions

uint16_t processSocket_Download( uint8_t *recvBuffer, uint16_t len );
uint16_t processSocket_Upload( uint8_t *recvBuffer, uint16_t len );






class Transfer_vars {

  public:

    enum FileProtocol_t { Xmodem, Xmodem_CRC, Xmodem_1K, Ymodem, Ymodem_G };
    enum PacketState_t  { HeaderByte, PacketNum1, PacketNum2, Data, Checksum, CRC1, CRC2, EOT,
                          StartUpload, SendHeader, SentHeader, Uploading, SendNullHeader, SentNullHeader };

    static const char startDownloadBytes[];
    static const char *protocolNames[];

    enum ParseHeaderRc_t { RequestNext, NoMoreFiles, BadFilename, PromptClobber, CantClobber };



    void startDownload( void );
    void startUpload( void );

    uint16_t processSocket_DownloadInternal( uint8_t *recvBuffer, uint16_t len );
    uint16_t processSocket_UploadInternal( uint8_t *recvBuffer, uint16_t len );

    void checkForDownloadTimeout( void );

    ParseHeaderRc_t parseYmodemHeader( void );

    int8_t processGoodPayload( void );

    void sendHeader( void );
    void sendNullHeader( void );
    void sendXmodemPacket( void );
    void transmitPacket( void );

    void startNextYmodemFile( void );

    int8_t statFileForUpload( void );


    #ifdef YMODEM_G
    void sendForYmodemG( void );
    #endif

    inline clockTicks_t getLastActivity( void ) { return lastActivity; }
    inline void bumpTimer( void ) { lastActivity = TIMER_GET_CURRENT( ); }



    FileProtocol_t fileProtocol;
    PacketState_t  packetState;

    uint8_t  waitingForHeader;
    uint8_t  waitingForFirstPacket;
    uint8_t  nextExpectedPacketNum;
    uint8_t  packetNum1;

    uint16_t expectedPayloadSize;  // Also used on uploading
    uint16_t payloadBytesRead;
    uint8_t  crc1;
    uint8_t  ymodemPacket[1024+5]; // Download: payload data; Upload: entire packet
    uint8_t  padding;
    uint8_t  retries;
    uint8_t  telnetIACSeen;
    uint8_t  canReceived;

    uint32_t bytesXferred;
    uint32_t packetsXferred;


    // File info

    char    filename[13];       // Filename
    uint8_t filenameIndex;      // Used when getting a filename from the user
    uint32_t expectedFilesize;  // Download: received from server  Upload: local filesize
    time_t   modificationDate;  // Greenwich meantime


    uint16_t resendPacketSize;

    clockTicks_t lastActivity;

    FILE *targetFile;

};



// Globals for Ymodem download

extern Transfer_vars transferVars;

extern char *ExtraFileBuffer;



#endif

#endif
