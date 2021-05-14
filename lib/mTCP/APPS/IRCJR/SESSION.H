
/*

   mTCP Session.h
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


   Description: Session handling data structures for IRCjr

   Changes:

   2011-05-27: Initial release as open source software
   2012-11-21: Add code to interpret mIRC color codes
   2013-03-23: Restructure printf and appendLog to cut down on code bloat

*/





#ifndef _SESSION_H
#define _SESSION_H

#include "types.h"
#include "ircjr.h"
#include "irc.h"


#define MAX_SESSIONS (10)



// A Session represents an open channel, a private conversation with another
// user, or the server messages channel.

class Session {

  public:

    int16_t init( const char *name_p, uint16_t bufferRows );
    void    destroy( void );


    // printf options (actually a bitmap: bit 1 is timestamp, bit 2 is part1/part2, bit 3 is logging
    //
    static const uint8_t PrintOpts_none  = 0;    // Just write to session
    static const uint8_t PrintOpts_Part2 = 5;    // Part 2 of a line; write and log (no timestamp)
    static const uint8_t PrintOpts_Part1 = 7;    // Part 1 of a line; write, timestamp and log

    void    printf( uint8_t options, uint8_t attr, char *fmt, ... );
    void    puts( uint8_t attr, const char *str );
    void    draw( void );         // Unconditional screen draw

    inline const char * getName( void ) const { return name; }

    inline bool isChannel( void ) const { return name[0] == '#'; }

    inline bool wasSessionUpdated( void ) const { return was_updated; }
    inline void drawIfUpdated( void ) { if ( was_updated ) { draw( ); was_updated = false; } }

    inline bool    isBackScrollAvailable( void ) const { return backScrollLines > 0; }
    inline int16_t getBackScrollLines( void ) const { return backScrollLines; }

    inline bool    isBackScrollAtHome( void ) const { return backScrollOffset == 0; }
    inline int16_t getBackScrollOffset( void ) const { return backScrollOffset; }
    inline void    resetBackScrollOffset( void ) { backScrollOffset = 0; }

    inline bool    isLoggingOn( void ) { return logging; }

    inline void adjustBackScrollOffset( int16_t rows ) {
      backScrollOffset += rows;
      if ( backScrollOffset > backScrollLines ) {
        backScrollOffset = backScrollLines;
      }
      else if ( backScrollOffset < 0 ) {
        backScrollOffset = 0;
      }
    }

    inline void resetColorAttrs( void ) {
      userAttr = 0x07;
      userAttrBold = false;
      userAttrFixed = false;
      userAttrReverse = false;
      userAttrItalics = false;
      userAttrUnderline = false;
    }

    void loggingToggle( void );
    void closeLogFile( void );
    void appendLog( bool part1, char *fmt, ... );


  private:

    uint16_t parseColorCode( const char *str );
    uint8_t computeAttr( void );


    char name[IRCNICK_MAX_LEN]; // Session name or identifier

    uint8_t   padding;

    uint16_t *virtBuffer;       // Pointer to virtual buffer
    uint16_t  virtBufferRows;   // How many rows does it have
    uint32_t  virtBufferSize;   // How many bytes allocated?

    uint16_t output_x;          // Current position to write to (x,y)
    uint16_t output_y;

    int16_t  backScrollLines;   // How many backscroll lines are there
    int16_t  backScrollOffset;  // Current backscroll offset (0 = none)

    bool     was_updated;       // Has the virtual buffer been modified since last draw?


    // Color and display attributes.  Really should be local to puts but they are
    // here because it is easier.

    uint8_t userAttr;

    bool userAttrBold;       // Toggle
    bool userAttrFixed;      // Toggle - Tracked by not used
    bool userAttrReverse;    // Toggle
    bool userAttrItalics;    // Toggle - Tracked by not used
    bool userAttrUnderline;  // Toggle - Only doable on MDA


    // Logging support

    bool logging;            // Toggle - is logging turned on?
    FILE *logFile;           // File handle we are logging to


    // Class variables and functions

    public:

      static int16_t getSessionIndex( Session *target );
      static int16_t getSessionIndex( const char *name_p );

      static Session * createAndMakeActive( const char *name_p, uint16_t rows, bool startLogging );
      static int16_t removeActiveSession( Session *target );

      static Session *activeSessionList[ MAX_SESSIONS ];
      static uint16_t activeSessions;
};


#endif
