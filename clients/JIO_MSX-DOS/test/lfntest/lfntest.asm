; Long host names (BDOS function 0E0H JIO_GET_LONG_NAME, JIO extension), FIB and buffer in page 2:
; - long name of each entry of the current directory (_FFIRST / _FNEXT, sub-function 1)
; - long whole path of the last entry found (sub-function 2)
; - long current directory of the current drive (sub-function 3)
; - buffer too small (sub-function 1, 4 bytes): error .PLONG (D8)
; Each result: "<error> <name>" on its own line, "LFNEND" at the end.
BDOS	EQU	5
PATH	EQU	9000H
FIB	EQU	9100H
BUF	EQU	9200H
	ORG	100H
	LD	HL,MASK			; path in page 2
	LD	DE,PATH
	LD	BC,4
	LDIR
	LD	DE,PATH
	LD	B,16			; directories too
	LD	IX,FIB
	LD	C,40H			; _FFIRST
	CALL	BDOS
LOOP:	OR	A
	JR	NZ,DONE
	LD	A,1			; long name of the FIB
	LD	DE,FIB
	CALL	LONG
	LD	IX,FIB
	LD	C,41H			; _FNEXT
	CALL	BDOS
	JR	LOOP

DONE:	LD	A,2			; long whole path of the last entry found
	CALL	LONG
	LD	A,3			; long current directory
	LD	DE,0			; E = drive: current
	CALL	LONG
	LD	A,1			; buffer too small
	LD	DE,FIB
	LD	HL,BUF
	LD	B,4
	LD	C,0E0H
	CALL	BDOS
	CALL	HEX
	LD	DE,MSGEND
	LD	C,9
	JP	BDOS

; A = sub-function, DE = FIB or E = drive: result printed (buffer of 256 bytes)
LONG:	LD	HL,BUF
	LD	B,0
	LD	C,0E0H			; JIO_GET_LONG_NAME
	PUSH	AF
	XOR	A
	LD	(BUF),A
	POP	AF
	CALL	BDOS
	CALL	HEX
	LD	HL,BUF
	CALL	PRSTR0
	LD	DE,CRLF
	LD	C,9
	JP	BDOS

PRSTR0:	LD	A,(HL)
	OR	A
	RET	Z
	PUSH	HL
	LD	E,A
	LD	C,2
	CALL	BDOS
	POP	HL
	INC	HL
	JR	PRSTR0
HEX:	PUSH	AF
	RRCA
	RRCA
	RRCA
	RRCA
	CALL	NIB
	POP	AF
	CALL	NIB
	LD	E,' '
	LD	C,2
	JP	BDOS
NIB:	AND	0FH
	ADD	A,'0'
	CP	'9'+1
	JR	C,NIB1
	ADD	A,7
NIB1:	LD	E,A
	LD	C,2
	JP	BDOS
MASK:	DEFB	"*.*",0
CRLF:	DEFB	13,10,"$"
MSGEND:	DEFB	13,10,"LFNEND",13,10,"$"
