/*

   mTCP NetDrive (REQHDR.H)
   Copyright (C) 2023-2025 Michael B. Brutman (mbbrutman@gmail.com)
   mTCP web page: http://www.brutman.com/mTCP/mTCP.html


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


   Description: DOS Request Header and BPB data structures.

*/


#include "types.h"


// http://www.oldlinux.org/Linux.old/distributions/cnix/FAT.pdf
// https://www.eit.lth.se/fileadmin/eit/courses/eitn50/Literature/fat12_description.pdf
//



// Device Attribute Bits, Character devices

//  0: 1 = STDIN
//  1: 1 = STDOUT
//  2: 1 = NUL
//  3: 1 = Clock device
//  4: 1 = special device; fast character output
//  5: Reserved, must be 0
//  6: 1 = Generic IOCTL supported (command 0x13)
//  7: 1 = Query IOCTL supported (command 0x19)
//  8: Reserved, must be 0
//  9: Reserved, must be 0
// 10: Reserved, must be 0
// 11: 1 = Device supports Open and Close (commands 0x0d and 0x0e)
// 12: Reserved, must be 0
// 13: 1 = Device supports Output until Busy (command 0x10)
// 14: 1 = Device supports IOCTL Read and Write (commands 0x03 and 0x0c)
// 15: 1 = Character device

// Device Attribute Bits, Block devices

//  0: Reserved, must be 0
//  1: 1 = Driver supports 32 bit sector addressing
//  2: Reserved, must be 0
//  3: Reserved, must be 0
//  4: Reserved, must be 0
//  5: Reserved, must be 0
//  6: 1 = Driver supports Generic IOCTL, Get Logical Device and Set Logical Device
//  7: 1 = Query IOCTL supported (command 0x19)
//  8: Reserved, must be 0
//  9: Reserved, must be 0
// 10: Reserved, must be 0
// 11: 1 = Device supports Open and Close and Removable Media (commands 0x0d, 0x0e, and 0x0f)
// 12: Reserved, must be 0
// 13: 1 = Driver requires DOS to supply the first 512 bytes of the FAT when calling Build BPB
// 14: 1 = Device supports IOCTL Read and Write (commands 0x03 and 0x0c)
// 15: 0 = Block device


// Required commands for block device drivers
//
// 0x00 Init
// 0x01 Media Check
// 0x02 Build BPB
// 0x03 IOCTL Read (if Device Attr Bit 14 is set)
// 0x04 Read
// ...
// 0x08 Write
// 0x09 Write with Verify
// ...
// 0x0C IOCTL Write (if Device Attr Bit 14 is set)
// ...
// More stuff I'm not going to implement.


#pragma pack (1)


// Common Request Header
typedef struct {
  uint8_t    len;                // Req packet length
  uint8_t    unitCode;           // Block devices only
  uint8_t    command;            // Command
  uint16_t   status;             // Returned by the driver
  uint8_t    reserved[8];
} ReqHdr_t;



// Init 0x00
//
// breakOffset/breakSegment has an input value too but we probably won't use it.  See DOS 5.
// bpbOffset/bpbSegment has an input value too but we probably won't use it.  See DOS 5.
// errMsgFlag will probably not be used.

typedef struct {
  ReqHdr_t   rh;
  uint8_t    units;              // Output: number of units supported
  uint16_t   breakOffset;        // Output: end of resident code 
  uint16_t   breakSegment;       // Output: end of resident code
  uint16_t   bpbOffset;          // Output: Offset of Pointer to array of BPB pointers
  uint16_t   bpbSegment;         // Output: Segment of Pointer to array of BPB pointers
  uint8_t    firstAvailDrive;    // DOS 3+, Input: first drive number
  uint16_t   errMsgFlag;         // DOS 5?, Output: error message flag
} ReqHdr_Init_t;



// Media Check 0x01
//
// mediaDesc will probably always be F8 (hard disk, any capacity)
// mediaStatus: 0xff Media changed, 0x00 Can't tell, 0x01 Not changed.
// volIdPtr: If there is no volume identifier use "NO NAME".

typedef struct {
  ReqHdr_t   rh;
  uint8_t    mediaDesc;          // Media descriptor from Drive Parm Block
  uint8_t    mediaStatus;        // Return code
  uint32_t   volIdPtr;           // DOS 5?, Output: volume ID pointer to zero terminated string.
} ReqHdr_MediaChk_t;



// Build BPB 0x02
//
// mediaDesc will probably always be F8 (hard disk, any capacity)
// bufOffset/bufSegment:
//   If DeviceAttr bit 13 is zero, buffer has first sector of the FAT and should not be edited.
//   If DeviceAttr bit 13 is one, this is scratch space.

typedef struct {
  ReqHdr_t   rh;
  uint8_t    mediaDesc;          // Input: Media descriptor from Drive Parm Block
  uint16_t   bufOffset;          // Input: Data transfer buffer offset (first FAT sector?)
  uint16_t   bufSegment;         // Input: Data transfer buffer segment (first FAT sector?)
  uint16_t   bpbOffset;          // Offset of Pointer to BPB array
  uint16_t   bpbSegment;         // Segment of Pointer BPB array
} ReqHdr_GetBPB_t;



// IOCTL Read 0x03
//
// The device driver book says there is a starting sector field after
// the byte count field.  The Microsoft book doesn't talk about it.
// We might trip on this when we convert to a block device.

typedef struct {
  ReqHdr_t   rh;
  uint8_t    mediaDesc;          // Block devices only?
  uint16_t   bufOffset;          // Input: offset of buffer to fill
  uint16_t   bufSegment;         // Input: segment of buffer to fill
  uint16_t   bytesRead;          // Input: Bytes to read, Output: Bytes read from device driver
} ReqHdr_IOCTL_Read_t;


// Read 0x04
//
// hugeStart is only used if Device Attributes Bit 1 is set.
//
// https://faydoc.tripod.com/structures/25/2597.htm: Use the RH length to see which variant
// you are working with:
//
// SmallDOS: RH length = 0x16, use start
// Later MSDOS: RH length = 0x1E, if start = 0xFFFF use hugeStart, otherwise use start.
// Compaq 3.31 and DR DOS 6: See the link above.

typedef struct {
  ReqHdr_t   rh;
  uint8_t    mediaDesc;          // Media descriptor from Drive Parm Block
  uint16_t   bufOffset;          // Data transfer buffer offset
  uint16_t   bufSegment;         // Data transfer buffer segment
  uint16_t   count;              // Input: Bytes/Sectors to read, Output: Bytes/Sectors read
  uint16_t   start;              // Start sector number (block devices only)
  uint32_t   volIdPtr;           // DOS 5?, Output: volume ID pointer to zero terminated string.
  uint32_t   hugeStart;          // If start = 0xFFFF use this, otherwise use start.
} ReqHdr_Input_t;


typedef struct {
  ReqHdr_t   rh;
  uint8_t    mediaDesc;          // Media descriptor from Drive Parm Block
  uint16_t   bufOffset;          // Data transfer buffer offset
  uint16_t   bufSegment;         // Data transfer buffer segment
  uint16_t   count;              // Transfer count (sectors if block device)
  uint16_t   start;              // Start sector number (block devices only)
} ReqHdr_Output_t;




// For small drives totalSectors is 65535 or less.  For large drives set it
// to zero and use hugeSectors.
//
// Suggested DOS 2 geometries:
//   512 *  1 * 4096 = 2MB (approx)
//   512 *  2 * 4096 = 4MB
//   512 *  4 * 4096 = 8MB
//   512 *  8 * 4096 = 16MB
//   512 * 16 * 4096 = 32MB


typedef struct {
  uint16_t   bytesPerSector;         // Set to 512 to be like a hard drive.
  uint8_t    sectorsPerCluster;      // Actually signed so the max is 64.
  uint16_t   reservedSectors;        // Usually just 1 for the boot sector?
  uint8_t    fatTables;              // Set to 1.
  uint16_t   rootDirEntries;         // Set to 128?
  uint16_t   totalSectors;
  uint8_t    mediaDesc;              // Set to F8
  uint16_t   fatSectors;             // What's a good rule of thumb here?
  uint16_t   sectorsPerTrack;        // DOS 2: Optional, not used by DOS
  uint16_t   heads;                  // DOS 2: Optional, not used by DOS
  union {
    struct {
    uint16_t hiddenSectorsSmall;     // DOS 2 DOS 3: Optional: Two bytes for hidden sectors
    uint16_t notPresent;             // Nothing here; do not touch.  Not yours.
    } smallDos;
    struct {
      uint32_t hiddenSectorsLarge;   // DOS 4+
      uint32_t hugeSectors;          // DOS 4+ If totalSectors is 0 use this.
    } largeDos;
  } u;
} BPB_t;


