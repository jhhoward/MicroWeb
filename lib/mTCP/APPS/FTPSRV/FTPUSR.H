
/*

   mTCP FtpUsr.h
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


   Description: Data structures for FTP server users

   Changes:

   2011-05-27: Initial release as open source software

*/



#ifndef _FTP_USER_H
#define _FTP_USER_H

#include "ftpsrv.h"


class FtpUser {

  public:

    char userName[USERNAME_LEN];
    char userPass[USERPASS_LEN];
    char sandbox[USR_MAX_PATH_LENGTH];
    char uploaddir[USR_MAX_PATH_LENGTH];

    uint8_t cmd_DELE;
    uint8_t cmd_MKD;
    uint8_t cmd_RMD;
    uint8_t cmd_RNFR;
    uint8_t cmd_STOR;
    uint8_t cmd_APPE;
    uint8_t cmd_STOU;

    inline void wipe( void ) { memset( this, 0, sizeof(FtpUser) ); }
    int sanityCheck( char *errMsgOut );

    static FILE *userFile;

    static int init( const char *userFilename );
    static int getUserRec( const char *targetUser, FtpUser *buffer );
    static int createUserRec( char *input, FtpUser *buffer );


};


#endif
