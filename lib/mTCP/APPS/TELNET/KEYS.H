
/*

   mTCP Keys.h
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


#ifndef _KEYS_H
#define _KEYS_H

#include "types.h"


// Define human readable symbols for our keys

#define K_NoKey        (0)
#define K_NormalKey    (1)
#define K_CursorUp     (2)
#define K_CursorDown   (3)
#define K_CursorLeft   (4)
#define K_CursorRight  (5)
#define K_PageUp       (6)
#define K_PageDown     (7)
#define K_Home         (8)
#define K_Insert       (9)
#define K_Delete      (10)
#define K_Backtab     (11)
#define K_Alt_R       (12)
#define K_Alt_W       (13)
#define K_Alt_H       (14)
#define K_Alt_X       (15)
#define K_Alt_B       (16)
#define K_Enter       (17)
#define K_Alt_E       (18)
#define K_Alt_N       (19)
#define K_Alt_D       (20)
#define K_Alt_U       (21)
#define K_Alt_F       (22)


typedef struct {
  uint8_t specialKey;  // K_NoKey, K_NormalKey, or a special key code
  uint8_t local;       // Interpreted locally by telnet?
  uint8_t normalKey;   // Valid only if specialKey = K_NormalKey
} Key_t;


Key_t getKey( void );



#endif
