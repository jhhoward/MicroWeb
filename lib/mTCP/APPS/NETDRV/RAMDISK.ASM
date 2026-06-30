; mTCP NetDrive (RAMDISK.ASM)
; Copyright (C) 2023-2025 Michael B. Brutman (mbbrutman@gmail.com)
; mTCP web page: http://www.brutman.com/mTCP/mTCP.html
;
;
; This file is part of mTCP.
;
; mTCP is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; mTCP is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with mTCP.  If not, see <http://www.gnu.org/licenses/>.
;
; Description: RAM disk related data structures.



; Fake RAM disk for when we are not connected.

                   even

; Repeat an offset to the RamDiskBPB for each configured unit
RamDiskBPBArray    dw         Max_Units dup(RamDiskBPB)

RamDisk            db         0xeb
                   db         0xfe
                   db         0x90
                   db         'RamDisk '      ; OEM identity field
RamDiskBPB         dw         0x40            ; bytes per sector
                   db         0x01            ; sectors per cluster
                   dw         0x01            ; reserved sectors
                   db         0x01            ; number of FATs
                   dw         0x02            ; root directory entries
                   dw         0x04            ; sectors = 64 KB/secsize
                   db         0xfc            ; media descriptor
                   dw         0x01            ; sectors per FAT

; Needed for later versions of DOS

                   dw         0x0000          ; DOS 3 sectors per track
                   dw         0x0000          ; heads
                   dw         0x0000          ; hidden sectors (lo)
                   dw         0x0000          ; hidden sectors (hi)
                   dw         0x0004          ; huge sectors (lo)
                   dw         0x0000          ; huge sectors (hi)
                   db         0x00            ; drive index
                   db         0x00            ; reserved
                   db         0x29            ; vol signature
                   dw         0x0000          ; serial number (lo)
                   dw         0x0000          ; serial number (hi)
                   db         'NO NAME    '
                   db         'FAT12   '

                   db         2 dup (0)      ; Fill out the first sector

FAKEFAT            db         0xfc
                   db         0xff
                   db         0xff
                   db         0xff
                   db         0x0f
                   db         0x3b dup (0)    ; Fill out the second sector

                   db         'NOTATTACHED'
                   db         0x08            ; Attribute byte
                   db         0x00            ; Reserved
                   db         0x00            ; Creation time in tenths
                   dw         0x6000          ; Creation time
                   dw         0x5741          ; Creation date
                   dw         0x5741          ; Last accessed date
                   dw         0x00            ; FAT32 use
                   dw         0x6000
                   dw         0x5741          ; date
                   db         0x06 dup(0)     ; reserved

                   db         'README  TXT'
                   db         0x01            ; Read only
                   db         0x00            ; Reserved
                   db         0x00            ; Creation time in tenths
                   dw         0x6000          ; Creation time
                   dw         0x5741          ; Creation date
                   dw         0x5741          ; Last accessed date
                   dw         0x00            ; FAT32 use
                   dw         0x6000          ; Last modification time
                   dw         0x5741          ; Last modification date
                   dw         0x0002          ; FAT entry
                   dw         64              ; Low word of file size
                   dw         0x00            ; High word of file size

                   db         'A silent drive waits,A connection to be made,Data soon follows',13,10
