
/*

   mTCP Patch.cpp
   Copyright (C) 2010-2020 Michael B. Brutman (mbbrutman@gmail.com)
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


   Description: Patch utility to fix some undesirable parts
     in the Watcom runtime.

   Changes:

   2011-05-27: Initial release as open source software

*/

#include <stdio.h>
#include <string.h>





int nheapgrowFound = 0;
int isNonIBMFound = 0;

unsigned long int targetSeg, targetOff;    // location of __CMain_nheapgrow_ symbol
unsigned long int targetSeg2, targetOff2;  // location of __is_nonIBM_ symbol

char exeFilename[40];
char mapFilename[40];



unsigned char buffer[512];




int readMapFile( void ) {

  puts( "  Reading map file" );
  fflush( NULL );

  FILE *mapFile = fopen( mapFilename, "r" );
  if ( mapFile == NULL ) {
    puts( "  Map file not found" );
    return 1;
  }

  while ( !feof( mapFile ) ) {

    char buffer[256];

    if ( fgets( buffer, 255, mapFile ) == NULL ) {
      break;
    }

    char tmpFuncName[256];

    unsigned long int tSeg, tOff;

    if ( sscanf( buffer, "%lx:%lx %s\n", &tSeg, &tOff, tmpFuncName ) == 3  ) {
      if ( strcmp( tmpFuncName, "__CMain_nheapgrow_" ) == 0 ) {
        targetSeg = tSeg; targetOff = tOff;
        printf( "  Found __CMain_nheapgrow_ in map file at %04x:%04x\n", targetSeg, targetOff );
        nheapgrowFound = 1;
      }
    }

    if ( sscanf( buffer, "%lx:%lx+ %s\n", &tSeg, &tOff, tmpFuncName ) == 3  ) {
      if ( strcmp( tmpFuncName, "__is_nonIBM_" ) == 0 ) {
        targetSeg2 = tSeg; targetOff2 = tOff;
        printf( "  Found __is_nonIBM_ in map file at %04x:%04x\n", targetSeg2, targetOff2 );
        isNonIBMFound = 1;
      }
    }

    
  }

  fclose( mapFile );

  return 0;
}



int main( int argc, char *argv[] ) {

  puts( "Patch" );
  fflush( NULL );

  if ( argc < 4 ) {
    printf( "Format: %s file.exe file.map memory_model\n", argv[0] );
    return 1;
  }


  strcpy( exeFilename, argv[1] );
  strcpy( mapFilename, argv[2] );
  
  printf( "Patching: %s\n", exeFilename );

  if ( readMapFile( ) ) {
    printf( "  Error reading map file or symbol not found\n" );
    return 1;
  }

  if ( !nheapgrowFound && !isNonIBMFound ) {
    puts("  Nothing to do!" );
    return 0;
  }


  fflush( NULL );
  FILE *myExe = fopen( exeFilename, "r+b" );

  if ( myExe == NULL ) {
    perror( "  File open error" );
    return 1;
  }


  int rc = fread( buffer, 1, 256, myExe );
  if ( rc < 0 ) {
    perror( "  Error reading header" );
    printf( "  Return code: %d\n", rc );
    return 1;
  }
  printf( "  %d bytes read from header\n", rc );

  if ( buffer[0] != 0x4d || buffer[1] != 0x5a ) {
    printf( "  %x %x\n", buffer[0], buffer[1] );
    puts( "  Wrong magic number" );
    return 1;
  }

  unsigned long int headerSize = (buffer[9]*256 + buffer[8]) * 16lu;
  unsigned long int initialCodeSegment = buffer[0x17]*256 + buffer[0x16];

  if ( targetSeg != initialCodeSegment ) {
    printf( "  Target segment %x doesn't match initial code segment; didn't plan for this\n", targetSeg, initialCodeSegment );
    return 1;
  }

  printf( "  Header size in bytes: %lu\n", headerSize );
  printf( "  Code Segment offset: %lu\n", initialCodeSegment );


  if ( nheapgrowFound ) {

    // Advance to start of code
    if ( fseek( myExe, headerSize, SEEK_SET ) ) { perror( "  Seek error" ); return 1; }

    // Move to start of our code segment (relative to start of code)
    if ( fseek( myExe, initialCodeSegment*16ul, SEEK_CUR ) ) { perror( "  Seek error" ); return 1; }

    // Move to our specific code offset (relative to code segment)
    if ( fseek( myExe, targetOff, SEEK_CUR ) ) { perror( "  Seek error" ); return 1; }

    // Remember where we are.
    long int absPos = ftell( myExe );

    if ( fread( buffer, 1, 256, myExe ) != 256 ) { perror( "  Error reading target area" ); return 1; }

    printf( "  Bytes at target: %x %x %x\n", buffer[0], buffer[1], buffer[2] );

    if ( buffer[0] != 0xe9 ) {
      printf( "  Expected byte to be E9, was %x\n", buffer[0] );
      return 1;
    }

    if ( stricmp( argv[3], "-ml" ) == 0 ) {
      buffer[0] = 0xcb;
      puts( "  Patching with a far return" );
    }
    else if ( stricmp( argv[3], "-mc" ) == 0 ) {
      buffer[0] = 0xc3;
      puts( "  Patching with a near return" );
    }
    else {
      puts( "  Not patching; unsupported memory model" );
    }

    // Go back to the code offset to patch it
    fseek( myExe, absPos, SEEK_SET );

    fwrite( buffer, 1, 1, myExe );

  }


  // Start again for __is_nonIBM_

  if ( isNonIBMFound ) {

    puts( "  Fixing __is_nonIBM_" );

    // Advance to start of code
    if ( fseek( myExe, headerSize, SEEK_SET ) ) { perror( "  Seek error" ); return 1; }

    // Move to start of our code segment (relative to start of code)
    if ( fseek( myExe, initialCodeSegment*16ul, SEEK_CUR ) ) { perror( "  Seek error" ); return 1; }

    // Move to our specific code offset (relative to code segment)
    if ( fseek( myExe, targetOff2, SEEK_CUR ) ) { perror( "  Seek error" ); return 1; }

    // Remember where we are.
    long int absPos = ftell( myExe );

    if ( fread( buffer, 1, 256, myExe ) != 256 ) { perror( "  Error reading target area" ); return 1; }

    printf( "  Bytes at target: %x %x %x\n", buffer[0], buffer[1], buffer[2] );

    if ( buffer[0] != 0x53 || buffer[1] != 0x51 || buffer[2] != 0x52 ) {
      printf( "Expected bytes to be 53, 51, 52 (PUSH BX, PUSH CX, PUSH DX), was %x %x %x\n", buffer[0], buffer[1], buffer[2] );
      return 1;
    }

    buffer[0] = 0xb8; buffer[1] = 0x0; buffer[2] = 0x0;

    if ( stricmp( argv[3], "-ml" ) == 0 ) {
      buffer[3] = 0xcb;
      puts( "  Patching with a far return" );
    }
    else if ( stricmp( argv[3], "-mc" ) == 0 ) {
      buffer[3] = 0xc3;
      puts( "  Patching with a near return" );
    }
    else {
      puts( "  Not patching; unsupported memory model" );
    }


    // Go back to the code offset to patch it
    fseek( myExe, absPos, SEEK_SET );
    fwrite( buffer, 1, 4, myExe );
  }


  printf( "All good!\n" );

  fclose( myExe );

  return 0;
}
