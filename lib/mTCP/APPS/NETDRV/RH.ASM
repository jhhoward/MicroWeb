; mTCP NetDrive (RH.ASM)
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
; Description: DOS Request Header data structures



; Data structures
;
; Request Header (common part)

rh                 struc                       ; DOS request header
rh_len             db          ?               ; Length of packet
rh_unit            db          ?               ; Unit code (block devices)
rh_cmd             db          ?               ; Command
rh_status          dw          ?               ; Return code?
rh_res1            dd          ?               ; Reserved, 32 bits
rh_res2            dd          ?               ; Reserved, 32 bits
rh                 ends


; Request Header (initialization)

rh0                struc
rh0_rh             db size rh dup (?)
rh0_units          db          ?               ; block devices only
rh0_brk_off        dw          ?               ; Offset for break
rh0_brk_seg        dw          ?               ; Segment for break
rh0_bpb_array_off  dw          ?               ; Input: parms, Output: BPB Array offset
rh0_bpb_array_seg  dw          ?               ; Input: parms, Output: BPB Array segment
rh0_first_drive    db          ?               ; Input: first drive number
rh0_err_msg_flag   dw          ?               ; Dos 4+ only? Not going to use this.
rh0                ends

; IOCTL

rh_ioctl           struc
rh_ioctl_rh        db size rh dup (?)
rh_ioctl_media     db          ?               ; Media descriptor from DPB
rh_ioctl_buf_off   dw          ?               ; Buffer offset
rh_ioctl_buf_seg   dw          ?               ; Buffer segment
rh_ioctl_count     dw          ?               ; Byte or sector count
rh_ioctl_start     dw          ?               ; Start sector number
rh_ioctl           ends


; Input and Output

rh_input           struc
rh_input_rh        db size rh dup (?)
rh_input_media     db          ?
rh_input_buf_off   dw          ?
rh_input_buf_seg   dw          ?
rh_input_count     dw          ?
rh_input_start     dw          ?
rh_input_volIdPtr  dd          ?
rh_input_hugeStart dd          ?
rh_input           ends


; Media Check

rh_media_chk       struc
rh_media_rh        db size rh dup (?)
rh_media_media     db          ?
rh_media_return    db          ?
rh_media_volid     dd          ?         ; Only needed if removable media dh attr set.
rh_media_chk       ends


; Build BPB

rh_get_bpb         struc
rh_get_bpb_rh      db size rh dup (?)
rh_get_bpb_media   db          ?
rh_get_bpb_dta_off dw          ?         ; Data Transfer Area offset (not used)
rh_get_bpb_dta_seg dw          ?         ; Data Transfer Area segment (not used)
rh_get_bpb_bpb_off dw          ?         ; Return value: BPB offset
rh_get_bpb_bpb_seg dw          ?         ; Return value: BPB segment
rh_get_bpb         ends



