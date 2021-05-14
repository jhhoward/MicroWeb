
/*

   mTCP Screen.cpp
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


   Description: Screen handling code for IRCjr

   Changes:

   2011-05-27: Initial release as open source software
   2013-02-03: Add pop-up color chart for sending color codes
   2013-02-15: 132 column awareness
   2013-04-10: Allow for some embedded color codes when printing on
               the screen.  (Saves space when painting the help screen.)

*/



#include <bios.h>
#include <ctype.h>
#include <conio.h>
#include <dos.h>
#include <mem.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <utils.h>

#include "screen.h"
#include "session.h"



// Class variables storage

uint8_t far *Screen::screenBase;
uint8_t far *Screen::separatorRowAddr;
uint8_t far *Screen::inputAreaStart;

uint16_t Screen::screenBaseSeg;

uint16_t Screen::screenRows;
uint16_t Screen::screenCols;
uint16_t Screen::separatorRow;
uint16_t Screen::outputRows;

uint16_t Screen::cur_x, Screen::cur_y, Screen::cur_y2;
uint16_t Screen::input_len;

char *Screen::userInputBuffer;
uint8_t *Screen::switchToSession;

bool Screen::colorMode;
bool Screen::insertMode;
bool Screen::eatNextChar;
bool Screen::colorPopup;

bool Screen::preventSnow = false;


// Not used ...

extern void setMode( void );
#pragma aux setMode = \
  "mov ax, 4f02h" \
  "mov bx, 55h"  \
  "int 10h"



int8_t Screen::init( char *userInputBuffer_p, uint8_t *switchToSession_p ) {

  // This always works: What is our current video mode?
  unsigned char mode = *((unsigned char far *)MK_FP( 0x40, 0x49 ));

  if ( mode == 7 ) {
    colorMode = false;
    screenBaseSeg = 0xb000;
  }
  else {
    colorMode = true;
    screenBaseSeg = 0xb800;
    if ( getenv( "MTCP_NO_SNOW" ) ) preventSnow = true;
  }
  screenBase = (uint8_t far *)MK_FP( screenBaseSeg, 0 );


  // Next, find out if we are on an EGA or VGA card
  //
  // If EGA or better is installed then BL gets set to 00 to 03, which is
  // a memory size.  If it comes back the same at the input (0x10) then
  // EGA or better is not installed

  bool isCGAorMDA;

  if ( getEgaMemSize( ) == 0x10 ) {
    // Failed.  Must be MDA or CGA
    screenCols = 80;
    screenRows = 25;
    isCGAorMDA = true;
  }
  else {
    screenCols = *((unsigned char far *)MK_FP( 0x40, 0x4A ));
    screenRows = *((unsigned char far *)MK_FP( 0x40, 0x84 )) + 1;
    isCGAorMDA = false;
  }


  // If we are on a CGA or MDA then do this port voodoo to disable blinking.
  // Otherwise, use the nice interrupt to do it instead.  This is to enable
  // the high bit background colors.

  if ( isCGAorMDA ) {

    uint8_t far *idBytePtr = (uint8_t far *)MK_FP( 0xffff, 0xe);

    if ( *idBytePtr == 0xFD ) {

      // Stupid PCjr does this in a non-standard way
      inp( 0x3da );      // Read from the VGA to get it in a good mood.
      outp( 0x3da, 3 );   // Tell it the register we want to hit.
      outp( 0x3da, 0 );   // Turn off blinking

    } else {

      uint16_t portBase = *((uint16_t far *)(MK_FP( 0x40, 0x63 ))) + 4;

      uint8_t modeSelectVal = *((uint8_t far *)(MK_FP( 0x40, 0x65 )));
      modeSelectVal = modeSelectVal & 0xDF;

      outp( portBase, modeSelectVal );

    }

  }
  else {

    turnOffEgaBlink( );

  }


  separatorRow = screenRows - INPUT_ROWS - 1;
  outputRows = screenRows - (INPUT_ROWS + 1);

  separatorRowAddr = screenBase + ( separatorRow * (screenCols<<1) );
  inputAreaStart = separatorRowAddr + (screenCols<<1);

  cur_y = cur_x = 0;
  cur_y2 = separatorRow + 1;

  input_len = 0;


  // Storage areas provided by our caller

  userInputBuffer = userInputBuffer_p;
  switchToSession = switchToSession_p;


  insertMode = true;
  eatNextChar = false;
  colorPopup = false;


  // Draw initial screen

  // First, erase the screen.  This is slower than just one call to
  // fillUsingWord, but it lets us deal with CGA snow properly.
  // Then draw the indicator line.

  for ( int i=0; i < screenRows; i++ ) {
    if ( preventSnow & ((i & 0x1) ==0) ) { waitForCGARetraceLong( ); }
    fillUsingWord( screenBase + i*screenCols*2, 7<<8|32, screenCols );
  }

  if ( preventSnow ) { waitForCGARetraceLong( ); }
  fillUsingWord( separatorRowAddr, 7<<8|196, screenCols );

  setBlockCursor( );

  return 0;
}





// clearInputArea
//
// Wipes out the user input and clears the user input area

void Screen::clearInputArea( void ) {

  if ( preventSnow ) { waitForCGARetraceLong( ); }
  fillUsingWord( inputAreaStart, (7<<8|32), INPUT_ROWS * screenCols );

  input_len = 0;
  cur_y = cur_x = 0;
  cur_y2 = separatorRow + 1;

  // Put the cursor back in the right spot to be pretty.
  gotoxy( cur_x, cur_y2 );

}



// repaintInputArea
//
// Used to repaint the portion of the input area that is being moved by an
// insert or delete operation.

void Screen::repaintInputArea( uint16_t offset, char far *buffer, uint16_t len ) {

  uint8_t far *target = inputAreaStart + offset*2;

  if ( preventSnow ) { waitForCGARetraceLong( ); }

  for ( uint16_t i=0; i < len; i++ ) {

    if ( *buffer < 32 ) {
      *target = 254;
    }
    else {
      *target = *buffer;
    }

    buffer++;
    target += 2;
  }

  // Clear out to the end of the input area.
  if ( preventSnow ) { waitForCGARetraceLong( ); }
  fillUsingWord( target, (7<<8|32), (SCBUFFER_MAX_INPUT_LEN-offset)*2 );

}



void Screen::displayColorChart( void ) {

  Screen::repeatCh( 0, separatorRow-6, scBorder, 205, screenCols );

  clearRows( separatorRow-5, 4 );

  if ( (Screen::colorMode == false) || (ColorScheme == 1) ) {

    // Monochrome users - give them lables

    Screen::printf( 0, separatorRow-4, scNormal, "0 Wht  1 Black   2 Navy Bl   3 Grn      4 Red    5 Brown   6 Magenta   7 Orange" );
    Screen::printf( 0, separatorRow-3, scNormal, "8 Yel  9 L Grn  10 Teal     11 L Cyan  12 L Bl  13 Pink   14 Gray     15 L Gray" );

  }
  else {

    // Color users - give them example colors with numbers

    for ( int i=0; i < 16; i++ ) {
      Screen::printf( i*4+8, separatorRow-4, scNormal, " %2d ", i );
    }

    for ( int i=0; i < 16; i++ ) {
      Screen::printf( i*4+8, separatorRow-3, mIRCtoCGAMap[i], "\xdb\xdb\xdb\xdb" );
    }

  }

  Screen::repeatCh( 0, separatorRow-1, scBorder, 205, screenCols );

  colorPopup = true;
  gotoxy( cur_x, cur_y2 );

}


void Screen::cursorBack( void ) {
  if ( cur_x == 0 ) {
    cur_x = screenCols - 1;
    cur_y--; cur_y2--;
  }
  else {
    cur_x--;
  }
}


void Screen::cursorForward( void ) {
  cur_x++;
  if ( cur_x == screenCols ) {
    cur_x = 0;
    cur_y++; cur_y2++;
  }
}


Screen::InputActions Screen::getInput2( void ) {

  gotoxy( cur_x, cur_y2 );

  uint16_t key = bioskey(0);

  if ( eatNextChar == true ) {
    eatNextChar = false;
    return AteOneKeypress;
  }

  bool removeColorPopup = false;

  if ( (key & 0xff) == 0 ) {

    // If any function key is pressed get rid of the color chart popup table
    removeColorPopup = true;

    // Function key
    uint8_t fkey = key >> 8;

    switch ( fkey ) {

      case 19: return ShowRawToggle;          // Alt-R
      case 20: return TimestampToggle;        // Alt-T
      case 31: return Stats;                  // Alt-S
      case 35: return Help;                   // Alt-H
      case 38: return LoggingToggle;          // Alt-L
      case 45: return EndProgram;             // Alt-X
      case 46: return CloseWindow;            // Alt-C
      case 48: return BeepToggle;             // Alt-B
      case 73: return BackScroll;             // PgUp
      case 81: return ForwardScroll;          // PgDn

      case 71: {  // Home
        cur_x = 0;
        cur_y = 0; cur_y2 = separatorRow+1;
        break;
      }

      case 72: {  // Up
        if ( cur_y ) { cur_y--; cur_y2--; } else { ERRBEEP( ); }
        break;
      }

      case 75: {  // Left
        if ( !isCursorHome( ) ) { cursorBack( ); } else { ERRBEEP( ); }
        break;
      }

      case 77: {  // Right
        if ( cur_x + (cur_y * screenCols) < input_len ) { cursorForward( ); } else { ERRBEEP( ); }
        break;
      }

      case 79: {  // End
        cur_x = input_len % screenCols;
        cur_y = input_len / screenCols;
        cur_y2 = cur_y + separatorRow + 1;
        break;
      }

      case 80: {  // Down
        if ( cur_x + ((cur_y+1)*screenCols) <= input_len ) { cur_y++; cur_y2++; } else { ERRBEEP( ); }
        break;
      }

      case 82: {  // Insert

        insertMode = !insertMode;
        if ( insertMode ) {
          sound(500); delay(50); sound(750); delay(50); nosound( );
        }
        else {
          // Not really complaining - just indicating back to overstrike
          sound(750); delay(50); sound(500); delay(50); nosound( );
        }

        break;
      }

      case 83: {  // Delete

        // Has to be on an existing char

        uint16_t startIndex = cur_x + cur_y*screenCols;

        if ( startIndex < input_len ) {

          memmove( userInputBuffer+startIndex, userInputBuffer+startIndex+1, (input_len-startIndex) );
          input_len--;
          userInputBuffer[input_len] = 0;

          repaintInputArea( startIndex, userInputBuffer+startIndex, (input_len-startIndex) );
        }
        else {
          ERRBEEP( );
        }

        break;
      }

      case 129: {  // Alt-0 (Server Session)
        *switchToSession = 0;
        return SwitchSession;
      }

      default: {

        if ( fkey >= 120 && fkey <= 128 ) {
          *switchToSession = fkey - 119;
          return SwitchSession;
        }

      }

    }



  }
  else {

    // Normal key
    int c = key & 0xff;


    if ( colorPopup == true ) {

      // Color codes are digits and possibly a comma.  If they stop entering one of those
      // then make a note to remove the menu.

      if ( (!isdigit(c)) && (c != ',')) removeColorPopup = true;

    }


    uint16_t startIndex = cur_x + cur_y*screenCols;


    // Special control keys used for font and color control
    //
    //  2 = Ctrl-B  map to  2  Bold
    //  9 = Ctrl-I  map to 29  Italics
    // 11 = Ctrl-K  map to  3  Color
    // 15 = Ctrl-O  map to 15  All attributes reset
    // 18 = Ctrl-R  map to 22  Reverse
    // 21 = Ctrl-U  map to 31  Underline

    if ( ((c > 31) && (c < 127)) || (c > 127) || (c == 2) || (c == 9) || (c == 11) || ( c == 15) || (c == 18) || (c == 21) )  {


      // If they entered a control code it might need to be mapped to the correct protocol
      // level code.

      if ( c == 9 ) c = 29;
      else if ( c == 11 ) c = 3;
      else if ( c == 18 ) c = 22;
      else if ( c == 21 ) c = 31;


      // Are we appending to the end of the buffer?

      if ( startIndex == input_len ) {

        // Is there room in the buffer to add more?
        if ( input_len < (SCBUFFER_MAX_INPUT_LEN-1) ) {
          userInputBuffer[ input_len++ ] = c;
          cursorForward( );
          if ( c < 32 ) putch( 254 ); else putch( c );
          if ( c == 3 ) displayColorChart( );
        }
        else {
          ERRBEEP( );
        }

      }
      else {

        // In the middle of the line - insert or replace?

        if ( insertMode ) {

          // Room to insert?
          if ( input_len < (SCBUFFER_MAX_INPUT_LEN-1) ) {

            memmove( userInputBuffer+startIndex+1, userInputBuffer+startIndex, input_len-startIndex+1 );
            userInputBuffer[startIndex] = c;
            input_len++;
            userInputBuffer[input_len] = 0;

            cursorForward( );
            repaintInputArea( startIndex, userInputBuffer+startIndex, (input_len-startIndex) );

          }
          else {
            ERRBEEP( );
          }

        }
        else {
          userInputBuffer[ startIndex ] = c;
          cursorForward( );
          if ( c < 32 ) putch( 254 ); else putch( c );
          if ( c == 3 ) displayColorChart( );
        }

      } // end editing in the middle of the line



    } // end of is it a printable character

    else if ( c == 8 ) {

      // If pressed at the end of the line eat the last char. If
      // pressed in the middle of the line slide remaining chars
      // back.

      if ( input_len ) {

        if ( startIndex == input_len ) {
          input_len--;
          cursorBack( );
          gotoxy( cur_x, cur_y2 ); putch( ' ' ); gotoxy( cur_x, cur_y2 );
        }
        else {

          if ( !isCursorHome( ) ) {
            memmove( userInputBuffer+startIndex-1, userInputBuffer+startIndex, (input_len-startIndex) );
            input_len--;
            startIndex--;
            cursorBack( );
            repaintInputArea( startIndex, userInputBuffer+startIndex, (input_len-startIndex) );
          }
          else {
            ERRBEEP( );
          }

        }

      }
      else {
        // No input to backspace over!
        ERRBEEP( );
      }


    }

    else if ( c == 13 ) {
      userInputBuffer[input_len] = 0;
      clearInputArea( );
      return InputReady;
    }

    else if ( c == 27 ) {
      clearInputArea( );
    }


  } // end non-function keys

  gotoxy( cur_x, cur_y2 );


  if ( removeColorPopup ) {
    colorPopup = false;
    return Redraw;
  }
  else {
    return NoAction;
  }

}




// Screen printf functions - these write directly on the screen and are not
// saved in any buffer.

char cprintfBuffer[100];

void Screen::printf( uint16_t x, uint16_t y, uint8_t attr, char *fmt, ... ) {

  va_list ap;
  va_start( ap, fmt );
  printf_internal( x, y, attr, fmt, ap );
  va_end( ap );

}


void Screen::printf( uint8_t attr, char *fmt, ... ) {

  int x = wherex( );
  int y = wherey( );

  va_list ap;
  va_start( ap, fmt );
  printf_internal( x, y, attr, fmt, ap );
  va_end( ap );

}


void Screen::printf_internal( uint16_t x, uint16_t y, uint8_t attr, char *fmt, va_list ap ) {

  vsnprintf( cprintfBuffer, 100, fmt, ap );

  cprintfBuffer[99] = 0;

  uint16_t far *start = (uint16_t far *)(screenBase + (y*screenCols+x)*2);

  uint16_t len = strlen( cprintfBuffer );

  for ( uint16_t i = 0; i < len; i++ ) {

    char c = cprintfBuffer[i];

    switch ( c ) {

      case 27: {

        // Ugly hack to allow embedded color codes, which makes putting up the help
        // screen so much more efficient.

        i++;
        c = cprintfBuffer[i] - '0';
        if ( c == 1 ) attr = scNormal;
        else if ( c == 2 ) attr = scCommandKey;
        else if ( c == 3 ) attr = scLocalMsg;
        else if ( c == 4 ) attr = scBright;
        break;
      }

      case '\n': {
        x = 0;
        start = (uint16_t far *)(screenBase + (y*screenCols+x)*2);
        break;
      }
      case '\r': {
        y++;
        start = (uint16_t far *)(screenBase + (y*screenCols+x)*2);
        break;
      }
      default: {
        x++;
        if ( x == screenCols ) { x=0; y++; };
        uint16_t ch = ( attr << 8 | c );
        if ( preventSnow ) { 
          writeCharWithoutSnow(screenBaseSeg, FP_OFF(start),  ch  );
        } else {
          *start = ch;
        }
        start++;
        break;
      }
    }

  } // end for

  gotoxy( x, y );

}


void Screen::repeatCh( uint16_t x, uint16_t y, uint8_t attr, char ch, uint16_t count ) {
  uint16_t far *start = (uint16_t far *)(screenBase) + (y * screenCols) + x;
  if ( preventSnow ) { waitForCGARetraceLong( ); }
  fillUsingWord( start, (attr<<8) | ch, count );
}

