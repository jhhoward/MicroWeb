
/*

   mTCP Screen.h
   Copyright (C) 2008-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Screen handling data structures for IRCjr

   Changes:

   2011-05-27: Initial release as open source software

*/


#ifndef _SCREEN_H
#define _SCREEN_H

#include <bios.h>
#include <stdarg.h>

#include "types.h"
#include "inlines.h"
#include "ircjr.h"


// The Screen represents the display device.  It paints the sessions on the
// screen and manages the status line and the user input area.

class Session;

class Screen {

  public:

    enum InputActions {
      NoAction=0,
      EndProgram,
      CloseWindow,
      InputReady,
      BackScroll,
      ForwardScroll,
      Stats,
      BeepToggle,
      Help,
      TimestampToggle,
      LoggingToggle,
      SwitchSession,
      AteOneKeypress,
      ShowRawToggle,
      Redraw,
    };



    static int8_t init( char *userInputBuffer_p, uint8_t *switchToSession );


    // Screen manipulation

    static void clearInputArea( void );

    static void repaintInputArea( uint16_t offset, char far *buffer, uint16_t len );

    static inline void clearRows( uint16_t startRow, uint16_t rows ) {
      uint16_t *startAddr = (uint16_t far *)(screenBase) + startRow*screenCols;
      if ( preventSnow ) { waitForCGARetraceLong( ); }
      fillUsingWord( startAddr, (scNormal<<8|32), rows*screenCols);
    }

    static inline void updateCursor( void ) { gotoxy( cur_x, cur_y2 ); }



    // These functions write directly to the screen buffer and are designed
    // for one or two lines of text at a time at most.

    static void printf( uint8_t attr, char *fmt, ... );
    static void printf( uint16_t x, uint16_t y, uint8_t attr, char *fmt, ... );

    static void repeatCh( uint16_t x, uint16_t y, uint8_t attr, char ch, uint16_t count );



    // Input handling

    static inline InputActions getInput( void ) {
      if ( bioskey(1) ) return getInput2( ); else return NoAction;
    }

    static inline void eatOneKeypress( void ) { eatNextChar = true; }


    // Interrogators

    static inline bool          isColorCard( void ) { return colorMode; }
    static inline bool          isPreventSnowOn( void ) { return preventSnow; }

    static inline uint16_t      getSeparatorRow( void ) { return separatorRow; }
    static inline uint16_t      getOutputRows( void ) { return outputRows; }

    static inline uint16_t      getScreenBaseSeg( void ) { return screenBaseSeg; }
    static inline uint8_t far * getScreenBase( void ) { return screenBase; }
    static inline uint8_t far * getSeparatorRowAddress( void ) { return separatorRowAddr; }

    static inline uint16_t      getScreenRows( void ) { return screenRows; }
    static inline uint16_t      getScreenCols( void ) { return screenCols; }


  private:


    static InputActions getInput2( void );

    // Move the cursor back or forward while handling wrapping around the screen

    static void cursorBack( void );
    static void cursorForward( void );

    static inline bool   isCursorHome( void ) { return (cur_x == 0) && (cur_y == 0); }

    static void displayColorChart( void );

    static void printf_internal( uint16_t x, uint16_t y, uint8_t attr, char *fmt, va_list ap );


    static uint8_t far *screenBase;          // Memory address of the top of the screen
    static uint8_t far *separatorRowAddr;    // Memory address of the separator row
    static uint8_t far *inputAreaStart;      // Memory address of the top of the user input area

    static uint16_t screenBaseSeg;           // Segment of the screen base

    static uint16_t screenRows;              // Total number of rows on the screen
    static uint16_t screenCols;              // Total number of columns on the screen
    static uint16_t separatorRow;            // Row number of the separator row (0 based)
    static uint16_t outputRows;              // Number of rows in the output area

    static uint16_t cur_x, cur_y, cur_y2;
    static uint16_t input_len;

    static char *userInputBuffer;
    static uint8_t *switchToSession;

    static bool colorMode;
    static bool insertMode;
    static bool eatNextChar;
    static bool colorPopup;

    static bool preventSnow;

};



#endif
