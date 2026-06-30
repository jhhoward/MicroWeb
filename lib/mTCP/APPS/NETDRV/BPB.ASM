; mTCP NetDrive (BPB.ASM)
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
; Description: Enough of the Bios Parameter Block data structure for
; our needs.


; BIOS Parameter Block (BPB) structure used by DOS.
;
; We don't name the fields because we don't use them but we need to know
; the size of the structure.

bpb    struc
       dw          ?               ; bytes per sector
       db          ?               ; sectors per cluster
       dw          ?               ; reserved sectors
       db          ?               ; number of FATs
       dw          ?               ; root directory entries
       dw          ?               ; sectors = 64 KB/secsize
       db          ?               ; media descriptor
       dw          ?               ; sectors per FAT (DOS 2 BPB stops here.)
       dw          ?               ; DOS 3 sectors per track
       dw          ?               ; heads
       dw          ?               ; hidden sectors (lo)
       dw          ?               ; hidden sectors (hi)
       dw          ?               ; huge sectors (lo)
       dw          ?               ; huge sectors (hi)
bpb    ends

