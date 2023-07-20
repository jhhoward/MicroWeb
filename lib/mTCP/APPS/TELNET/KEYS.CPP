/*

   mTCP Keys.cpp
   Copyright (C) 2011-2023 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Keyboard handling routines shared by telnet and ymodem.

   Changes:

   2012-04-29: Split out from telnet when xmodem and ymodem were added

*/


#include <stdio.h>

#include "inlines.h"
#include "keys.h"


#ifdef TELNET_UNICODE

#include "unicode.h"

// Unicode composition handling.  Compose is a special mode where you
// enter a Unicode codepoint using four hex digits.

static bool    compose_active;    // Is Compose active?
static uint8_t unicode_bytes[5];  // Includes a terminating nul char for sscanf.
static uint8_t unicode_len;       // How many hex digits have we read?

#endif


Key_t getKey( void ) {

  Key_t rc;
  rc.action = K_NoKey;
  rc.local = 0;
  rc.normalKey = 0;

  uint16_t c = biosKeyRead( );

  // Special key?
  if ( (c & 0xff) == 0 ) {

    #ifdef TELNET_UNICODE
    if ( compose_active == true ) {
      // Hitting a special key in compose mode immediately ends compose mode.
      compose_active = false;
    }
    #endif

    uint8_t fkey = c>>8;

    switch ( fkey ) {

      case 15: { rc.action = K_Backtab;     break; }
      case 17: { rc.action = K_Alt_W;       rc.local = 1; break; }
      case 18: { rc.action = K_Alt_E;       rc.local = 1; break; }
      case 19: { rc.action = K_Alt_R;       rc.local = 1; break; }
      case 22: { rc.action = K_Alt_U;       rc.local = 1; break; }
      case 32: { rc.action = K_Alt_D;       rc.local = 1; break; }
      case 33: { rc.action = K_Alt_F;       rc.local = 1; break; }
      case 35: { rc.action = K_Alt_H;       rc.local = 1; break; }
      case 45: { rc.action = K_Alt_X;       rc.local = 1; break; }
      case 48: { rc.action = K_Alt_B;       rc.local = 1; break; }
      case 49: { rc.action = K_Alt_N;       rc.local = 1; break; }
      case 71: { rc.action = K_Home;        break; }
      case 72: { rc.action = K_CursorUp;    break; }
      case 73: { rc.action = K_PageUp;      rc.local = 1; break; }
      case 75: { rc.action = K_CursorLeft;  break; }
      case 77: { rc.action = K_CursorRight; break; }
      case 80: { rc.action = K_CursorDown;  break; }
      case 81: { rc.action = K_PageDown;    rc.local = 1; break; }
      case 82: { rc.action = K_Insert;      break; }
      case 83: { rc.action = K_Delete;      break; }

      #ifdef TELNET_UNICODE
      case 130: { // Alt-Minus (-)
        rc.action = K_Compose_Unicode;
        rc.local = 1;
        compose_active = true;
        unicode_len = 0;
        break;
      }
      #endif

    }

  } else {

    #ifdef TELNET_UNICODE
    if ( compose_active == true ) {

      // If the user hits ESC end compose mode early.
      if ( (c & 0xff) == 27 ) {

        rc.action = K_NoKey;
        rc.local = 0;
        rc.normalKey = 0;
        compose_active = false;

      } else {

        unicode_bytes[unicode_len++] = ( c & 0xff );

        if ( unicode_len == 4 ) {
          compose_active = false;
          int r = sscanf( (char *)unicode_bytes, "%x", &rc.unicode_cp );
          if ( r == 1 ) {
            rc.action = K_Unicode_CP;
            rc.normalKey = Unicode::findDisplayChar( rc.unicode_cp );
          } else {
            rc.action = K_NoKey;
            rc.local = 0;
            rc.normalKey = 0;
          }

        }

      }

      return rc;
    }
    #endif

    rc.action = K_NormalKey;
    rc.normalKey = ( c & 0xff );

    // Special enter key processing - we want to be able to tell the
    // difference between Enter and Ctrl-M.
    if ( rc.normalKey == 13 ) {
      if ( (c>>8) == 0x1c ) { // This was the Enter key.
        rc.action = K_Enter;
        rc.normalKey = 0;
      }
    }

  } // end if special key

  return rc;
}

