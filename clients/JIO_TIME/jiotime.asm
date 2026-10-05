; ------------------------------------------------------------------------------
; jiotime.asm
; JIOTIME.COM: sets the date and time of the MSX from the JIO server
; (COMMAND_DATE_TIME, see msxjio_protocol_specification.md)
;
; Works with MSX-DOS 1 and MSX-DOS 2 (_SDATE and _STIME), whatever the mode of the
; server (disk image or directories). On turbo R, the transfer is done in Z80 mode.
; Usage: JIOTIME [J1|J2|C[<port>]]
;   J1, J2     joystick port 1 or 2 (default: 2)
;   C          JIO cartridge (herraa1/msx-jio-cart-v1) instead of joystick port 2, port detected
;   C<port>    JIO cartridge at this I/O port (hex: 00, 20 or 30)
; 115K2 transmit/receive routines based on code by Nyyrikki (clients/JIO_NFS)
; ------------------------------------------------------------------------------

BDOS		equ	0005h
RDSLT		equ	000Ch
CALSLT		equ	001Ch
EXPTBL		equ	0FCC1h
MSXVER		equ	002Dh		; main ROM: MSX version (3 = turbo R)
CHGCPU		equ	0180h
GETCPU		equ	0183h

TRIES		equ	3		; tries before "no answer" (time-out about 1 s each)

INCLUDE "../../common/drv_jio.inc"

		org	0100h

		ld	de,MSG_TITLE
		call	PrintString

		; parameters: J1, J2 (joystick port), C (JIO cartridge, port detected) or C<port> (hex)
		ld	hl,0080h
		ld	e,(hl)
		ld	d,0
		inc	hl
		push	hl
		add	hl,de
		ld	(hl),0			; end of the parameters
		pop	hl
		call	SkipSpaces
		jr	z,ArgsDone		; no parameter: joystick port 2
		and	0DFh			; upper case
		cp	'J'
		jr	z,ParseJoystick
		cp	'C'
		jp	nz,Usage
		inc	hl
		ld	a,(hl)
		call	IsEnd
		jr	nz,ParsePort
		call	DetectPort		; C: port detected
		cp	0FFh
		jr	nz,SetPort
		ld	de,MSG_NO_CART
		jp	PrintString

ParsePort:	call	HexDigit
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
		ld	b,a
		inc	hl
		call	SkipSpaces
		jp	nz,Usage
		ld	a,b
		cp	0FEh			; 0FEh, 0FFh: joystick ports
		jp	nc,Usage
		jr	SetPort

ParseJoystick:	inc	hl			; J1 or J2
		ld	a,(hl)
		sub	'1'
		cp	2
		jp	nc,Usage
		add	a,0FEh			; J1: 0FEh, J2: 0FFh
		ld	(_JioPort),a
		inc	hl
		call	SkipSpaces
		jp	nz,Usage
		ld	a,(_JioPort)
		inc	a
		jr	z,ArgsDone		; joystick port 2
		ld	de,MSG_JOY1
		call	PrintString
		jr	ArgsDone

SetPort:	ld	(_JioPort),a
		push	af
		ld	de,MSG_CART
		call	PrintString
		pop	af
		call	PrintHex
		ld	de,MSG_PORT_END
		call	PrintString
ArgsDone:

		; turbo R: Z80 mode for the transfer
		ld	a,(EXPTBL)
		ld	hl,MSXVER
		call	RDSLT
		cp	3
		jr	c,NoTurbo
		ld	ix,GETCPU
		call	CallBIOS
		ld	(CPU_MODE),a
		ld	a,1
		ld	(IS_TURBO),a
		xor	a			; Z80 mode
		ld	ix,CHGCPU
		call	CallBIOS
NoTurbo:
		ld	b,TRIES
Retry:		push	bc
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
		jr	nz,Received
		djnz	Retry

		call	RestoreCPU
		ld	de,MSG_NO_ANSWER
		jp	PrintString

Received:	call	RestoreCPU

		ld	hl,(A_YEAR)
		ld	a,(A_MONTH)
		ld	d,a
		ld	a,(A_DAY)
		ld	e,a
		ld	c,2Bh			; _SDATE
		call	BDOS
		or	a
		jr	nz,Invalid

		ld	a,(A_HOUR)
		ld	h,a
		ld	a,(A_MINUTE)
		ld	l,a
		ld	a,(A_SECOND)
		ld	d,a
		ld	e,0
		ld	c,2Dh			; _STIME
		call	BDOS
		or	a
		jr	nz,Invalid

		; date and time of the MSX, read back
		ld	c,2Ah			; _GDATE
		call	BDOS
		ld	(A_YEAR),hl
		ld	a,d
		ld	(A_MONTH),a
		ld	a,e
		ld	(A_DAY),a
		ld	c,2Ch			; _GTIME
		call	BDOS
		ld	a,h
		ld	(A_HOUR),a
		ld	a,l
		ld	(A_MINUTE),a
		ld	a,d
		ld	(A_SECOND),a

		ld	de,MSG_SET
		call	PrintString
		ld	hl,(A_YEAR)
		ld	ix,POW4
		call	PrintNumber
		ld	a,'-'
		ld	hl,A_MONTH
		call	PrintField
		ld	a,'-'
		ld	hl,A_DAY
		call	PrintField
		ld	a,' '
		ld	hl,A_HOUR
		call	PrintField
		ld	a,':'
		ld	hl,A_MINUTE
		call	PrintField
		ld	a,':'
		ld	hl,A_SECOND
		call	PrintField
		ld	de,MSG_CRLF
		jp	PrintString

Invalid:	ld	de,MSG_INVALID
		jp	PrintString

Usage:		ld	de,MSG_USAGE
		jp	PrintString

; ------------------------------------------------------------------------------
; Parameters
; ------------------------------------------------------------------------------
; Skip the spaces at HL. Output: A = next character, Z if end of the parameters
SkipSpaces:	ld	a,(hl)
		cp	' '
		jr	nz,IsEnd
		inc	hl
		jr	SkipSpaces

; Z if A is the end of a parameter (0, space, CR)
IsEnd:		or	a
		ret	z
		cp	' '
		ret	z
		cp	13
		ret

; Hex digit A (0-9, A-F, a-f) -> A = 0 to 15, carry set if not a hex digit
HexDigit:	sub	'0'
		ret	c
		cp	10
		ccf
		ret	nc
		and	0DFh			; lower case letters ('a' - '0' = 31H)
		sub	'A'-'0'
		ret	c
		cp	6
		ccf
		ret	c
		add	a,10
		ret

; I/O port of the JIO cartridge: probe of the ports of the IOSEL switches (00H, 20H, 30H)
; (detection routine of herraa1, as the JIO ROM). Output: A = port, 0FFh = not found
DetectPort:	ld	hl,CART_PORTS
DP_Probe:	ld	a,(hl)
		cp	0FFh
		ret	z
		ld	c,a
		ld	a,2Fh
		out	(c),a
		in	a,(c)
		and	0FCh
		cp	0CCh
		jr	nz,DP_Next
		ld	a,0DBh
		out	(c),a
		in	a,(c)
		and	0FCh
		cp	88h
		jr	nz,DP_Next
		ld	a,0F7h
		out	(c),a
		in	a,(c)
		and	0FCh
		cp	44h
		ld	a,c
		ret	z
DP_Next:	inc	hl
		jr	DP_Probe

CART_PORTS:	defb	00h,20h,30h,0FFh

; Print A in hex (2 digits)
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
		jr	c,PD_Out
		add	a,'A'-'9'-1
PD_Out:		jp	PrintChar

; ------------------------------------------------------------------------------
; Restore the CPU mode of the turbo R
; ------------------------------------------------------------------------------
RestoreCPU:	ld	a,(IS_TURBO)
		or	a
		ret	z
		ld	a,(CPU_MODE)
		ld	ix,CHGCPU

; Call the main ROM routine at IX
CallBIOS:	ld	iy,(EXPTBL-1)
		jp	CALSLT

; ------------------------------------------------------------------------------
; Print character A, then the byte at HL with 2 digits
; ------------------------------------------------------------------------------
PrintField:	push	hl
		call	PrintChar
		pop	hl
		ld	l,(hl)
		ld	h,0
		ld	ix,POW2

; Print HL with leading zeros, IX = powers of ten (words, last one is 1)
PrintNumber:	ld	e,(ix+0)
		ld	d,(ix+1)
		ld	a,'0'
PN_Sub:		and	a			; carry reset, A kept
		sbc	hl,de
		jr	c,PN_Digit
		inc	a
		jr	PN_Sub
PN_Digit:	add	hl,de
		push	hl
		push	ix
		call	PrintChar
		pop	ix
		pop	hl
		ld	a,(ix+0)
		dec	a
		or	(ix+1)			; power 1: last digit
		inc	ix
		inc	ix
		jr	nz,PrintNumber
		ret

PrintChar:	ld	e,a
		ld	c,02h			; _CONOUT
		jp	BDOS

PrintString:	ld	c,09h			; _STROUT
		jp	BDOS

; ------------------------------------------------------------------------------
; Serial routines (HL = data, DE = size; bJIOReceive returns A = 1 received, 0 time-out)
; ------------------------------------------------------------------------------
_JioPort:	defb	0FFh		; 0FFh = joystick port 2, 0FEh = joystick port 1, else I/O port of the JIO cartridge
				; (transmit.asm, receive.asm)
INCLUDE "../JIO_NFS/transmit.asm"
INCLUDE "../JIO_NFS/receive.asm"

; ------------------------------------------------------------------------------
COMMAND:	defb	"JIO",0,COMMAND_DATE_TIME
COMMAND_END:

ANSWER:
A_YEAR:		defw	0
A_MONTH:	defb	0
A_DAY:		defb	0
A_HOUR:		defb	0
A_MINUTE:	defb	0
A_SECOND:	defb	0
ANSWER_END:

IS_TURBO:	defb	0
CPU_MODE:	defb	0

POW4:		defw	1000,100
POW2:		defw	10,1

MSG_TITLE:	defb	"JIOTIME: JIO server clock",13,10,"$"
MSG_SET:	defb	"Date and time set:",13,10,"$"
MSG_NO_ANSWER:	defb	"No answer from the JIO server",13,10,"$"
MSG_INVALID:	defb	"Invalid date or time from the JIO server",13,10,"$"
MSG_CRLF:	defb	13,10,"$"
MSG_CART:	defb	"JIO cartridge, port $"
MSG_PORT_END:	defb	"H",13,10,"$"
MSG_NO_CART:	defb	"JIO cartridge not found (IOSEL switches: 00, 20 or 30)",13,10,"$"
MSG_JOY1:	defb	"Joystick port 1",13,10,"$"
MSG_USAGE:	defb	"Usage: JIOTIME [J1|J2|C[<port>]]",13,10
		defb	"  J1, J2     joystick port 1 or 2 (default: 2)",13,10
		defb	"  C          JIO cartridge instead of joystick port 2 (port detected)",13,10
		defb	"  C<port>    JIO cartridge at this I/O port (hex: 00, 20 or 30)",13,10,"$"
