/*

   mTCP Ftp.cpp
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


   Description: Your typical run-of-the-mill DOS ftp client ...

   Changes:

   2011-05-27: Initial release as open source software
   2011-10-01: Fix minor display bug while editing cmdline;
               make PASV mode the default for file transfers
   2012-05-10: Rewrite send file loop to improve performance.
   2012-11-25: Fix 125/150 response code handling/race conditions
   2013-03-18: Increase user input area to two lines
   2013-03-28: Add limited packet handling during user input;
               Add support for screens larger than 80x25
   2015-01-18: Minor change to Ctrl-Break and Ctrl-C handling.
   2015-02-27: Add mdelete command, redo help text.
   2015-02-28: Collapse send and recieve file buffers into one buffer;
               Remove restrictions on local file length to allow for
               paths and filenames.  DOS will protect us against
               bad paths and names.
   2015-05-29: Update PASV parsing to ignore third party servers.

*/


#include <bios.h>
#include <conio.h>
#include <ctype.h>
#include <direct.h>
#include <dos.h>
#include <io.h>
#include <malloc.h>
#include <mem.h>
#include <stdarg.h>
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
#include "udp.h"
#include "dns.h"
#include "tcp.h"
#include "tcpsockm.h"



#define _NL_ "\r\n"



#define CONTROL_RECV_SIZE     (512) // Control socket recv buffer
#define INBUFSIZE             (512) // Command line buffer
#define MLIST_BUF_SIZE       (4096) // Buffer for MGET/MPUT filename list
#define TCP_RECV_SIZE        (8192) // Default Data socket recv buffer
#define FILE_BUF_SIZE        (8192) // Default file buffer size

#define FTPSERVERNAME_MAX_LEN  (64) // Max len of the ftp server name
#define USERINPUTBUF_MAX_LEN  (140) // Max len of input line a user can type
#define COMMAND_MAX_LEN        (20) // Max len of a user command
#define FILESPEC_MAX_LEN       (80) // Max len of a filespec
#define USERNAME_MAX_LEN       (64) // Max len of a username
#define PASSWORD_MAX_LEN       (40) // Max len of a password

#define SERVER_RESP_MAX_LEN   (160) // Max line len coming back from server


enum ClientState_t {

  Uninitialized = 0,
  ServerConnected,    // Got a response back from the server
  SentUser,           // Send username
  UserOkSendPass,     // Got a response back after sending username
  SentPass,           // Sent password

  CmdLine,            // Made it to command line

  BinStuffed,         // Stuff a BIN command in.  (Optional)

  ListSentPasv,       // Sent PASV, will send LIST next
  ListSentAfterPasv,  // Sent LIST after sending PASV   - we will connect
  ListSentActive,     // Sent LIST while in active mode - they will connect
  ListSentPort,       // Sent PORT, will send LIST next
  ListSentAfterPort,  // Sent LIST after sending PORT   - they will connect

  NListSentPasv,      // Sent PASV, will send NLST next
  NListSentAfterPasv, // Sent NLST after sending PASV   - we will connect
  NListSentActive,    // Sent NLST while in active mode - they will connect
  NListSentPort,      // Sent PORT, will sent NLST next
  NListSentAfterPort, // Sent NLST after sending PORT   - they will connect

  RetrSentPasv,       // Sent PASV, will send RETR next
  RetrSentAfterPasv,  // Sent RETR after sending PASV   - we will connect
  RetrSentActive,     // Sent RETR while in active mode - they will connect
  RetrSentPort,       // Sent PORT, will send RETR next
  RetrSentAfterPort,  // Sent RETR after sending PORT   - they will connect

  StorSentPasv,       // Sent PASV, will send STOR next
  StorSentAfterPasv,  // Sent STOR after sending PASV   - we will connect
  StorSentActive,     // Sent STOR while in active mode - they will connect
  StorSentPort,       // Sent PORT, will send STOR next
  StorSentAfterPort,  // Sent STOR after sending PORT   - they will connect

  RenameFromSent,     // Sent first part of rename command (RNFR)
  RenameToSent,       // Sent second part of rename command (RNTO)

  CmdSent,            // Sent a generic command
  Closing             // The server told us go away - all done folks
};


ClientState_t ClientState = Uninitialized;

bool MultilineResponse = false;




// Sockets and helper functions

TcpSocket *ControlSocket;  // Used to send commands/status
TcpSocket *DataSocket;     // Used for data transfer
TcpSocket *ListenSocket;   // Listening socket for when they connect to us

int8_t listenForDataSocket( void ); // Create listener for passive data socket
int8_t waitForDataSocket( void );   // Do passive connect for data socket
int8_t connectDataSocket( void );   // Do active connect for data socket
void   closeDataSockets( void );    // Force close on listen and data sockets



// IP addresses and ports

char     FtpServer[ FTPSERVERNAME_MAX_LEN ];  // FTP server text name
IpAddr_t FtpServerAddr;                       // Resolved IP address

uint16_t ControlPort;        // Our local port for the control connection.
uint16_t DataPort;           // Port for data socket connections from server.
uint16_t FtpServerPort = 21; // Default FTP server control port.

uint16_t NextDataPort;       // Used on PORT commands

IpAddr_t PasvAddr;           // IP addr parsed from PASV response
uint16_t PasvPort;           // Port to use parsed from PASV response

uint32_t ConnectTimeout = 10000ul;



// Data structure used to send packets.  1460 is the maximum payload for
// a normal TCP/IP packet with no options.  The sender has to remember that
// the other side might have an MSS less than this, or that the local MTU
// might be smaller than 1500.

typedef struct {
  TcpBuffer b;
  uint8_t data[1460];
} DataBuf;




enum TransferModes_t {
  Classic,                   // Original - not firewall friendly
  Passive,                   // Best for firewalls - we do active connect
  PortFirst                  // Specify a port before each transfer
};

TransferModes_t TransferMode = Passive;

char *TransferModeStrings[] = { "Classic", "Passive", "Port" };


bool StuffBinCommandAtStart = true;



char ServerFile[FILESPEC_MAX_LEN];    // Also used as Rename 'from' file
char LocalFile[FILESPEC_MAX_LEN];     // Name to use locally
char RenameToParm[FILESPEC_MAX_LEN];  // Used only as Rename 'to' parm



uint16_t TcpRecvSize = TCP_RECV_SIZE;   // TCP socket receive buffer size
uint16_t FileBufSize  = FILE_BUF_SIZE;  // File transfer buffer size

uint8_t *FileBuffer;



// mList buffer for mput, mget and mdelete

uint16_t mListBufSize = MLIST_BUF_SIZE; // Can override with env var

char    *mList = NULL;       // Pointer to allocated buffer
char    *mListIndex = NULL;  // Pointer that shows how much buffer was used

bool ReadingForMget = false;  // Toggle for use in receiveFileList
bool MgetMputPrompt = true;   // Toggle for prompting on mget/mput



uint8_t ScreenPager = 0;


void parseArgs( int argc, char *argv[] );
void readConfigParms( void );

int8_t resolveServer( char *name, IpAddr_t addr );
void   shutdown( int rc );



// Control socket handling
//
// pollSocket scans the control socket for input and adds to inBuf
// processSocketInput handles a line of input scanned from inBuf
//
void pollSocket( TcpSocket *s, uint32_t timeout );
void processSocketInput( void );


// getLineFromInBuf scans inBuf for a full line to return
//
// Returns: 0 if nothing retreived,
//          1 if a line retrieved,
//         -1 overrun error
//
int8_t getLineFromInBuf( char *target, int16_t targetLen );

uint8_t  *inBuf;                 // Input buffer for Control socket
uint16_t  inBufIndex=0;          // Index to next char to fill
uint16_t  inBufSearchIndex=0;    // Where to continue searching for \r\n



bool isValidDosFilename( const char *filename );

char    *current_directory(char *path);


// Stdin and Stdout helpers

bool IsStdinFile = false;
bool IsStdoutFile = false;
void probeStdinStdout( void );

int readStdin( char *buffer, int bufLen );
int readConsole( char *buffer, int bufLen, bool enableCmdEdit );




void processUserInput( char *buffer );
void processUserInput2( char *lineBuffer );
void processSimpleUserCmd( char *serverCmd, char *nextTokenPtr );

void processCmd_User( char *username );
void processCmd_Pass( char *password );


int8_t parsePASVResponse( char *pos );


// Primitives
//
void sendPasvCommand( void );
void sendPortCommand( void );
void sendListCommand( char *cmd );
void sendRetrCommand( void );
void sendStorCommand( void );

void doNlst( void );
void doGet( void );
void doPut( void );
void doMput( char *filespec );
void doMget( char *filespec );
void doMdelete( char *filespec );


// Data Socket data transfer functions
//
int8_t readFileList( void );
int8_t receiveFile( void );
int8_t sendFile( void );





// Ctrl-Break and Ctrl-C handler.  Check the flag once in a while to see if
// the user wants out.

volatile uint8_t CtrlBreakDetected = 0;

void __interrupt __far ctrlBreakHandler( ) {
  CtrlBreakDetected = 1;
}




uint8_t far *Screen_base;
uint16_t ScreenRows, ScreenCols;


// Command history support
//
// readConsole assumes that commands only span two lines.  If you
// want to span a 3rd line with longer commands, then you have some
// work to do.

#define PREVIOUS_COMMANDS (11)

static void complain( void ) { sound(500); delay( 50); nosound( ); }

char previousCommands[PREVIOUS_COMMANDS][USERINPUTBUF_MAX_LEN];
int previousCommandIndex = 0;

static void initCommandHistory( void ) {

  for ( int i=0; i < PREVIOUS_COMMANDS; i++ ) {
    previousCommands[i][0] = 0;
  }

}




// Useful strings

const char BytesTransferred_fmt[] = "Bytes transferred: %lu";
const char CtrlBreakCmdState_msg[] = "\nCtrl-Break detected - cleaning up to go to command state.\n";
const char DataSocketClosed_fmt[] = "Data socket closed early - close reason: %d\n";
const char NotEnoughMemory_msg[] = "Not enough free memory";
const char PressAKey_msg[] = "Press a key to continue ...";
const char TransferModeIs_fmt[] = "Transfer mode is set to: %s\n";
const char XferAborted_msg[] = "Xfer aborted due to Ctrl-Break";
const char NeedAFilename_msg[] = "You need to provide a filename";
const char NoMatches_msg[] = "No names on the remote server matched.";
const char Spaces[] = "                                        ";


static char CopyrightMsg[] = "mTCP FTP by M Brutman (mbbrutman@gmail.com) (C)opyright 2008-2020\nVersion: " __DATE__ "\n";

int main( int argc, char *argv[] ) {

  puts( CopyrightMsg );

  // Parse args, environment and read config file
  parseArgs( argc, argv );

  if ( Utils::parseEnv( ) != 0 ) {
    exit(1);
  }

  readConfigParms( );

  probeStdinStdout( );



  // Find out some basic information about the screen we are using.

  unsigned char screenMode = *((unsigned char far *)MK_FP( 0x40, 0x49 ));

  if ( screenMode == 7 ) {
    Screen_base = (uint8_t far *)MK_FP( 0xb000, 0 );
  }
  else {
    Screen_base = (uint8_t far *)MK_FP( 0xb800, 0 );
  }


  if ( getEgaMemSize( ) == 0x10 ) {
    // Failed.  Must be MDA or CGA
    ScreenCols = 80;
    ScreenRows = 25;
  }
  else {
    ScreenCols = *((unsigned char far *)MK_FP( 0x40, 0x4A ));
    ScreenRows = *((unsigned char far *)MK_FP( 0x40, 0x84 )) + 1;
  }



  initCommandHistory( );


  inBuf      = (uint8_t *)malloc( INBUFSIZE );
  FileBuffer = (uint8_t *)malloc( FileBufSize );
  mList      = (char *)malloc( mListBufSize );

  if ( (inBuf == NULL) || (FileBuffer == NULL) || (mList == NULL) ) {
    puts( NotEnoughMemory_msg );
    exit(1);
  }




  // Three sockets (Control, Listen, and Data)
  // 10 outgoing TCP buffers
  if ( Utils::initStack( 3, 10, ctrlBreakHandler, ctrlBreakHandler ) ) {
    puts( "Could not start TCP/IP" );
    exit(1);
  }


  // From this point forward you must use shutdown( ) to exit.


  if ( resolveServer( FtpServer, FtpServerAddr ) ) {
    printf( "Error resolving FTP address: %s\n", FtpServer );
    shutdown(-1);
  }


  // Don't bother checking the return codes - should not fail.
  ControlSocket = TcpSocketMgr::getSocket( );
  ListenSocket  = TcpSocketMgr::getSocket( );
  DataSocket    = NULL;


  // Open socket to server
  int8_t rc = ControlSocket->setRecvBuffer( CONTROL_RECV_SIZE );
  if ( rc ) {
    puts( NotEnoughMemory_msg );
    shutdown( -1 );
  }


  ControlPort  = 1024 + ( rand( ) % 1024 );
  NextDataPort = 4096 + ( rand( ) % 20480 );

  printf( "\nOpening control connection to %d.%d.%d.%d:%u with local port %u\n",
          FtpServerAddr[0], FtpServerAddr[1], FtpServerAddr[2], FtpServerAddr[3],
          FtpServerPort, ControlPort );

  rc = ControlSocket->connect( ControlPort, FtpServerAddr, FtpServerPort, ConnectTimeout );

  if ( rc != 0 ) {
    puts( "Connection failed!" );
    shutdown( -1 );
  }

  puts( "Connected\n" );



  char lineBuffer[ USERINPUTBUF_MAX_LEN ];


  while ( 1 ) {

    // Check ControlSocket for input
    pollSocket( ControlSocket, 300 );

    // Check for connection closed after input is checked.
    if ( ControlSocket->isRemoteClosed( ) ) {
      puts( "\nServer closed control connection" );
      break;
    }


    if ( CtrlBreakDetected ) {

      if ( (ClientState > SentPass) && (ClientState < Closing) ) {

        puts( CtrlBreakCmdState_msg );
        CtrlBreakDetected = 0;
        closeDataSockets( );

        // Give one more chance to read input from the server before
        // presenting the command line.
        pollSocket( ControlSocket, 500 );
        ClientState = CmdLine;

      }
      else {
        // We were not logged in yet.  End program.
        break;
      }

    }


    if ( (ClientState == ServerConnected) || (ClientState == UserOkSendPass) || (ClientState == CmdLine) ) {

      if ( ClientState == ServerConnected ) {
        printf( "Userid: " );
      }
      else if ( ClientState == UserOkSendPass ) {
        printf( "Password: " );
      }
      else printf( "\n--> " );

      if ( IsStdinFile ) {
        int rc = readStdin( lineBuffer, USERINPUTBUF_MAX_LEN );
        if ( rc ) break;
      }
      else {

        if ( ClientState == CmdLine ) {
          readConsole( lineBuffer, USERINPUTBUF_MAX_LEN, 1 );
        }
        else {
          // No advanced command editing when entering userid or password
          readConsole( lineBuffer, USERINPUTBUF_MAX_LEN, 0 );
        }

        if ( CtrlBreakDetected ) break;
      }

      processUserInput( lineBuffer );

    }

  }

  ControlSocket->close( );

  shutdown(0);
}




void usage( void ) {
  puts( "\n"
        "ftp [options] ftp_server_name\n\n"
        "Options:\n"
        "  -help        Shows this help\n"
        "  -port <n>    Specify FTP server port\n"
  );
  exit( 1 );
}



void parseArgs( int argc, char *argv[] ) {

  if ( argc < 2 ) usage( );

  uint16_t i=1;
  for ( ; i<argc; i++ ) {

    if ( stricmp( argv[i], "-port" ) == 0 ) {
      i++;
      if ( i == argc ) {
        puts( "Need to provide a port with the -port option" );
        usage( );
      }
      FtpServerPort = atoi( argv[i] );
      if ( FtpServerPort == 0 ) {
        puts( "Bad port specified on -port option" );
        usage( );
      }
    }
    else if ( stricmp( argv[i], "-help" ) == 0 ) {
      printf( "Options and usage ...\n" );
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
    printf( "Need to provide a server name to connect to\n" );
    usage( );
  }


  // Next argument is always the server name
  strncpy( FtpServer, argv[i], FTPSERVERNAME_MAX_LEN );
  FtpServer[ FTPSERVERNAME_MAX_LEN - 1 ] = 0;
}




void readConfigParms( void ) {

  char tmp[10];
  uint16_t tmpVal;

  Utils::openCfgFile( );

  if ( Utils::getAppValue( "FTP_CONNECT_TIMEOUT", tmp, 10 ) == 0 ) {
    tmpVal = atoi( tmp );
    if ( tmp != 0 ) {
      ConnectTimeout = atoi( tmp );
      ConnectTimeout = ConnectTimeout * 1000ul;
    }
  }

  if ( Utils::getAppValue( "FTP_TCP_BUFFER", tmp, 10 ) == 0 ) {
    tmpVal = atoi( tmp );
    if ( (tmpVal >= 512) && (tmpVal <= 16384) ) TcpRecvSize = tmpVal;
  }

  if ( Utils::getAppValue( "FTP_FILE_BUFFER", tmp, 10 ) == 0 ) {
    tmpVal = atoi( tmp );
    if ( (tmpVal >= 512) && (tmpVal <= 32768) ) FileBufSize = tmpVal;
  }

  if ( Utils::getAppValue( "FTP_MLIST_BUFFER", tmp, 10 ) == 0 ) {
    tmpVal = atoi( tmp );
    if ( (tmpVal >= 512) && (tmpVal <= 16384) ) mListBufSize = tmpVal;
  }

  if ( Utils::getAppValue( "FTP_BIN_CMD_STUFF", tmp, 10 ) == 0 ) {
    if ( stricmp( tmp, "false" ) == 0 ) StuffBinCommandAtStart = false;
  }

  Utils::closeCfgFile( );

}




int8_t resolveServer( char *name, IpAddr_t addr ) {

  // Resolve the name (send initial request)
  int8_t rc = Dns::resolve( name, addr, 1 );
  if ( rc < 0 ) {
    return -1;
  }

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( 1 ) {

    if ( !Dns::isQueryPending( ) || CtrlBreakDetected ) break;

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    // Tcp::drivePackets( );
    Dns::drivePendingQuery( );

  }

  // Query is no longer pending or we bailed out of the loop.
  rc = Dns::resolve( name, addr, 0 );

  if ( rc != 0 ) return -1;

  uint32_t t = Timer_diff( startTime, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;
  printf( "FTP server resolved in %ld.%02ld seconds\n", (t/1000), (t%1000) );

  return 0;
}



void shutdown( int rc ) {
  Utils::endStack( );
  puts( "\nPlease send comments and bug reports to mbbrutman@gmail.com\n");
  exit( rc );
}



static char *HelpMenu[] = {

"",
CopyrightMsg,

"Directory operations:\n",

"  dir [<filespec>]    Show a detailed directory list",
"  ls  [<filespec>]    Directory list without detail",
"  pager <n>           Pause ls or dir output after approximately n lines\n",

"  cd [<directory>]    Change directory on server (Alias: cwd)",
"  cdup                Move up one directory on server",
"  pwd                 Show current directory on server\n",

"  lcd [<dir>]         Show current dir or change dir on local machine",
"  lmd <newdir>        Create new directory on local machine\n",

"  mkdir <dirname>     Make directory <dirname> on FTP server (Alias: md)",
"  rmdir <dirname>     Remove directory <dirname> on FTP server (Alias: rd)\n",

"#", // Break output here and wait for keyboard input

"Setting the file transfer mode:\n",

"  ascii               Set ASCII transfer mode",
"  image               Set IMAGE transfer mode",
"  binary or bin       Aliases for image command\n",

"  Note! The server determines the default file transfer mode.  To be safe",
"  always set the mode before moving a file.  IMAGE is usually what you want.\n",

"File operations:\n",

"  get <file> [<new>]  Get <file> from server, use <new> for target",
"  put <file> [<new>]  Send <file> to server, use <new> for target",
"  delete <filename>   Delete <filename> on FTP server (Alias: del)\n",

"  prompt              Toggle mget/mput/mdelete prompting on or off\n",

"  mget <filespec>     Multi-file get",
"  mput <filespec>     Multi-file put",
"  mdelete <filespec>  Multi-file delete\n",

"  rename <from> <to>  Rename file on server\n",

"#", // Break output here and wait for keyboard input

"Other commands:\n",

"  xfermode [<mode>]   Show the current transfer mode or set file transfer",
"                      mode to CLASSIC, PORT or PASSIVE.\n",

"  Hint: xfermode PASSIVE works well with most firewalls. CLASSIC is obsolete.\n",

"  quote <string>      Send <string> to FTP server to be interpreted",
"  quit                Self explanatory (Aliases: exit bye close)",
"  shell               Shell to DOS (use caution!)",
"  interactive         Useful only when running a script - see the docs\n",

"Ctrl-Break will usually interrupt a pending file transfer.  At the",
"command line it will end the program, so don't be too impatient!\n",

NULL

};



// Entering a user name or entering a password need to be done if the server
// prompts for it.  Otherwise, we are just processing a generic command.

void processUserInput( char *buffer ) {

  if ( ClientState < CmdLine ) {

    if ( ClientState == ServerConnected ) {
      processCmd_User( buffer );
    }
    else if ( ClientState == UserOkSendPass ) {
      processCmd_Pass( buffer );
    }

  }

  else {
    processUserInput2( buffer );
  }

}



void processUserInput2( char *lineBuffer ) {

  // This is used by mget, mput and mdelete.  mget and mput only expect a DOS
  // filespec to be 13 chars including the trailing null.  mdelete can have
  // a longer filespec because we'll just pass it through to the server,
  // which might support longer filenames.

  char filespec[60];

  char command[COMMAND_MAX_LEN];
  char *nextTokenPtr = Utils::getNextToken( lineBuffer, command, COMMAND_MAX_LEN );

  if ( *command == 0 ) return;

  TRACE(( "Ftp: user input: %s\n", lineBuffer ));


  if ( stricmp( command, "help" ) == 0 ) {

    uint16_t i=0;
    while ( HelpMenu[i] != NULL ) {

      if ( HelpMenu[i][0] == '#' ) {
        int startX = wherex( );
        int startY = wherey( );
        printf( PressAKey_msg );
        fflush( stdout );
        while ( bioskey(1) == 0 ) { }
        bioskey(0);

        // Write enough spaces to clear out the prompt
        gotoxy( startX, startY );
        cputs( Spaces );
        gotoxy( startX, startY );
      }
      else {
        puts( HelpMenu[i] );
      }
      i++;
    }

  }

  else if ( stricmp( command, "dir" ) == 0 ) {

    // Is there an optional parm?
    Utils::getNextToken( nextTokenPtr, ServerFile, FILESPEC_MAX_LEN );

    ReadingForMget = false;

    if ( TransferMode == Passive ) {
      sendPasvCommand( );
      ClientState = ListSentPasv;
    }
    else if ( TransferMode == PortFirst ) {
      sendPortCommand( );
      ClientState = ListSentPort;
    }
    else {
      // Ancient history: connect back to us at the same port we are
      // using for the ControlPort.
      DataPort = ControlPort;
      listenForDataSocket( );
      sendListCommand( "LIST" );
      ClientState = ListSentActive;
    }

  }

  else if ( stricmp( command, "ls" ) == 0 ) {

    // Is there an optional parm?
    Utils::getNextToken( nextTokenPtr, ServerFile, FILESPEC_MAX_LEN );

    ReadingForMget = false;
    doNlst( );
  }

  else if ( stricmp(command, "get") == 0 ) {

    // Did they provide a filename?

    char *pos = Utils::getNextToken( nextTokenPtr, ServerFile, FILESPEC_MAX_LEN );

    if ( *ServerFile == 0 ) {
      puts( NeedAFilename_msg );
    }
    else {

      // Did they provide a new filename for the local target file?
      Utils::getNextToken( pos, LocalFile, FILESPEC_MAX_LEN );

      // doGet expects ServerFile and NewFile to be set.
      doGet( );

    }
  }

  else if ( stricmp(command, "put") == 0 ) {

    // Did they provide a filename?

    char *pos = Utils::getNextToken( nextTokenPtr, LocalFile, FILESPEC_MAX_LEN );

    if ( *LocalFile == 0 ) {
      puts( NeedAFilename_msg );
    }
    else {

      // Did they provide a new name for the server filename?
      Utils::getNextToken( pos, ServerFile, FILESPEC_MAX_LEN );

      // Does this file exist?

      struct stat statbuf;
      stat(LocalFile, &statbuf);

      if ( !(statbuf.st_mode & S_IFREG) ) {
        printf( "Error: %s is not a file.\n", LocalFile );
      }
      else {
        // doPut expects ServerFile and NewFile to be set.
        doPut( );
      }

    }

  }

  else if ( (stricmp(command, "cwd") == 0) || (stricmp(command, "cd") == 0) ) {
    processSimpleUserCmd( "CWD", nextTokenPtr );
  }

  else if ( stricmp(command, "cdup" ) == 0 ) {
    ControlSocket->send( (uint8_t *)"CDUP" _NL_, 6 );
    ClientState = CmdSent;
  }

  else if ( stricmp(command, "pwd" ) == 0 ) {
    ControlSocket->send( (uint8_t *)"PWD" _NL_, 5 );
    ClientState = CmdSent;
  }

  else if ( stricmp(command, "ascii" ) == 0 ) {
    ControlSocket->send( (uint8_t *)"TYPE A" _NL_, 8 );
    ClientState = CmdSent;
  }

  else if ( (stricmp(command, "binary" ) == 0) ||
            (stricmp(command, "bin" ) == 0 ) ||
            (stricmp(command, "image" ) == 0) )
  {
    ControlSocket->send( (uint8_t *)"TYPE I" _NL_, 8 );
    ClientState = CmdSent;
  }

  else if ( (stricmp(command, "del") == 0) || (stricmp(command, "delete") == 0) ) {
    processSimpleUserCmd( "DELE", nextTokenPtr );
  }

  else if ( (stricmp(command, "rmdir") == 0) || (stricmp(command, "rd") == 0) ) {
    processSimpleUserCmd( "RMD", nextTokenPtr );
  }

  else if ( (stricmp(command, "mkdir") == 0) || (stricmp(command, "md") == 0) ) {
    processSimpleUserCmd( "MKD", nextTokenPtr );
  }


  else if ( stricmp(command, "xfermode" ) == 0 ) {

    char newMode[10];

    Utils::getNextToken( nextTokenPtr, newMode, 10 );
    if ( *newMode == 0 ) {
      printf( TransferModeIs_fmt, TransferModeStrings[TransferMode] );
    }
    else {

      uint16_t i;
      for (i=0; i < 3; i++ ) {
        if ( stricmp( newMode, TransferModeStrings[i] ) == 0 ) {
          TransferMode = (TransferModes_t)i;
          printf( TransferModeIs_fmt, TransferModeStrings[TransferMode] );
          break;
        }
      }

      if ( i == 3 ) {
        puts( "Bad option ... Use classic, port or passive" );
      }

    }

  }


  else if ( (stricmp(command, "quit") == 0) ||
            (stricmp(command, "exit") == 0) ||
            (stricmp(command, "close") == 0) ||
            (stricmp(command, "bye") == 0) )
  {
    ControlSocket->send( (uint8_t *)"QUIT"  _NL_, 6 );
    ClientState = Closing;
  }

  else if ( stricmp( command, "quote" ) == 0 ) {

    bool enoughInput = true;

    if ( nextTokenPtr != NULL ) {

      char tmp[5];
      Utils::getNextToken( nextTokenPtr, tmp, 5 );

      if ( *tmp != 0 ) {
        ControlSocket->send( (uint8_t *)nextTokenPtr+1, strlen(nextTokenPtr+1) );
        ControlSocket->send( (uint8_t *)_NL_, 2 );
        ClientState = CmdSent;
      }
      else {
        enoughInput = false;
      }

    }
    else {
      enoughInput = false;
    }

    if ( !enoughInput ) {
      puts( "You need to provide a command to send." );
    }

  }

  else if ( stricmp( command, "shell" ) == 0 ) {
    puts(
     "\nWarning: Your server connection is not being serviced while you are\n"
     "in DOS.  Keep it quick and don't do anything fancy.  Use the 'exit'\n"
     "command to return.  Also, Ctrl-Break is disabled so don't use it."
    );

    system( "command" );

    // Just in case they were foolish enough to use it while they were away.
    CtrlBreakDetected = 0;
  }

  else if ( stricmp( command, "mput" ) == 0 ) {

    // Is there an optional parm?
    Utils::getNextToken( nextTokenPtr, filespec, FILESPEC_MAX_LEN );

    if ( *filespec == 0 ) {
      puts( NeedAFilename_msg );
    }
    else {
      doMput( filespec );
      ClientState = CmdLine;
    }
  }

  else if ( stricmp( command, "mget" ) == 0 ) {

    // Is there an optional parm?
    Utils::getNextToken( nextTokenPtr, filespec, FILESPEC_MAX_LEN );

    if ( *filespec == 0 ) {
      puts( NeedAFilename_msg );
    }
    else {
      doMget( filespec );
      ClientState = CmdLine;
    }
  }

  else if ( stricmp( command, "mdelete" ) == 0 ) {

    // Is there an optional parm?
    Utils::getNextToken( nextTokenPtr, filespec, FILESPEC_MAX_LEN );

    if ( *filespec == 0 ) {
      puts( NeedAFilename_msg );
    }
    else {
      doMdelete( filespec );
      ClientState = CmdLine;
    }
  }

  else if ( stricmp( command, "prompt" ) == 0 ) {
    MgetMputPrompt = !MgetMputPrompt;
    printf( "Prompting is now: %s\n", MgetMputPrompt?"On":"Off" );
  }

  else if ( stricmp( command, "rename" ) == 0 ) {

    char *pos = Utils::getNextToken( nextTokenPtr, ServerFile, FILESPEC_MAX_LEN );
    Utils::getNextToken( pos, RenameToParm, FILESPEC_MAX_LEN );

    if ( *ServerFile == 0 || *RenameToParm == 0 ) {
      puts( "Format: rename <current_name> <new_name>" );
    }
    else {
      char outBuf[ FILESPEC_MAX_LEN + 10 ];
      int bytes = sprintf( outBuf, "RNFR %s" _NL_, ServerFile );
      ControlSocket->send( (uint8_t *)outBuf, bytes );
      ClientState = RenameFromSent;
    }

  }

  else if ( stricmp( command, "lcd" ) == 0 ) {

    #if defined ( __WATCOMC__ ) || defined ( __WATCOM_CPLUSPLUS__ )
    char dir[PATH_MAX+1];
    Utils::getNextToken( nextTokenPtr, dir, PATH_MAX+1 );
    #else 
    char dir[MAXPATH+1];
    Utils::getNextToken( nextTokenPtr, dir, MAXPATH );
    #endif

    if ( *dir ) {

      char *newDir = dir;

      if ( strlen(dir) > 1 ) {

        if ( dir[1] == ':' ) {

          // Drive was specified
          newDir = dir + 2;
          int rc = _chdrive( toupper( dir[0] ) - 'A' + 1 );

          if ( rc ) {
            printf( "Error: Bad drive letter\n" );
            *newDir = 0;
          }

        }

      }

      if ( *newDir ) {
        if ( chdir( newDir ) ) {
          printf( "Error: Directory not changed\n" );
        }
      }

    }

    current_directory( dir );
    printf( "The current directory is: %s\n", dir );


  }

  else if ( stricmp( command, "lmd" ) == 0 ) {

    char dir[PATH_MAX+1];
    Utils::getNextToken( nextTokenPtr, dir, PATH_MAX+1 );

    if ( *dir ) {
      int rc = mkdir( dir );
      if ( rc ) {
        printf( "Error creating %s\n", dir );
      }
    }

  }

  else if ( stricmp( command, "pager" ) == 0 ) {

    char parm[10];
    Utils::getNextToken( nextTokenPtr, parm, 10 );

    if ( *parm == 0 ) {
      puts( "Pager requires a number of lines (0 to disable)" );
    }
    else {
      ScreenPager = atoi( parm );
      printf( "Pager set to %d lines\n", ScreenPager );
    }

  }

  else if ( stricmp( command, "interactive" ) == 0 ) {

    // Assuming that they started FTP with stdin redirected from a file,
    // this will switch stdin back to the console.
    //
    // No ill effect if they were reading from the console already.

    freopen( "CON", "rt", stdin );
    IsStdinFile = false;

  }

  else {
    printf( "\nUnknown command: %s\n", command );
  }


}


void processSimpleUserCmd( char *serverCmd, char *nextTokenPtr ) {

  char parmName[ FILESPEC_MAX_LEN ];
  char outBuf[ FILESPEC_MAX_LEN + 10 ];

  Utils::getNextToken( nextTokenPtr, parmName, FILESPEC_MAX_LEN );

  if ( *parmName == 0 ) {
    printf( "Need to provide a file or directory name.\n" );
  }
  else {
    int bytes = sprintf( outBuf, "%s %s" _NL_, serverCmd, parmName );
    ControlSocket->send( (uint8_t *)outBuf, bytes );
    ClientState = CmdSent;
  }
}





// Timeout is specified in milliseconds

void pollSocket( TcpSocket *s, uint32_t timeout ) {

  clockTicks_t startTime = TIMER_GET_CURRENT( );

  while ( 1 ) {

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    int rc = s->recv( inBuf + inBufIndex, (INBUFSIZE - inBufIndex) );
    if ( rc > -1 ) inBufIndex += rc;

    processSocketInput( );

    uint32_t t_ms = Timer_diff( startTime, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;

    // Timeout?
    if ( t_ms > timeout ) break;

  }

}



void processSocketInput( void ) {

  // Is this a safe size for FTP responses?
  char tmpBuffer[ SERVER_RESP_MAX_LEN ];
  char tmpToken[30];

  if ( inBufIndex == 0 ) return;


  int8_t glRc = getLineFromInBuf( tmpBuffer, SERVER_RESP_MAX_LEN );

  if ( glRc == 0 ) {
    // We didn't get a full line of response back from the server yet.
    return;
  }

  if ( glRc < 0 ) {
    // The response was too long, but we can probably parse it anway
    // because of the 3 digit code.
    puts( "\nWarning: This response overflowed the buffer:" );
  }


  TRACE(( "Ftp: Server msg: %s\n", tmpBuffer ));

  puts( tmpBuffer );


  if ( strlen(tmpBuffer) > 2 ) {

    if ( isdigit(tmpBuffer[0]) && isdigit(tmpBuffer[1]) && isdigit(tmpBuffer[2]) ) {

      // Great, it's a server response.

      if ( (strlen(tmpBuffer) > 3) && (tmpBuffer[3] == '-') ) {
        // Multi-line reponse
        MultilineResponse = true;
      }
      else {
        // Normal return code or end of a multiline response.
        MultilineResponse = false;
      }

    }
    else {
      // Not a response code; don't parse
      return;
    }

  }
  else {
    // Not a response code; don't parse
  }

  if ( MultilineResponse ) {
    // Don't parse until we see the end marker
    return;
  }

  /*
  if ( (strlen(tmpBuffer) > 3) && (tmpBuffer[3] == '-') ) {
    // This message crosses several lines - dont bother processing it.
    return;
  }
  */

  // Get the numerical reply to figure out what to do
  char *pos = tmpBuffer;
  pos = Utils::getNextToken( pos, tmpToken, 30 );
  uint16_t numReply = atoi( tmpToken );

  switch ( numReply ) {

    case 110: // Restart marker reply - should never see this.
    case 120: // Service ready in n minutes
    {
      // These are preliminary replies - wait for another reply
      // before going to command line state.
      break;
    }

    case 125: // Connection already open; transfer starting
    case 150: // File status ok; about to open data connection
    {

      // If we are in PASSIVE mode then we know that the connection is opened
      // and established because we made the socket connection before sending
      // the user command.  If the server sees that then they will send a 125.
      // If they have not seen it yet (accept not done?) then we will see a
      // 150.  Either way, we don't care - it is the same.

      // If we are in PORT mode then we have their problem above - we have a
      // listening socket but have not done the accept yet.  If the server
      // has opened the socket they will send a 125, but that doesn't matter
      // because we have not done the accept yet.  If the server has not opened
      // the socket then we need to go into the accept loop anyway to wait for it.

      bool errorCleanupNeeded = false;

      if ( ClientState == RetrSentAfterPasv ) {
        receiveFile( );
      }
      else if ( (ClientState == RetrSentActive) || (ClientState == RetrSentAfterPort) ) {
        if ( waitForDataSocket( ) == 0 ) {
          receiveFile( );
        }
        else {
          errorCleanupNeeded = true;
        }
      }
      else if ( ClientState == StorSentAfterPasv ) {
        sendFile( );
      }
      else if ( (ClientState == StorSentActive) || (ClientState == StorSentAfterPort) ) {
        if ( waitForDataSocket( ) == 0 ) {
          sendFile( );
        }
        else {
          errorCleanupNeeded = true;
        }
      }
      else if ( (ClientState == ListSentAfterPasv) || (ClientState == NListSentAfterPasv) ) {
        readFileList( );
      }
      else if ( (ClientState == ListSentActive) || (ClientState == NListSentActive) ||
                (ClientState == ListSentAfterPort) || (ClientState == NListSentAfterPort) ) {
        if ( waitForDataSocket( ) == 0 ) {
          readFileList( );
        }
        else {
          errorCleanupNeeded = true;
        }
      }

      // A 226 will come back on the control socket.

      if ( errorCleanupNeeded ) {

        // If the user hit Ctrl-Break while we were in waitForDataSocket
        // we might not get a 226 from the server.  Just close things and
        // go to command state to be safe.

        ClientState = CmdLine;
        closeDataSockets( );
      }

      break;
    }

    case 200: { // Command Okay

      if ( ClientState == ListSentPort ) {
        sendListCommand( "LIST" );
        ClientState = ListSentAfterPort;
      }
      else if ( ClientState == NListSentPort ) {
        sendListCommand( "NLST" );
        ClientState = NListSentAfterPort;
      }
      else if ( ClientState == RetrSentPort ) {
        sendRetrCommand( );
        ClientState = RetrSentAfterPort;
      }
      else if ( ClientState == StorSentPort ) {
        sendStorCommand( );
        ClientState = StorSentAfterPort;
      }
      else if ( ClientState == BinStuffed ) {
        puts( "File transfer mode set to BIN." );
        ClientState = CmdLine;
      }
      else {
        ClientState = CmdLine;
      }

      break;
    }

    case 202: // Command not implemented or superfluous
    case 211: // System status or help reply
    case 212: // Directory status
    case 213: // File status
    case 214: // Help message
    case 215: // NAME system type
    {
      ClientState = CmdLine;
      break;
    }

    case 220: // Service ready for new user
    {
      ClientState = ServerConnected;
      break;
    }

    case 221: // Service closing control connection
    {
      ClientState = Closing;
      break;
    }

    case 225: // Data connection open but no transfer in progress
    case 226: // Closing Data Connection after file xfer or abort
    {
      ClientState = CmdLine;
      break;
    }

    case 227: { // PASV response

      if ( parsePASVResponse(pos) ) {
        puts( "Error: Unable to parse PASV response" );
        ClientState = CmdLine;
        break;
      }

      printf( "Socket for PASV connect will be %d.%d.%d.%d:%u\n",
              PasvAddr[0], PasvAddr[1], PasvAddr[2], PasvAddr[3], PasvPort );

      if ( (ClientState == ListSentPasv) || (ClientState == NListSentPasv) ||
           (ClientState == RetrSentPasv) || (ClientState == StorSentPasv) ) {

        int8_t rc = connectDataSocket( );
        if ( rc ) {
          puts( "Error connecting data socket" );
          ClientState = CmdLine;
          break;
        }

      }

      if ( ClientState == ListSentPasv ) {
        sendListCommand( "LIST" );
        ClientState = ListSentAfterPasv;
      }
      else if ( ClientState == NListSentPasv ) {
        sendListCommand( "NLST" );
        ClientState = NListSentAfterPasv;
      }
      else if ( ClientState == RetrSentPasv ) {
        sendRetrCommand( );
        ClientState = RetrSentAfterPasv;
      }
      else if ( ClientState == StorSentPasv ) {
        sendStorCommand( );
        ClientState = StorSentAfterPasv;
      }
      else {
        ClientState = CmdLine;
      }

      break;
    }

    case 230: // Logged in
    {
      if ( StuffBinCommandAtStart ) {
        // Stuff a BIN command to save the user from themselves.
        puts( "\nSetting the server file transfer mode to BIN" );
        ControlSocket->send( (uint8_t *)"TYPE I" _NL_, 8 );
        ClientState = BinStuffed;
      }
      else {
        puts( "\nRemember: For Great Justice set BIN mode before transfering binary files!" );
        ClientState = CmdLine;
      }
      break;
    }

    case 250: // Requested file action completed
    case 257: // Pathname created
    {
      ClientState = CmdLine;
      break;
    }

    case 331: { ClientState = UserOkSendPass; break; }

    case 332: // Need account for login
    {
      ClientState = CmdLine;
      break;
    }

    case 350: // Requested file action pending further information
    {
      if ( ClientState == RenameFromSent ) {
        char outBuf[ FILESPEC_MAX_LEN + 10 ];
        int bytes = sprintf( outBuf, "RNTO %s" _NL_, RenameToParm );
        ControlSocket->send( (uint8_t *)outBuf, bytes );
        ClientState = RenameToSent;
      }
      else {
        // Dunno what's going on here
        ClientState = CmdLine;
      }
      break;
    }


    case 421: // Service not available closing control connection
    {
      ClientState = Closing;
      break;
    }



    case 425: // Can't open data connection
    case 426: // Connection closed and transfer aborted
    {
      ClientState = CmdLine;
      break;
    }


    case 450: // File unavailable or Permission denied (transient)
    case 451: // Requested action aborted; local error in processing
    case 452: // Insufficient storage
    case 500: // Unrecognized command
    case 501: // Syntax error in parameters or arguments
    case 502: // Command not implemented
    case 503: // Bad sequence of commands
    case 504: // Command not implemented for that parameter
    case 550: // File unavailable or Permission denied
    case 551: // Requested action aborted; page type unknown
    case 552: // Requested file action aborted; exceeded storage allocation
    case 553: // File name not allowed
    {
      if ( ClientState == BinStuffed ) {
        puts( "Warning: Failed to set file transfer mode to BIN" );
      }
      ClientState = CmdLine;
      break;
    }

    case 530: // Not logged in
    {
      ClientState = ServerConnected;
      break;
    }


    default: {
      puts( "Warning: Unrecognized response from server" );
      break;
    }


  }

  // Just a little code that is sloppy but convenient
  if ( ClientState == CmdLine ) {
    closeDataSockets( );
  }

}




void processCmd_User( char *nextTokenPtr ) {

  char outBuf[ USERNAME_MAX_LEN + 10 ];

  char userName[USERNAME_MAX_LEN];

  Utils::getNextToken( nextTokenPtr, userName, USERNAME_MAX_LEN );

  if ( *userName == 0 ) {
    puts( "You need to enter a username" );
  }
  else {
    int8_t bytes = sprintf( outBuf, "USER %s" _NL_, userName );
    ControlSocket->send( (uint8_t *)outBuf, bytes );
    ClientState = SentUser;
  }

}


void processCmd_Pass( char *nextTokenPtr ) {

  char outBuf[ PASSWORD_MAX_LEN + 10 ];

  char password[PASSWORD_MAX_LEN];

  Utils::getNextToken( nextTokenPtr, password, PASSWORD_MAX_LEN );

  int8_t bytes = sprintf( outBuf, "PASS %s" _NL_, password );
  ControlSocket->send( (uint8_t *)outBuf, bytes );
  ClientState = SentPass;

}




int8_t parsePASVResponse( char *pos ) {

  // Find first digit.  This should be the start of the IP addr and port
  // string.

  bool found = false;
  while ( *pos ) {
    if ( isdigit( *pos ) ) {
      found = true;
      break;
    }
    pos++;
  }



  if ( !found ) {
    PasvAddr[0] = 0; PasvAddr[1] = 0; PasvAddr[2] = 0; PasvAddr[3] = 0; PasvPort = 0;
    return -1;
  }


  uint16_t t0, t1, t2, t3, t4, t5;
  int rc = sscanf( pos, "%d,%d,%d,%d,%d,%d",
               &t0, &t1, &t2, &t3, &t4, &t5 );
  if ( rc != 6 ) {
    PasvAddr[0] = 0; PasvAddr[1] = 0; PasvAddr[2] = 0; PasvAddr[3] = 0; PasvPort = 0;
    return -1;
  }

  // This line is correct as per the original RFCs.  But we are finding a lot
  // of FTP servers behind firewalls with broken configurations such that the
  // servers are responding to PASV commands with the wrong address.
  // From now on just ignore any IP address and use the original server
  // address with the new port.
  //
  // PasvAddr[0] = t0; PasvAddr[1] = t1; PasvAddr[2] = t2; PasvAddr[3] = t3;

  if ( (t0 != FtpServerAddr[0]) || (t1 != FtpServerAddr[1]) || (t2 != FtpServerAddr[2]) || (t3 != FtpServerAddr[3]) ) {
    puts( "Warning: Found a third party address on the PASV response.  Ignoring it." );
  }

  Ip::copy( PasvAddr, FtpServerAddr );
  PasvPort = (t4<<8) + t5;

  return 0;
}




void sendPasvCommand( void ) {
  ControlSocket->send( (uint8_t *)"PASV" _NL_, 6 );
}



void sendPortCommand( ) {
  char outBuf[ 50 ];
  DataPort = NextDataPort;
  NextDataPort = ( ( (NextDataPort-4096) + 1) % 20480 ) + 4096;
  int bytes = sprintf( outBuf, "PORT %d,%d,%d,%d,%d,%d" _NL_,
                       MyIpAddr[0], MyIpAddr[1], MyIpAddr[2], MyIpAddr[3],
                       (DataPort/256), (DataPort%256) );
  ControlSocket->send( (uint8_t *)outBuf, bytes );
  TRACE(( "Ftp: Sent %s\n", outBuf ));

  // The only reason to send a PORT command is because we are expecting an
  // incoming data connection shortly.  Open for listen now, and if anything
  // goes wrong we'll close the listening socket.
  listenForDataSocket( );
}

void sendListCommand( char *cmd ) {
  char outBuf[ FILESPEC_MAX_LEN + 10 ];
  int bytes;
  if ( *ServerFile == 0 ) {
    bytes = sprintf( outBuf, "%s" _NL_, cmd );
  }
  else {
    bytes = sprintf( outBuf, "%s %s" _NL_, cmd, ServerFile );
  }
  ControlSocket->send( (uint8_t *)outBuf, bytes );
}


void sendRetrCommand( void ) {
  char tmp[ FILESPEC_MAX_LEN + 10 ];
  int bytes = sprintf( tmp, "RETR %s" _NL_, ServerFile );
  ControlSocket->send( (uint8_t *)tmp, bytes );
}

void sendStorCommand( void ) {

  // Assume the name of the server is the same as the name on our side,
  // but if they provided a server side name use it.
  char *filename = LocalFile;
  if ( *ServerFile != 0 ) filename = ServerFile;

  char tmp[ FILESPEC_MAX_LEN + 10 ];
  int bytes = sprintf( tmp, "STOR %s" _NL_, filename );
  ControlSocket->send( (uint8_t *)tmp, bytes );
}


void doNlst( void ) {

  if ( TransferMode == Passive ) {
    sendPasvCommand( );
    ClientState = NListSentPasv;
  }
  else if ( TransferMode == PortFirst ) {
    sendPortCommand( );
    ClientState = NListSentPort;
  }
  else {
    // Ancient history: connect back to us at the same port we are
    // using for the ControlPort.
    DataPort = ControlPort;
    listenForDataSocket( );
    sendListCommand( "NLST" );
    ClientState = NListSentActive;
  }

}


// doGet and doPut expect ServerFile and LocalFile to be set by this point.
// If they are not you will blow up later when trying to open the file to
// send or receive it.

void doGet( void ) {

  TRACE(( "Ftp: doGet: receiving %s in mode: %s\n",
          ServerFile, TransferModeStrings[TransferMode] ));

  if ( TransferMode == Passive ) {
    sendPasvCommand( );
    ClientState = RetrSentPasv;
  }
  else if ( TransferMode == PortFirst ) {
    sendPortCommand( );
    ClientState = RetrSentPort;
  }
  else {
    // Ancient history: connect back to us at the same port we are
    // using for the ControlPort.
    DataPort = ControlPort;
    listenForDataSocket( );
    sendRetrCommand( );
    ClientState = RetrSentActive;
  }
}


void doPut( void ) {

  TRACE(( "Ftp: doPut: sending %s in mode: %s\n",
          LocalFile, TransferModeStrings[TransferMode] ));

  if ( TransferMode == Passive ) {
    sendPasvCommand( );
    ClientState = StorSentPasv;
  }
  else if ( TransferMode == PortFirst ) {
    sendPortCommand( );
    ClientState = StorSentPort;
  }
  else {
    // Ancient history: connect back to us at the same port we are
    // using for the ControlPort.
    DataPort = ControlPort;
    listenForDataSocket( );
    sendStorCommand( );
    ClientState = StorSentActive;
  }

}



// Wait until we get back to CmdLine state
//
// 0 - We made it
// 1 - Control socket closed
// 2 - User hit Ctrl-Break

int8_t driveLoopUntilCmdLine( ) {

  while ( ClientState != CmdLine ) {

    // Check ControlSocket for input
    pollSocket( ControlSocket, 150 );

    // Check for connection closed after input is checked.
    if ( ControlSocket->isRemoteClosed( ) ) {
      return 1;
    }

    if ( CtrlBreakDetected ) {
      puts( CtrlBreakCmdState_msg );
      CtrlBreakDetected = 0;
      closeDataSockets( );
      ClientState = CmdLine;
      return 2;
    }

  }

  return 0;
}


// Return codes:
//   0 - prompting is turned off
//   1 - person said yes
//   2 - person said no
//   3 - person said quit
//   4 - Ctrl-Break

int8_t promptMgetMput( char *cmd ) {

  if ( !MgetMputPrompt ) return 0;

  while ( 1 ) {

    printf( "  %s this file? (y/n/q) ", cmd );

    char userInput[5];

    // This isn't going to be scripted, so read from the console
    readConsole( userInput, 5, 0 );  // No advanced command editing
    if ( CtrlBreakDetected ) {
      CtrlBreakDetected = 0;
      ClientState = CmdLine;
      puts( "\nCtrl-Break detected\n" );
      return 4;
    }

    char answer[5];
    Utils::getNextToken( userInput, answer, 5 );

    if ( (stricmp(userInput, "y") == 0) || (stricmp(userInput, "yes") == 0) ) {
      return 1;
    }
    else if ( (stricmp(userInput, "n") == 0) || (stricmp(userInput, "no") == 0) ) {
      return 2;
    }
    else if ( (stricmp(userInput, "q") == 0) || (stricmp(userInput, "quit") == 0) ) {
      return 3;
    }

  }

}



void doMput( char *filespec ) {

  // First build up the list of files to be transferred.

  mListIndex = mList;

  struct find_t fileinfo;
  int done = _dos_findfirst( filespec, _A_NORMAL, &fileinfo);

  while (!done) {

    uint16_t len = strlen( fileinfo.name );

    // Make sure we are not going past the end of our allocation
    if ( mListIndex + len + 1 < mList + mListBufSize ) {
      strcpy( mListIndex, fileinfo.name );
      mListIndex = mListIndex + strlen( fileinfo.name ) + 1;
      done = findnext( &fileinfo );
    }
    else {
      printf( "List of files to send is too long: aborting\n" );
      return;
    }

  }

  if ( mListIndex == mList ) {
    puts( NoMatches_msg );
    return;
  }



  // LocalFile will be updated each time through the loop.
  // ServerFile will never be used.
  *ServerFile = 0;

  char *nextFile = mList;

  while ( nextFile < mListIndex ) {

    strcpy( LocalFile, nextFile );

    printf( "\nMPUT: sending %s\n", LocalFile );

    int rc = promptMgetMput( "Send" );

    if ( rc == 0 || rc == 1 ) {
      doPut( );
      if ( driveLoopUntilCmdLine( ) ) break;
    }
    else if ( rc == 3 || rc == 4 ) {
      break;
    }

    nextFile = nextFile + strlen(nextFile) + 1;

  } // end while

}



int16_t fetchFilelistFromServer( const char *filespec ) {

  strcpy( ServerFile, filespec );

  ReadingForMget = true;

  mListIndex = mList;
  mListIndex[0] = 0;


  // Need to wait for the results of the NLST to come back.
  doNlst( );
  if ( driveLoopUntilCmdLine( ) ) return -1;
  
  if ( mListIndex == mList ) {
    puts( NoMatches_msg );
    return -1;
  }

  return 0;
}


void doMget( char *filespec ) {

  // Get the list of files to transfer from the server using an NLST command.
  if ( fetchFilelistFromServer( filespec ) ) return;


  // ServerFile will be updated each time through the loop.
  // LocalFile will only be used if the incoming filename
  // has an invalid character, length or format.

  int skippedFiles = 0;

  char *i = mList;

  while ( i < mListIndex ) {

    if ( isValidDosFilename(i) ) {

      strcpy( ServerFile, i );
      *LocalFile = 0;

      printf( "\nMGET: receiving %s\n", ServerFile );

      int rc = promptMgetMput( "Get" );

      if ( rc == 0 || rc == 1 ) {
        doGet( );
        if ( driveLoopUntilCmdLine( ) ) break;
      }
      else if ( rc == 3 || rc == 4 ) {
        break;
      }

    }
    else {
      printf( "Skipping %s because it is not a valid DOS filename.\n", i );
      skippedFiles++;
    }

    i = i + strlen(i) + 1;

  } // end while

  if ( skippedFiles ) {
    printf( "\nWarning: %d files were skipped because they had invalid DOS filenames.\n",
            skippedFiles );
  }

}


void doMdelete( char *filespec ) {

  // Get the list of files to delete from the server using an NLST command.
  if ( fetchFilelistFromServer( filespec ) ) return;


  // ServerFile will be updated each time through the loop.
  // LocalFile will only be used if the incoming filename
  // has an invalid character, length or format.

  int skippedFiles = 0;

  char *i = mList;

  while ( i < mListIndex ) {

    printf( "\nMDELETE: deleting %s\n", i );

    int rc = promptMgetMput( "Delete" );

    if ( rc == 0 || rc == 1 ) {
      processSimpleUserCmd( "DELE", i );
      if ( driveLoopUntilCmdLine( ) ) break;
    }
    else if ( rc == 3 || rc == 4 ) {
      break;
    }

    i = i + strlen(i) + 1;

  } // end while

}






int8_t listenForDataSocket( void ) {

  // Make sure it is clean.
  ListenSocket->close( );
  ListenSocket->reinit( );

  // DataPort has to be set before we get here.

  TRACE(( "Opening listening socket on port %u\n", DataPort ));

  // Open for listening - should return right back.
  int8_t rc = ListenSocket->listen( DataPort, TcpRecvSize );
  if ( rc ) {
    printf( "Error opening listening socket for incoming data (%d)\n", rc );
    TRACE(( "Error opening listening socket on port %u\n", DataPort ));
    return -1;
  }

  printf( "Listening on port %u for incoming data\n", DataPort );

  return 0;
}



int8_t waitForDataSocket( void ) {

  TRACE(( "Waiting for incoming socket.\n" ));

  while ( 1 ) {

    if ( CtrlBreakDetected ) {
      return -1;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    DataSocket = TcpSocketMgr::accept( );
    if ( DataSocket != NULL ) {

      TRACE(( "New data socket on port %u from %d.%d.%d.%d:%u\n",
        DataSocket->srcPort, DataSocket->dstHost[0], DataSocket->dstHost[1],
        DataSocket->dstHost[2], DataSocket->dstHost[3],
        DataSocket->dstPort ));
 
      // Fixme: Right incoming port?
      ListenSocket->close( );
      break;
    }

  }

  // At this point the listening socket is closed and the transient
  // socket is ready to rock and roll.

  return 0;
}



static void closeAndFreeDataSocket( void ) {
  DataSocket->close( );
  TcpSocketMgr::freeSocket( DataSocket );
  DataSocket = NULL;

  TRACE(( "DataSocket closed\n" ));
}


int8_t connectDataSocket( void ) {

  // Should never fail
  DataSocket = TcpSocketMgr::getSocket( );

  int8_t rc = DataSocket->setRecvBuffer( TcpRecvSize );
  if ( rc ) {
    puts( NotEnoughMemory_msg );
    closeAndFreeDataSocket( );
    return -1;
  }

  uint16_t port = NextDataPort;
  NextDataPort = ( ( (NextDataPort-4096) + 1) % 20480 ) + 4096;

  rc = DataSocket->connect( port, PasvAddr, PasvPort, ConnectTimeout );

  if ( rc != 0 ) {
    printf( "Data connection failed\n" );
    closeAndFreeDataSocket( );
    return -1;
  }

  return 0;
}



// This is a recovery/cover our ass procedure.  Close everything
// in a safe way.
void closeDataSockets( void ) {

  TRACE(( "Ftp: closeDataSockets: ListenSocket\n" ));
  ListenSocket->close( );

  if ( DataSocket != NULL ) {
    TRACE(( "Ftp: closeDataSockets: DataSocket\n" ));
    closeAndFreeDataSocket( );
  }

  // Get rid of any pending sockets that were not accepted
  while ( 1 ) {
    DataSocket = TcpSocketMgr::accept( );
    if ( DataSocket != NULL ) {
      puts( "Cleaning up socket that was not accepted." );
      closeAndFreeDataSocket( );
    }
    else {
      break;
    }
  }

}





int8_t readFileList( void ) {

  if ( DataSocket == NULL ) {
    return -1;
  }


  // If we are reading for mput/mget set the list pointer to the beginning
  // of the buffer.
  mListIndex = mList;

  // Used to indicate that we have stopped processing because of a buffer
  // overrun, even though incoming data might still be streaming in.

  int8_t mListAbort = 0;

  uint16_t lines = 0;

  uint8_t done = 0;
  while ( !done ) {

    if ( CtrlBreakDetected ) {
      done = 2;
      break;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    if ( DataSocket->isRemoteClosed( ) ) {
      done = 1;
    }

    int bytesRead;

    while ( bytesRead = DataSocket->recv( FileBuffer, FileBufSize-1 ) ) {

      if ( bytesRead < 0 ) {
        done = 3;
        break;
      }
      if ( bytesRead == 0 ) break;


      if ( !ReadingForMget ) {

        // Screen output

        // Just read a bunch of bytes from the socket and have it in a
        // buffer.  We want to find full lines (delimited by CR/LF)
        // and print them so that we can count the number of lines that
        // have passed.  If we run into the end of the buffer and we
        // are not on a line boundary, adjust the buffer.

        if ( ScreenPager == 0 ) {
          // Fast path for no paging or output is piped to a file
          write( 1, FileBuffer, bytesRead );
        }
        else {

          uint16_t lineStart = 0;

          // Find CR/LF
          for ( uint16_t i=0; i < bytesRead-1; i++ ) {

            if ( FileBuffer[i] == '\r' && FileBuffer[i+1] == '\n' ) {

              FileBuffer[i] = 0;               // Punch out the \r
              puts( (char *)(FileBuffer + lineStart) );  // Output line

              // Bump number of lines that were scrolled.
              lines++;

              // If this line was long, assume it used an extra line
              if ( (i-lineStart) > (ScreenCols-1) ) lines++;

              // Skip past the \n
              i++;
              lineStart = i+1;

              if ( lines >= ScreenPager ) {
                printf( PressAKey_msg );
                fflush( stdout );

                while ( bioskey(1) == 0 ) {
                  PACKET_PROCESS_SINGLE;
                  Arp::driveArp( );
                  Tcp::drivePackets( );
                }

                puts("");

                // Eat new keystroke
                bioskey(0);

                lines = 0;
              }

            }
          }

          // Slap a NULL on the end of the buffer (and hope we left
          // room for that) and print the partial line.
          //
          // Just kidding - we left space for the NULL on the receive

          FileBuffer[bytesRead] = 0;
          printf( "%s", FileBuffer + lineStart );

        }



      }
      else {

        // Reading for mget/mput

        if ( !mListAbort) {

          int i = 0;
          while ( i < bytesRead ) {

            // Spin until we hit a delim, we are out of chars, or
            // we are out of buffer.
            while ( i < bytesRead ) {

              if ( (FileBuffer[i] == '\n') || (FileBuffer[i] == '\r') ) break;

              // Are we going to exceed our buffer?
              if ( (mListIndex+1) == (mList+mListBufSize) ) {
                puts( "File list to receive is too long." );
                mListIndex = mList;
                mListAbort = 1;
                break;
              }

              *mListIndex++ = FileBuffer[i++];
            }


            // We are here because we hit a delimeter char, we have hit the end
            // of the buffer, or we are out of chars.

            if ( mListAbort || (i==bytesRead) ) break;

            *mListIndex++ = 0;

            while ( i < bytesRead ) {
              if ( (FileBuffer[i] == '\n') || (FileBuffer[i] == '\r') ) {
                i++;
              }
              else {
                break;
              }
            }

            // Ok, by this point we have processed delim chars.  Time to
            // go back and get the next filename, if there are chars.

          }

        } // endif mListAbort

      } // end reading mList

    }

  } // end while

  closeAndFreeDataSocket( );

  if ( done == 2 ) {
    printf( "Listing aborted with Ctrl-Break\n" );
  }
  else if ( done == 3 ) {
    printf( "Listing aborted due to socket error (%d)\n" );
  }

  return 0;
}


static uint32_t computeRate( uint32_t bytes, uint32_t elapsed ) {

  uint32_t rate;

  if ( elapsed == 0 ) elapsed = 55;

  if ( bytes < 2000000ul ) {
    rate = (bytes * 1000) / elapsed;
  }
  else if ( bytes < 20000000ul ) {
    rate = (bytes * 100) / (elapsed/10);
  }
  else if ( bytes < 200000000ul ) {
    rate = (bytes * 10) / (elapsed/100);
  }
  else {
    rate = bytes / (elapsed / 1000);
  }

  return rate;
}



int8_t receiveFile( void ) {

  if ( DataSocket == NULL ) {
    return -1;
  }

  // If a LocalFile was not specified then use the filename sent to
  // the server on the RETR command.

  char *targetFileName = LocalFile;
  if ( *targetFileName == 0 ) {
    targetFileName = ServerFile;
  }

  // Open the target file
  FILE *targetFile = fopen( targetFileName, "wb" );

  if ( targetFile == NULL ) {
    printf( "Local error opening file %s for writing\n", targetFileName );
    closeAndFreeDataSocket( );
    return -1;
  }

  // int fd = open( ServerFile, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IWRITE );

  clockTicks_t startTicks = TIMER_GET_CURRENT( );

  uint32_t totalBytesReceived = 0;

  uint16_t bytesRead = 0;
  uint16_t bytesToRead = FileBufSize;

  int x = wherex( );
  int y = wherey( );
  
  uint8_t update = 0;

  uint8_t done = 0;
  while ( !done ) {

    if ( CtrlBreakDetected) {
      done = 4;
      break;
    }

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    if ( DataSocket->isRemoteClosed( ) ) {
      // We are done, but stay in the loop so we drain the receive buffer.
      done = 1;
    }


    int16_t recvRc;

    while ( recvRc = DataSocket->recv( FileBuffer+bytesRead, bytesToRead ) ) {

      if ( recvRc > 0 ) {

        totalBytesReceived += recvRc;

        bytesRead += recvRc;
        bytesToRead -= recvRc;

        if ( bytesToRead == 0 ) {
          int rc = fwrite( FileBuffer, bytesRead, 1, targetFile );
          if (rc != 1 ) {
            done = 3;
          }
          //write( fd, FileBuffer, bytesRead );
          bytesToRead = FileBufSize;
          bytesRead = 0;

          if ( !IsStdoutFile ) {
            if ( update == 0 ) {
              gotoxy( x, y );
              cprintf( BytesTransferred_fmt, totalBytesReceived );
            }
            update = (update + 1) & 0x03;
          }

        }

      }

      else if ( recvRc < 0 ) {
        done = 2; // Error code
        break;

      }

    } // end while recv

  } // end while

  gotoxy( x, y );

  // Flush remaining bytes
  if ( bytesRead ) {
    int rc = fwrite( FileBuffer, bytesRead, 1, targetFile );
    if (rc != 1 ) {
      done = 3;
    }
    // write( fd, FileBuffer, bytesRead );

  }

  switch ( done ) {
    case 1: {
      puts( "Transfer completed with no errors" );
      break;
    }
    case 2: {
      printf( DataSocketClosed_fmt, DataSocket->getCloseReason( ) );
      break;
    }

    case 3: {
      printf( "Local error writing to file %s - disk full?\n", targetFileName );
      break;
    }

    case 4: {
      puts( XferAborted_msg );
      break;
    }

  }

  fclose( targetFile );
  // close(fd);

  uint32_t elapsed = Timer_diff( startTicks, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;

  closeAndFreeDataSocket( );

  uint32_t rate = computeRate( totalBytesReceived, elapsed );

  printf( "%lu bytes received in %lu.%03lu seconds (%lu.%03lu KBytes/sec)\n",
          totalBytesReceived, (elapsed/1000), (elapsed%1000), (rate/1024), (rate%1024) );

  return ( done == 1 ? 0 : -1 );
}







// sendFile
//
// This version sets a large buffer for the C runtime to use when reading
// the file and uses the more primitive enqueue interface to submit packets
// for sending.  That cuts out an extra memcpy that send would normally
// have.

int8_t sendFile( void ) {

  if ( DataSocket == NULL ) {
    return -1;
  }

  // Open the target file.
  FILE *sourceFile = fopen( LocalFile, "rb" );

  if ( sourceFile == NULL ) {
    printf( "Local error opening file %s for reading\n", LocalFile );
    closeAndFreeDataSocket( );
    return -1;
  }

  // Set a large buffer for the C runtime.
  setvbuf( sourceFile, (char *)FileBuffer, _IOFBF, FileBufSize );

  clockTicks_t startTicks = TIMER_GET_CURRENT( );

  uint32_t totalBytesSent = 0;

  int x = wherex( );
  int y = wherey( );

  uint8_t update = 0;

  DataBuf *buf = NULL;


  // The while loop exits when done gets set.  After that, the close call will
  // push out any remaining queued packets.  We need to be careful with the file
  // read-ahead; that buffer has to get enqueued before we can leave the loop.

  uint8_t done = 0;  // 1=done, 2=socket error, 3=local abort, 4=file error

  while ( !done ) {

    if ( CtrlBreakDetected ) {
      done = 3;
      break;
    }


    // Try to pick up and process as many returning ACK packets as
    // possible.  This makes room for new packets going out.

    PACKET_PROCESS_MULT(5);
    Arp::driveArp( );
    Tcp::drivePackets( );


    if ( DataSocket->isRemoteClosed( ) ) {
      done = 2;
      break;
    }


    // Don't bother trying to send packets if we don't have room
    // for new packets in the outgoing queue.

    while ( DataSocket->outgoing.hasRoom( ) ) {

      if ( CtrlBreakDetected ) {
        done = 3;
        break;
      }

      // In an ideal world we have already read ahead in the file and filled
      // a buffer so that it is ready to send immediately.  If a buffer is
      // not ready then read more data into one now.

      if ( buf == NULL ) {

        buf = (DataBuf *)TcpBuffer::getXmitBuf( );

        if ( buf == NULL ) {

          // Could not get an outgoing buffer.  It is unlikely that all of them
          // are in use but if they are, just break out of this loop without
          // doing anything and it will retry on the next pass.

          break;  // Exit the inner while loop

        }


        // Read file data directly into one of our outgoing TCP buffers.
        //
        // maxEnqueueSize is the lesser of the other side's MSS and our MSS.
        // (Both of those take MTU into account.)

        uint16_t bytesToSend = fread( buf->data, 1, DataSocket->maxEnqueueSize, sourceFile );

        if ( bytesToSend == 0 ) {

          TcpBuffer::returnXmitBuf( (TcpBuffer *)buf );

          if ( feof( sourceFile ) ) {

            // No more data to read and all of the previous data was already enqueued.
            // (Buf was null when we came in here.)  Set done = 1 so that we break the
            // outer while loop.

            done = 1;

          }
          else if ( ferror( sourceFile ) ) {

            // Error reading the file - abort
            done = 4;

          }

          // Hmm - nothing read, but not end of file or an error.
          // This shouldn't happen, but it's a legal case.

          break;  // Exit the inner while loop

        }

        // We have a buffer and were able to read from the file.  Set the
        // length of the buffer.

        buf->b.dataLen = bytesToSend;
      }


      // If you get to this point we have a buffer with data in it.
      // We have outgoing room - the while loop enforces that.  So enqueue
      // should only fail if the socket closes early.

      if ( DataSocket->enqueue( &buf->b ) ) {
        done = 2;
        break;
      }

      totalBytesSent += buf->b.dataLen;

      PACKET_PROCESS_MULT(5);
      Tcp::drivePackets( );


      if ( !IsStdoutFile ) {
        if ( update == 0 ) {
          gotoxy( x, y );
          cprintf( BytesTransferred_fmt, totalBytesSent );
        }
        update = (update + 1) & 0x0F;
      }

      // We no longer hold a pointer to that buffer.  Set our copy to
      // NULL so that we don't accidentally touch it, and also to
      // indicate that we need to get another one.

      buf = NULL;

    } // End inner while loop



    // If you are here then you might have a buffer with data to send in it
    // already that could not be sent because there was no room in the
    // outgoing ring queue, or you sent everything you had and need to
    // read more.  (The while loop is not guaranteed to run, so you could
    // have pre-read a buffer, not been able to enter the while loop, and
    // gotten here again.)

    if ( buf == NULL ) {

      // Need to read more of the file.  Do this here to take advantage of
      // the time the other packets are on the wire.  If you can't get
      // another outgoing buffer then do nothing; we'll try again the next
      // time around.

      buf = (DataBuf *)TcpBuffer::getXmitBuf( );

      if ( buf != NULL ) {

        uint16_t bytesToSend = fread( buf->data, 1, DataSocket->maxEnqueueSize, sourceFile );

        if ( bytesToSend == 0 ) {

          TcpBuffer::returnXmitBuf( (TcpBuffer *)buf );

          if ( feof( sourceFile ) ) {

            // No more data to read and all of the previous data was already enqueued.
            // (Buf was null when we came in here.)  Set done = 1 so that we break the
            // outer while loop.

            done = 1;

          }
          else if ( ferror( sourceFile ) ) {
            // Error reading the file - abort
            done = 4;
          }

          // Hmm - nothing read, but not end of file or an error.
          // This shouldn't happen, but it's a legal case.

        }
        else {
          buf->b.dataLen = bytesToSend;
        }

      }

    } // end buf == NULL so need to try to read ahead in the file

  } // end main while


  gotoxy( x, y );

  if ( done == 2 ) {
    printf( DataSocketClosed_fmt, DataSocket->getCloseReason( ) );
  }
  else if ( done == 3 ) {
    puts( XferAborted_msg );
  }
  else if ( done == 4 ) {
    puts( "Xfer aborted due to filesystem error" );
  }


  fclose( sourceFile );

  closeAndFreeDataSocket( );

  uint32_t elapsed = Timer_diff( startTicks, TIMER_GET_CURRENT( ) ) * TIMER_TICK_LEN;

  uint32_t rate = computeRate( totalBytesSent, elapsed );

  printf( "%lu bytes sent in %lu.%03lu seconds (%lu.%03lu KBytes/sec)\n",
          totalBytesSent, (elapsed/1000), (elapsed%1000), (rate/1024), (rate%1024) );

  return ( done == 1 ? 0 : -1 );
}



// Stolen from Turbo C++ help screen

char *current_directory(char *path) {

  #if defined ( __WATCOMC__ ) || defined ( __WATCOM_CPLUSPLUS__ )
  getcwd( path, PATH_MAX + 1);
  #else
  path[0] = 'A' + getdisk();    /* replace X with current drive letter */
  path[1] = ':';
  path[2] = '\\';
  getcurdir(0, path+3);  /* fill rest of string with current directory */
  #endif
  
  return(path);
}



/* Ye olde way ...

   char DosChars[] = "!@#$%^&()-_{}`'~";

   bool isValidDosChar( char c ) {

     if ( isalnum( c ) || (c>127) ) return true;

     for ( uint16_t i=0; i < 16; i++ ) {
       if ( c == DosChars[i] ) return true;
     }

     return false;
   }


   Ye better way - a pre-computed table that just reduces isValidDosChar
   to a byte read from a table and some bit shifting.

   Here is the code to generate the table:

   for ( int i=0; i < 256; i++ ) {
     if ( isValidDosChar(i) ) {
       int mapByte = i / 8;
       int mapBit = i % 8;
       map[mapByte] = map[mapByte] | (1<<mapBit);
     }
   }

*/

static uint8_t DosCharMap[] = {
  0x00, 0x00, 0x00, 0x00, 0xFA, 0x23, 0xFF, 0x03,
  0xFF, 0xFF, 0xFF, 0xC7, 0xFF, 0xFF, 0xFF, 0x6F,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

inline bool isValidDosChar( char c ) {
  return ( DosCharMap[c>>3] & (1 << (c & 0x7)) );
}



bool isValidDosFilename( const char *filename ) {

  if (filename==NULL) return false;

  uint16_t len = strlen(filename);

  if ( len == 0 ) return false;

  if ( !isValidDosChar( filename[0] ) ) return false;

  uint16_t i;
  for ( i=1; (i<8) && (i<len) ; i++ ) {
    if ( filename[i] == '.' ) break;
    if ( !isValidDosChar( filename[i] ) ) return false;
  }

  if ( i == len ) return true;

  if ( filename[i] != '.' ) return false;

  i++;
  uint16_t j;
  for ( j=0; (j+i) < len; j++ ) {
    if ( !isValidDosChar( filename[j+i] ) ) return false;
  }

  if ( j > 3 ) return false;

  return true;
}





// If there is a full line of input in the input buffer:
//
// - return a copy of the line in target
// - adjust the input buffer to remove the line
//
// Side effects: to keep from redundantly searching, store the index
// of the last char searched in inBufSearchIndex.
//
// If the server sends a line that is too long the caller gets a partial
// line back.  It is the caller's problem to resync afterwards.

int8_t getLineFromInBuf( char *target, int16_t targetLen ) {

  int i;
  for ( i=inBufSearchIndex; (i < (inBufIndex-1)) && (i < (targetLen-1)); i++ ) {

    if ( inBuf[i] == '\r' && inBuf[i+1] == '\n' ) {
      // Found delimiter
      memcpy( target, inBuf, i );
      target[i] = 0;

      memmove( inBuf, inBuf+i+2, (inBufIndex - (i+2)) );
      inBufIndex = inBufIndex - (i+2);
      inBufSearchIndex = 0;
      return 1;
    }
  }

  if ( i < (targetLen-1) ) {
    inBufSearchIndex = i;
    return 0;
  }
  else {
    // Line too long - Should never happen, but it did.
    // Return what we can.  The caller has to deal with problems
    // that arise later as a result.
    memcpy( target, inBuf, targetLen-1 );
    target[targetLen-1] = 0;

    memmove( inBuf, inBuf+(targetLen-1), (inBufIndex - (targetLen-1)) );
    inBufIndex = inBufIndex - (targetLen-1);
    inBufSearchIndex = 0;
    return -1;
  }

}





// Are STDIN and STDOUT the console or redirected?


void probeStdinStdout( void ) {

  union REGS inregs, outregs;

  inregs.x.ax = 0x4400;

  inregs.x.bx = 0;
  intdos( &inregs, &outregs );


  if ( outregs.x.cflag == 0 ) {
    if ( (outregs.x.dx & 0x0080) == 0 ) {
      IsStdinFile = true;
    }
  }

  inregs.x.bx = 1;
  intdos( &inregs, &outregs );

  if ( outregs.x.cflag == 0 ) {
    if ( (outregs.x.dx & 0x0080) == 0 ) {
      IsStdoutFile = true;
    }
  }

}



// readStin
//
// Reads input from STDIN.  This is used when STDIN is a redirected file.
//
// Returns 0 if everything is normal
// Returns -1 if the user hits Ctrl-Break or EOF is hit

int readStdin( char *buffer, int bufLen ) {

  uint16_t bufferIndex = 0;

  while ( 1 ) {

    if ( CtrlBreakDetected ) {
      // Partial input!
      buffer[ bufferIndex ] = 0;
      return -1;
    }

    int c = getchar( );

    if ( c == EOF ) {
      buffer[ bufferIndex ] = 0;
      return -1;
    }

    if ( c == '\n' ) {
      buffer[ bufferIndex ] = 0;
      puts( buffer );
      break;
    }
    else if ( ((c > 31) && ( c < 127 )) || (c>127) ) {
      if ( bufferIndex < (bufLen-1) ) {
        buffer[ bufferIndex ] = c;
        bufferIndex++;
      }
    }

  }

  return 0;
}



// drawCursor
//
// Used by readConsole.  It is assumed that there is a prompt that we don't
// want to allow the user to backspace over.  So the input parameters
// include the starting position as well as the offset from the start.

void drawCursor( int startX, int startY, int offset ) {
  startX = startX + offset;
  if ( startX > (ScreenCols-1) ) {
    startX = startX - ScreenCols;
    startY++;
  }
  gotoxy( startX, startY );
}


void clearInputArea( int startX, int startY, bool spanningTwoLines ) {

  gotoxy( startX, startY );

  // Write enough to blank out the line without causing scrolling.
  int chars = ScreenCols - 1 - startX;
  if ( spanningTwoLines ) chars = chars + ScreenCols;

  while (chars) {
    if (chars > 40) {
      cputs( Spaces );
      chars = chars - 40;
    } else {
      cputs( Spaces + (40-chars) );
      chars = 0;
    }
  }

}




// readConsole
//
// Reads input from the keyboard.  This is designed to be used interactively.
//
// Returns 0 if everything is normal
// Returns -1 if the user hits Ctrl-Break or Ctrl-C

int readConsole( char *buffer, int bufLen, bool enableCmdEdit ) {

  fflush( stdout );

  bool insertMode = true;

  int recallOffset = 0;

  uint16_t bufferLen = 0;   // How many chars are in the buffer
  uint16_t bufferIdx = 0;   // Current cursor position in the buffer for editing

  // User reported bug: zero the buffer because the server or TCP/IP can break
  // out of this routine, and without the buffer zeroed it would look like the
  // user entered the previous input again.
  *buffer = 0;


  // Remember the row and column that we start at so that we can preserve the prompt.
  int startY = wherey( );
  int startX = wherex( );

  // Remember if we roll into a second line so we can refresh/erase it if needed.
  bool spanningTwoLines = false;

  while ( 1 ) {

    // Not full blown packet handling, but better than nothing.  This should allow
    // us to respond to ping requests (ICMP) receive TCP packets and send ACKS,
    // but we won't attempt to process until we return back.

    PACKET_PROCESS_SINGLE;
    Arp::driveArp( );
    Tcp::drivePackets( );

    // Cheap hack - if the user has not entered data and the server sent something
    // then return early to process it.

    if ( (bufferLen == 0) && (ControlSocket->recvDataWaiting() == true) ) return 0;


    if ( CtrlBreakDetected ) {
      // Partial input!
      buffer[ bufferLen ] = 0;
      return -1;
    }


    if ( bioskey(1) ) {

      uint16_t key = bioskey(0);

      if ( (key & 0xff) == 0 ) {

        // Function key
        uint8_t fkey = key >> 8;

        if ( !enableCmdEdit ) {
          // Sleazy, but effective - supress command editing
          fkey = 0;
        }

        switch ( fkey ) {

          case 72: { // Up

            if ( recallOffset == 0 ) {
              // If we were entering a new command save what we have so far
              buffer[ bufferLen ] = 0;
              strcpy( previousCommands[previousCommandIndex], buffer );
            }

            recallOffset++;
            if ( recallOffset == PREVIOUS_COMMANDS ) {
              // Too far - complain
              recallOffset = PREVIOUS_COMMANDS - 1;
              complain( );
            }

            int targetCommand = previousCommandIndex - recallOffset;
            if ( targetCommand < 0 ) targetCommand += PREVIOUS_COMMANDS;

            strcpy( buffer, previousCommands[targetCommand] );
            bufferLen = strlen( buffer );
            bufferIdx = bufferLen;

            clearInputArea( startX, startY, spanningTwoLines );
            gotoxy( startX, startY );
            cputs( buffer );


            // If the length of this command plus the start location after the
            // user prompt is greater than the current line, we know we wrapped.
            //
            // In addition, if we had started on the last row of the screen and
            // we are still on the last line then we caused the screen to scroll
            // too and our starting position is now back a line.

            if ( bufferIdx + startX > (ScreenCols-1) ) {
              spanningTwoLines = true;
              if ( (startY == (ScreenRows-1)) && (wherey( ) == (ScreenRows-1)) ) {
                startY--;
              }
            }

            break;
          }

          case 80: { // Down

            if ( recallOffset > 0 ) {
              recallOffset--;
            }
            else {
              complain( );
            }

            int targetCommand = previousCommandIndex - recallOffset;
            if ( targetCommand < 0 ) targetCommand += PREVIOUS_COMMANDS;

            strcpy( buffer, previousCommands[targetCommand] );
            bufferLen = strlen( buffer );
            bufferIdx = bufferLen;

            clearInputArea( startX, startY, spanningTwoLines );
            gotoxy( startX, startY );
            cputs( buffer );


            // Same logic as above - detect spanning two rows and scrolling.

            if ( bufferIdx + startX > (ScreenCols-1) ) {
              spanningTwoLines = true;
              if ( (startY == (ScreenRows-1)) && (wherey( ) == (ScreenRows-1)) ) {
                startY--;
              }
            }

            break;
          }

          case 75: { // Left
            if ( bufferIdx > 0 ) { bufferIdx--; } else { complain( ); }
            break;
          }

          case 77: { // Right
            if ( bufferIdx < bufferLen ) { bufferIdx++; } else { complain( ); }
            break;
          }

          case 82: { // Insert

            insertMode = !insertMode;
            if ( insertMode ) {
              sound(500); delay(50); sound(750); delay(50); nosound( );
            }
            else {
              // Not really complaining - just indicating back to overstrike
              complain( );
            }

            break;
          }


          case 83: { // Delete

            // Has to be on an existing char
            if ( bufferLen > 0 && bufferIdx < bufferLen ) {
              memmove( buffer+bufferIdx, buffer+bufferIdx+1, (bufferLen-bufferIdx) );
              bufferLen--;
              buffer[bufferLen] = 0;

              // Update only the part we need to
              drawCursor( startX, startY, bufferIdx );
              cputs( buffer + bufferIdx );
              putch( ' ' );
	      gotoxy( startX, startY );

            }
            else {
              complain( );
            }

            break;
          }

          case 71: { // Home
            bufferIdx = 0;
            break;
          }

          case 79: { // End
            bufferIdx = bufferLen;
            break;
          }

        } // end switch


        // All actions redraw the cursor at the end.
        drawCursor( startX, startY, bufferIdx );

      }
      else {

        // Normal key
        int c = key & 0xff;

        if ( c == 13 ) {
          // Accept the enter key anywhere in the line
          buffer[ bufferLen ] = 0;
          puts( "" );
          break;
        }

        else if ( c == 27 ) {
          // Wipe out the current input
          bufferIdx = bufferLen = 0;
          buffer[ 0 ] = 0;
          clearInputArea( startX, startY, spanningTwoLines );
          gotoxy( startX, startY );
        }

        else if ( ((c > 31) && ( c < 127 )) || (c>127) ) {

          // Are we adding to the end of the line?  (This case is easy.)

          if ( bufferIdx == bufferLen ) {

            if ( bufferLen < (bufLen-1) ) {
              buffer[ bufferLen++ ] = c;
              bufferIdx++;

              if ( ClientState != UserOkSendPass ) {
                putch( c );
              }
              else {
                putch( '*' );
              }

            }
            else {
              complain( );
            }

          }
          else {

            // In the middle of the line - insert or replace?

            if ( insertMode ) {

              // Room to insert?
              if ( bufferLen < (bufLen-1) ) {
                memmove( buffer+bufferIdx+1, buffer+bufferIdx, bufferLen-bufferIdx+1 );
                buffer[bufferIdx] = c;
                bufferLen++;
                buffer[bufferLen] = 0;

                // Redisplay the line starting from the cursor position.
                // We are adding a char so no need to clear anything.
                cputs( buffer + bufferIdx );

                bufferIdx++;

              }
              else {
                complain( );
              }

            }
            else {
              buffer[ bufferIdx ] = c;
              bufferIdx++;

              if ( ClientState != UserOkSendPass ) {
                putch( c );
              }
              else {
                putch( '*' );
              }
            }

          } // end editing in the middle of the line


          // Detect if we moved down a line and/or scrolled the screen

          if ( wherex( ) == 0 ) {
            spanningTwoLines = true;
            if ( startY == (ScreenRows-1) ) {
              // Just wrapped the line.  If we were at the bottom already then the screen scrolled too.
              startY--;
            }
          }

          drawCursor( startX, startY, bufferIdx );

        }

        else if ( c == 8 ) {

          // If pressed at the end of the line eat the last char. If
          // pressed in the middle of the line slide remaining chars
          // back.

          if ( bufferIdx ) {

            if ( bufferIdx == bufferLen ) {

              // Easy case - at the end of the line

              bufferIdx--;
              bufferLen--;

              drawCursor( startX, startY, bufferIdx );
              putch( ' ' );
              drawCursor( startX, startY, bufferIdx );

            }
            else {
              memmove( buffer+bufferIdx-1, buffer+bufferIdx, (bufferLen-bufferIdx) );
              bufferIdx--;
              bufferLen--;
              buffer[bufferLen] = 0;

              drawCursor( startX, startY, bufferIdx );
              cputs( buffer + bufferIdx );
              putch( ' ' );
              drawCursor( startX, startY, bufferIdx );
            }

          }
          else {
            complain( );
          }


        }
        else if ( c == 3 ) {
          CtrlBreakDetected = 1;
          buffer[ bufferLen ] = 0;
          return -1;
        }

      }

    } // end if key pressed


    // Be nice in emulated environments.  Will cause no harm to plain old DOS.

    #ifdef SLEEP_CALLS
    SLEEP( );
    #endif

  }

  if ( enableCmdEdit ) {

    // Every time we get a new command add it to the command buffer.
    // The buffer is circular so that we don't have to keep copying
    // strings around.

    strcpy( previousCommands[previousCommandIndex], buffer );
    previousCommandIndex++;
    if ( previousCommandIndex == PREVIOUS_COMMANDS ) previousCommandIndex = 0;
  }

  return 0;
}


