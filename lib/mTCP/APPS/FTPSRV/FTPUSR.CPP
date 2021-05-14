
/*

   mTCP FtpUsr.cpp
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


   Description: Code for FTP server user file management

   Changes:

   2011-05-27: Initial release as open source software

*/




#include <stdio.h>
#include <string.h>
#include <sys/stat.h>


#include "utils.h"
#include "ftpusr.h"




extern int isDrivePrefixPresent( const char *userPath );
extern int isDriveInValidTable( int driveLetter );
extern int normalizeDir( char *buffer, int bufferLen );
extern int convertToDosPath( char *buffer_p );

extern void addToScreen( int writeLog, const char *fmt, ... );




// Ftp User design
//
// The general idea is to store all of the ftp users in a file and to leave
// the file open for reading while the program is running.  When we need to
// find a user we'll do a linear scan through the file.
//
// It is quite probable that the file will be fully resident in memory so
// that no disk I/O actually occurs.  If the disk I/O does become a problem
// implement a small cache of the 5 most commonly used users, and use an LRU
// algorithm.  That will cover most of our needs quite easily.



FILE *FtpUser::userFile = NULL;




// After each user is read their fields are sanity checked to ensure nothing
// too horrible is going on.

int FtpUser::sanityCheck( char *errMsgOut ) {

  char tmp[USR_MAX_PATH_LENGTH];
  uint8_t isSandbox = 0;
  int tmpLen;


  // If the user has a sandbox:
  //
  //  - It must start with /DRIVE_X/
  //  - The drive letter must be valid as per our startup scan
  //  - The path can not have syntax errors in it
  //  - The directory (or drive root) has to exist

  if ( strcmp( sandbox, "[NONE]" ) != 0 ) {

    isSandbox++;

    // Be nice and remove the trailing slash if they added one by accident
    // and it is not the root of a drive.  They might have specified a drive
    // root without the trailing slash; we don't bother correcting that.

    tmpLen = strlen( sandbox );
    if ( tmpLen > 9  && sandbox[tmpLen-1] == '/' ) {
      sandbox[tmpLen-1] = 0;
    }

    if ( !isDrivePrefixPresent( sandbox ) ) {
      strcpy( errMsgOut, "sandbox field should start with /DRIVE_x/" );
      return 1;
    }

    if ( !isDriveInValidTable( sandbox[7] ) ) {
      strcpy( errMsgOut, "bad drive letter in sandbox field" );
      return 1;
    }

    if ( normalizeDir( sandbox, USR_MAX_PATH_LENGTH ) ) {
      strcpy( errMsgOut, "sandbox path is not valid" );
      return 1;
    }

    strcpy( tmp, sandbox );
    convertToDosPath( tmp );

    // Try to stat the sandbox
    struct stat statbuf;
    int rc = stat( tmp, &statbuf );

    if ( (rc != 0) || ( (rc == 0) && (S_ISDIR(statbuf.st_mode) == 0) ) ) {
      strcpy( errMsgOut, "sandbox is not a directory" );
      return 1;
    }

  }


  // Ok, now if there is an upload dir it has to be valid too.
  // For sandbox users the upload dir is appended to the sandbox dir.
  // For non sandbox users if an upload dir is specified it has to be
  // fully qualified.

  if ( strcmp( uploaddir, "[ANY]" ) != 0 ) {

    if ( uploaddir[0] != '/' ) {
      strcpy( errMsgOut, "uploaddir needs to start with a '/'" );
      return 1;
    }

    if ( normalizeDir( uploaddir, USR_MAX_PATH_LENGTH ) ) {
      strcpy( errMsgOut, "uploaddir field is not valid" );
      return 1;
    }

    tmp[0] = 0;
    if ( isSandbox ) {
      strcpy( tmp, sandbox );
    }

    if ( strlen(tmp) + strlen(uploaddir) >= USR_MAX_PATH_LENGTH ) {
      strcpy( errMsgOut, "combined sandbox and incoming dirs too long" );
      return 1;
    }

    strcat( tmp, uploaddir );
    convertToDosPath( tmp );

    // Try to stat the upload directory
    struct stat statbuf;
    int rc = stat( tmp, &statbuf );

    if ( (rc != 0) || ( (rc == 0) && (S_ISDIR(statbuf.st_mode) == 0) ) ) {
      strcpy( errMsgOut, "incoming is not a directory" );
      return 1;
    }

  }

  return 0;
}



int FtpUser::init( const char *userFilename ) {

  addToScreen( 1, "Opening password file at %s\n", userFilename );

  userFile = fopen( userFilename, "r" );
  if ( userFile == NULL ) {
    addToScreen( 1, "  Error reading user file\n" );
    return 1;
  }


  // Scan file for a sanity check

  char lineBuffer[256];
  int lineNo = 0, errors = 0;
  while ( 1 ) {
    
    if ( feof( userFile ) ) break;
    
    char *rc = fgets( lineBuffer, 255, userFile );
    lineNo++;
    if ( rc == NULL ) break; // EOF or read error

    // Wipe out trailing newline char if it is there.
    int l = strlen( lineBuffer );
    if ( lineBuffer[l] == '\n' ) lineBuffer[l] = 0;
    
    l = strlen( lineBuffer );
    if ( l == 0 || ( (l) && (lineBuffer[0] == '#') ) ) continue; // Comment or blank line
    
    char tmpUserName[ USERNAME_LEN ];
    char *nextTokenPtr = Utils::getNextToken( lineBuffer, tmpUserName, USERNAME_LEN );
    if ( *tmpUserName == 0 ) continue; // Blank line


    // Try to create the user rec and sanity check it.

    FtpUser buffer;
    char errMsg[40];

    switch ( createUserRec( lineBuffer, &buffer ) ) {

      case 0: { 
        // Ok, sanity check it
        if ( buffer.sanityCheck( errMsg ) ) {
          addToScreen( 1, "  Error on line: %d, Error: %s\n", lineNo, errMsg );
          errors++;
        }
        break;
      }
      case 1: {
        addToScreen( 1, "  Missing fields on line: %d\n", lineNo );
        errors++;
        break;
      }
      case 2: {
        addToScreen( 1, "  Unrecognized permissions text on line: %d\n", lineNo );
        errors++;
        break;
      }

    } // end switch.

    // addToScreen( 9, "%-10s %s %s\n", buffer.userName, buffer.sandbox, buffer.uploaddir );
  }

  if ( errors ) {
    addToScreen( 1, "  Total errors found: %d\n", errors );
    return 1;
  }

  addToScreen( 1, "  Password file looks reasonable.\n\n" );
  return 0;
}




// Returns 0 if not found, 1 if found

int FtpUser::getUserRec( const char *targetUser, FtpUser *buffer ) {

  // Position to beginning
  fseek( userFile, 0, SEEK_SET );

  // Start looking for our user

  char lineBuffer[256];
  while ( 1 ) {

    if ( feof( userFile ) ) break;

    char *rc = fgets( lineBuffer, 255, userFile );
    if ( rc == NULL ) break; // EOF or error

    // Wipe out trailing newline char if it is there.
    int l = strlen( lineBuffer );
    if ( lineBuffer[l] == '\n' ) lineBuffer[l] = 0;

    l = strlen( lineBuffer );
    if ( l == 0 || ( (l) && (lineBuffer[0] == '#') ) ) continue; // Comment or blank line

    char tmpUserName[ USERNAME_LEN ];
    char *nextTokenPtr = Utils::getNextToken( lineBuffer, tmpUserName, USERNAME_LEN );
    if ( *tmpUserName == 0 ) continue; // Blank line

    if ( stricmp( targetUser, tmpUserName ) == 0 ) {
      if ( createUserRec( lineBuffer, buffer ) == 0 ) {
        return 1; // Return found
      }
      else {
        return 0; // Was found, but in error - return not found
      }

    }

  }

  return 0;

}




// Returns 0 if successful, 1 if missing a field, 2 if unrecognized
// permission field.

int FtpUser::createUserRec( char *input, FtpUser *buffer ) {

  // Ensure the target buffer is clean
  buffer->wipe( );

  // If the input is null for some reason exit early.
  if ( input[0] == 0 ) return 1;

  char *nextTokenPtr = Utils::getNextToken( input, buffer->userName, USERNAME_LEN );
  if ( nextTokenPtr == NULL || buffer->userName[0] == 0 ) return 1;

  nextTokenPtr = Utils::getNextToken( nextTokenPtr, buffer->userPass, USERPASS_LEN );
  if ( nextTokenPtr == NULL || buffer->userPass[0] == 0 ) return 1;

  if ( stricmp( buffer->userPass, "[EMAIL]" ) == 0 ) {
    strupr( buffer->userPass );
  }


  // Read the sandbox.  If it is [NONE] we will convert it to a null string
  // at signon time.  Otherwise we will sanity check it at server init time.
  // Sanity checking means it has to exist.  If it exists it has to start with
  // /DRIVE_X/ and it does not end with a slash.

  nextTokenPtr = Utils::getNextToken( nextTokenPtr, buffer->sandbox, USR_MAX_PATH_LENGTH );
  if ( nextTokenPtr == NULL || buffer->sandbox[0] == 0 ) return 1;
  strupr( buffer->sandbox );


  // Read the incoming directory.  If it is [ANY] it is unrestricted.  Otherwise,
  // it should start and end with a /.

  nextTokenPtr = Utils::getNextToken( nextTokenPtr, buffer->uploaddir, USR_MAX_PATH_LENGTH );
  if ( buffer->uploaddir[0] == 0 ) return 1;
  strupr( buffer->uploaddir );



  // Read permissions

  char tmpPermToken[10];

  // Permissions are optional.  If you don't give a user any explicit
  // permissions then they are only to do reads and other non-filesystem
  // altering operations.

  while ( nextTokenPtr != NULL ) {

    nextTokenPtr = Utils::getNextToken( nextTokenPtr, tmpPermToken, 10 );
    if ( tmpPermToken[0] == 0 ) break;

    strupr( tmpPermToken );

    if ( strcmp( tmpPermToken, "ALL" ) == 0 ) {
      buffer->cmd_DELE = 1; buffer->cmd_MKD = 1; buffer->cmd_RMD = 1; buffer->cmd_RNFR = 1;
      buffer->cmd_STOR = 1; buffer->cmd_APPE = 1; buffer->cmd_STOU = 1;
    }
    else if ( strcmp( tmpPermToken, "DELE" ) == 0 ) { buffer->cmd_DELE = 1; }
    else if ( strcmp( tmpPermToken, "MKD" ) == 0 )  { buffer->cmd_MKD = 1; }
    else if ( strcmp( tmpPermToken, "RMD" ) == 0 )  { buffer->cmd_RMD = 1; }
    else if ( strcmp( tmpPermToken, "RNFR" ) == 0 ) { buffer->cmd_RNFR = 1; }
    else if ( strcmp( tmpPermToken, "STOR" ) == 0 ) { buffer->cmd_STOR = 1; }
    else if ( strcmp( tmpPermToken, "APPE" ) == 0 ) { buffer->cmd_APPE = 1; }
    else if ( strcmp( tmpPermToken, "STOU" ) == 0 ) { buffer->cmd_STOU = 1; }
    else {
      return 2;
    }

  } // end while

  return 0;
}
