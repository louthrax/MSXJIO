; ------------------------------------------------------------------------------
; drv_jio.asm
;
; Copyright (C) 2025 [put your copyright notice here]
;
; [put your license terms here]
; ------------------------------------------------------------------------------
; [optional: description and highlights of the driver]
; ------------------------------------------------------------------------------

        INCLUDE	"disk.inc"	; Assembler directives
        INCLUDE	"msx.inc"	; MSX constants and definitions

        SECTION	DRV_JIO

        ; Mandatory symbols defined by the disk hardware interface driver
        PUBLIC	DRVINIT	; Initialize hardware interface driver
        PUBLIC	INIENV	; Initialize driver environment
        PUBLIC	DSKIO	; Disk I/O routine
        PUBLIC	DSKCHG	; Disk change routine
        PUBLIC	READSEC	; Read sector (MBR / bootsector) routine
        PUBLIC	DRVMEM	; Memory for hardware interface variables

        EXTERN	GETWRK	; Get address of disk driver's work area
        EXTERN	GETSLT	; Get slot of this interface
        EXTERN	DRVSIZE	; DOS driver workarea size (offset)
        EXTERN	W_CURDRV	; Workarea variable defined by the DOS driver
        EXTERN	W_BOOTDRV	; "
        EXTERN	W_DRIVES	; "

        ; Driver routines, use in DRVINIT/INIENV only
        EXTERN	PART_BUF

        EXTERN	PrintMsg
        EXTERN	PrintCRLF
        EXTERN	PrintString

; Hardware driver variables
W_FLAGS	equ	DRVSIZE
W_TMP	equ	DRVSIZE+1
DRVMEM	equ	2

#include "flags.inc"

CHGCPU	equ	$180
GETCPU	equ	$183

;********************************************************************************************************************************
; Initialize hardware interface driver
;********************************************************************************************************************************

DRVINIT:
        ld	(ix+W_FLAGS),0

        call	PrintMsg
IFDEF IDEDOS1
        db	"JIO MSX-DOS 1",13,10
ELSE
        db	"JIO MSX-DOS 2",13,10
ENDIF
        db	"Rev.: "
        INCLUDE	"rdate.inc"	; Revision date
        db	13,10
        db	"Waiting for server,",13,10
        db	"press [ESC] to cancel",0


DRVINIT_Retry:
        ld	a,7
        call	SNSMAT
        and	4
        ret     z

        ld	a,'.'
        rst	$18

        ld	e,0xFF
        ld	d,e
        ld	c,e

        ld	b,1
        ld	hl,PART_BUF

        ld      a,COMMAND_INFO
        call	ReadOrWriteSectors
        jr	c,DRVINIT_Retry

        ld	hl,PART_BUF
        ld	a,(hl)
        ld	(ix+W_FLAGS),a
        inc	hl
        call	PrintString

        xor	a
        ret

;********************************************************************************************************************************
; INIENV - Initialize the work area (environment)
; Input : None
; Output: None
; May corrupt: AF,BC,DE,HL,IX,IY
;********************************************************************************************************************************

INIENV:	call	GETWRK	; HL and IX point to work buffer
        xor	a
        or	(ix+W_DRIVES)	; number of drives 0?
        ret	z
        ld	(ix+W_CURDRV),$ff	; Init current drive
        call	GETSLT
        ld 	hl,DRVTBL
        ld	b,a	; B = this disk interface slot number
        ld	c,$00

TestInterface:	ld	a,(hl)
        add	a,c
        ld	c,a
        inc	hl
        ld	a,(hl)
        inc	hl
        cp	b	; this interface?
        jr	nz,TestInterface	; nz=no

        dec	hl
        dec	hl
        ld	a,c
        sub	(hl)
        ld	b,(ix+W_BOOTDRV)	; Get boot drive
        add	a,b
        ld	(ix+W_BOOTDRV),a	; Set boot drive
        call	PrintMsg
        db	"Drives: ",0
        ld	a,(ix+W_DRIVES)
        add	a,'0'
        rst	$18
        jp	PrintCRLF

;********************************************************************************************************************************
; DSKIO - Disk Input / Output
; Input:
;   Carry flag = clear ==> read, set ==> write
;   A  = drive number
;   B  = number of sectors to transfer
;   C  = if bit7 is set then media descriptor byte
;	else first logical sector number bit 16..22
;   DE = first logical sector number (bit 0..15)
;   HL = transfer address
; Output:
;   Carry flag = clear ==> successful, set ==> error
;   If error then
;	 A = error code
;	 B = remaining sectors
; May corrupt: AF,BC,DE,HL,IX,IY
;********************************************************************************************************************************

DSKIO:	push	hl
	push	de
	push	bc

	push	af
	cp	$08	; Max 8 drives (partitions) supported
	jr	nc,r404
	call	GETWRK	; Base address of workarea in hl and ix
	pop	af

        ld      (ix+W_TMP),COMMAND_WRITE
	jr	c,WriteFlag
        ld      (ix+W_TMP),COMMAND_READ
WriteFlag:

        ld	e,a
        add	a,a
        add	a,a
        add	a,e
        ld	e,a	; a * 5
        ld	d,$00
        add	hl,de

        push	hl
        pop	iy

        pop	bc
        pop	de
        pop	hl

        xor	a
        or	(iy+$04)	; Test if partition exists (must have nonzero partition type)
        jr	z,r405

        ; Translate logical to physical sector number
        push	bc	; Save sector counter
        push 	hl	; Save transfer address
        ex	de,hl
        bit	7,c	; If bit 7 of media descriptor is 0 then use 23-bit sector number
        jr	nz,r401	; nz if 16-bit sector number
        ld	e,c	; Bit 16-22 of sector number
        ld	d,$00
        jr	r402

r401:	ld	de,$0000

r402:	ld	c,(iy+$00)
        ld	b,(iy+$01)
        add	hl,bc
        ex	de,hl	; LBA address: de=00..15
        ld	c,(iy+$02)
        ld	b,(iy+$03)
        adc	hl,bc
	ld	c,l	; LBA address: c=16..23
	pop	hl	; Restore transfer address
	pop	af	; Restore sector counter
	ld	b,a

        ld      a,(ix+W_TMP)
        jp	ReadOrWriteSectors

	; Disk i/o error
r404:	pop	af
	pop	bc
	pop	de
	pop	hl

r405:	ld	a,$04	; Error 4 = Data (CRC) error (abort,retry,ignore message)
	scf
        ret

;********************************************************************************************************************************
; DSKCHG - Disk change
; Input:
;   A  = Drive number
;   B  = 0
;   C  = Media descriptor
;   HL = Base address of DPB
; Output:
;   If successful then
;	 Carry flag reset
;	 B = Disk change status
;	 1= Disk unchanged, 0= Unknown, -1=Disk changed
;   else
;	 Carry flag set
;	 Error code in A
; May corrupt: AF,BC,DE,HL,IX,IY
;********************************************************************************************************************************
DSKCHG:
IFDEF IDEDOS1
        push	af
        call	GETWRK
        pop	af
        cp	(ix+W_CURDRV)	; current drive
        ld	(ix+W_CURDRV),a
        jr	nz,DiskChanged
        ld	b,$01	; unchanged
        xor	a
        ret

DiskChanged:	ld	b,$FF	; changed
        xor	a
        ret
ELSE
        ; Always return unchanged for DOS2 (disks are not hot-pluggable)
        ld	b,$01
        xor	a
        ret
ENDIF

INCLUDE	"drv_jio_c.asm"
INCLUDE	"crt.asm"

;********************************************************************************************************************************
; Read (boot) sector
; Input: C,DE = sector number
;********************************************************************************************************************************
READSEC:
        ld      a,COMMAND_READ
        ld	b,1

; CDE : Sector
; B   : Length
; HL  : Address

ReadOrWriteSectors:
        push    af
        ld      a,(ix+W_FLAGS)
        and     0xF0
        ld      (ix+W_FLAGS),a
        pop     af

        or      (ix+W_FLAGS)
        ld      (ix+W_FLAGS),a

        ld      a,c             ; Flags
        ld      c,(ix+W_FLAGS)
        push    bc

        push    hl              ; Address

        ld      c,b             ; Length
        push    bc

        ld      b,0
        ld      c,a

        di
        call    ucReadOrWriteSectors
        pop hl
        pop hl
        pop hl

        or      a
        ret     z
        scf
        ret

;********************************************************************************************************************************
;********************************************************************************************************************************

eGetCPU:
        ld	a,(GETCPU)
        cp	$C3
        ret     nz
        jp      GETCPU

;********************************************************************************************************************************
;********************************************************************************************************************************

vSetCPU:
        ld	a,(CHGCPU)
        cp	$C3
        ret     nz
        ld      a,e
        jp      CHGCPU

;********************************************************************************************************************************
; IN:  HL = DATA
;      BC = LENGTH
;********************************************************************************************************************************

vJIOTransmit:
        ex      de,hl
        inc	bc
        exx
        ld	a,15
        out	($a0),a
        in	a,($a2)
        or	4
        ld	e,a
        xor	4
        ld	d,a
        ld	c,$a1

        db	$3e
JIOTransmitLoop:	ret	nz
        out	(c),e
        exx
        ld	a,(hl)
        cpi
        ret	po
        exx
        rrca
        out	(c),d	; =0
        ret	nz
        jp	c,TRANSMIT10
        out	(c),d	; -0
        rrca
        jp	c,TRANSMIT11
;________________________________________________________________________________________________________________________________

TRANSMIT01:	out	(c),d	; -1
        rrca
        jr	c,TRANSMIT12
        nop

TRANSMIT02:	out	(c),d	; -0
        rrca
        jp	c,TRANSMIT13

TRANSMIT03:	out	(c),d	; -1
        rrca
        jr	c,TRANSMIT14
        nop

TRANSMIT04:	out	(c),d	; -0
        rrca
        jp	c,TRANSMIT15

TRANSMIT05:	out	(c),d	; -1
        rrca
        jr	c,TRANSMIT16
        nop

TRANSMIT06:	out	(c),d	; -0
        rrca
        jp	c,TRANSMIT17

TRANSMIT07:	out	(c),d	; -1
        jp	JIOTransmitLoop
;________________________________________________________________________________________________________________________________

TRANSMIT10:	out	(c),e	; -0
        rrca
        jp	nc,TRANSMIT01

TRANSMIT11:	out	(c),e	; -1
        rrca
        jr	nc,TRANSMIT02
        nop

TRANSMIT12:	out	(c),e	; -0
        rrca
        jp	nc,TRANSMIT03

TRANSMIT13:	out	(c),e	; -1
        rrca
        jr	nc,TRANSMIT04
        nop

TRANSMIT14:	out	(c),e	; -0
        rrca
        jp	nc,TRANSMIT05

TRANSMIT15:	out	(c),e	; -1
        rrca
        jr	nc,TRANSMIT06
        nop

TRANSMIT16:	out	(c),e	; -0
        rrca
        jp	nc,TRANSMIT07

TRANSMIT17:	out	(c),e	; -1
        jp	JIOTransmitLoop

;********************************************************************************************************************************
;********************************************************************************************************************************

bJIOReceive:
        ld      h,d
        ld      l,e
        ld      d,b
        ld      e,c

        push	ix
        push	de

        ld      d,0
        ld      e,d

        dec	hl
        ld	b,(hl)	; What if HL=0 ?
        ld	c,$a2
        ld	ix,0
        add	ix,sp
        ld	a,15
        out	($a0),a
        in	a,($a2)
        or	64
        out	($a1),a
        ld	a,14
        out	($a0),a
        in	a,($a2)
        or	1
        jp	pe,HeaderPE
;________________________________________________________________________________________________________________________________

HeaderPO:	dec	de	;  7
        ld	a,d	;  5
        or	e	;  5
        jr	z,ReceiveTimeOut	;  8

        in	f,(c)	; 14
        jp	po,HeaderPO	; 11   LOOP=50 (2-CLOCKS)
        rlc	a
        in	f,(c)	; 14
        jp	po,HeaderPO	; 11   At least 2 clocks needed to be down

WU_PO:	in	f,(c)	; 14
        jp	pe,WU_PO	; 11   LOOP=25
        pop	de
        push	de

RX_PO:	in	f,(c)	; 14
        jp	po,RX_PO	; 11   LOOP=25
        ld	(hl),b	;  8  = 33 CYCLES

        in	a,(c)	; 14   Bit 0
        nop		;  5
        rrca		;  5
        dec	de	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 1
        xor	b	;  5
        rrca		;  5
        inc	hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 2
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 3
        xor	b	;  5
        rrca	                             	;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 4
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 5
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 6
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 7
        xor	b	;  5
        rrca		;  5

        ld	b,a	;  5
        ld	a,d	;  5
        or	e	;  5
        jp	nz,RX_PO	; 11

        jp	ReceiveOK
;________________________________________________________________________________________________________________________________

ReceiveOK:
        ld  (hl),b
        ld	sp,ix

        pop	de
        pop	ix
        ld      a,1
        ret

ReceiveTimeOut:
        pop	de
        pop	ix
        xor     a
        ret

;________________________________________________________________________________________________________________________________

HeaderPE:	dec	de	;  7
        ld	a,d	;  5
        or	e	;  5
        jr	z,ReceiveTimeOut	;  8

        in	f,(c)	; 14
        jp	pe,HeaderPE	; 11   LOOP= 50 (2-CLOCKS)
        rlc	a	; 10
        in	f,(c)	; 14
        jp	pe,HeaderPE	; 11   At least 2 clocks needed to be down

WU_PE:	in	f,(c)	; 14
        jp	po,WU_PE	; 11   LOOP=25
        pop	de
        push	de

RX_PE:	in	f,(c)	; 14
        jp	pe,RX_PE	; 11   LOOP=25
        ld	(hl),b	;  8 = 33 CYCLES

        in	a,(c)	; 14   Bit 0
        cpl		;  5
        rrca		;  5
        dec	de	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 1
        xor	b	;  5
        rrca		;  5
        inc	hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 2
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 3
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 4
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 5
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 6
        xor	b	;  5
        rrca		;  5
        ld	sp,hl	;  7 = 31 CYCLES

        in	b,(c)	; 14   Bit 7
        xor	b	;  5
        rrca		;  5

        ld	b,a	;  5
        ld	a,d	;  5
        or	e	;  5
        jp	nz,RX_PE	; 11

        jp	ReceiveOK

;________________________________________________________________________________________________________________________________

; Compute xmodem CRC-16
; Input:  DE    = buffer
;         BC    = bytes
;         Stack = CRC-16
; Output: HL    = updated CRC-16

uiXModemCRC16:
    ld  l,c
    ld  h,b

    ld  b,l
    dec hl
    inc h
    ld  c,h

    pop hl
    pop hl
    push hl
    dec sp
    dec sp

crc16:
    push bc
    ld	a,(de)
    inc	de
    xor     h
    ld      b,a
    ld      c,l
    rrca
    rrca
    rrca
    rrca
    ld      l,a
    and     0fh
    ld      h,a
    xor     b
    ld      b,a
    xor     l
    and     0f0h
    ld      l,a
    xor     c
    add     hl,hl
    xor     h
    ld      h,a
    ld      a,l
    xor     b
    ld      l,a

    pop     bc
    djnz    crc16

    dec     c
    jp      nz,crc16
    ret
