
/*

   mTCP Session.cpp
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


   Description: Session handling code for IRCjr

   Changes:

   2011-05-27: Initial release as open source software
   2012-11-21: Add code to interpret mIRC color codes
   2013-02-15: Use halloc to allow for larger backscroll buffers;
               132 col awareness
   2013-03-23: Combine printing and logging in one call

*/



#include <ctype.h>
#include <dos.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "session.h"
#include "screen.h"




Session *Session::activeSessionList[MAX_SESSIONS];
uint16_t Session::activeSessions = 0;




int16_t Session::init( const char *name_p, uint16_t bufferRows ) {

  // Add one extra row for the separator line.  Not used if we don't have a
  // backscroll buffer
  bufferRows++;

  // Fixme: Consolidate these two checks.
  if ( bufferRows > 400 ) bufferRows = 400;

  uint32_t bufferAllocSize = (uint32_t)(bufferRows) * (uint32_t)(Screen::getScreenCols() << 1);
  if ( bufferAllocSize >= 126*1024ul ) {
    return -1;
  }

  uint16_t *tmpVirtBuffer = (uint16_t *)halloc( bufferAllocSize, 1 );
  if ( tmpVirtBuffer == NULL ) {
    return -1;
  }

  strncpy( name, name_p, IRCNICK_MAX_LEN );
  name[IRCNICK_MAX_LEN - 1] = 0;

  virtBuffer = tmpVirtBuffer;
  virtBufferRows = bufferRows;
  virtBufferSize = bufferAllocSize;

  // Clear the buffer
  uint16_t *tmp = virtBuffer;
  uint16_t cols = Screen::getScreenCols();
  for ( uint16_t i = 0; i < virtBufferRows; i++ ) {
    fillUsingWord( tmp, (7<<8|32), cols );
    addToPtr( tmp, ( cols << 1), uint16_t * );
  }


  // fillUsingWord( virtBuffer, (7<<8|32), (bufferAllocSize>>1) );

  output_x = output_y = 0;
  backScrollLines = virtBufferRows - Screen::getOutputRows() - 1;

  backScrollOffset = 0;

  was_updated = false;

  resetColorAttrs( );

  logging = false;

  return 0;
}


void Session::destroy( void ) {
  if ( virtBuffer ) { hfree( virtBuffer ); }
  closeLogFile( );
  memset( this, 0, sizeof( Session ) );
}





// Find a session by pointer
//
int16_t Session::getSessionIndex( Session *target ) {
  for ( uint8_t i=0; i < activeSessions; i++ ) {
    if ( activeSessionList[i] == target ) return i;
  }
  return -1;
}


// Find a session by name
//
int16_t Session::getSessionIndex( const char *name_p ) {
  for ( uint8_t i=0; i < activeSessions; i++ ) {
    if ( stricmp( name_p, activeSessionList[i]->name ) == 0 ) return i;
  }
  return -1;
}



// createAndMakeActive
//
// Ensures a session with the same name doesn't exist already, allocates the
// storage for a new session, and adds it to the active list.  If anything
// goes wrong you get a NULL back.

Session *Session::createAndMakeActive( const char *name_p, uint16_t bufferRows, bool startLogging ) {

  if ( activeSessions >= MAX_SESSIONS ) return NULL;

  // Ensure it's not active yet
  if ( getSessionIndex( name_p ) != -1 ) return NULL;

  Session *tmpSession = (Session *)malloc( sizeof(Session) );
  if ( tmpSession == NULL ) return NULL;

  if ( tmpSession->init( name_p, bufferRows ) ) {
    free( tmpSession );
    return NULL;
  }

  activeSessionList[activeSessions] = tmpSession;
  activeSessions++;

  if ( startLogging ) tmpSession->loggingToggle( );

  return tmpSession;
}


int16_t Session::removeActiveSession( Session *target ) {

  int8_t index = getSessionIndex( target );
  if ( index == -1 ) return -1;

  // Slide everything down
  for ( int8_t i=index; i < activeSessions-1; i++ ) {
    activeSessionList[i] = activeSessionList[i+1];
  }
  activeSessions--;

  target->destroy( );
  free( target );

  return 0;
}




// For an MDA adapter the following is possible:
//
// 0x01  Underlined white on black
// 0x09  Bright underlined white on black
// 0x07  White on black
// 0x0F  Bright White on black
// 0x70  Black on white


uint8_t Session::computeAttr( void ) {

  // Default is the current userAttr
  uint8_t attr = userAttr;


  // MDA: Figure out if we are underlined or normal
  if ( Screen::isColorCard() == false ) {
    if ( userAttrUnderline ) {
      attr = 0x01;
    }
    else {
      attr = 0x07;
    }
  }

  // Reverse overrides foreground and background
  // Reverse has priority over underline and other attributes except bold
  if ( userAttrReverse ) attr = 0x70;

  // Apply bold if possible.
  if ( userAttrBold ) attr = attr | 0x08;

  return attr;
}





// mIRC color code parsing
//
// Color codes start with a ^C (ASCII 3).  If a foreground color is present
// it can be one or two digits with a value from 0 to 15.  Because two digits
// are parsed the value can be larger, but that behavior is undefined.  If a
// background color is present it will be separated from the foreground color
// by a comma, and then it will be one or two digits long.
//
// If the text you are coloring starts with a digit then you must use the two
// digit color codes.  The sending client is responsible for doing that; we
// just need to ensure that we stop parsing colors after the second digit.
//
// A ^C by itself means turn coloring off.

uint8_t mIRCtoCGAMap[] = {

      // mIRC color  -> Rendered as on CGA

  15, // White       -> White
   0, // Black       -> Black
   1, // Navy Blue   -> Blue
   2, // Green       -> Dark Green
   4, // Red         -> Red
   6, // Brown       -> Brown
   5, // Purple      -> Magenta     (Not perfect)
  12, // Orange      -> light red
  14, // Yellow      -> Yellow
  10, // Light Green -> Light Green
   3, // Teal        -> Dark Cyan
  11, // Light Cyan  -> Light Cyan
   9, // Light Blue  -> Light Blue
  13, // Pink        -> Light Magenta
   8, // Grey        -> Grey
   7  // Light Grey  -> Light Grey

};



// parseColorCode - parse mIRC color codes
//
// On entry src is pointing to a ^C (ASCII 3) that is the start of the color
// code string.
//
// Returns the number of bytes that were consumed while parsing the color code.
// That is guaranteed to be a minimum of 1.

uint16_t Session::parseColorCode( const char *src ) {

  const char * str = src;

  // The state enum tells us what we are expecting to see next in the data.

  enum State_t { Foreground1, Foreground2, Comma, Background1, Background2, Done };

  State_t state = Foreground1;


  // We use value 255 as "uninitialized"

  uint8_t newForeground = 255;
  uint8_t newBackground = 255;


  str++; // Skip the first byte (^C)


  while ( (*str) && (state != Done) ) {

    switch ( state ) {

      case Foreground1: {
        if ( isdigit( *str ) ) {
          newForeground = (*str - '0');
          state = Foreground2;
        }
        else {
          state = Done;
        }
        break;
      }
      case Foreground2: {
        if ( *str == ',' ) {
          state = Background1;
        }
        else if ( !isdigit( *str ) ) {
          state = Done;
        }
        else {
          newForeground = newForeground * 10;
          newForeground = newForeground + (*str - '0');
          state = Comma;
        }
        break;
      }
      case Comma: {
        if ( *str != ',' ) {
          state = Done;
        }
        else {
          state = Background1;
        }
        break;
      }
      case Background1: {
        if ( isdigit( *str ) ) {
          newBackground = (*str - '0');
          state = Background2;
        }
        else {
          // Could interpret this as an error, or could just say that they
          // intended to color the comma.  Go with the latter.
          state = Done;
          str--;
        }
        break;
      }
      case Background2: {
        if ( isdigit( *str ) ) {
          newBackground = newBackground * 10;
          newBackground = newBackground + (*str - '0');
        }
        else {
          state = Done;
        }
        break;
      }

    } // end switch

    if ( state != Done ) str++;

  } // end while


  // At first we reported parsing errors.  But it is pointless.  Fail silently
  // instead and save the bytes for where it really matters.


  // If somebody punked us with invalid color codes then just make it look
  // like none were received so that we reset back to our normal attribute.

  if (newForeground > 15) newForeground = 255;
  if (newBackground > 15) newBackground = 255;

  if ( Screen::isColorCard() == false ) {
    // Don't bother with a true MDA card - it will make a mess
    newForeground = 255;
    newBackground = 255;
  }

  if ( newForeground == 255 ) {
    // Color code not set, invalid, or a parse error
    userAttr = scNormal;
  }
  else {

    if ( newBackground == 255 ) {
      // Background was inherited and not set; preserve existing one.
      userAttr = (userAttr & 0xf0) | mIRCtoCGAMap[newForeground];
    }
    else {
      userAttr = ( mIRCtoCGAMap[newBackground] << 4 | mIRCtoCGAMap[newForeground] );
    }

  }

  return (str - src);

}





// puts - Add text to a virtual session.
//
// This is the lowest level primitive that is used for adding text to a virtual
// session.  It maintains a flag called was_updated that allows us to quickly
// determine if the physical screen is out of date and needs to be refreshed.
// Other functions (printf) use this function.
//
// On entry we are given color attributes to start with.  If color
// codes are embedded in the string we will change the current attribute
// and run with that.  The userAttr class variable is used to hold the
// current attribute so that if we go into reverse mode we can figure out
// what we were supposed to be when reverse ends.
//
// A new line always starts as "normal".  Changes to the color only last as
// long as that line.  The high level caller (IRC) is responsible for
// resetting the attributes before calling because we really don't know when
// a new line is starting at this level.

void Session::puts( uint8_t attr, const char *str ) {

  // Indicate that we need repainting.
  was_updated = true;


  // We are never going to add more than a few hundred bytes
  // at a time.  Start by normalizing a pointer into the buffer,
  // then convert the normalized pointer back to a far pointer.
  // This avoids the overhead of huge pointer arithmetic on every
  // character.

  uint16_t *current = virtBuffer;
  uint32_t offset = (uint32_t)(output_y) * (Screen::getScreenCols() << 1) + (output_x<<1);
  addToPtr( current, offset, uint16_t * );

  // At this point current pointing at the next location to be written to.
  // (AddToPtr macro normalized the pointer.)


  while ( *str ) {

    if ( *str == 2 ) {

      userAttrBold = !userAttrBold;
      attr = computeAttr( );
      str++;

    }
    else if ( *str == 15 ) {

      // Reset to Normal - turn off everything else
      resetColorAttrs( );
      attr = computeAttr( );
      str++;

    }
    else if ( *str == 17 ) {

      // Can't do much with Fixed
      userAttrFixed = !userAttrFixed;
      str++;

      if ( userAttrFixed ) {
        puts( scLocalMsg, "<fixed font on>");
      }
      else {
        puts( scLocalMsg, "<fixed font off>");
      }

      // Recompute the current screen location
      current = virtBuffer;
      offset = output_y * (Screen::getScreenCols() << 1) + (output_x<<1);
      addToPtr( current, offset, uint16_t * );

    }
    else if ( *str == 18 || *str == 22 ) {

      // Toggle reverse/inverse
      userAttrReverse = !userAttrReverse;
      attr = computeAttr( );
      str++;

    }
    else if ( *str == 29 ) {

      // Ctrl-I for Italics.  Yeah, right
      userAttrItalics = !userAttrItalics;
      str++;

      if ( userAttrItalics ) {
        puts( scLocalMsg, "<italics on>");
      }
      else {
        puts( scLocalMsg, "<italics off>");
      }

      // Recompute the current screen location
      current = virtBuffer;
      offset = output_y * (Screen::getScreenCols() << 1) + (output_x<<1);
      addToPtr( current, offset, uint16_t * );

    }
    else if ( *str == 31 ) {

      // Ctrl-U for Underlined.  Only in mono modes.
      userAttrUnderline = !userAttrUnderline;
      attr = computeAttr( );
      str++;

      if ( Screen::isColorCard() == true ) {


        if ( userAttrUnderline ) {
          puts( scLocalMsg, "<underline on>");
        }
        else {
          puts( scLocalMsg, "<underline off>");
        }

        // Recompute the current screen location
        current = virtBuffer;
        offset = output_y * (Screen::getScreenCols() << 1) + (output_x<<1);
        addToPtr( current, offset, uint16_t * );

      }

    }
    else if ( *str == 0x3 ) {

      str += parseColorCode( str );
      attr = computeAttr( );

    }
    else if ( *str == '\n' ) {

      // Newline processing - drop to the start of the next line

      str++;
      current += Screen::getScreenCols() - output_x;
      output_y++;
      output_x=0;
    }
    else {

      // Normal character: write it and advance one position

      *current = (attr<<8|*str);
      current++;
      str++;
      output_x++;

      // Did we wrap around the right edge?
      if ( output_x == Screen::getScreenCols() ) {
        output_y++;
        output_x=0;
      }
    }

    // If after processing that character we are on a new line then
    // we need to check to see if we wrapped around the end of the buffer,
    // clear the new line, and if we have backscroll capability draw the
    // divider.

    // Fix me: the divider should only be drawn once outside of the loop.


    if ( output_x==0 ) {

      if ( output_y == virtBufferRows ) {
        // Wrapped around the end of the buffer.
        output_y = 0;
        current = virtBuffer;
      }

      // Clear new line.  (It was probably used before.)
      fillUsingWord( current, 0, Screen::getScreenCols() );


      // Put a divider on the next line in case the backscroll buffer
      // is being displayed while it is being modified.

      if ( virtBufferRows > (Screen::getOutputRows() + 1) ) {

        // Tmp is already at the start of the line, but we need to be
        // sure that it wasn't supposed to wrap to the start of the buffer.

        // Skip past open line
        uint16_t *tmp = current + Screen::getScreenCols();

        if ( (output_y + 1) == virtBufferRows ) {
          tmp = (uint16_t *)virtBuffer;
        }

        fillUsingWord( tmp, 0x0FCD, Screen::getScreenCols() );

      }

    }

  } // end for


}




extern bool Timestamp;
extern char *getTimeStr( void );

static char VirtPrintfBuf[1024];

void Session::printf( uint8_t options, uint8_t attr, char *fmt, ... ) {

  va_list ap;
  va_start( ap, fmt );
  vsnprintf( VirtPrintfBuf, 1024, fmt, ap );
  va_end( ap );

  VirtPrintfBuf[1023] = 0;

  bool part1 = (options & 0x2);

  if ( part1 == true ) resetColorAttrs( );

  if ( (part1 == true) && (options & 0x1) && (Timestamp == true) ) {
    puts( scLocalMsg, getTimeStr( ) );
    puts( scNormal, " " );
  }

  puts( attr, VirtPrintfBuf );

  if ( (options & 0x4) && (logging == true) ) {
    appendLog( part1, "%s", VirtPrintfBuf );
  }

}


void Session::draw( void ) {

  was_updated = false;


  // We start from the bottom of the screen and calculate where the top of
  // the screen is.  If we have a partial line of output on the bottom
  // of the screen we need to make it look like the bottom of the screen
  // is one line lower in the virtual buffer so that we display the
  // partial line.

  int16_t start_y = output_y;
  if ( output_x ) start_y++;

  int16_t topRow = start_y - Screen::getOutputRows() - backScrollOffset;
  if ( topRow < 0 ) topRow += virtBufferRows;

  // uint8_t far *startAddr = (uint8_t far *)virtBuffer + (topRow * (Screen::getScreenCols() << 1));
  uint8_t *startAddr = (uint8_t *)virtBuffer;
  addToPtr(startAddr, ((uint32_t)topRow * (uint32_t)(Screen::getScreenCols() << 1ul)), uint8_t * );

  uint8_t *screenBase = Screen::getScreenBase( );

  for ( uint16_t i=0; i < Screen::getOutputRows(); i++ ) {

    if ( ((i & 0x1) == 0) && Screen::isPreventSnowOn( ) ) waitForCGARetraceLong( );

    _fmemcpy( screenBase, startAddr, (Screen::getScreenCols() << 1) );
    screenBase += (Screen::getScreenCols() << 1);

    // startAddr += (Screen::getScreenCols() << 1);
    addToPtr( startAddr, (Screen::getScreenCols() << 1), uint8_t * );

    if ( (topRow + i) == (virtBufferRows - 1) ) {
      // Wrapped
      startAddr = (uint8_t *)virtBuffer;
    }
  }

}



extern char LogDirectory[];
extern char IRCServer[];

void Session::loggingToggle( void ) {

  if ( logging == true ) {
    puts( scLocalMsg, "Logging turned off\n" );
    closeLogFile( );
  }

  else {

    // 66 chars for drive letter and path, 12 for filename, and 1 for null.
    char logFilename[79];

    // LogDirectory either does not exist or already terminates in a backslash.
    strcpy( logFilename, LogDirectory );
    strncat( logFilename, name, 8 );
    strcat( logFilename, ".irc" );

    logFile = fopen( logFilename, "a+t" );
    if ( logFile == NULL ) {
      printf( PrintOpts_none, scErr, "Error opening %s - not logging\n", logFilename );
      logging = false;
    }
    else {
      logging = true;
      appendLog( true, "IRCjr start logging (%s %s)\n", IRCServer, name );
      printf( PrintOpts_none, scLocalMsg, "Logging new output to %s\n", logFilename );
    }

  }

}



void Session::closeLogFile( void ) {
  if ( logging == true ) {
    appendLog( true, "IRCjr stop logging\n" );
    fclose( logFile );
    logging = false;
  }
}




extern char *getTimeStr( void );

void Session::appendLog( bool part1, char *fmt, ... ) {

  if ( logging == false ) return;

  if ( part1 == true ) {

    DosDate_t currentDate;
    getdate( &currentDate );

    fprintf( logFile, "%04d-%02d-%02d %s ",
             currentDate.year, currentDate.month, currentDate.day,
             getTimeStr( ) );

  }

  va_list ap;
  va_start( ap, fmt );
  vfprintf( logFile, fmt, ap );
  va_end( ap );

}

