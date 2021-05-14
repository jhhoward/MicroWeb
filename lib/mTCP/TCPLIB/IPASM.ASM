
;  mTCP IpAsm.asm
;  Copyright (C) 2010-2020 Michael B. Brutman (mbbrutman@gmail.com)
;  mTCP web page: http://www.brutman.com/mTCP
;
;
;  This file is part of mTCP.
;
;  mTCP is free software: you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, either version 3 of the License, or
;  (at your option) any later version.
;
;  mTCP is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with mTCP.  If not, see <http://www.gnu.org/licenses/>.
;
;
;  Description: Performance sensitive IP checksumming routines.  Special
;    thanks to Krister Nordvall for bug fixes and performance help.
;
;  Changes:
;
;  2011-05-27: Initial release as open source software



.8086


name IPASM


ifdef __SMALL__
  ; near code, near data
  X EQU 4
  .model small
else
  ifdef __COMPACT__ 
    ; near code, far data
    X equ 4
    .model compact
  else
    ifdef __LARGE__
      ; far code, far data
      X equ 6
      .model large
    else
      ifdef __MEDIUM__
        ; far code, near data
        X equ 6
        .model medium
      else
        ifdef __HUGE__
          ; far code, far data
          X equ 6
          .model huge
        endif
      endif
    endif
  endif
endif


.code



; Generic checksum code.  Give it a starting address and a length and you get
; an IP style checksum back.  Not loop unrolled or anything fancy.

public _ipchksum

_ipchksum proc

  push     bp
  mov      bp,sp

  push     ds
  push     si

  mov      dx, [bp+X+4]   ; Length
  mov      cx, dx         ; Length here too
  shr      cx, 1          ; Convert length to words
  xor      bx, bx         ; Zero checksum register

  cld                     ; Direction flag forward

  lds      si, [bp+X]     ; Get segment and offset of data loaded
  clc                     ; Ensure the carry bit is clear

  top1:
    lodsw
    adc    bx, ax
    loop   top1

  adc      bx, 0

  and      dx, 1
  jz       notodd1

  lodsw
  xor      ah, ah
  add      bx, ax
  adc      bx, 0

  notodd1:

  not bx                  ; Complement
  mov      ax, bx         ; Return in AX

  pop      si
  pop      ds
  pop      bp
  ret

_ipchksum endp





; IP pseudo-header checksum code. Give it the extra parameters for the
; pseudo header and a pointer to your protocol header (TCP or UDP).
; Loop unrolled, and fast.

public _ip_p_chksum

_ip_p_chksum proc

  push     bp
  mov      bp,sp

  push     ds
  push     si



  mov dx, [bp+X+14]; Save original len here
  xor bx, bx       ; Zero checksum register

  cld              ; Clear direction flag for LODSW
  clc              ; Clear the carry bit

  ; Setup addressing: src IP addr
  lds      si, [bp+X]     ; Get segment and offset of data loaded
  lodsw                   ; Get first word
  add      bx, ax         ; No carry to worry about
  lodsw                   ; Get next word
  adc      bx, ax         ; Add with carry

  ; Setup addressing: dest IP addr
  lds      si, [bp+X+4]   ; Get segment and offset of data loaded
  lodsw                   ; Get first word
  adc      bx, ax         ; No carry to worry about
  lodsw                   ; Get next word
  adc      bx, ax         ; Add with carry

  adc      bx, 0          ; Add in any extra carry


  ; Add in protocol and length
  ;
  ; The algorithm works the same on both little endian and big endian
  ; machines.  Data loaded from memory is one way, while the values we
  ; have in variables are the other.  Therefore, swap before adding.

  xor      cl, cl         ; Zero this out because protocol is 8 bits
  mov      ch, [bp+X+12]  ; Set protocol
  add      bx, cx         ; Add only - carry was done already above.

  xchg     dl, dh         ; Swap len to add it
  adc      bx, dx         ; Add with carry
  adc      bx, 0          ; Add in any extra carry.
  xchg     dl, dh         ; Restore len back to correct byte order


  ; We know that we are always called with a minimum of 8 bytes, which is
  ; four words.  (UDP has a header size of 8 bytes, and TCP has a minimum
  ; header size of 20 bytes.)  The loop is unrolled to do four words per
  ; iteration, which will always be safe.  Odd numbers of words at the end
  ; are handled by a smaller loop.

  mov cx, dx
  shr cx, 1               ; Number of words
  shr cx, 1               ; Divide by 2
  shr cx, 1               ; Divide by 2 again ..  loop is unrolled 4x.
  clc                     ; Clear the carry bit in case shr set it.

  ; Setup addressing: data from user
  lds si, [bp+X+8];

  top2_1:
    lodsw
    adc bx, ax
    lodsw
    adc bx, ax
    lodsw
    adc bx, ax
    lodsw
    adc bx, ax

    loop top2_1

  adc bx, 0               ; Add any extra carry bit


  ; Are there words left over?

  mov cx, dx              ; DX has the original length
  shr cx, 1               ; Get to number of words
  and cx, 3               ; Figure out how many words are left
  jz  endwords2           ; If zero, skip ahead.

  clc                     ; Clear carry bit from shr above

  top2_2:
    lodsw;
    adc bx, ax            ; Add with carry
    loop top2_2;

    adc bx, 0             ; Add any extra carry bit


  endwords2:


  ; Is there a last byte?

  and dx, 1               ; Was the original length odd?
  jz notodd2

  lodsw
  xor ah, ah              ; Zero the high byte of the 16 bit reg we just loaded
  add bx, ax              ; Add to checksum
  adc bx, 0               ;  Get the last carry

  notodd2:

  not bx                  ; Ones complement
  mov ax, bx


  pop      si
  pop      ds
  pop      bp
  ret


_ip_p_chksum endp






; Pretty much the same as above, but the header and user data are split.
; We do this when we need to compute a UDP checksum for a packet that is
; going to be fragmented.  On the first fragment we have a UDP header and
; some of the user data in the packet waiting to go, but we need to checksum
; over all of the user data which is in the original data.

public _ip_p_chksum2

_ip_p_chksum2 proc

  push     bp
  mov      bp,sp

  push     ds
  push     si



  mov dx, [bp+X+14]; Get header len
  add dx, [bp+X+20]; Add in the data len
  xor bx, bx       ; Zero checksum register

  cld              ; Clear direction flag for LODSW
  clc              ; Clear the carry bit

  ; Setup addressing: src IP addr
  lds      si, [bp+X]     ; Get segment and offset of data loaded
  lodsw                   ; Get first word
  add      bx, ax         ; No carry to worry about
  lodsw                   ; Get next word
  adc      bx, ax         ; Add with carry

  ; Setup addressing: dest IP addr
  lds      si, [bp+X+4]   ; Get segment and offset of data loaded
  lodsw                   ; Get first word
  adc      bx, ax         ; No carry to worry about
  lodsw                   ; Get next word
  adc      bx, ax         ; Add with carry

  adc      bx, 0          ; Add in any extra carry


  ; Add in protocol and length
  ;
  ; The algorithm works the same on both little endian and big endian
  ; machines.  Data loaded from memory is one way, while the values we
  ; have in variables are the other.  Therefore, swap before adding.

  xor      cl, cl         ; Zero this out because protocol is 8 bits
  mov      ch, [bp+X+12]  ; Set protocol
  add      bx, cx         ; Add only - carry was done already above.

  xchg     dl, dh         ; Swap len to add it
  adc      bx, dx         ; Add with carry
  adc      bx, 0          ; Add in any extra carry.
  xchg     dl, dh         ; Restore len back to correct byte order


  ; Unlike ip_p_chksum, we compute the header checksum separate from
  ; the data.  The header will only be 8 or 20 bytes.  We can only
  ; do 4 bytes at a time in the loop, not 8.  And we don't handle
  ; odd length headers, which are not supposed to happen.

  mov cx, [bp+X+14]       ; Get the header len
  shr cx, 1               ; Number of words
  shr cx, 1               ; Divide by 2
  clc                     ; Clear the carry bit in case shr set it.

  ; Setup addressing: data from user
  lds si, [bp+X+8];

  top3_1:
    lodsw
    adc bx, ax
    lodsw
    adc bx, ax

    loop top3_1

  adc bx, 0               ; Add any extra carry bit


  ; Header portion is done.  Do the user data now

  ; Setup addressing before jumping, otherwise
  ; when there are less than 8 bytes we won't setup addressing.

  lds si, [bp+X+16];

  mov cx, [bp+X+20]       ; Get the data len
  mov dx, cx              ; Copy it for later
  cmp cx, 8
  jb nodataunroll3

  shr cx, 1               ; Number of words
  shr cx, 1               ; Divide by 2
  shr cx, 1               ; Divide by 2 again ..  loop is unrolled 4x.
  clc                     ; Clear the carry bit in case shr set it.

  top3_2:
    lodsw
    adc bx, ax
    lodsw
    adc bx, ax
    lodsw
    adc bx, ax
    lodsw
    adc bx, ax

    loop top3_2

  adc bx, 0               ; Add any extra carry bit




  nodataunroll3:

  ; Are there words left over?


  mov cx, dx              ; Get the original data length
  shr cx, 1               ; Get to number of words
  and cx, 3               ; Figure out how many words are left
  jz  endwords3           ; If zero, skip ahead.

  clc                     ; Clear carry bit from shr above

  top3_3:
    lodsw;
    adc bx, ax            ; Add with carry
    loop top3_3;

    adc bx, 0             ; Add any extra carry bit


  endwords3:


  ; Is there a last byte?

  and dx, 1               ; Was the original length odd?
  jz notodd3

  lodsw
  xor ah, ah              ; Zero the high byte of the 16 bit reg we just loaded
  add bx, ax              ; Add to checksum
  adc bx, 0               ;  Get the last carry

  notodd3:

  not bx                  ; Ones complement
  mov ax, bx


  pop      si
  pop      ds
  pop      bp
  ret


_ip_p_chksum2 endp



end
