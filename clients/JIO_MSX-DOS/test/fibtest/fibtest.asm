; FIB test: open a file with the FIB of _FFIRST after _FNEXT returned "no more files"
; (as COMMAND2 2.31 does with AUTOEXEC.BAT)
BDOS	EQU	5
	ORG	100H
	LD	DE,NAME
	LD	B,0
	LD	IX,FIB
	LD	C,40H		; _FFIRST
	CALL	BDOS
	CALL	HEX
	LD	IX,FIB
	LD	C,41H		; _FNEXT: no more files
	CALL	BDOS
	CALL	HEX
	LD	DE,FIB
	XOR	A
	LD	C,43H		; _OPEN with the FIB
	CALL	BDOS
	PUSH	BC
	CALL	HEX
	POP	BC
	LD	DE,BUF
	LD	HL,5
	LD	C,48H		; _READ
	CALL	BDOS
	CALL	HEX
	LD	DE,MSG
	LD	C,9
	CALL	BDOS
	LD	DE,BUF
	LD	C,9
	CALL	BDOS

; _FNEW of a file on an existing directory: error .DIRX, the FIB describes the directory
	LD	DE,DNAME
	LD	B,10H
	LD	IX,FIB
	LD	C,42H		; _FNEW: create the directory
	CALL	BDOS
	CALL	HEX
	LD	DE,DNAME
	LD	B,0
	LD	IX,FIB
	LD	C,42H		; _FNEW: file with the name of the directory
	CALL	BDOS
	CALL	HEX
	LD	A,(FIB)		; 0FFH: FIB of the existing directory
	CALL	HEX
	LD	DE,FIB
	LD	C,4DH		; _DELETE with the FIB
	CALL	BDOS
	CALL	HEX
	LD	DE,MSG2
	LD	C,9
	JP	BDOS
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
	JR	C,N1
	ADD	A,7
N1:	LD	E,A
	LD	C,2
	JP	BDOS
MSG:	DEFM	"FIBTEST: $"
MSG2:	DEFM	"FNEW DIRX",13,10,"$"
DNAME:	DEFM	"FIBDIR",0
NAME:	DEFM	"HELLO.TXT",0
BUF:	DEFS	5,0
	DEFM	13,10,"$"
FIB:	DEFS	64,0
