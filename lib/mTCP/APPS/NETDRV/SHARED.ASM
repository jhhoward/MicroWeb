; mTCP NetDrive (SHARED.ASM)
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
; Description: Data structures shared with the C program.



; Network control data structure and per-unit data.  These are shared with
; the command line tool and need to stay synchronized with it.

unit_data           struc
unit_media_check    db          ?               ; 0 (unknown), 1 (no change) or 0xff (changed)
unit_readonly       db          ?               ; If 1 unit is readonly
unit_server_ip      db          4 dup(?)        ; Server IP address
unit_server_port    dw          ?               ; Server UDP port number
unit_next_hop_mac   db          6 dup(?)        ; Next hop to get to this server
unit_session        dw          ?               ; Session number (assigned by server)
unit_sequence       dw          ?               ; UDP packet sequence number
unit_remote_name    db          40 dup(?)       ; Null terminated remote drive name
unit_bpb            db          size bpb dup(?) ; Drive parameter block for this unit.
                    db          ?               ; padding
unit_blocks_read    dd          ?
unit_blocks_written dd          ?
unit_blocks_retries dd          ?
unit_data           ends




net_data           struc

net_segment        dw          ?               ; (ReadOnly) Our code segment
net_receiver_off   dw          ?               ; (ReadOnly) Packet receiver function offset
net_shim_off       dw          ?               ; (ReadOnly) Packet driver shim offset
net_int_inst_off   dw          ?               ; (ReadOnly) Interrupt instruction offset
net_units_start    dw          ?               ; (Readonly) Offset to unit specific data
net_first_letter   db          ?               ; (ReadOnly) Not valid for DOS 2.x
net_connected_cnt  db          ?               ; How many drives are connected?

; NETDRIVE.EXE wipes everything after this

net_pkt_drv_hndl   dw          ?               ; Packet driver handle
net_pkt_drv_int    db          ?               ; Packet driver software interrupt
net_pkt_drv_int2   db          ?               ; Shim software interrupt
                   db          0
net_pkt_recv_on    db          ?               ; Are we receiving packes
net_pkt_timeout    dw          ?               ; Timeout threshold
net_pkt_retries    dw          ?               ; How many attempts

net_my_ip          db          4 dup(?)        ; My IP address
net_my_port        dw          ?               ; My UDP port number
net_my_mac         db          6 dup(?)        ; MAC address of this machine

net_c_r_total      dd          0               ; Total packets received
net_c_r_our_mac    dd          0               ; Packets sent to our MAC address
net_c_r_our_udp    dd          0               ; UDP from our server sent to us
net_c_s_arp_resp   dd          0               ; How many ARP responses have we sent?
net_c_send_errs    dd          0               ; How many times did the packet driver refuse to send?

net_arp_eye        db          8 dup(?)        ; Eye catcher
net_arp_resp       db size pkt_arp dup(?)      ; Prebuilt ARP response

ifdef CHKSUM_TESTING
test_IP            dd          ?
test_UDP           dd          ?
endif

net_data           ends


