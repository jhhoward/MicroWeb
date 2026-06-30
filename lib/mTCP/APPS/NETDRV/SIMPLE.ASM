; mTCP NetDrive (SIMPLE.ASM)
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
; Description: The device driver code.
;
; Why is this called SIMPLE.ASM?  Because I started with a very simple
; character mode device driver.  The name is a tribute to that very
; simple original piece of code.



; UDP Testing
;
; If you add -dCHKSUM_TESTING to the wasm command line in the makefile you will
; enable some extra code that can be used for testing the IP and UDP checksum
; routines.  The extra code can be called as a far function and is used with
; a test program that generates random data, calls the IP and UDP checksum
; functions, and then compares the result with known good implementations.


Max_Units equ 24


cseg    segment para public 'code'
net     proc far
        assume cs:cseg, es:cseg, ds:cseg


include "RH.ASM"
include "PKT.ASM"
include "BPB.ASM"
include "SHARED.ASM"


; ============================================================================
;
; Device Driver Header

begin:

next_dev           dd          -1
attribute          dw          0x6002          ; Block device, IOCTL, non-IBM format, 32 bit LBAs
strategy           dw          dev_strategy    ; Strategy routine offset
interrupt          dw          dev_int         ; Interrupt routine offset
unit_count         db          4               ; Supports up to 4 units.
                   db          'NETDRV '       ; 7 more chars, unused by block drivers


; ============================================================================
;
; Local data - please keep everything on even boundaries.

                   even
rh_off             dw          ?               ; request header offset
rh_seg             dw          ?               ; request header segment


; This both an eye-catcher and our version number.  It must immediately
; preceed the shared data area or the EXE won't be able to find it.

                   db          'NETDRIVE', 0x00, 0x03
shared_data        db          size net_data dup(0)

                   even
                   db          'DEBUG   '
pkt_dbg1           dw          0
pkt_dbg2           dw          0



; Currently executing (or last command)
;
; DOS reads and writes are executed by breaking them up into smaller network
; reads and writes.  These variables keep the state of the current command
; execution.

                   db          'COMMAND '
cmd_op             db          ?               ; operation (our protocol)
cmd_result         db          ?               ; Result code from server
cmd_start          dd          ?               ; Starting sector
cmd_count          dw          ?               ; Remaing sector count
cmd_user_buf_off   dw          ?               ; User buffer offset
cmd_user_buf_seg   dw          ?               ; User buffer segment
cmd_sectors_req    dw          ?               ; Sectors requested in the last UDP pkt?
cmd_retry_count    dw          ?               ; Countdown retry for current packet
cmd_resp_recved    db          0               ; Do we have a response yet?
cmd_active         db          0               ; Are we even waiting for a response?

curr_unit          dw          ?               ; Offset to the current unit_data


; Misc networking stuff: IP and ARP housekeeping

ip_ident           dw          ?               ; IP ident; increases each time we send a packet
sendAttempts       db          ?               ; How many attempts to make when pushing a packet out
                   db          ?               ; Padding

                   db          'ARP DATA'
arp_pending        db          0               ; Is there a pending ARP request to respond to?
                   db          0               ; Padding


; Timer
;
; Keep an address to the original interrupt vector before we hooked it
; so that we can chain to it.  Also define a countdown timer that we use
; for timeout detection.

                   even
                   db          'TIMER   '
timerOrigInt       dd          ?               ; Original interrupt vector before we hooked it
countdownTimer     dw          0               ; Use for detecting timeouts


; Packet driver shim stuff

theirReceiver      dd          0               ; Their receiver function
dontShare          db          0               ; Are we passing this packet along?
                   db          ?



; Private stacks
;
; Sizes are chosen based on experience with a few machines and need to be
; generous because we don't know what code we are interrupting, what code
; is calling DOS that is calling us, or what packet driver might be in use.

                   even
                   db          'STACKTIM'
stack_ptr_timerint dw          ?
stack_seg_timerint dw          ?
                   db          127 dup ('X')
                   db          0
stack_top_timerint equ         $-2

                   even
                   db          'STACKRCV'
stack_ptr_recvint  dw          ?
stack_seg_recvint  dw          ?
                   db          127 dup ('Y')
                   db          0
stack_top_recvint  equ         $-2

                   even
                   db          'STACKINT'
stack_ptr_dosint   dw          ?
stack_seg_dosint   dw          ?
                   db          191 dup ('Z')
                   db          0
stack_top_dosint   equ         $-2


switchToStack macro stackname:REQ
  ; Order is important - always update SS first, then SP to take advantage of
  ; the x86 deferring interrupts until after SP is updated.
  push ax
  mov cs:stack_seg_&stackname, ss
  mov cs:stack_ptr_&stackname, sp
  mov ax, cs
  mov ss, ax
  mov sp, OFFSET stack_top_&stackname
endm


switchFromStack macro stackname:REQ
  ; Order is important - always update SS first, then SP to take advantage of
  ; the x86 deferring interrupts until after SP is updated.
  mov ss, cs:stack_seg_&stackname
  mov sp, cs:stack_ptr_&stackname
  pop ax
endm



; Tiny Ramdisk to use while a virtual drive is not connected.
include "RAMDISK.ASM"



; Packet buffers
;
; We need room for the Ethernet header (14), IP header (20), UDP header (8),
; and a payload of up to 1KB.  Also leave room for IP options and some slop.
;
; Incoming is full sized because we might be passing packets to another
; program.  Outgoing is probably oversized by 40 bytes but that is not
; worth worrying about right now.

ETH_BUF_RECV_SIZE  equ         1514

                   even
                   db          'INCOMING'
incoming_buf       db          ETH_BUF_RECV_SIZE dup(?)
                   db          'OUTGOING'
outgoing_buf       db          1100 dup(?)
                   db          'PKT_END '




; ============================================================================
;
; Start of code!


; strategy routine - just saves the pointers to the request header.

dev_strategy:
  mov cs:rh_seg, es
  mov cs:rh_off, bx
  ret



; interrupt - does the actual work on the saved request header.

dev_int:

  ; Switch to a private stack first.  We use stack3 here.
  ; Order is important - always update SS first, then SP to take advantage of
  ; the x86 deferring interrupts until after SP is updated.

  switchToStack dosint

  pushf
  push   bx
  push   cx
  push   dx
  push   bp
  push   si
  push   di
  push   ds
  push   es

  ; Clear the direction flag; we never want to go backwards.  The pushf
  ; above preserves the original state for when we return, as per the
  ; DOS device driver programming guidelines.
  cld

  ; Restore pointer to request header in ES:BX for commands
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off

  ; Make our data addressable in DS
  mov    ax, cs
  mov    ds, ax

  ; Set a default good return code
  mov    es:[bx].rh_status, 0000h

  ; Get addressability to the unit data
  xor    dx, dx
  mov    dl, es:[bx].rh_unit
  mov    ax, size unit_data
  mul    dx
  add    ax, OFFSET units_storage
  mov    curr_unit, ax

  ; Jump to the appropriate command.
  mov    al, es:[bx].rh_cmd
  cmp    al, 12
  jle    dev_int_good_cmd

  ; Whoops - we don't handle this one.
  mov    es:[bx].rh_status, 0x8003
  jmp    done

  dev_int_good_cmd:
  rol    al, 1                      ; Multiply by two so we can use cmdtab.
  mov    ah, 0
  mov    di, OFFSET cmdtab
  add    di, ax
  jmp    word ptr[di]

cmdtab   label  byte
         dw     cmd_init            ;  0: Initialization
         dw     cmd_media_check     ;  1: Media check
         dw     cmd_get_bpb         ;  2: Build BPB
         dw     cmd_ioctl_input     ;  3: IOCTL input
         dw     cmd_input           ;  4: Input (read)
         dw     done                ;  5: Char only: Non-destrutive input
         dw     done                ;  6: Char only: Input status
         dw     done                ;  7: Char only: Input flush
         dw     cmd_output          ;  8: Output (write)
         dw     cmd_output_verify   ;  9: Output with verify
         dw     done                ; 10; Char only: Output status
         dw     done                ; 11: Char only: Output flush
         dw     done                ; 12: IOCTL output (not used)


cmd_init:

  ; Almost all of the work, including setting the proper fields in the
  ; request header is done in the initial call.  We want as much done
  ; there to avoid having any code here, which is just dead bytes after
  ; initialization is complete.

  call   initial

  ; Clear the unit storage area; all of the registers are already setup
  ; to do this.  On the error path CX will be zero and this will be a no-op.
  rep    stosw

  ; Restore ES:BX to set 'done' in the RH before ending.
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off

  jmp    done



; Media Check
;
; We did not set the "Removable Media" attribute so there is no need to set
; the previous volume label pointer.  If we wanted to set it to be safe we'd
; need to check the DOS version too to avoid stomping on storage that DOS 2.x
; doesn't allocate in the RH.
;
; This is really simple; we rely on NETDRIVE.EXE to set unit_media_check to
; changed whenever it does something to the drive.  When DOS calls here and
; is told that the media changed, it will call Get BPB, which will then
; set unit_media_check back to no-change.
;
; The only edge case here is when we are first starting up and the RAM disk
; is being used.  The initial state of unit_media_check is 0 (unknown), which
; works out fine - there are no dirty buffers possible so DOS will assume the
; media ; has changed.  It will call Get BPB which will set it to unchanged,
; and all is good.
;
cmd_media_check:

  mov    si, curr_unit
  mov    al, [si].unit_media_check
  mov    es:[bx].rh_media_return, al
  jmp    done


; Get BIOS Parameter Block (BPB)
;
cmd_get_bpb:

  ; Reset the unit_media_check value to unchanged.
  mov    si, curr_unit
  mov    [si].unit_media_check, 1

  ; One way or the other, we are pointing at something in our code segment.
  mov    word ptr es:[bx].rh_get_bpb_bpb_seg, cs

  ; If the session number is 0 we are not connected and will return a BPB
  ; for the tiny RAM disk.  If we are connected we use the real BPB for the drive.
  cmp    [si].unit_session, 0
  jnz    cmd_get_bpb_connected

  mov    word ptr es:[bx].rh_get_bpb_bpb_off, OFFSET RamDiskBPB
  jmp    done
  
  cmd_get_bpb_connected:
  add    si, unit_bpb
  mov    word ptr es:[bx].rh_get_bpb_bpb_off, si
  jmp    done



; IOCTL input - read from the driver.

cmd_ioctl_input:

  ; If there are less than 8 bytes to write to fail the operation.
  cmp    es:[bx].rh_ioctl_count, 8
  jge    cmd_ioctl_input1

  ; Set a bad return code and leave.
  mov    es:[bx].rh_ioctl_count, 0
  mov    es:[bx].rh_status, 0x800c
  jmp    done


  cmd_ioctl_input1:

  ; Copy two far pointers to the user buffer:
  ;  * net_data structure
  ;  * address to the current unit structure

  ; ES needs to temporarily point at their buffer, so save it first.
  push   es

  ; Create a far pointer to the user buffer in ES:DI.
  les    di, es:[bx].rh_ioctl_buf_off

  mov    ax, OFFSET shared_data   ; net_data offset
  stosw
  mov    ax, cs                   ; net_data segment
  stosw

  mov    ax, curr_unit            ; curr_unit offset
  stosw
  mov    ax, cs                   ; curr_unit segment
  stosw

  ; Restore the rh segment
  pop    es

  ; Write the return code (number of bytes written)
  mov    es:[bx].rh_ioctl_count, 8

  jmp    done




; Input (Read) from the driver
;
; Perform a read.  The read will be done two sectors at a time if possible
; for performance reasons.  Any bad return code from the server halts the
; entire operation.
;
cmd_input:

  mov    cmd_active, 1

  ; If the packet drive is not connected use the fake ram drive area
  mov    si, curr_unit
  cmp    [si].unit_session, 0
  jnz    cmd_input_connected

  ; Compute the starting byte to read:
  mov    ax, es:[bx].rh_input_start      ; Start LBA

  ; DOS 6.3 is being silly and using a 32 bit LBA for our 4 sector ram disk.
  cmp    ax, 0xffff
  jnz    cmd_input_1

  mov    ax, word ptr es:[bx].rh_input_hugeStart

  cmd_input_1:
  mov    cl, 6                           ; Multiply by sector size
  shl    ax, cl                          ; 
  add    ax, OFFSET RamDisk

  mov    dx, es:[bx].rh_input_count      ; Sectors ...
  mov    cl, 5                           ; Multiply by 32 to get words
  shl    dx, cl                          ; 
  mov    cx, dx

  ; Using MOVSW so source is in DS:SI, target is in ES:DI
  mov    si, ax
  mov    di, es:[bx].rh_input_buf_off
  mov    es, es:[bx].rh_input_buf_seg
  rep    movsw

  jmp    cmd_input_finished_good


  cmd_input_connected:
  mov    cmd_op, Op_Read

  call   setup_cmd_vars

  ; Everything we need is in the cmd_ variables and we are done with the
  ; RH for now.  Set ES so we can use it for our data.
  mov    ax, cs
  mov    es, ax


  ; Setup outgoing UDP command header
  ; These fields do not change even when reading multiple sectors.

  mov    outgoing_buf.out_udp_start.pkt_nd_vers, 0x0001
  mov    si, curr_unit
  mov    ax, [si].unit_session
  mov    outgoing_buf.out_udp_start.pkt_nd_session, ax
  mov    outgoing_buf.out_udp_start.pkt_nd_op, Op_Read


  ; Setup the loop to do multiple sectors.  Try to do two at a time
  ; until there are one or zero left.

  cmd_input_loop_top:
  cmp    cmd_count, 0
  jz     cmd_input_finished_good

  mov    ax, shared_data.net_pkt_retries ; Reset the retry counter
  mov    cmd_retry_count, ax 

  cmd_input_loop_retry_top:
  call   set_outgoing_ssc                ; Set outgoing Sequence, Start and Count


  ; Fill in the Eth, IP, and UDP headers and send the packet.  DX needs to
  ; contain the length of the UDP payload (not including the UDP header.)
  ; After the packet has been sent, setup the countdown timer.

  mov    dx, ND_Len                      ; Fixed len command header.
  mov    cmd_resp_recved, 0
  call   sendUDP
  mov    ax, shared_data.net_pkt_timeout
  mov    countdownTimer, ax


  cmd_input_spinwait:
  ; Now spin and wait for the response
  cmp    cmd_resp_recved, 1
  jz     cmd_input_check_result
  cmp    countdownTimer, 0
  jnz    cmd_input_spinwait

  ; Timeout ..
  cmp    cmd_retry_count, 0
  jz     cmd_input_timeout

  ; Nothing should have moved so just go back to the top of the loop and try again.

  mov    si, curr_unit
  add    word ptr [si].unit_blocks_retries, 1
  adc    word ptr [si].unit_blocks_retries+2, 0

  dec    cmd_retry_count
  jmp    cmd_input_loop_retry_top


  cmd_input_check_result:
  cmp    cmd_result, 0
  jnz    cmd_input_bad_result


  ; The last response was good.  Advance the loop variables and update the stats.

  mov    si, curr_unit
  mov    ax, cmd_sectors_req
  add    word ptr [si].unit_blocks_read, ax
  adc    word ptr [si].unit_blocks_read+2, 0

  call   update_local_ssc                ; Update our local Sequence, Start and Count
  jmp    cmd_input_loop_top              ; And go back to do it all again ...


  cmd_input_finished_good:
  ; Restore rh header and segment
  ; The number of sectors read should already be in the RH.
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off
  jmp    done


  ; Bad result means we got a response, so update the sequence number.
  ; Then continue through the timeout code because both report an error.
  cmd_input_bad_result:
  mov    si, curr_unit
  inc    [si].unit_sequence

  cmd_input_timeout:
  ; Restore rh header and segment
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off

  ; Give them a general read fault and the number of sectors read.
  ; Tell them it was a read fault.  Given this is a timeout it could
  ; be a "drive not ready" too but read fault works for both timeout
  ; and bad result.

  mov    es:[bx].rh_status, 0x800b
  mov    ax, es:[bx].rh_input_count      ; Total number of sectors -
  sub    ax, cmd_count                   ; Sectors still left to transfer
  mov    es:[bx].rh_input_count, ax      ; Successful sectors transfered.
  jmp    done



cmd_output_verify:
  mov cmd_op, Op_Write_Verify
  jmp cmd_output_common

cmd_output:
  mov cmd_op, Op_Write
  ; Fall through because normal writes are our fast path.



; Output (Write) from the driver
;
; Perform a write.  This is basically the same as the read, except here we
; copy the user data first into the outgoing buffer.  As with read we try
; to do two sectors at a time.
;
cmd_output_common:

  mov    cmd_active, 1

  ; Are we connected?  If not, just return an error - we don't support writing
  ; to the ram disk.
  mov    si, curr_unit
  cmp    [si].unit_session, 0
  jnz    cmd_output_connected

  jmp cmd_output_write_protect

  cmd_output_connected:
  ; Even if we are connected we might be read-only.
  cmp    [si].unit_readonly, 0
  jz     cmd_output_rw

  cmd_output_write_protect:
  mov    es:[bx].rh_status, 0x8000
  mov    es:[bx].rh_input_count, 0
  jmp    done

  cmd_output_rw:

  call   setup_cmd_vars

  ; Everything we need is in the cmd_ variables and we are done with the
  ; RH for now.  Set ES so we can use it for our data.
  mov    ax, cs
  mov    es, ax


  ; Setup outoing UDP command header
  ; These fields do not change even when reading multiple sectors.

  mov    outgoing_buf.out_udp_start.pkt_nd_vers, 0x0001
  mov    si, curr_unit
  mov    ax, [si].unit_session
  mov    outgoing_buf.out_udp_start.pkt_nd_session, ax
  mov    al, cmd_op
  mov    outgoing_buf.out_udp_start.pkt_nd_op, al



  ; Setup the loop to do multiple sectors.  Try to do two at a time
  ; until there are one or zero left.

  cmd_output_loop_top:
  cmp    cmd_count, 0
  jz     cmd_output_loop_finished

  mov ax, shared_data.net_pkt_retries    ; Reset the retry counter
  mov cmd_retry_count, ax 

  cmd_output_loop_retry_top:
  call set_outgoing_ssc                  ; Set outgoing Sequence, Start and Count


  ; Copy the user data to the payload portion.
  ; Using MOVSW so source is in DS:SI, target is in ES:DI

  mov    di, OFFSET outgoing_buf.out_udp_start.pkt_nd_data
  mov    si, cmd_user_buf_off
  mov    ds, cmd_user_buf_seg            ; Order is important here ... muck up ds last.

  ; DX has number of sectors in it from above as a side effect of set_outgoing_ssc.
  ; Convert to number of dwords.  (No need to check for an odd number of bytes here.)

  mov    dh, dl                          ; Multiply by 256
  xor    dl, dl

  mov    cx, dx                          ; Do the copy
  rep    movsw

  mov    ax, cs                          ; Restore ds
  mov    ds, ax

  ; Setup the length for the UDP send
  shl    dx, 1                           ; Go from words to bytes.
  add    dx, Nd_Len                      ; Add the fixed len command header.


  ; Fill in the Eth, IP, and UDP headers and send the packet.  DX needs to
  ; contain the length of the UDP payload (not including the UDP header.)
  ; After the packet has been sent, setup the countdown timer.

  mov    cmd_resp_recved, 0
  call   sendUDP
  mov    ax, shared_data.net_pkt_timeout
  mov    countdownTimer, ax


  cmd_output_spinwait:
  ; Now spin and wait for the response
  cmp    cmd_resp_recved, 1
  jz     cmd_output_check_result
  cmp    countdownTimer, 0
  jnz    cmd_output_spinwait

  ; Timeout ..
  cmp    cmd_retry_count, 0
  jz     cmd_output_timeout

  ; Nothing should have moved so just go back to the top of the loop and try again.

  mov    si, curr_unit
  add    word ptr [si].unit_blocks_retries, 1
  adc    word ptr [si].unit_blocks_retries+2, 0

  dec    cmd_retry_count
  jmp    cmd_output_loop_retry_top


  cmd_output_check_result:
  cmp    cmd_result, 0
  jnz    cmd_output_bad_result


  ; The last response was good.  Advance the global sequence number.

  mov    si, curr_unit
  mov    ax, cmd_sectors_req
  add    word ptr [si].unit_blocks_written, ax
  adc    word ptr [si].unit_blocks_written+2, 0

  call   update_local_ssc                ; Update our local Sequence, Start and Count
  jmp    cmd_output_loop_top             ; And go back to do it all again ...


  cmd_output_loop_finished:

  ; Restore rh header and segment
  ; The number of sectors read should already be in the RH.
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off
  jmp    done


  ; Bad result means we got a response, so update the sequence number
  ; Then continue through the timeout code because both report an error.
  cmd_output_bad_result:
  mov    si, curr_unit
  inc    [si].unit_sequence

  cmd_output_timeout:
  ; Restore rh header and segment
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off

  ; Give them a general write fault and the number of sectors written.
  ; Tell them it was a write fault.  Given this is a timeout it could
  ; be a "drive not ready" too but write fault works for both timeout
  ; and bad result.

  mov    es:[bx].rh_status, 0x800a
  mov    ax, es:[bx].rh_input_count      ; Total number of sectors -
  sub    ax, cmd_count                   ; Sectors still left to transfer
  mov    es:[bx].rh_input_count, ax      ; Successful sectors transfered.
  jmp    done




; Done: set the done bit, restore state and exit
done:
  mov    cs:cmd_active, 0
  or     es:[bx].rh_status, 0x0100

  pop    es
  pop    ds
  pop    di
  pop    si
  pop    bp
  pop    dx
  pop    cx
  pop    bx
  popf

  ; Restore prior stack.
  switchFromStack dosint

  ret




; ============================================================================
;
; Utility functions for Input and Output

setup_cmd_vars proc near

  ; Copy the user buffer address, start LBA and count from the RH and store
  ; them locally.  We will be modifying our local copies at the main loop
  ; progresses.

  mov    ax, es:[bx].rh_input_buf_off    ; User buffer offset
  mov    cmd_user_buf_off, ax
  mov    ax, es:[bx].rh_input_buf_seg    ; User buffer segment
  mov    cmd_user_buf_seg, ax
  mov    ax, es:[bx].rh_input_count      ; Sector count
  mov    cmd_count, ax

  ; What DOS are we on?  Use the RH length to determine where to read
  ; the starting LBA from as it can be either 16 or 32 bits.

  cmp    es:[bx].rh_input_rh.rh_len, 0x16
  jnz    setup_cmd_vars_bigger_dos

  setup_cmd_vars_use_start:

  ; This is either an older DOS (2.x or 3.x) or DOS 4 and the start LBA is
  ; small enough.  (Ignore Compaq DOS 3.3 and DR DOS for now.)

  mov    ax, es:[bx].rh_input_start      ; 16 bit sector address
  mov    word ptr cmd_start, ax
  xor    ax, ax                          ; Clear the upper 16 bits.
  mov    word ptr cmd_start+2, ax

  ret

  setup_cmd_vars_bigger_dos:

  ; This is PC or MS DOS 4.x or above.  If the 16 bit start LBA is 0xFFFF
  ; then use the 32 bit version, otherwise use the 16 bit version.
  ;
  ; Fixme: Add support for Compaq DOS 3.31 and DR DOS

  cmp    es:[bx].rh_input_start, 0xFFFF
  jnz    setup_cmd_vars_use_start

  mov    ax, word ptr es:[bx].rh_input_hugeStart   ; Low 16 bits
  mov    word ptr cmd_start, ax
  mov    ax, word ptr es:[bx].rh_input_hugeStart+2 ; High 16 bits
  mov    word ptr cmd_start+2, ax

  ret

setup_cmd_vars endp



; Sets outgoing packet sequence, start and count, leaves count in DX.

set_outgoing_ssc proc near

  mov    si, curr_unit
  mov    ax, [si].unit_sequence
  mov    outgoing_buf.out_udp_start.pkt_nd_sequence, ax

  mov    ax, word ptr cmd_start
  mov    word ptr outgoing_buf.out_udp_start.pkt_nd_start, ax
  mov    ax, word ptr cmd_start+2
  mov    word ptr outgoing_buf.out_udp_start.pkt_nd_start+2, ax

  mov    dx, 1                           ; Doing at least one
  cmp    cmd_count, 1
  jz     set_outgoing_ssc_just_one
  inc    dx                              ; Doing two.

  set_outgoing_ssc_just_one:

  ; Update the sectors requested for this iteration and also set it in the packet.

  mov    cmd_sectors_req, dx
  mov    outgoing_buf.out_udp_start.pkt_nd_count, dx

  ret

set_outgoing_ssc endp




; Update local sequence, start and count for the next loop iteration

update_local_ssc proc near

  mov    si, curr_unit
  inc    [si].unit_sequence

  ; Also bump the start LBA, decrement the count, and advance the
  ; target buffer address.  (Advancing the segment to avoid wrap around
  ; complications with the offset.)

  mov    ax, cmd_sectors_req
  add    word ptr cmd_start, ax          ; 32 bits so add with carry.
  adc    word ptr cmd_start+2, 0         ; Capture the carry bit.

  add    cmd_user_buf_seg, 0x20          ; Advance 512 bytes
  cmp    ax, 1
  jz     update_local_ssc_2
  add    cmd_user_buf_seg, 0x20          ; Unless we needed another 512 bytes.

  update_local_ssc_2:

  sub    cmd_count, ax                   ; Update the number of sectors left to do.
  ret

update_local_ssc endp


; ============================================================================




; processArpRequest
;
; If this an ARP request for us?  If so, stash the requesting
; IP address and MAC address so that we can send a response
; under the timer tick interrupt.
;
; This is being overly thorough ...  not all of the fields that I am
; checking need to be checked.  Also, it's not performance sensitive
; as ARPs are infrequent.  If you wanted to speed it up you could
; check for the target_ip first to see if it is us before checking
; everything else.
;
; Assumes: DS and ES are set
; Saves and restores any register it touches
;
; Preconditions: Ethernet target hardware address = broadcast and EtherType = ARP
;
; ARP Checks
;   Hardware type = 1
;   Protocol type = 0x0800
;   hlen = 6
;   plen = 4
;   Op = 1
;   Target_ip = our ip

processArpRequest proc near

  push   ax
  push   si
  push   di
  push   cx

  ; If there is already a pending ARP request do not obliterate it.
  cmp    arp_pending, 1
  jz     arp_req_exit

  cmp    word ptr incoming_buf.pkt_arp_hdw_type, 0x0100
  jnz    arp_req_exit
  cmp    word ptr incoming_buf.pkt_arp_prot_type, 0x0008
  jnz    arp_req_exit
  cmp    incoming_buf.pkt_arp_hlen, 0x06
  jnz    arp_req_exit
  cmp    incoming_buf.pkt_arp_plen, 0x04
  jnz    arp_req_exit
  cmp    word ptr incoming_buf.pkt_arp_op, 0x0100
  jnz    arp_req_exit
  mov    ax, word ptr shared_data.net_my_ip
  cmp    ax, word ptr incoming_buf.pkt_arp_target_ip
  jnz    arp_req_exit
  mov    ax, word ptr shared_data.net_my_ip+2
  cmp    ax, word ptr incoming_buf.pkt_arp_target_ip+2
  jnz    arp_req_exit

  ; It's ours ..  stash what we need for later.
  ; Fixme: this is last in wins.  Do we need to support
  ; a second pending request?

  mov    arp_pending, 1

  ; Copy the sending hardware address to the target Ethernet address
  mov    si, OFFSET incoming_buf.pkt_arp_sender_ha
  mov    di, OFFSET shared_data.net_arp_resp.pkt_arp_eth_dest
  movsw
  movsw
  movsw

  ; Copy the sending hardware address and IP address.
  ; Using MOVSW so source is in DS:SI, target is in ES:DI
  mov    si, OFFSET incoming_buf.pkt_arp_sender_ha
  mov    di, OFFSET shared_data.net_arp_resp.pkt_arp_target_ha
  movsw
  movsw
  movsw
  movsw
  movsw

  ; And now it is ready to be sent at the next opportunity.

  arp_req_exit:
  pop    cx
  pop    di
  pop    si
  pop    ax
  ret

processArpRequest endp



; receivepacket
;
; The packet driver will be given this function to use.  For each packet
; there will be two calls:
;
; First call: AX=0, CX=packet len
;   If you can handle the packet then return an address to a buffer in ES:DI
;   If not, ES:DI is set to 0.
;
; Second call: CX=packet len
;   Process the buffer now.  For this driver we need a quick check to see
;   if it is our IP addess and port.  If it is, then we can check the checksum
;   and other processing, otherwise exit quickly to avoid slowing things down.

receivePacket proc far

       ; Is this the first call from the packet driver?
       test ax, ax
       jnz receiveSecondCall

       ; If we are not processing packets yet then jump to the code that returns
       ; a zero for the target buffer.

       ; Test is like a non-destructive AND.  ZF is set if all bits are off.
       test cs:shared_data.net_pkt_recv_on, 1
       jz receiveDisabled

       ; Packet too big?  Reject it.
       cmp cx, ETH_BUF_RECV_SIZE
       jg receiveDisabled

       ; Give them the buffer to copy to for the next call
       mov di, OFFSET incoming_buf 
       mov ax, cs
       mov es, ax
       retf                                   ; Early return!

receiveDisabled:
       xor ax, ax
       mov di, ax
       mov es, ax
       retf                                   ; Early return!

receiveSecondCall:

       ; Push flags to preserve the direction flag for the caller, then clear it for us.
       pushf
       cld

       ; Switch to a private stack first.
       switchToStack recvint

       ; We're going to be here a while.  Save registers that we are going to
       ; touch and re-establish es and ds so we don't need to keep prefixing with cs.

       push bx
       push cx
       push dx
       push bp
       push si
       push di
       push ds
       push es

       mov ax, cs
       mov ds, ax
       mov es, ax


       ; Increment total number of packets received.
       add word ptr shared_data.net_c_r_total, 1
       adc word ptr shared_data.net_c_r_total+2, 0

       ; Assume this will be shared with a downstream program.
       mov  dontShare, 0

       ; Is this packet for us? (Same MAC address)
       mov di, OFFSET incoming_buf 
       mov si, OFFSET shared_data.net_my_mac
       mov cx, 3
       repz cmpsw 

       ; If you get here and zero flag is set all 3 words were compared and are equal.
       jz ourpacket

       ; Was this a broadcast packet?
       cmp word ptr incoming_buf.pkt_arp_eth_dest, 0xffff
       jnz receiveExit
       cmp word ptr incoming_buf.pkt_arp_eth_dest+2, 0xffff
       jnz receiveExit
       cmp word ptr incoming_buf.pkt_arp_eth_dest+4, 0xffff
       jnz receiveExit

       ; The only broadcast packet we care about is ARP.
       cmp word ptr incoming_buf.pkt_arp_eth_type, Eth_Type_Arp
       jnz receiveExit

       ; processArpRequest modifies ax but we don't need it anymore.
       call processArpRequest
       jmp receiveExit


ourpacket:
       add word ptr shared_data.net_c_r_our_mac, 1
       adc word ptr shared_data.net_c_r_our_mac+2, 0

       ; Was it ARP directly to us?
       cmp word ptr incoming_buf.pkt_arp_eth_type, Eth_Type_Arp
       jnz receiveIpCheck

       call processArpRequest
       jmp receiveExit
       

receiveIpCheck:

       ; Do some quick checks to see if it's IPv4 and UDP.  If it is then we
       ; can check to see if it is to us and from the server.  This order of
       ; checking allows us to bail out quickly if it's not for this driver.

       ; EtherType is IP (0x0800)
       cmp word ptr incoming_buf.pkt_ip_eth_type, Eth_Type_Ip
       jnz receiveExit

       ; IP version is 0x04
       mov al, incoming_buf.pkt_ip_vers_hlen
       and al, 0xf0
       cmp al, 0x40
       jnz receiveExit

       cmp incoming_buf.pkt_ip_protocol, Ip_Udp_protocol
       jnz receiveExit

       mov ax, word ptr shared_data.net_my_ip
       cmp ax, word ptr incoming_buf.pkt_ip_dest
       jnz receiveExit
       mov ax, word ptr shared_data.net_my_ip+2
       cmp ax, word ptr incoming_buf.pkt_ip_dest+2
       jnz receiveExit

       mov si, curr_unit

       mov ax, word ptr [si].unit_server_ip
       cmp ax, word ptr incoming_buf.pkt_ip_src
       jnz receiveExit
       mov ax, word ptr [si].unit_server_ip+2
       cmp ax, word ptr incoming_buf.pkt_ip_src+2
       jnz receiveExit


       ; Checking the UDP port is a bit more involved, as the UDP packet is
       ; not in a fixed location because of possible IP header options.
       ; The hlen field represents the header length in 4 byte quantities.
       ; Read the number of bytes for the IP header and compute an offset
       ; to the start of the UDP header.

       xor dx, dx
       mov dl, incoming_buf.pkt_ip_vers_hlen
       and dl, 0x0f
       shl dx, 1               ; DX now has the IP header length in words
       shl dx, 1               ; DX now has the IP header length in bytes
       add dx, Eth_hdr_len     ; DX now has an offset to the UDP header.

       ; The source port needs to match the server port and the destination
       ; port needs to match our local port.  (And remember, network byte order.)

       mov si, OFFSET incoming_buf
       add si, dx              ; Skip past Eth and IP headers.

       lodsw                   ; pkt_udp_src
       xchg ah, al
       mov di, curr_unit
       cmp ax, [di].unit_server_port
       jnz receiveExit

       lodsw                   ; pkt_udp_dest
       xchg ah, al
       cmp ax, shared_data.net_my_port
       jnz receiveExit

       ; This packet is definitely for us; don't pass it to the next user.
       mov  dontShare, 1

       ; Ok, everything matches.  Verify the IP checksum
       mov  si, OFFSET incoming_buf
       call computeIpChecksum
       cmp  ax, 0
       jnz  receiveExit


       ; UDP checksums are optional.  If it is 0 then don't check it.

       ; Get to the IP header, then extract the length and get to the UDP header.
       mov   si, OFFSET incoming_buf.pkt_ip_vers_hlen
       xor   ax, ax
       mov   al, byte ptr [si]
       and   al, 0x0f
       shl   ax, 1
       shl   ax, 1
       add   si, ax
       mov   ax, [si+pkt_udp_chksum]

       cmp   ax, 0
       jz    receiveSkipUdpChksum

       ; The UDP checksum is set so check it.  

       mov  si, OFFSET incoming_buf
       call computeUdpChecksum
       cmp  ax, 0
       jnz receiveExit

       receiveSkipUdpChksum:

       ; Checksum was good.  Bump the counter of UDP packets destined for us.
       add word ptr shared_data.net_c_r_our_udp, 1
       adc word ptr shared_data.net_c_r_our_udp+2, 0

       ; Verify the session and sequence number.  Both have to match perfectly.
       ; If they do match, copy what we need from the packet because the next
       ; incoming packet will obliterate it.
       ;
       ; If it is a good read copy the payload too.  (You can only copy the
       ; payload if you know there is a user buffer setup waiting for it.)


       ; Setup so we can directly access the UDP portion of the packet.
       ; Then check the fields strictly to ensure this is what we asked for.
       mov bx, OFFSET incoming_buf.pkt_udp_src
       add bx, dx

       mov si, curr_unit
       mov ax, [si].unit_session
       cmp ax, [bx].pkt_nd_session
       jnz receiveExit

       mov ax, [si].unit_sequence
       cmp ax, [bx].pkt_nd_sequence
       jnz receiveExit

       mov al, cmd_op
       cmp al, [bx].pkt_nd_op
       jnz receiveExit

       mov ax, word ptr cmd_start
       cmp ax, word ptr [bx].pkt_nd_start
       jnz receiveExit
       mov ax, word ptr cmd_start+2
       cmp ax, word ptr [bx].pkt_nd_start+2
       jnz receiveExit

       mov ax, cmd_sectors_req
       cmp ax, [bx].pkt_nd_count
       jnz receiveExit

       ; One last check - are we actually waiting for something?  Also, if we are
       ; already processing something that has been received then don't clobber
       ; anything as this was either a dupe or speculative.  It's a small timing
       ; window but we need to check it.

       cmp cmd_active, 1
       jnz receiveExit
       cmp cmd_resp_recved, 0
       jnz receiveExit


       ; Everything matches.  Copy what we need.

       mov al, [bx].pkt_nd_result
       mov cmd_result, al

       mov cmd_resp_recved, 1


       ; Was this a write with verify?
       cmp cmd_op, Op_Write_Verify
       jz receive_write_verify


       ; Only copy the payload for a successful read.  The server might send
       ; diagnostic information in the payload which shows up in a packet
       ; trace but we generally don't have a buffer to copy it too.

       cmp cmd_op, Op_read
       jnz receiveExit
       cmp cmd_result, 0
       jnz receiveExit


       ; This is a successful read.  Copy the payload, but only copy what we
       ; requested.  If the number of received bytes doesn't match the expected
       ; bytes then we mark this as bad.

       mov ax, [bx].pkt_udp_len
       xchg al, ah                  ; Never forget, network byte order for UDP fields.
       sub ax, Udp_Nd_Len
       jz receiveExit

       mov dx, cmd_sectors_req      ; Number of sectors we requested on the last pass.
       mov cl, 9
       shl dx, cl                   ; Times 512.

       cmp ax, dx
       jz receive_copy              ; All good, copy the payload.

       ; Mark the command as bad and exit
       mov cmd_result, 1
       jmp receiveExit

receive_copy:

       ; Using MOVSW so source is in DS:SI, target is in ES:DI

       lea  si, [bx+pkt_nd_data]
       mov  es, cmd_user_buf_seg
       mov  di, cmd_user_buf_off
       mov  cx, ax                  ; Payload length from above
       shr  cx, 1                   ; Divide by two for word moves
       rep  movsw

       and  ax, 1
       jz receiveExit
       movsb                        ; Move the last odd byte.
       jmp receiveExit


receive_write_verify:

       ; We have the buffer we went sitting in cmd_user_buf_seg and _off.
       ; The payload should have the re-read of what we just sent.

       mov dx, cmd_sectors_req
       mov cl, 8
       shl dx, cl                   ; Number of words to compare.
       mov cx, dx

       ; No need to verify the incoming buffer length.  If it's wrong we'll probably
       ; have a miscompare, and that is fine.

       mov  es, cmd_user_buf_seg
       mov  di, cmd_user_buf_off

       lea  si, [bx+pkt_nd_data]

       repz cmpsw

       ; If you get here and zero flag is set everything was compared and equal.
       jz receiveExit

       ; Set a bad return code ...
       mov cmd_result, 1

receiveExit:

       pop es
       pop ds
       pop di
       pop si
       pop bp
       pop dx
       pop cx
       pop bx

       ; Restore prior stack.  We still have flags pushed on the original stack
       ; because we cleared the direction flag and we still need to restore it.
       switchFromStack recvint


       ; Is there somebody waiting for this packet?
       ; I'm not proud of this code but it seems to work.  Make it more robust.

       cmp    word ptr cs:theirReceiver, 0
       jnz    receiveExit_receiver1
       cmp    word ptr cs:theirReceiver+2, 0
       jz     receive_realexit

       receiveExit_receiver1:
       cmp    cs:dontShare, 1
       jz     receive_realexit

       ; Call them the first time.  If they give us a buffer continue.
       mov    ax, 0
       call   cs:[theirReceiver]
       cmp    di, 0
       jnz    receiveExit_receiver2
       mov    ax, es
       cmp    ax, 0
       jz     receive_realexit


       receiveExit_receiver2:

       ; The user gave us a pointer to a buffer in ES:DI.  CX is already set; it
       ; was saved on entry and restored with the pop command above.

       ; Save ES:DI (their buffer) for later when we have to return it in DS:SI.
       push   es
       push   di

       ; Source is our packet (DS:SI)
       mov    si, OFFSET incoming_buf 
       mov    ax, cs
       mov    ds, ax

       ; Setup for moving a word at a time.  Preserve CX so we can see if
       ; there is an odd byte to move.
       push   cx
       shr    cx, 1
       rep    movsw
       pop    cx
       and    cx, 1
       jz     receiveExit_receiver3
       movsb

       receiveExit_receiver3:

       ; We're supposed to set DS:SI to the buffer we copied the packet into.
       pop    si
       pop    ds

       mov    ax, 1
       call   cs:[theirReceiver]

       receive_realexit:
       popf
       retf

receivePacket endp



ifdef CHKSUM_TESTING

testIpChksum proc far
       call computeIpChecksum
       retf
testIpChksum endp

testUdpChksum proc far
       call computeUdpChecksum
       retf
testUdpChksum endp

endif



; Compute Checksum routines
;
; When constructing a packet set the checksum field to zero first.
; This will give you the value to place in the checksum field.
;
; When checking a packet the value computed here should be 0.
;
; Inputs:
;   SI has an offset to the packet start.  (The Eth header.)
;
; Outputs:
;   AX returns the IP checksum
;
; BX and CX are destroyed, DX is untouched.


computeIpChecksum proc near

       ; Skip to the start of the IP header.
       add   si, Eth_hdr_len

       ; Get the length of the IP header in 32 bit words.  This code moves
       ; two bytes when it just needs to move one, but the AND operation
       ; will clear the upper byte fixing that.
       mov   cx, word ptr [si]
       and   cx, 0x0f

       ; Clear the carry bit and result register.
       clc
       xor   bx, bx

       computeIpChecksumLoop:
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       loop  computeIpChecksumLoop

       adc   bx, 0     ; Capture any extra carry

       not   bx
       mov   ax, bx

       ret

computeIpChecksum endp


computeUdpChecksum proc near

       ; Clear the carry bit.
       clc

       ; Save SI so we can use it again later
       push  si

       ; Start with the IP source and destination (4 16 bit words).
       add   si, pkt_ip_src

       lodsw
       mov   bx, ax    ; Use mov instead of add to save on zeroing out bx
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       adc   bx, 0     ; Capture any extra carry

       ; Next the protocol and the byte of 0's.  We already know it is
       ; UDP otherwise we wouldn't be here.
       xor   al, al
       mov   ah, Ip_Udp_protocol
       add   bx, ax
       adc   bx, 0     ; Capture any extra carry

       ; Compute where the UDP header starts so that we can get the length
       ; field from it.
       pop   si
       add   si, Eth_hdr_len

       ; Get the length of the IP header in bytes.
       xor   ax, ax
       mov   al, byte ptr [si]
       and   al, 0x0f
       shl   ax, 1
       shl   ax, 1

       ; Add the length of the IP header to get to the start of the UDP header.
       add   si, ax

       ; Add the OFFSET to the UDP header length
       mov   ax, [si+pkt_udp_len]
       add   bx, ax
       adc   bx, 0     ; Capture any extra carry

       ; AX has a byte swapped UDP length.  Move it to CX and swap the bytes
       ; so that we get an actual length.  Also save it so we can do the
       ; even/odd check at the end.
       mov   cx, ax
       xchg  cl, ch
       push  cx


       ; Loop over the UDP header and data.  SI points at the UDP header
       ; and CX has the length of the UDP packet in bytes.  We are doing
       ; words at a time loop unrolling 8x.

       shr   cx, 1  ; # of words
       shr   cx, 1
       shr   cx, 1
       shr   cx, 1  ; Divided by 8.

       ; Skip the loop unrolled part if there is not enough data.
       jcxz  computeUdpChecksum_2

       clc        ; Clear the carry bit in case it was set during this ptr math

       computeUdpChecksum_1:
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       lodsw
       adc   bx, ax
       loop  computeUdpChecksum_1

       adc   bx, 0     ; Capture any extra carry

       computeUdpChecksum_2:
       ; Any words left to do?
       pop   cx
       push  cx

       shr   cx, 1
       and   cx, 7
       jz    computeUdpChecksum_4

       ; Checksum up to 7 words
       clc
       computeUdpChecksum_3:
       lodsw
       adc   bx, ax
       loop  computeUdpChecksum_3

       adc   bx, 0     ; Capture any extra carry

       computeUdpChecksum_4:
       ; Check if we have an odd byte ...
       pop   cx
       and   cx, 1
       jz    computeUdpChecksum_not_odd

       clc
       
       lodsw
       xor   ah, ah
       add   bx, ax

       adc   bx, 0     ; Capture any extra carry

       computeUdpChecksum_not_odd:

       not   bx
       mov   ax, bx

       ret

computeUdpChecksum endp



; On entry DX contains the length of the UDP payload, not counting the UDP
; header.  The caller has already copied the payload to the outgoing_buf
; packet for us.
;
; No registers are preserved.

sendUDP proc near

       ; IP Header

       ; We are using MOVSW so source is in DS:SI, target is in ES:DI.

       mov   di, OFFSET outgoing_buf.out_pkt_start  ; Set the target (ES:DI)

       mov   si, curr_unit                   ; Copy next hop MAC to Ethernet dest
       add   si, unit_next_hop_mac
       movsw
       movsw
       movsw

       mov   si, OFFSET shared_data.net_my_mac ; Copy our MAC to Ethernet source
       movsw
       movsw
       movsw

       mov   ax, Eth_Type_Ip                 ; EthType
       stosw

       mov   ax, 0x0045                      ; IP Vers/Hlen and Service type (byte swapped)
       stosw

       mov   ax, dx                          ; DX has the UDP payload length
       add   ax, 0x1C                        ; Add IP Header and UDP header to the payload length
       xchg  al, ah                          ; Byteswap because network byte order
       stosw

       inc   ip_ident                        ; Increment ident because this is a new packet.
       mov   ax, ip_ident                    ; Also byte swapped.
       xchg  al, ah
       stosw

       mov   ax, 0x0040                      ; Flags and fragment offset: don't fragment (byte swapped)
       stosw
       
       mov   ax, 0x11FF                      ; Protocol and TTL (255) (byte swapped)
       stosw

       xor   ax, ax                          ; Checksum (zero for now, to be filled in soon)
       stosw

       mov   si, OFFSET shared_data.net_my_ip ; Our IP address to SRC IP address
       movsw
       movsw

       mov   si, curr_unit                   ; Copy server IP address to target IP address.
       add   si, OFFSET unit_server_ip
       movsw
       movsw


       ; UDP Header - source port, target port, length and checksum fields.

       mov   ax, shared_data.net_my_port     ; Byte swapped source port
       xchg  al, ah
       stosw

       ; si is pointing at unit_server_port (which is byte swaped)
       lodsw
       xchg  al, ah
       stosw

       ; Compute the total UDP length (header + payload) and store a byte
       ; swapped version in the UDP header.  Save the unswapped version
       ; for later in DX.

       add   dx, 8                           ; Add UDP header length to payload length
       mov   ax, dx
       xchg  al, ah                          ; Byte swap the length to store in the header.
       stosw

       ; Set the UDP checksum to zero for now.  We call the checksum routine soon.
       xor   ax, ax
       stosw


       ; Ok, at this point the packet is formed.  Now we need to compute the checksums.

       mov  si, OFFSET outgoing_buf
       call computeIpChecksum
       mov  outgoing_buf.pkt_ip_chksum, ax

       ; computeUdpCheck is used for both incoming checking and sending.  It doesn't
       ; have the logic to look for 0x0000 and change it to 0xFFFF because that would
       ; be incorrect when the incoming checksum is set and you need to check it.
       ; Do that check here.

       mov   si, OFFSET outgoing_buf
       call  computeUdpChecksum
       cmp   ax, 0
       jnz   sendUdpChksum1
       mov   ax, 0xFFFF

       sendUdpChksum1:
       mov   outgoing_buf.out_udp_start.pkt_udp_chksum, ax


       ; Sending a packet:
       ;   AH = 4, CX = len (min 60), DS:SI = packet addr (DS already set)

       mov   sendAttempts, 5 

       sendUDP_send_packet:
       mov   ah, 4
       mov   cx, dx                          ; User buffer length from above, includes UDP header.
       add   cx, 0x22                        ; Add length of the Eth header and IP headers.

       ; Runt packet?
       cmp   cx, 0x3c                        ; Min packet size is 60 bytes
       jge   sendUDPNotRunt                  ; If greater than or equal proceed.
       mov   cx, 0x3c

       sendUDPNotRunt:
       mov   si, OFFSET outgoing_buf.out_pkt_start

       ; Push dx and pop it because the driver will communicate an error code in dh,
       ; and we need dx preserved for the UDP packet length.

       push  dx
       call  doPacketInterrupt
       pop   dx

       jnc   sendUDP_end

       ; Carry flag was set so the packet driver is not happy.  Retry the send.
       ;
       ; Kill some time to give the hardware a chance to change state.
       ; On slow machines it is at least 6ms.

       mov   cx,0x8000
       sendUDP_delay_loop:
       loop  sendUDP_delay_loop

       add   word ptr shared_data.net_c_send_errs, 1
       adc   word ptr shared_data.net_c_send_errs+2, 0
       dec   sendAttempts
       cmp   sendAttempts, 0
       jnz   sendUDP_send_packet

       ; If we run out of attempts just fall through and return.
       ; Eventually a timeout will cause a retry.

       sendUDP_end:
       ret



sendUDP endp



; Timer interrupt routine
;
; If the countdown timer is active (greater than zero) decrement it.  This
; is used to detect timeouts.  We only have one countdown timer.
;
; If there is an ARP request pending update the prebuilt ARP response packet
; and send it out.
;
;
; 3Com PCI packet driver notes:
;
; This packet driver is extremely picky.  You can't respond to ARP under the
; receive interrupt.  You must chain to the other interrupts first via the
; pushf and call and then iret from here.  You can't do this under the BIOS
; tick interrupt.  But this specific set of code seems to work.
;
; In previous code that works everywhere else we send a response to the ARP
; request, and then chain by doing a far jmp to the next interrupt handler.
; That doesn't work here.

timerInt proc far

  pushf
  call   cs:[timerOrigInt]

  cmp    cs:countdownTimer, 0
  jz     timerInt_checkArpPending
  dec    cs:countdownTimer

  timerInt_checkArpPending:
  cmp    cs:arp_pending, 0
  jz     timerInt_chain_only


  ; Switch to a private stack first.
  switchToStack timerint

  push   bx
  push   cx
  push   dx
  push   bp
  push   si
  push   di
  push   ds
  push   es

  ; Reestablish DS
  mov    ax, cs
  mov    ds, ax

  ; ARP packets are small so they are always runt packets.  Set the
  ; packet length to the minimum of 60 (0x3c) to avoid offending anybody.

  ; Sending a packet: AH = 4, CX = len (min 60), DS:SI = packet addr (DS already set)

  mov    sendAttempts, 3

  timerInt_send_packet:
  mov    ah, 4
  mov    cx, 0x3c
  mov    si, OFFSET shared_data.net_arp_resp
  call   doPacketInterrupt

  jnc    timerInt_packet_sent

  ; Carry flag was set so the packet driver is not happy.  Retry the send.
  ;
  ; Introduce a small delay here to give the hardware time to recover.
  ; This should be rare so don't worry about the timer interrupt too much.

  mov    cx,0x8000
  timerInt_delay_loop:
  loop timerInt_delay_loop

  add    word ptr shared_data.net_c_send_errs, 1
  adc    word ptr shared_data.net_c_send_errs+2, 0
  dec    sendAttempts
  cmp    sendAttempts, 0
  jnz    timerInt_send_packet

  ; If we run out of attempts just fall through and return.
  ; This will leave it pending and not update the sent counter.
  ; Maybe the problem will clear up on the next attempt.
  jmp    timerInt_end

  timerInt_packet_sent:
  mov    arp_pending, 0
  add    word ptr shared_data.net_c_s_arp_resp, 1
  adc    word ptr shared_data.net_c_s_arp_resp+2, 0

  timerInt_end:
  pop    es
  pop    ds
  pop    di
  pop    si
  pop    bp
  pop    dx
  pop    cx
  pop    bx

  ; Restore prior stack.
  switchFromStack timerint

  timerInt_chain_only:
  iret

timerInt endp



; doPacketInterrupt
;
; This routine doesn't alter registers.  But who knows what the
; packet driver will do; check the function you are invoking to
; find out.


doPacketInterrupt proc near
  int 0x60
  ret
doPacketInterrupt endp



; Our lightweight packet driver ...
;
; The intent here is to implement enough packet driver functions to make
; the existing mTCP programs run correctly.  The limitations are listed
; ahead of each function.

shimName db "mTCP NetDrive", 0

shimInt proc far

  jmp  shimInt_branchtable
  nop

  db   "PKT DRVR"


  ; Don't bother converting this to a real branch table; we don't want to
  ; touch any registers that might have parameters in them.  Put the check
  ; for send first for performance reasons, as it is most likely to be used.

  shimInt_branchtable:

  cmp  ah, 4
  jz   shimInt_send
  cmp  ah, 1
  jz   shimInt_driverInfo
  cmp  ah, 2
  jz   shimInt_accesstype
  cmp  ah, 3
  jz   shimInt_releasetype
  cmp  ah, 6
  jz   shimInt_getmac
  cmp  ah, 7
  jz   shimInt_reset

  ; Anything else ...
  mov  dh, 11                 ; BAD_COMMAND
  stc
  jmp  shimInt_exit


  shimInt_driverInfo:

  mov  bx, 1                  ; Shim version
  mov  cx, 0x0100             ; ch = Class (type 1 Ethernet), cl = interface number (0)
  mov  dx, 240                ; Type  (Made up ...)
  mov  ax, cs                 ; Nul terminated driver name
  mov  ds, ax
  mov  si, OFFSET shimName
  mov  al, 1                  ; Basic functions only.
  clc
  jmp  shimInt_exit


  shimInt_accesstype:

  ; Only Class 1 (Ethernet) should be calling here so we don't try to
  ; match the requested Class or Type.  Interface 0 is the only option
  ; so don't bother checking that either.  And mTCP always asks for
  ; the wildcard Ethernet type so don't bother matching those; we're
  ; going to pass every packet through.  (And it only uses accesstype
  ; once, so the NO_SPACE error is not an option for us.)

  ; Save their receiver function and give them a bogus handle to use.
  mov  word ptr cs:theirReceiver, di
  mov  word ptr cs:theirReceiver+2, es
  mov  ax, 1234
  clc
  jmp  shimInt_exit


  shimInt_releasetype:

  ; Always disconnects and never fails.  It doesn't bother checking the handle;
  ; mTCP programs always use just one handle so checking it is not needed.

  mov  word ptr cs:theirReceiver, 0
  mov  word ptr cs:theirReceiver+2, 0
  clc
  jmp  shimInt_exit


  shimInt_send:

  ; If the packet driver sets the CF flag we'll pass it through on return.

  call doPacketInterrupt
  jmp  shimInt_exit


  shimInt_getmac:

  ; This happens rarely so don't optimize it.
  ;
  ; Save their handle (in bx) in case they need it; alter it to the real
  ; handle being used, and then call the packet driver.

  push bx
  mov  bx, cs:shared_data.net_pkt_drv_hndl
  call doPacketInterrupt
  pop  bx

  clc
  jmp  shimInt_exit


  shimInt_reset:
  ; mTCP doesn't use this so return an error if something tries.  We can
  ; add it later if somebody complains.
  mov  dh, 15            ; CANT_RESET
  stc
  jmp  shimInt_exit


  shimInt_exit:

  ; Based on the state of the Carry Flag, manipulate the saved flags
  ; word to set the return code for the caller.

  push bp
  mov  bp, sp
  jc   shimInt_cf
  and  byte ptr [bp+6], 0xfe
  jmp  shimInt_cf_done
  shimInt_cf:
  or   byte ptr [bp+6], 1
  shimInt_cf_done:
  pop  bp

  iret
shimInt endp




; After the initialization routine runs we are going to reuse the data area
; for the per-unit connection data.  This allows us to use exactly the amount
; of the storage called for by the number of units.

even
units_storage label unit_data


drive_units_opt    db          1         ; Assume 1 unit will be installed
ten                db         10         ; Used to divide by 10
cursor_off         dw          ?         ; current offset of our config.sys line
cursor_seg         dw          ?         ; segment of our config.sys line


printMsg macro mesg:REQ
  mov dx, OFFSET mesg
  mov ah, 9
  int 0x21
endm

printErrAndExit macro mesg:REQ
  mov dx, OFFSET mesg
  mov ah, 9
  int 0x21
  jmp   initial_error_path
endm


initial proc near

  ; Set some addresses that the NETDRIVE.EXE program will use.
  ;
  ; Everything else in shared_data/net_data is initialized to zeros.

  mov   shared_data.net_segment, cs
  mov   shared_data.net_receiver_off, OFFSET receivePacket
  mov   shared_data.net_shim_off, OFFSET shimInt
  mov   shared_data.net_int_inst_off, OFFSET doPacketInterrupt

  ifdef CHKSUM_TESTING
  mov   word ptr shared_data.test_IP, OFFSET testIpChksum
  mov   word ptr shared_data.test_IP+2, cs
  mov   word ptr shared_data.test_UDP, OFFSET testUdpChksum
  mov   word ptr shared_data.test_UDP+2, cs
  endif

  printMsg Hello


  ; scanswitch setup
  ;
  ; The request header has a pointer to our line in config.sys after "device='.
  ; Place that address in cursor_off and cursor_seg, where scanswitch can load it.
  ; scanswitch will advance it and use cursor_off and cursor_seg to remember where
  ; it left off.

  mov   ax, es:[bx].rh0_bpb_array_off
  mov   cursor_off, ax
  mov   ax, es:[bx].rh0_bpb_array_off+2
  mov   cursor_seg, ax


  initial_scanswitches_top:
  push  ds                       ; scanswitch alters ds
  call  scanswitch
  pop   ds                       ; restore ds

  cmp   cl, 0
  jz    initial_no_more_switches

  cmp   cl, 255
  jz    initial_switch_format_error

  cmp   cl, 'd'
  jnz   initial_unknown_switch


  ; The -d switch is only legal between 1 and 4 drives.  Reject other values.
  cmp   bx, 1
  jb    initial_bad_drive_count
  cmp   bx, Max_Units
  ja    initial_bad_drive_count

  mov   drive_units_opt, bl
  jmp   initial_scanswitches_top


  initial_bad_drive_count:
  printErrAndExit BadDriveCnt

  initial_unknown_switch:
  printErrAndExit UnknownSwitch

  initial_switch_format_error:
  printErrAndExit BadSwitchFormat


  initial_no_more_switches:

  mov   ah, 0x30                         ; Get DOS version
  int   0x21
  cmp   al, 2
  jnz   initial_above_dos_2              ; Skip if better than DOS 2

  printMsg Hello2_DOS2
  jmp   initial_num_drives

  initial_above_dos_2:

  ; reload es:bx to get the drive number from the RH header
  mov   ax, cs:rh_seg
  mov   es, ax
  mov   bx, cs:rh_off

  mov   al, es:[bx].rh0_first_drive       ; Drive number from RH header
  add   al, 'A'                           ; Make it ASCII
  mov   shared_data.net_first_letter, al  ; Save it for future reference.
  mov   DriveLetter, al
  printMsg Hello2

  initial_num_drives: 

  xor   ax, ax
  mov   al, drive_units_opt
  div   ten
  add   al, 48
  add   ah, 48
  cmp   al, 48
  jnz   initial_double_digits

  ; Just one digit to display
  mov   UnitsSupported2, ah
  jmp   initial_print_number

  initial_double_digits:
  mov   UnitsSupported2, al
  mov   UnitsSupported2+1, ah

  initial_print_number:
  printMsg UnitsSupported1

  initial_timer_setup:

  ; Hook timer interrupt so we can have a countdown timer.
  ; We should be able to use the BIOS tick interrupt for this but the
  ; 3Com PCI packet driver hangs if you do, so hook IRQ 0 instead.

  push  ds
  xor   ax, ax
  mov   ds, ax
  mov   ax, ds:[8h*4]
  mov   dx, ds:[8h*4+2]
  pushf
  cli
  mov   ds:[8h*4], OFFSET timerInt
  mov   ds:[8h*4+2], cs
  mov   word ptr cs:[timerOrigInt], ax
  mov   word ptr cs:[timerOrigInt+2], dx
  sti
  popf
  pop ds


  ; Set the base pointer for the unit specific storage to make it easy to find.
  mov shared_data.net_units_start, OFFSET units_storage


  ; DOS 2.x claims to update the header with the number of units based on the
  ; return code and later versions of DOS actually do it.  Do it unconditionally
  ; here so DOS 2.x is consistent and we can use the value from the command line
  ; utility.

  mov    al, drive_units_opt
  mov    cs:unit_count, al


  ; Setup to wipe out the unit specific data to zeros.  The actual wiping
  ; will be done on return, as it steps on the memory where this code is
  ; currently sitting.

  xor    dx, dx                          ; Get number of units in dx
  mov    dl, al
  mov    ax, size unit_data              ; Size of unit structure
  mul    dx
  mov    dx, ax                          ; Save bytes in dx for later
  shr    ax, 1                           ; Divide by 2 to get words
  mov    cx, ax                          ; Setup count for the rep stosw loop


  ; Restore pointer to request header in ES:BX
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off

  ; Return number of supported units.  Also provde a pointer to the BPB array
  ; for the initial ram disks (one for each unit.)

  mov    al, cs:unit_count
  mov    es:[bx].rh0_units, al
  mov    word ptr es:[bx].rh0_bpb_array_off, OFFSET RamDiskBPBArray
  mov    word ptr es:[bx].rh0_bpb_array_seg, cs


  ; Set the address to load the next driver at.  After our unit storage.

  add    dx, OFFSET units_storage
  mov    es:[bx].rh0_brk_off, dx
  mov    es:[bx].rh0_brk_seg, cs


  ; Setup to clear the bytes on return; CX still has the words to clear.

  mov    ax, cs                          ; es:di points to the unit storage
  mov    es, ax
  mov    di, OFFSET units_storage 
  xor    ax, ax

  ret



  initial_error_path:

  ; Restore pointer to request header in ES:BX
  mov    ax, cs:rh_seg
  mov    es, ax
  mov    bx, cs:rh_off

  ; Set units to 0, set the error flag, and set the break location to the
  ; very start (the attribute word) so that DOS discards the storage and
  ; installs nothing.

  mov    es:[bx].rh0_units, 0
  mov    es:[bx].rh_status, 0x8000
  mov    ax, OFFSET next_dev
  mov    es:[bx].rh0_brk_off, ax
  mov    es:[bx].rh0_brk_seg, cs

  ; Copy no bytes on return
  mov    cx, 0

  ret

initial endp




; Return values: cl is the switch letter (lower case) and bx is the value.
; cl == 0 means no more switches and cl == 255 is a format error

scanswitch proc near

  ; Clear return codes
  xor bx, bx
  mov cx, bx

  ; Reload the current position into ds:si
  mov ax, cs:cursor_seg
  mov ds, ax
  mov si, cs:cursor_off

  ; Look for our switch character ('-') or the end of the line.
  scanswitch1:
  lodsb
  cmp   al, 13
  jz    scanswitch_done
  cmp   al, 10
  jz    scanswitch_done
  cmp   al, '-'
  jnz   scanswitch1

  lodsb
  or    al, 0x20
  cmp   al, 'a'
  jb    scanswitch_badswitch
  cmp   al, 'z'
  ja    scanswitch_badswitch
  mov   cl, al
  lodsb
  cmp   al, ':'
  jnz   scanswitch_badswitch

  lodsb
  cmp   al, '0'
  jb    scanswitch_badswitch
  cmp   al, '9'
  ja    scanswitch_badswitch

  sub   al, '0'
  cbw
  mov   bx, ax

  getnum:
  lodsb
  cmp   al, ' '
  jz    scanswitch_done
  cmp   al, 13
  jz    scanswitch_done
  cmp   al, 10
  jz    scanswitch_done
  cmp   al, '0'
  jb    scanswitch_badswitch
  cmp   al, '9'
  ja    scanswitch_badswitch

  sub   al, '0'
  cbw
  xchg  ax, bx
  mov   dx, 10
  mul   dx
  add   bx, ax
  jmp   getnum


  scanswitch_badswitch:
  mov cl, 255
  ret

  
  scanswitch_done:

  mov cs:cursor_off, si
  mov cs:cursor_seg, ds

  ret

scanswitch endp



Hello           db    'mTCP NetDrive (Nov 10 2023) by M Brutman (C) 2023-2025 (www.brutman.com/mTCP)',13,10,'$'
Hello2_DOS2     db    '  First network drive is (unknown - DOS 2.x detected), ','$'
Hello2          db    '  First network drive is '
DriveLetter     db    'X:, ','$'
UnitsSupported1 db    'Number of drives: '
UnitsSupported2 db    '  ',13,10,'$'

BadSwitchFormat db    '  Error: Bad switch format, not loading',13,10,'$'
BadDriveCnt     db    '  Error: Bad number of drives on -d switch, not loading',13,10,'$'
UnknownSwitch   db    '  Error: Invalid switch specified, not loading',13,10,'$'


net    endp
cseg   ends
       end begin
