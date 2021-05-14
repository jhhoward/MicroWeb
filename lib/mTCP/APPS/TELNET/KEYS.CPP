
/*

   mTCP Keys.cpp
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


   Description: Keyboard handling routines shared by telnet and ymodem.

   Changes:

   2012-04-29: Split out from telnet when xmodem and ymodem were added

*/


#include <bios.h>

#include "keys.h"


Key_t getKey( void ) {

  Key_t rc;
  rc.specialKey = K_NoKey;
  rc.local = 0;
  rc.normalKey = 0;

  uint16_t c = bioskey(0);

  // Special key?
  if ( (c & 0xff) == 0 ) {

    uint8_t fkey = c>>8;

    switch ( fkey ) {

      case 15: { rc.specialKey = K_Backtab;     break; }
      case 17: { rc.specialKey = K_Alt_W;       rc.local = 1; break; }
      case 18: { rc.specialKey = K_Alt_E;       rc.local = 1; break; }
      case 19: { rc.specialKey = K_Alt_R;       rc.local = 1; break; }
      case 22: { rc.specialKey = K_Alt_U;       rc.local = 1; break; }
      case 32: { rc.specialKey = K_Alt_D;       rc.local = 1; break; }
      case 33: { rc.specialKey = K_Alt_F;       rc.local = 1; break; }
      case 35: { rc.specialKey = K_Alt_H;       rc.local = 1; break; }
      case 45: { rc.specialKey = K_Alt_X;       rc.local = 1; break; }
      case 48: { rc.specialKey = K_Alt_B;       rc.local = 1; break; }
      case 49: { rc.specialKey = K_Alt_N;       rc.local = 1; break; }
      case 71: { rc.specialKey = K_Home;        break; }
      case 72: { rc.specialKey = K_CursorUp;    break; }
      case 73: { rc.specialKey = K_PageUp;      rc.local = 1; break; }
      case 75: { rc.specialKey = K_CursorLeft;  break; }
      case 77: { rc.specialKey = K_CursorRight; break; }
      case 80: { rc.specialKey = K_CursorDown;  break; }
      case 81: { rc.specialKey = K_PageDown;    rc.local = 1; break; }
      case 82: { rc.specialKey = K_Insert;      break; }
      case 83: { rc.specialKey = K_Delete;      break; }

    }

  }
  else {

    rc.specialKey = K_NormalKey;
    rc.normalKey = ( c & 0xff );

    // Special enter key processing - we want to be able to tell the
    // difference between Enter and Ctrl-M.
    if ( rc.normalKey == 13 ) {
      if ( (c>>8) == 0x1c ) { // This was the Enter key.
        rc.specialKey = K_Enter;
        rc.normalKey = 0;
      }
    }

  }

  return rc;
}

