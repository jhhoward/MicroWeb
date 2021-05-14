/*

   mTCP FtpSrv.h
   Copyright (C) 2011-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: FTP Server defines and some inlines

   Changes:

   2011-10-01: Initial version for some restructuring
   2014-05-25: Mild restructuring and cleanup

*/

#ifndef _FTPSRV_H
#define _FTPSRV_H



// Application configuration options


#define FTP_MAX_CLIENTS (10)       // Maximum number of concurrent clients

#define COMMAND_MAX_LEN (20)
#define USERNAME_LEN    (10)
#define USERPASS_LEN    (10)

#define INPUTBUFFER_SIZE   (120)   // Max length for a command line from a client
#define OUTPUTBUFFER_SIZE (1024)   // Max bytes we can build up for client output


// Defaults, but can be changed by config parameter at run time
#define FILEBUFFER_SIZE   (8192)
#define DATA_RCV_BUF_SIZE (8192)

// We need to be careful not to overflow the client output buffer.
// With a 1K client output buffer 800 bytes for MOTD is plenty.
#define MOTD_MAX_SIZE (OUTPUTBUFFER_SIZE-200)



// DOS limits
//   drive letter + colon + path + null = 67
//   above, plus filename = 79
//
#define DOS_MAX_PATH_LENGTH     (67)
#define DOS_MAX_PATHFILE_LENGTH (79)

// For user paths we express the drive letter as /DRIVE_X instead of X: .
// That is longer, so give them longer paths
//
#define USR_MAX_PATH_LENGTH     (67 + 6)
#define USR_MAX_PATHFILE_LENGTH (79 + 6)

#define USR_MAX_PATH_LENGTH_PADDED     (67 + 20)
#define USR_MAX_PATHFILE_LENGTH_PADDED (79 + 20)



// Screen handling

// Includes the separator line
#define STATUS_LINES (2)





// Data structures used for interpreting directory entries

typedef union {
  unsigned short us;
  struct {
    unsigned short twosecs : 5; /* seconds / 2 */
    unsigned short minutes : 6; /* minutes (0,59) */
    unsigned short hours : 5;   /* hours (0,23) */
  } fields;
} ftime_t;

typedef union {
  unsigned short us;
  struct {
    unsigned short day : 5;   /* day (1,31) */
    unsigned short month : 4; /* month (1,12) */
    unsigned short year : 7;  /* 0 is 1980 */
  } fields;
} fdate_t;

static char *Months[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};




#endif
