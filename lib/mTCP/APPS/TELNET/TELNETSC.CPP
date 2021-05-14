/*

   mTCP TelnetSc.cpp
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


   Description: Screen handling code for Telnet client

   Changes:

   2011-05-27: Initial release as open source software
   2013-03-15: Add scroll region support, DEC origin mode, and make
               cursor handling at the right margin operate like putty
               because they seem to do it right.
   2015-01-22: Add 132 column support
   2018-10-27: Add code to prevent CGA snow

*/




#include <conio.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


#include "types.h"
#include "telnetsc.h"
#include "utils.h"
//#include "trace.h"




// Virtual/Backscroll buffer
//
// Scrolling a terminal screen is very expensive, especially on older
// hardware.  It is a massive (4K) memory move at a minimum - with a 50
// line VGA card it is 8K.  The older video cards also take forever to
// do the scrolling.
//
// Solve the problem by using a ring buffer of terminal lines instead.
// Scrolling is achieved by bumping a pointer to the top of your virtual
// terminal in the ring buffer.  You have to be aware that your virtual
// terminal will wrap around in the buffer, but this is far cheaper than
// trying to do the memory move and screen updates.
//
// For performance reasons, make batch updates to the virtual screen.
// The penalty is that you will have to do a full screen repaint if you
// update the virtual screen.  This is still far faster than doing multiple
// 4K moves, one for each time the screen scrolls.
//
// For useability you can update the real screen and the virtual screen at
// the same time.  Do this on small updates for as long as you can until
// you try to do something laggy, like scrolling.


// General rules for updating the screen.
//
// - If updateRealScreen is on then a function is expected to update the
//   virtual buffer and the real screen.
// - If updateRealScreen is on and a function determines it is too slow
//   or undesirable to keep updating the real screen, it may set it off.
//   But then it should set virtualUpdated.
// - If virtualUpdated is set then the screens are out of sync and you
//   need to repaint.
// - Once virtualUpdated is set you may not turn on updateRealScreen
//   again.  Only painting can do that.
//
// A function might call another helper function, which might change
// these flags.




int8_t Screen::init( uint8_t backScrollPages, uint8_t initWrapMode ) {

  preventSnow = false;

  // This always works:
  unsigned char mode = *((unsigned char far *)MK_FP( 0x40, 0x49 ));

  if ( mode == 7 ) {
    colorCard = false;
    screenBaseSeg = 0xb000;
  }
  else {
    colorCard = true;
    screenBaseSeg = 0xb800;
    if ( getenv( "MTCP_NO_SNOW" ) ) preventSnow = true;
  }
  Screen_base = (uint8_t far *)MK_FP( screenBaseSeg, 0 );

  if ( getEgaMemSize( ) == 0x10 ) {

    // Failed.  Must be MDA or CGA
    terminalLines = 25;
    terminalCols = 80;

  }
  else {

    terminalLines = *((unsigned char far *)MK_FP( 0x40, 0x84 )) + 1;
    terminalCols = *((unsigned char far *)MK_FP( 0x40, 0x4A ));

  }


  bytesPerLine = terminalCols * 2;


  // Setup the virtual buffer.  The virtual buffer also serves as the
  // backscroll buffer.  We need to clear the buffer and set the char
  // attributes to something predictable for at least the virtual screen
  // part.  I do the entire buffer here because I am lazy.

  // Desired size of virtual buffer
  uint32_t desiredBufferSize = backScrollPages * terminalLines;
  desiredBufferSize = desiredBufferSize * bytesPerLine;

  if ( desiredBufferSize > 64001ul ) {
    // Too much .. get them into 64K
    uint16_t newBackScrollPages = 64000ul / (terminalLines * bytesPerLine );
    backScrollPages = newBackScrollPages;
  }


  totalLines = terminalLines * backScrollPages;

  bufferSize = totalLines * bytesPerLine;

  buffer = (uint8_t *)malloc( bufferSize );

  if ( buffer == NULL ) {
    Screen_base = 0;
    return -1;
  }

  // Used to quickly detect when we are past the end of our buffer.
  bufferEnd = buffer + bufferSize;

  wrapMode = initWrapMode;

  resetTerminalState( );

  return 0;
}


void Screen::resetTerminalState( void ) {

  // Clear out the backscroll buffer

  for ( uint16_t i=0; i < bufferSize; i=i+2 ) {
    buffer[i] = 32;
    buffer[i+1] = 7;
  }

  // Reset the cursor, attributes, and terminal properties

  cursor_x = 0;
  cursor_y = 0;
  curAttr = 7;

  overhang = false;

  topOffset = 0;
  backScrollOffset = 0;

  updateRealScreen = 1;
  virtualUpdated = 0;

  // We are going to keep this up to date instead of computing it for each
  // character.
  vidBufPtr = Screen_base + ( (cursor_x<<1) + (cursor_y*bytesPerLine) );

  clearConsole( );
  gotoxy( 0, 0 );
  setBlockCursor( );


  // Terminal emulation state

  scrollRegion_top = 0;       // The host sends these as 1 based; we use 0 for a base.
  scrollRegion_bottom = terminalLines - 1;

  originMode = false;
  autoWrap = false;

  // If the save area has not been initialized then these values are used.
  // Which also happen to be the same as the initial values.

  cursorSaveArea.cursor_x = 0;
  cursorSaveArea.cursor_y = 0;
  cursorSaveArea.curAttr = 7;
  cursorSaveArea.originMode = false;
  cursorSaveArea.autoWrap = false;

}



// Updates the real screen, nothing else ...

void Screen::clearConsole( void ) {
  for ( int i=0; i < terminalLines; i++ ) {
    if ( preventSnow & ((i & 0x1) ==0) ) { waitForCGARetraceLong( ); }
    fillUsingWord( (uint16_t far *)Screen_base + (i*terminalCols), (7<<8|32), terminalCols );
  }
}



char cprintfBuffer[100];


void Screen::myCprintf( uint8_t x, uint8_t y, uint8_t attr, char *fmt, ... ) {

  va_list ap;
  va_start( ap, fmt );
  myCprintf_internal( x, y, attr, fmt, ap );
  va_end( ap );

}


void Screen::myCprintf_internal( uint8_t x, uint8_t y, uint8_t attr, char *fmt, va_list ap ) {

  vsnprintf( cprintfBuffer, 100, fmt, ap );

  cprintfBuffer[99] = 0;

  uint16_t far *start = (uint16_t far *)(Screen_base + (y*terminalCols+x)*2);

  uint16_t len = strlen( cprintfBuffer );

  for ( uint16_t i = 0; i < len; i++ ) {

    char c = cprintfBuffer[i];

    switch ( c ) {
      case '\n': {
        x = 0;
        start = (uint16_t far *)(Screen_base + (y*terminalCols+x)*2);
        break;
      }
      case '\r': {
        y++;
        start = (uint16_t far *)(Screen_base + (y*terminalCols+x)*2);
        break;
      }
      default: {
        x++;
        if ( x == terminalCols ) { x=0; y++; };
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






// Origin mode rules
//
// - If origin mode is on all line numbers are relative to the start of the scroll region.
// - If origin mode is off all line numbers are absolute.
//
// Scroll region rules
//
// - If the cursor is in the scroll region it does not leave the scroll region.
// - To get the cursor out of the scroll region set the position while origin mode is off
//   - While outside the scroll region the cursor can move freely.
//   - If the cursor moves into the scroll region it gets stuck there again.




void Screen::setHorizontal( int16_t newHorizontal ) {

  // Inputs are 0 based.  Origin mode is not a factor.

  if ( newHorizontal < 0 ) {
    newHorizontal = 0;
  }
  else if ( newHorizontal >= terminalCols ) {
    newHorizontal = terminalCols - 1;
  }

  cursor_x = newHorizontal;
}


void Screen::setVertical( int16_t newVertical ) {

  // Inputs are 0 based.

  // Negative values should not happen - check for them just to be safe.
  if ( newVertical < 0 ) newVertical = 0;

  if ( originMode == false ) {

    // Non-origin mode: screen positions are absolute

    cursor_y = newVertical;

    if ( cursor_y >= terminalLines ) {
      cursor_y = terminalLines - 1;
    }

  }
  else {

    // Origin mode - everything is relative to the current window
    // and can not get outside of it.

    cursor_y = scrollRegion_top + newVertical;

    if ( cursor_y > scrollRegion_bottom ) {
      cursor_y = scrollRegion_bottom;
    }

  }

}


void Screen::adjustVertical( int16_t delta ) {

  int newCursor_y = cursor_y + delta;

  bool isInScrollRegion = ( cursor_y >= scrollRegion_top ) && ( cursor_y <= scrollRegion_bottom );

  if ( (isInScrollRegion == false) &&
       (cursor_y < scrollRegion_top) && (newCursor_y >= scrollRegion_top) ||
       (cursor_y > scrollRegion_bottom) && (newCursor_y <= scrollRegion_bottom)  )
  {
    isInScrollRegion = true;
  }

  cursor_y = newCursor_y;

  if ( isInScrollRegion == true ) {
    if ( cursor_y < scrollRegion_top ) {
      cursor_y = scrollRegion_top;
    }
    else if ( cursor_y > scrollRegion_bottom ) {
      cursor_y = scrollRegion_bottom;
    }
  }
  else {
    if ( cursor_y < 0 ) {
      cursor_y = 0;
    }
    else if ( cursor_y >= terminalLines ) {
      cursor_y = terminalLines - 1;
    }
  }

}







/*
void Screen::repeatCh( uint8_t x, uint8_t y, uint8_t attr, char ch, uint8_t count ) {
  uint16_t far *start = (uint16_t far *)(Screen_base + (y*terminalCols+x)*2);
  uint16_t fillWord = (attr<<8) | ch;
  fillUsingWord( start, fillWord, count );
}
*/






// Screen::scroll
//
// Move the cursor down one line.
//
// Scrolling is a high latency operation.  If we were at the bottom row
// then we are going to scroll.  Don't bother trying to keep the screens
// in sync if this happens.

void Screen::scroll( void ) {

  if ( cursor_y == scrollRegion_bottom ) {
    // Was on the bottom line of the scroll region.
    // Need to scroll the scroll region, which might be just part of the screen.
    scrollInternal( );
  }
  else {

    // Was above or below the bottom line of the scroll region.  Just make
    // sure the cursor stays on the screen.

    cursor_y++;
    if ( cursor_y >= terminalLines ) cursor_y = terminalLines - 1;

  }

}



// Screen::scrollInternal
//
// This does the actual work of scrolling the screen.
//
// In normal full screen operations this is easy - move topOffset down
// by one line and erase the new bottom line.
//
// In a screen with an active scroll region this is different.  Only
// the scroll region is affected.  Scrolling adds nothing to our
// private backscroll buffer because nothing is actually pushed off
// of the screen.

void Screen::scrollInternal( void ) {

  if ( scrollRegion_top == 0 && scrollRegion_bottom == (terminalLines - 1) ) {

    // Classic scrolling behavior - lines at the top get pushed into to
    // scroll buffer.

    topOffset += bytesPerLine;
    if ( topOffset == bufferSize ) topOffset = 0;

    // Clear the newly opened line in the virtual buffer.
    // In theory we could do this with the clear method but that has far
    // higher overhead because it is general purpose.

    uint16_t far *tmp = (uint16_t *)(buffer + ScrOffset( 0, cursor_y ));

    uint16_t fillWord = (curAttr<<8) | 0x20;
    if ( preventSnow ) { waitForCGARetraceLong( ); }
    fillUsingWord( tmp, fillWord, terminalCols );

  }

  else {

    // They are using a scroll region - do not add to our private
    // backscroll buffer.

    delLine( scrollRegion_top );

  }

  // Stop updating the real screen - scrolling is slow.
  updateRealScreen = 0;
  virtualUpdated = 1;

}




// This seems silly and we could solve the problem by adding
// a default value to the length parameter on the other add
// method, but this is called so many places that the overhead
// of the extra parameter makes the file bigger!

void Screen::add( char *buf ) {
  add( buf, strlen(buf) );
}





// Overhang mode is kind of goofy and I created it based on experimentation I
// did with putty.  Basically, if the cursor is in the last column and you
// print a character there you do not automatically wrap.  You only wrap to
// the first column on the next line if another character gets printed.
// This allows you to put a character in the last column, and then interpret
// a control code such as Backspace, LF or CR while still on that same line.

void Screen::add( char *buf, uint16_t len ) {

  // Easier to just always update this here rather than branch if unnecessary
  updateVidBufPtr( );

  for ( uint16_t i=0; i < len; i++ ) {

    uint8_t c = buf[i];

    if ( c == 0 ) {         // Null char
      // Do nothing
    }

    else if ( c == '\r' ) { // Carriage Return
      cursor_x = 0;
      overhang = false;
      updateVidBufPtr( );
    }

    else if ( c == '\n' ) { // Line Feed
      scroll( );
      overhang = false;
      updateVidBufPtr( );
    }

    else if ( c == '\a' ) { // Attention/Bell
      sound(1000); delay(100); nosound( );
    }

    else if ( c == '\t' ) { // Tab
      overhang = false;
      uint16_t newCursor_x = (cursor_x + 8) & 0xF8;
      if ( newCursor_x < terminalCols ) cursor_x = newCursor_x;
      updateVidBufPtr( );
    }

    else if ( c == 8 || c == 127 ) { // Backspace or Delete Char

      if ( overhang == true ) {
        overhang = false;
      }
      else {

        // Fixme: If this was delete char we really should blank it out.

        // Backspace across columns works in putty so we'll do it here.
        // The good news is that if we are in the home position we don't
        // have to scroll the screen down.

        if ( cursor_x > 0 ) {
          cursor_x--;
        }
        else {
          cursor_x = terminalCols - 1;
          if ( cursor_y > 0 ) cursor_y--;
        }
        updateVidBufPtr( );
      }

    }

    else {

      // Remember this in case we need to repeat the last char for an ANSI op
      lastChar = c;

      //TRACE(( "Printing char %x, overhang = %u, cursor_x = %u\n", c, overhang, cursor_x ));

      // If the previous cursor position left us in the overhang now we
      // can wrap (if required) and scroll down.  Do this before printing
      // the next character.

      if ( overhang == true ) {

        if ( wrapMode ) {
          vidBufPtr += 2;  // Wrap to the next line.
          cursor_x = 0;
          scroll( );
        }
        else {
          cursor_x = terminalCols - 1;
        }

        overhang = false;
      }


      buffer[ ScrOffset(cursor_x, cursor_y ) ] = c;
      buffer[ ScrOffset(cursor_x, cursor_y ) + 1 ] = curAttr;


      // If you are in the last column do not advance; go into the "overhang"
      // instead and wait to see what the next character is.

      if ( cursor_x == terminalCols-1 ) {
        overhang = true;
      }
      else {
        cursor_x++;
      }

      //TRACE(( "    overhang = %u, cursor_x = %u\n", overhang, cursor_x ));

      if ( updateRealScreen ) {

        uint16_t ch = ( curAttr << 8 | c );

	if ( preventSnow ) {
	  writeCharWithoutSnow( screenBaseSeg, FP_OFF(vidBufPtr), ch );
	} else {
	  *((uint16_t far *)vidBufPtr) = ch;
	}

        // If overhang is not set then we can advance.  Otherwise, wait
        if ( overhang == false ) {
          vidBufPtr += 2;  // Wrap to the next line.
        }

      }
      else {
        virtualUpdated = 1;
      }

    }

  } // end for


  // If we were keeping the real screen in sync then update the cursor
  // position.  If not, then note that the virtual screen has changed.

  if ( updateRealScreen ) {
    gotoxy( cursor_x, cursor_y );
  }
  else {
    virtualUpdated = 1;
  }

}


void Screen::paint( void ) {

  uint16_t vOffset = ScrOffset( 0, 0 );
  uint16_t sOffset = 0;

  for ( uint8_t i = 0; i < terminalLines; i++ ) {

    if ( preventSnow & ((i & 0x1) ==0) ) { waitForCGARetraceLong( ); }
    memcpy( Screen_base + sOffset, buffer + vOffset, bytesPerLine );
    sOffset = sOffset + bytesPerLine; // No need to check for wrap here.
    vOffset = vOffset + bytesPerLine; // But need to check for wrap here.
    if ( vOffset >= bufferSize ) vOffset = 0;

  }

  backScrollOffset = 0;

  // We are back to keeping things in sync.
  updateRealScreen = 1;
  virtualUpdated = 0;

  gotoxy( cursor_x, cursor_y );
}



void Screen::paint( int16_t offsetLines ) {

  // I'm a little paranoid about mixing signed and unsigned types, so jump
  // through a little extra code to ensure that backScrollOffset can be
  // a uint type.

  int16_t newBackScrollOffset = backScrollOffset + offsetLines;

  if ( newBackScrollOffset > (int16_t)(totalLines-terminalLines) ) {
    newBackScrollOffset = totalLines-terminalLines;
  }
  else if ( newBackScrollOffset <= 0 ) {
    backScrollOffset = 0;
    paint( );
    return;
  }

  backScrollOffset = newBackScrollOffset;


  // The backscroll offset is relative to the current topOffset in the
  // buffer.  Compute things relative to number of lines to make it
  // conceptually easier, then convert to a real byte offset for actual
  // display.

  // Do this math in such a way as to ensure that we don't have a
  // negative result, even if temporarily.

  uint16_t topOffsetLines = topOffset/bytesPerLine;

  uint16_t newOffsetLines;
  if ( topOffsetLines < backScrollOffset ) {
    newOffsetLines = (topOffsetLines + totalLines) - backScrollOffset;
  }
  else {
    newOffsetLines = topOffsetLines - backScrollOffset;
  }

  uint8_t far *source = buffer + ( newOffsetLines * bytesPerLine );


  uint16_t sOffset = 0;
  for ( uint8_t i = 0; i < terminalLines; i++ ) {

    if ( preventSnow & ((i & 0x1) ==0) ) { waitForCGARetraceLong( ); }
    memcpy( Screen_base + sOffset, source, bytesPerLine );
    sOffset = sOffset + bytesPerLine;
    source = source + bytesPerLine;
    if ( source >= bufferEnd ) source = buffer;
  }


  // Don't update the real screen from this point forward ..
  updateRealScreen = 0;

}




// It is assumed that you are calling this with good inputs.
// We're not going to check for badness.

void Screen::clear( uint16_t top_x, uint16_t top_y, uint16_t bot_x, uint16_t bot_y ) {

  uint16_t far *start = (uint16_t far *)(buffer + ScrOffset( top_x, top_y ));
  uint16_t far *end = (uint16_t far *)(buffer + ScrOffset( bot_x, bot_y ));

  // Add the +1 at the end because we need to clear the last byte inclusive,
  // not just up to the last byte.

  uint16_t chars = (bot_y*terminalCols+bot_x) - (top_y*terminalCols+top_x) + 1;
  uint16_t bytes = chars << 1;

  uint16_t fillWord = (curAttr<<8) | 0x20;

  if ( start <= end ) {

    // Contiguous storage
    //
    // for ( uint16_t i=0; i < bytes; i=i+2 ) { *start++ = fillWord; }
    fillUsingWord( start, fillWord, chars );

  }
  else {

    // Wrapped around ...

    uint16_t bytes2 = bufferEnd - (uint8_t *)start;

    // for ( uint16_t i=0; i < bytes2; i=i+2 ) { *start++ = fillWord; }
    uint16_t t1 = bytes2 / 2;
    fillUsingWord( start, fillWord, t1 );


    start = (uint16_t *)buffer;
    bytes = bytes - bytes2;

    // for ( uint16_t i=0; i < bytes; i=i+2 ) { *start++ = fillWord; }
    uint16_t t2 = bytes / 2;
    fillUsingWord( start, fillWord, t2 );

  }


  // If this was a small clear then update the real screen.  Otherwise,
  // punt and set the flag that says we need a repaint.

  if ( updateRealScreen && (bytes<1024) ) {

    // This is a minor operation so update the screen at the same time.

    uint16_t far *scStart = (uint16_t far *)(Screen_base + ( (top_x<<1) + (top_y*bytesPerLine) ) );

    /*
    for ( uint16_t i=0; i < bytes; i=i+2 ) {
      *scStart++ = fillWord;
    }
    */


    // Replacement code for the original C code above.
    // This loop clears 10 words at a time, and pauses
    // while a screen refresh is in progress.
    //
    // The remainder is done outside the loop.

    // for ( uint16_t i=0; i < chars; i++ ) { *scStart++ = fillWord; }
    if ( preventSnow ) { waitForCGARetraceLong( ); }
    fillUsingWord( scStart, fillWord, chars );

  }
  else {
    // Don't update the real screen anymore - this is going to require
    // a repaint.
    updateRealScreen = 0;
    virtualUpdated = 1;
  }

}



// Insert a line
//
// Scrolling below the scrolling area has no effect.
// Scrolling above the scrolling area pushes into the scrolling area.
// Fixme: what attribute should the new line have?

void Screen::insLine( uint16_t line_y ) {

  if ( line_y > scrollRegion_bottom ) return;

  // To insert a line, all visible lines below the current line get
  // copied downward bytesPerLine bytes.

  for ( uint8_t i=scrollRegion_bottom; i > line_y; i-- ) {
    memcpy( buffer + ScrOffset( 0, i ), buffer + ScrOffset( 0, i-1 ), bytesPerLine );
  }

  // For one line at the bottom it makes sense to keep the screen in sync,
  // but for multiple lines being inserted or an insert near top it does not.
  updateRealScreen = 0;

  // Clear will determine if we can update the screen in a reasonable
  // amount of time and will set updateRealScreen and virtualUpdated
  // accordingly.
  clear( 0, line_y, terminalCols-1, line_y );

  // Don't update the real screen anymore - this is going to require
  // a repaint.
  virtualUpdated = 1;
}


// Delete a line
//
// Scrolling below the scrolling area has no effect.
// Scrolling above the scrolling area pushes into the scrolling area.
// Fixme: what attribute should the new line have?

void Screen::delLine( uint16_t line_y ) {

  if ( line_y > scrollRegion_bottom ) return;

  for ( uint8_t i=line_y; i < scrollRegion_bottom; i++ ) {
    memcpy( buffer + ScrOffset( 0, i ), buffer + ScrOffset( 0, i+1 ), bytesPerLine );
  }

  // For one line at the bottom it makes sense to keep the screen in sync,
  // but for multiple lines being inserted or an insert near top it does not.
  updateRealScreen = 0;

  // Clear will determine if we can update the screen in a reasonable
  // amount of time and will set updateRealScreen and virtualUpdated
  // accordingly.
  clear( 0, scrollRegion_bottom, terminalCols-1, scrollRegion_bottom );

  // Don't update the real screen anymore - this is going to require
  // a repaint.
  virtualUpdated = 1;
}




// delChars
//
// Delete chars at the current cursor position, moving remaining
// chars to the left.
//
// ThisTextShallRemainGETRIDOFMEThisTextMoves|
// ThisTextShallRemainThisTextMoves          |

void Screen::delChars( uint16_t len ) {

  uint16_t affectedChars = terminalCols - cursor_x;

  if ( len > affectedChars ) {
    // Deleting more than we have on the line.
    len = affectedChars;
  }

  uint16_t charsToMove = affectedChars - len;
  uint16_t bytesToMove = charsToMove << 1;

  uint16_t startClearCol = cursor_x + charsToMove;


  // Slide the line in the virtual buffer first.
  if ( bytesToMove ) {
    memmove( buffer + ScrOffset( cursor_x, cursor_y ),
	     buffer + ScrOffset( cursor_x + len, cursor_y ),
	     bytesToMove
	   );
  }

  // Clear will update both the virtual buffer and possibly the real screen.
  // Move any screen data that we might need first.

  // If we are keeping the screens in sync perform the same up on the
  // real video buffer.  Otherwise, note that we changed the virutal buffer.

  if ( updateRealScreen ) {

    if ( bytesToMove ) {

      // This is a minor operation so update the screen at the same time.
      uint8_t far *src = Screen_base + ( ((cursor_x+len)<<1) + (cursor_y*bytesPerLine) );
      uint8_t far *dst = Screen_base + ( (cursor_x<<1) + (cursor_y*bytesPerLine) );
      if ( preventSnow ) { waitForCGARetraceLong( ); }
      memmove( dst, src, bytesToMove );

    } // endif bytesToMove

  }
  else {
    virtualUpdated = 1;
  }

  // Clear the remainder of the line.  Btw, this is a clreol op from
  // cursor_x + len.

  clear( startClearCol, cursor_y, terminalCols-1, cursor_y );

}



// ThisTextShallRemainThisTextMoves     |
// ThisTextShallRemainADDMEThisTextMoves|


void Screen::insChars( uint16_t len ) {

  uint16_t affectedChars = terminalCols - cursor_x;

  if ( len > affectedChars ) {
    // Inserting more than we have room for on the line
    len = affectedChars;
  }

  uint16_t charsToMove = affectedChars - len;
  uint16_t bytesToMove = charsToMove << 1;

  // Add a -1 to this because the clear function is inclusive of the last pos
  uint16_t clearToCol  = (cursor_x + len) - 1;


  // Slide the line in the virtual buffer first.
  if ( bytesToMove ) {
    memmove( buffer + ScrOffset( cursor_x+len, cursor_y ),
	     buffer + ScrOffset( cursor_x, cursor_y ),
	     bytesToMove
	   );
  }


  // Clear will update both the virtual buffer and possibly the real screen.
  // Move any screen data that we might need first.

  // If we are keeping the screens in sync perform the same up on the
  // real video buffer.  Otherwise, note that we changed the virutal buffer.

  if ( updateRealScreen ) {

    if ( bytesToMove ) {

      // This is a minor operation so update the screen at the same time.
      uint8_t far *src = Screen_base + ( (cursor_x<<1) + (cursor_y*bytesPerLine) );
      uint8_t far *dst = Screen_base + ( ((cursor_x+len)<<1) + (cursor_y*bytesPerLine) );
      if ( preventSnow ) { waitForCGARetraceLong( ); }
      memmove( dst, src, bytesToMove );

    } // else if bytesToMove


  }
  else {
    virtualUpdated = 1;
  }

  // Now clear the newly opened area
  clear( cursor_x, cursor_y, clearToCol, cursor_y );
}


void Screen::eraseChars( uint16_t len ) {

  // Set the next n chars to be a space with the current attribute.
  // Do not adjust the cursor position to do this.

  uint16_t affectedChars = terminalCols - cursor_x;

  if ( len > affectedChars ) {
    // Inserting more than we have room for on the line
    len = affectedChars;
  }


  uint16_t fillAttr = (curAttr<<8) | 0x20;

  // Virtual buffer first.
  uint16_t far *tmp = (uint16_t *)(buffer + ScrOffset( cursor_x, cursor_y ));

  for (uint16_t i=0; i < len; i++ ) { *tmp++ = fillAttr; }
  fillUsingWord( tmp, fillAttr, len );


  if ( updateRealScreen ) {

    // Same thing, but now on the real screen.

    uint16_t far *src = (uint16_t *)(Screen_base + ( (cursor_x<<1) + (cursor_y*bytesPerLine) ));
    if ( preventSnow ) { waitForCGARetraceLong( ); }
    for ( uint16_t i=0; i < len; i++ ) {
      *src++ = fillAttr;
    }

  }
  else {
    virtualUpdated = 1;
  }

}


void Screen::saveCursor( void ) {
  cursorSaveArea.cursor_x = cursor_x;
  cursorSaveArea.cursor_y = cursor_y;
  cursorSaveArea.curAttr = curAttr;
  cursorSaveArea.originMode = originMode;
  cursorSaveArea.autoWrap = autoWrap;
}


void Screen::restoreCursor( void ) {
  cursor_x = cursorSaveArea.cursor_x;
  cursor_y = cursorSaveArea.cursor_y;
  curAttr = cursorSaveArea.curAttr;
  originMode = cursorSaveArea.originMode;
  autoWrap = cursorSaveArea.autoWrap;
}
