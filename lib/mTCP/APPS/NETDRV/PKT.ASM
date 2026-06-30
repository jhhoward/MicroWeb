; mTCP NetDrive (PKT.ASM)
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


; Network packet structures


; ARP packet (full)
;
; The ARP part of the packet always starts immediately after the Ethernet
; header so this structure can be used to map all of the parts of an ARP
; packet.

pkt_arp            struc
pkt_arp_eth_dest   db          6 dup(?)        ; Ethernet dest
pkt_arp_eth_src    db          6 dup(?)        ; Ethernet source
pkt_arp_eth_type   dw          ?               ; Ethernet type
pkt_arp_hdw_type   dw          ?               ; ARP hardware type (0x0001)
pkt_arp_prot_type  dw          ?               ; ARP protocol type (0x0800)
pkt_arp_hlen       db          ?               ; ARP hardware length (6)
pkt_arp_plen       db          ?               ; ARP protocol length (4)
pkt_arp_op         dw          ?               ; ARP operation (0x0001 request, 0x0002 response)
pkt_arp_sender_ha  db          6 dup(?)        ; Sender hardware address
pkt_arp_sender_ip  db          4 dup(?)        ; Sender IP address
pkt_arp_target_ha  db          6 dup(?)        ; Target hardware address
pkt_arp_target_ip  db          4 dup(?)        ; Target IP address
pkt_arp_filler     db          18 dup(?)       ; Advertising space
pkt_arp            ends


; IP packet (partial)
;
; The IP part of the packet always starts immediately after the Ethernet
; header, however the IP part can be variable length.  So you can use this
; to inspect the fixed part of the IP header, but to look beyond that you
; must compute an offset to the IP payload.  (The UDP header in our case.)

pkt_ip             struc
pkt_ip_eth_dest    db          6 dup(?)        ; Ethernet dest
pkt_ip_eth_src     db          6 dup(?)        ; Ethernet source
pkt_ip_eth_type    dw          ?               ; Ethernet type
pkt_ip_vers_hlen   db          ?               ; Version and header length
pkt_ip_service     db          ?
pkt_ip_total_len   dw          ?
pkt_ip_ident       dw          ?
pkt_ip_flags       dw          ?
pkt_ip_ttl         db          ?
pkt_ip_protocol    db          ?
pkt_ip_chksum      dw          ?
pkt_ip_src         db          4 dup(?)
pkt_ip_dest        db          4 dup(?)
pkt_ip             ends


; UDP Header, NetDrive Command, and optional NetDrive Payload
;
; When sending we won't use IP header options so this will appear right after
; the IP header.  But when receiving this might be after optional IP header
; options, so always compute an address to the UDP header before referencing
; it.

Udp_Nd_Len         equ         0x16            ; Fixed portion len: UDP and NetDrive hdrs
Nd_Len             equ         0x0e            ; Just the NetDrive (_nd_) headers part

pkt_udp            struc
pkt_udp_src        dw          ?
pkt_udp_dest       dw          ?
pkt_udp_len        dw          ?
pkt_udp_chksum     dw          ?
pkt_nd_vers        dw          ?               ; 16 bit protocol version
pkt_nd_session     dw          ?               ; Session assigned by server
pkt_nd_sequence    dw          ?               ; netdrive sequence number
pkt_nd_op          db          ?               ; netdrive operation
pkt_nd_result      db          ?               ; return code
pkt_nd_start       dd          ?               ; 32 bit start LBA
pkt_nd_count       dw          ?               ; 16 bit number of sectors
pkt_nd_data        db          ?               ; Data starts here
pkt_udp            ends



; Outgoing packet
;
; Use pkt_ip to map out_pkt_start and pkt_udp to map out_udp_start

out_pkt            struc
out_pkt_start      db size pkt_ip dup(?)       ; Space for Ethernet and IP headers
out_udp_start      dw          ?               ; Overlay pkt_udp here
out_pkt            ends



Op_Connect        equ   1
Op_Disconnect     equ   2
Op_Read           equ   3
Op_Write          equ   4
Op_Write_Verify   equ   5


Eth_hdr_len       equ 0x0e      ; Ethernet header length (14 bytes)

Eth_Type_Arp      equ 0x0608    ; Network byte order
Eth_Type_Ip       equ 0x0008    ; Network byte order

Ip_Udp_protocol   equ 0x11      ; IP UDP protocol type
