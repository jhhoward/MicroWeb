/*

   mTCP Unicode.H
   Copyright (C) 2023 Michael B. Brutman (mbbrutman@gmail.com)


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


   Description: Unicode support for mTCP, or at least the parts of
     mTCP that choose to use it.

*/


#ifndef _UNICODE_H
#define _UNICODE_H



#ifdef TEST_UNICODE
#if defined(__WATCOMC__) || defined(__WATCOM_CPLUSPLUS__)
typedef unsigned char uint8_t;     //  8 bit int, range 0 to 255
typedef unsigned int  uint16_t;    // 16 bit int, range 0 to 64K
typedef unsigned long uint32_t;    // 32 bit int, range 0 to 4GB
#else
#include <sys/types.h>
#endif
#else
#include "types.h"
#endif

#ifdef TEST_UNICODE
#include <assert.h>
#endif


// Unicode to codepage mapping types
//
// Your standard old PC has 256 characters built into the ROM, with the
// first half looking like US-ASCII and the second half having some
// additional Latin-based characters, line drawing characters, and
// symbols.  It is inadequate.  That led to swappable code pages, which
// were still inadequate.  Unicode is the accepted solution to this now.
//
// However, Unicode isn't free.  And unless you are using a bitmapped
// graphics display, you are still stuck with what your computer can
// display.  Even with swappable code pages things are still limited.
//
// This code tries to improve things by allowing you to decode UTF-8
// (a common encoding for data) and map it to a character efficiently
// using a hash table.
//
// The actual mapping from Unicode to your code page is not defined
// here; that's done in a separate file that is read at run-time.  A
// sample file that maps Unicode to CP437 is provided.
//
// To make things compact this uses a 16 bit value for the codepoint.
// That limits the Unicode support to Plane 0, the Basic Multilingual
// Plane.  This should be more than enough to cover our needs.  If you
// need the full range then you can change small_cp_t to be the same as
// unicode_cp_t (32 bits), but obviously that costs more space and
// performance.


typedef unsigned long unicode_cp_t;  // Actual Unicode codepoint type
typedef unsigned int  small_cp_t;    // What we use to save space.


class Unicode {

  public:

    static void    loadXlateTable( const char *filename );
    static void    addToXlateTable( small_cp_t u, uint8_t c );

    // Remember, even though these handle the full range of Unicode,
    // the other functions only deal with Plane 0.
    static uint8_t decodeUTF8( const uint8_t *s, unicode_cp_t *cp );
    static uint8_t encodeUTF8( unicode_cp_t, uint8_t *buffer );

    static bool     XlateTableLoaded( void ) { return XlateTableItems > 0; }
    static uint16_t getXlateTableMappings( void ) { return XlateTableItems; }

    // Given a Unicode cp tell us which glyph to put on the screen.
    static uint8_t findDisplayChar( small_cp_t u );


    // Given a local char what Unicode codepoint should we use?
    // 7 bit ASCII gets sent as-is.  High bit ASCII gets mapped
    // to the first Unicode codepoint in the translation table.
    //
    // If you use this you will probably use it while scanning outgoing
    // strings, finding the Unicode for each high bit ASCII value and
    // then converting it to UTF-8 to put in a buffer.

    static small_cp_t charToUnicode( uint8_t c ) {
      if ( c < 0x80 ) {
        return c;
      } else {
        return Unicode::UpperAsciiCodepoints[c-128];
      }
    }

    #ifdef TEST_UNICODE
    static void analyzeHashTable( void );
    #endif

  private:

    static const int XlateTableLen = 512;  // Max length of the mapping table.
    static const uint8_t Tofu = 0xfe;      // Displayed when no glyph is available.

    static inline uint16_t getStartBucket( small_cp_t u ) {
      uint16_t rc = ((u * 158lu) & 0x03fe) >> 1;
      #ifdef TEST_UNICODE
      assert( rc < XlateTableLen );
      #endif
      return rc;
    }


    #pragma pack ( push ) ;
    #pragma pack ( 1 ) ;
    typedef struct {
      small_cp_t codepoint;
      uint8_t    display;
    } Codepoint_Mapping_t;
    #pragma pack ( pop ) ;
    #pragma pack ( 1 ) ;

    static Codepoint_Mapping_t XlateTable[];
    static uint16_t XlateTableItems;

    static small_cp_t UpperAsciiCodepoints[128];

};


#endif
