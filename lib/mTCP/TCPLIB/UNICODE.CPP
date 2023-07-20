/*

   mTCP Unicode.CPP
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

#include "unicode.h"

#include <stdio.h>
#include <string.h>


// A hash table that maps Unicode codepoints to our local character set.
// Remember, we are only dealing with Plane 0 so we can use 16 bit values.

Unicode::Codepoint_Mapping_t Unicode::XlateTable[XlateTableLen];
uint16_t Unicode::XlateTableItems = 0;

// The inverse mapping - given a high bit ASCII character what Unicode
// character should we send instead?  (The low bit ASCII characters are
// sent as is so we don't need a mapping for them.)
//
// Mutiple Unicode code points can be mapped to a code page glyph, but
// a code page glyph should only map to one Unicode code point.  We
// use the first Unicode code point to appear for a code page glyph.
small_cp_t Unicode::UpperAsciiCodepoints[128];



// File format:
//
// - Blank lines are allowed
// - Comment lines start with the # character.
// - Mappings are a Unicode code point to a local code page glyph, specified
//   in hex.  Unicode code points are 16 bits while local code page glyphs
//   are eight bits.

void Unicode::loadXlateTable( const char *filename ) {

  memset( XlateTable, 0, sizeof(XlateTable));
  XlateTableItems = 0;

  memset( UpperAsciiCodepoints, 0, sizeof(UpperAsciiCodepoints) );

  FILE *tableFile = fopen( filename, "r" );
  if ( tableFile == NULL ) {
    puts( "Error opening table file." );
    return;
  }

  char lineBuffer[100];

  while ( !feof( tableFile ) ) {

    if ( fgets( lineBuffer, 100, tableFile ) != lineBuffer ) {
      break;
    }

    // Skip blank lines or comments.
    if ( (lineBuffer[0] != '\n') && (lineBuffer[0] != '#') ) {

      int u, c;
      int rc = sscanf( lineBuffer, "%x %x", &u, &c );

      if ( rc == EOF ) {
        // eof, no problem
      } else if (rc != 2 ) {
        printf( "Failed to read two values on a line." );
      } else {

        addToXlateTable( u, c );

        // Local upper bit value to Unicode: store the first Unicode
        // codepoint to use.
        if ( (c > 127) && (UpperAsciiCodepoints[c-128] == 0) ) {
          UpperAsciiCodepoints[c-128] = u;
        }
      }

    } // Not a blank line or a comment.

  }

  fclose( tableFile );

}



void Unicode::addToXlateTable( small_cp_t u, uint8_t c ) {

  // Don't do anything if the table is full.
  //
  // Note that we always leave one empty slot in the table to make it
  // easy to know when to terminate a search.
  if ( XlateTableItems == (XlateTableLen-1) ) {
    return;
  }

  uint16_t bucket = getStartBucket( u );

  // Find the first empty hole.  This is guaranteed to end if we don't
  // completely fill the table and leave one slot open.
  while ( XlateTable[bucket].codepoint != 0 ) {

    if ( XlateTable[bucket].codepoint == u ) {
      // This codepoint has already been mapped to something.  There is
      // nothing to do.
      return;
    }

    bucket++;
    if ( bucket == XlateTableLen ) bucket = 0;
  }

  XlateTable[bucket].codepoint = u;
  XlateTable[bucket].display = c;
  XlateTableItems++;

}



// Given a Unicode cp, tell us which local char to put on the screen.
// If the char is not in our mapping table we return the "tofu"
// character, which indicates the code was valid but we don't have
// a glyph for it.

uint8_t Unicode::findDisplayChar( small_cp_t u ) {

  uint16_t bucket = getStartBucket( u );

  // Never fill the last element of the table.  If you do this condition
  // will never be false and you will spin forever.
  while ( XlateTable[bucket].codepoint != 0 ) {

    if ( XlateTable[bucket].codepoint == u ) {
      return XlateTable[bucket].display;
    }

    bucket++;
    if ( bucket == XlateTableLen ) bucket = 0;
  }

  // We didn't find it ..  return the "tofu" character.
  return Tofu;

}



// Our 16 bit machines hate 32 bit values.  And our compiler tries but
// fails at certain simple things, like bit shifting 32 bit values.  So
// as painful as this is, split the Unicode codepoint into two sixteen
// bit values we can more easily work with.

typedef union {
  uint32_t r;
  struct {
    uint16_t s0;
    uint16_t s1;
  } s;
} u;


uint8_t Unicode::decodeUTF8( const uint8_t *s, unicode_cp_t *cp ) {

  uint8_t c0 = s[0];

  u rc;
  rc.r = 0;

  if ( (c0 & 0xE0) == 0xC0 ) {
    rc.s.s0 = (s[1] & 0x3F) | ((c0 & 0x1f) << 6);
    *cp = rc.r;
    return 2;

  } else if ( (c0 & 0xF0) == 0xE0 ) {
    rc.s.s0 = (s[2] & 0x3F) | ((s[1] & 0x3F) << 6) | (c0 << 12);
    *cp = rc.r;
    return 3;
  } else if ( (c0 & 0xF8) == 0xF0 ) {
    rc.s.s1 = ((s[1] & 0x3F) >> 4) | ((c0 & 0x07) << 2);
    rc.s.s0 = (s[3] & 0x3F) | ((s[2] & 0x3F) << 6) | (s[1] << 12);
    *cp = rc.r;
    return 4;
  }

  // Invalid ...

  *cp = 0xFFFFFFFF;
  return 1;
}



uint8_t Unicode::encodeUTF8( unicode_cp_t cp, uint8_t *buffer ) {

  u t;
  t.r = cp;

  if ( t.s.s1 == 0 ) {
    if ( t.s.s0 < 0x80 ) {
      buffer[0] = t.s.s0;
      return 1;
    } else if ( t.s.s0 < 0x800 ) {
      buffer[0] = 0xc0 | ((t.s.s0 & 0x7c0) >> 6);
      buffer[1] = 0x80 | (t.s.s0 & 0x3f);
      return 2;
    } else {
      buffer[0] = 0xe0 | ((t.s.s0 & 0xf000) >> 12);
      buffer[1] = 0x80 | ((t.s.s0 & 0x0fc0) >> 6);
      buffer[2] = 0x80 | (t.s.s0 & 0x3f);
      return 3;
    }
  } else {
    buffer[0] = 0xF0 | ((t.s.s1 & 0x7c) >> 2);
    buffer[1] = 0x80 | ((t.s.s1 & 0x03) << 4) | ((t.s.s0 & 0xf000) >> 12);
    buffer[2] = 0x80 | ((t.s.s0 & 0x0fc0) >> 6);
    buffer[3] = 0x80 | (t.s.s0 & 0x3f);
    return 4;
  }

}




#ifdef TEST_UNICODE


// To compile ...
//
// Watcom: wcl -0 -s -i=../INCLUDE UNICODE.CPP -DTEST_UNICODE
// g++: g++ -I ../INCLUDE UNICODE.CPP -DTEST_UNICODE
// 
//
// To test:
//
// a.exe <path_to_mapping_file> <sample_file_in_utf-8>


void Unicode::analyzeHashTable( void ) {

  printf( "Hashtable entries: %d, max entries: %d\n", XlateTableItems, XlateTableLen );

  // For every mapping in the table do a lookup and see how many comparison
  // operations it took.

  int bucketsWithCollisions = 0;
  int longestChain = 0;

  for ( int i=0; i < XlateTableLen; i++ ) {

    if ( XlateTable[i].codepoint != 0 ) {

      // This is a valid target.
      small_cp_t target = XlateTable[i].codepoint;

      uint16_t bucket = getStartBucket( target );

      if ( XlateTable[bucket].codepoint == target ) {

        // Hit it on the first try.

      } else {

        bucketsWithCollisions++;
        int chainLen = 0;

        while ( XlateTable[bucket].codepoint != 0 ) {

          chainLen++;
          if ( XlateTable[bucket].codepoint == target ) {
            break;
          }

          bucket++;
          if ( bucket == XlateTableLen ) bucket = 0;
        }

        if (chainLen > longestChain) longestChain = chainLen;

      }

    }
  }

  printf("Collisions: %d, Longest chain of collisions: %d\n", bucketsWithCollisions, longestChain );

  for ( int i=0; i < XlateTableLen; i++ ) {
    if ( XlateTable[i].codepoint == 0 ) {
      printf(".");
    } else {
      printf("X");
    }
  }
  puts("");

}


void testEncode( void );
void testFileRead( char * );


uint8_t lineBuffer[1024];


int main( int argc, char *argv[] ) {

  Unicode::loadXlateTable( argv[1] );
  Unicode::analyzeHashTable( );

  testFileRead( argv[2] );
  testEncode( );

  return 0;
}



void testFileRead( char *filename ) {

  FILE *f = fopen( filename, "r" );
  if ( f == NULL ) {
    puts( "Error opening file." );
    return;
  }

  while ( !feof(f) ) {

    if ( fgets( ( char *)lineBuffer, 1024, f ) == NULL ) {
      break;
    }

    uint8_t *i = lineBuffer;
    while ( *i ) {

      if ( *i < 0x80 ) {
        putchar( *i );
        i++;
      } else {
        unicode_cp_t cp;
        int l = Unicode::decodeUTF8( i, &cp );
        i = i + l;
        putchar( Unicode::findDisplayChar(cp) );
      }
    }

  }

  fclose(f);
}




// Test the ability to encode a Unicode codepoint into UTF-8.


typedef struct {
  unicode_cp_t testCp;
  uint8_t len;
  uint8_t bytes[4];
} Test_rec_t;

// Test table. Generated with https://www.cogsci.ed.ac.uk/~richard/utf-8.cgi
Test_rec_t EncodeTest[] = {
  { 0x24,     1, 0x24,    0,    0, 0 },
  { 0x7f,     1, 0x7f,    0,    0, 0 },
  { 0x80,     2, 0xc2, 0x80,    0, 0 },
  { 0xa3,     2, 0xc2, 0xa3,    0, 0 },
  { 0x7ff,    2, 0xdf, 0xbf,    0, 0 },
  { 0x800,    3, 0xe0, 0xa0, 0x80, 0 },
  { 0x939,    3, 0xe0, 0xa4, 0xb9, 0 },
  { 0x20ac,   3, 0xe2, 0x82, 0xac, 0 },
  { 0xd55c,   3, 0xed, 0x95, 0x9c, 0 },
  { 0xffff,   3, 0xef, 0xbf, 0xbf, 0 },
  { 0x10000,  4, 0xf0, 0x90, 0x80, 0x80 },
  { 0x10348,  4, 0xf0, 0x90, 0x8d, 0x88 },
  { 0x10FFFF, 4, 0xf4, 0x8f, 0xbf, 0xbf },
  { 0, 0, 0, 0, 0 }
};

void testEncode( void ) {

  for ( int i=0; EncodeTest[i].len !=0; i++ ) {
    lineBuffer[0] = 0; lineBuffer[1] = 0; lineBuffer[2] = 0; lineBuffer[3] = 0;
    int rc = Unicode::encodeUTF8( EncodeTest[i].testCp, lineBuffer );
    if ( rc == EncodeTest[i].len ) {
      bool passed = true;
      for ( int j=0; j<rc; j++ ) {
        if ( EncodeTest[i].bytes[j] != lineBuffer[j] ) {
          passed = false;
          break;
        }
      }
      printf( "Unicode 0x%06lx: 0x%02x 0x%02x 0x%02x 0x%02x %s\n",
          EncodeTest[i].testCp, lineBuffer[0], lineBuffer[1], lineBuffer[2], lineBuffer[3],
          passed ? "passed" : "failed" );
    }
  }

}


#endif
