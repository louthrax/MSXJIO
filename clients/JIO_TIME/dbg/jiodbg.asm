; ------------------------------------------------------------------------------
; jiodbg.asm
; JIODBG.COM: diagnostic of the serial line of the JIO cartridge (temporary test tool)
; Usage: JIODBG [<port>]     port in hex (default: first port found among 00, 20, 30), J2 = joystick port 2
; 1. ports 00H, 20H, 30H: values read back after writing 2FH, DBH, F7H (cartridge: CC, 88, 44 with bits 1-0
;    masked) and value of the register at rest
; 2. date and time request (as JIOTIME), answer received by the receive routine, or time-out
; 3. same request, then changes of bit 0 (receive line) counted during about 0.5 s
; ------------------------------------------------------------------------------

BDOS		equ	0005h
COMMAND_DATE_TIME equ	23

		org	0100h

		ld	de,MSG_TITLE
		call	PrintString

		; 1. registers
		ld	hl,PORTS
Probe:		ld	a,(hl)
		cp	0FFh
		jr	z,ProbeEnd
		push	hl
		ld	c,a
		ld	de,MSG_PORT
		call	PrintStringC
		ld	a,c
		call	PrintHexC
		ld	de,MSG_IDLE
		call	PrintStringC
		in	a,(c)
		call	PrintHexC
		ld	b,0
		ld	a,2Fh
		call	ProbeValue
		ld	a,0DBh
		call	ProbeValue
		ld	a,0F7h
		call	ProbeValue
		ld	a,b
		cp	3
		jr	nz,ProbeNo
		ld	a,(FOUND)
		inc	a
		jr	nz,ProbeNo
		ld	a,c
		ld	(FOUND),a
ProbeNo:	push	bc
		ld	de,MSG_OK
		ld	a,b
		cp	3
		jr	z,ProbeMsg
		ld	de,MSG_NO
ProbeMsg:	call	PrintString
		pop	bc
		; register back to the idle state of a serial line: bit 2 = 1 (transmit line at rest), bit 3 kept
		in	a,(c)
		and	08h
		or	04h
		out	(c),a
		pop	hl
		inc	hl
		jr	Probe

; Write A to port C, print the value read back, B + 1 if it is the value of the cartridge
; (bits 3 and 2 written read back in bits 7, 3 and 6, 2; bits 1 and 0 masked)
ProbeValue:	push	af			; value written
		out	(c),a
		in	a,(c)
		push	af			; value read
		ld	de,MSG_SPACE
		call	PrintStringC
		pop	af
		push	af
		call	PrintHexC
		pop	af
		and	0FCh
		ld	e,a
		pop	af
		and	0Ch
		ld	d,a
		rlca
		rlca
		rlca
		rlca
		or	d
		cp	e
		ret	nz
		inc	b
		ret

ProbeEnd:	; port to test: parameter, else the first cartridge found
		ld	hl,0080h
		ld	e,(hl)
		ld	d,0
		inc	hl
		push	hl
		add	hl,de
		ld	(hl),0
		pop	hl
SkipSp:		ld	a,(hl)
		cp	' '
		jr	nz,ArgEnd
		inc	hl
		jr	SkipSp
ArgEnd:		or	a
		jr	z,UseFound
		and	0DFh
		cp	'J'
		jr	nz,ArgHex
		ld	a,0FFh			; J2: joystick port 2
		jr	SetPort
ArgHex:		ld	a,(hl)
		call	HexDigit
		jp	c,Usage
		rlca
		rlca
		rlca
		rlca
		ld	b,a
		inc	hl
		ld	a,(hl)
		call	HexDigit
		jp	c,Usage
		or	b
		jr	SetPort
UseFound:	ld	a,(FOUND)
		cp	0FFh
		jr	nz,SetPort
		ld	de,MSG_NOCART
		call	PrintString
		ld	a,30h			; nothing found: tested on 30H anyway
SetPort:	ld	(_JioPort),a
		ld	de,MSG_TEST
		call	PrintStringC
		ld	a,(_JioPort)
		call	PrintHex
		ld	de,MSG_CRLF
		call	PrintString

		; 2. request and answer
		ld	b,3
Try:		push	bc
		di
		ld	hl,COMMAND
		ld	de,COMMAND_END-COMMAND
		call	vJIOTransmit
		ld	hl,ANSWER
		ld	de,ANSWER_END-ANSWER
		call	bJIOReceive
		ei
		pop	bc
		or	a
		jr	nz,Answer
		push	bc
		ld	de,MSG_TIMEOUT
		call	PrintString
		pop	bc
		djnz	Try
		jr	Raw
Answer:		ld	de,MSG_ANSWER
		call	PrintString
		ld	hl,ANSWER
		ld	b,ANSWER_END-ANSWER
AnsLoop:	ld	a,(hl)
		push	hl
		push	bc
		call	PrintHexC
		ld	a,' '
		call	PrintChar
		pop	bc
		pop	hl
		inc	hl
		djnz	AnsLoop
		ld	de,MSG_CRLF
		call	PrintString

		; 3. raw activity of the receive line after the request
Raw:		ld	de,MSG_RAW
		call	PrintString
		di
		ld	hl,COMMAND
		ld	de,COMMAND_END-COMMAND
		call	vJIOTransmit
		ld	a,(_JioPort)
		ld	c,a
		inc	a
		jr	nz,RawCart
		ld	a,15			; joystick port 2: PSG register 14
		out	(0A0h),a
		in	a,(0A2h)
		or	64
		out	(0A1h),a
		ld	a,14
		out	(0A0h),a
		ld	c,0A2h
RawCart:	in	a,(c)
		ld	(FIRST),a
		and	1
		ld	d,a			; last level
		ld	hl,0			; changes
		ld	iy,0			; samples (65536, about 0.5 s)
RawLoop:	in	a,(c)
		and	1
		cp	d
		jr	z,RawSame
		ld	d,a
		inc	hl
RawSame:	dec	iy
		ld	a,iyh
		or	iyl
		jr	nz,RawLoop
		ei
		push	hl
		ld	de,MSG_FIRST
		call	PrintString
		ld	a,(FIRST)
		call	PrintHexC
		ld	de,MSG_CHANGES
		call	PrintString
		pop	hl
		ld	a,h
		call	PrintHexC
		ld	a,l
		call	PrintHexC
		ld	de,MSG_HEXEND
		jp	PrintString

Usage:		ld	de,MSG_USAGE
		jp	PrintString

; ------------------------------------------------------------------------------
HexDigit:	sub	'0'
		ret	c
		cp	10
		ccf
		ret	nc
		and	0DFh
		sub	'A'-'0'
		ret	c
		cp	6
		ccf
		ret	c
		add	a,10
		ret

; Print A in hex (2 digits), keeps BC
PrintHexC:	push	bc
		call	PrintHex
		pop	bc
		ret
PrintHex:	push	af
		rrca
		rrca
		rrca
		rrca
		call	PrintDigit
		pop	af
PrintDigit:	and	0Fh
		add	a,'0'
		cp	'9'+1
		jr	c,PrintChar
		add	a,'A'-'9'-1
PrintChar:	ld	e,a
		ld	c,02h
		jp	BDOS

PrintStringC:	push	bc
		call	PrintString
		pop	bc
		ret
PrintString:	ld	c,09h
		jp	BDOS

; ------------------------------------------------------------------------------
_JioPort:	defb	0FFh
INCLUDE "../../JIO_NFS/transmit.asm"
INCLUDE "../../JIO_NFS/receive.asm"

COMMAND:	defb	"JIO",0,COMMAND_DATE_TIME
COMMAND_END:
ANSWER:		defs	7,0
ANSWER_END:
FIRST:		defb	0
FOUND:		defb	0FFh
PORTS:		defb	00h,20h,30h,0FFh

MSG_TITLE:	defb	"JIODBG: JIO cartridge diagnostic",13,10
		defb	"port: idle, 2F DB F7 read back",13,10,"$"
MSG_PORT:	defb	"$"
MSG_IDLE:	defb	": $"
MSG_SPACE:	defb	" $"
MSG_OK:		defb	" cartridge",13,10,"$"
MSG_NO:		defb	" -",13,10,"$"
MSG_NOCART:	defb	"No cartridge found, test on 30",13,10,"$"
MSG_TEST:	defb	"Date request on port $"
MSG_TIMEOUT:	defb	"Time-out",13,10,"$"
MSG_ANSWER:	defb	"Answer: $"
MSG_RAW:	defb	"Raw: $"
MSG_FIRST:	defb	"first $"
MSG_CHANGES:	defb	", bit 0 changes $"
MSG_HEXEND:	defb	"H",13,10,"$"
MSG_CRLF:	defb	13,10,"$"
MSG_USAGE:	defb	"Usage: JIODBG [<port in hex>|J2]",13,10,"$"
