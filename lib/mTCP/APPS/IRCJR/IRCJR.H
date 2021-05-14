
/*

   mTCP Ircjr.h
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


   Description: Some common defines and inline functions for IRCjr

   Changes:

   2011-05-27: Initial release as open source software

*/


#ifndef _IRCJR_H
#define _IRCJR_H



// Configuration

#define INPUT_ROWS (3)
#define SCBUFFER_MAX_INPUT_LEN (240)



// Common defines and utilities


extern void ERRBEEP( void );




#define normalizePtr( p, t ) {       \
  uint32_t seg = FP_SEG( p );        \
  uint16_t off = FP_OFF( p );        \
  seg = seg + (off/16);              \
  off = off & 0x000F;                \
  p = (t)MK_FP( (uint16_t)seg, off );          \
}

#define addToPtr( p, o, t ) {        \
  uint32_t seg = FP_SEG( p );        \
  uint16_t off = FP_OFF( p );        \
  seg = seg + (off/16);              \
  off = off & 0x000F;                \
  uint32_t p2 = seg << 4 | off ;       \
  p2 = (p2) + (o);                       \
  p = (t)MK_FP( (uint16_t)((p2)>>4), (uint16_t)((p2)&0xf) );          \
}




// Colors

extern uint8_t scErr;          // Error messages
extern uint8_t scNormal;       // Normal text
extern uint8_t scBright;       // Bright/Bold
extern uint8_t scReverse;      // Black on white
extern uint8_t scServerMsg;    // Message from the IRC server
extern uint8_t scUserMsg;      // Input from the local user
extern uint8_t scOtherUserMsg; // Message from an IRC user
extern uint8_t scActionMsg;    // Used for CTCP ACTION
extern uint8_t scTitle;        // Title - used only at startup
extern uint8_t scLocalMsg;     // For locally injected messages (like help, stats)

extern uint8_t scBorder;       // Border lines on help window
extern uint8_t scCommandKey;   // Used in help menu


extern uint8_t ColorScheme;


extern uint8_t mIRCtoCGAMap[];




#endif
