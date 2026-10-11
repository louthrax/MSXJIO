; FCB random block read (_RDBLK) at the end of a file: BLKTEST.DAT (300 bytes) read with a record size of 1,
; 1000 records asked, then 10 more at the end of the file. Shows A and HL of each read (expected: 01 012C, then 01 0000)
BDOS	EQU	5

	ORG	100H

	LD	DE,FCB
	LD	C,0FH		; open
	CALL	BDOS
	LD	HL,1
	LD	(FCB+14),HL	; record size 1
	LD	HL,0
	LD	(FCB+33),HL	; random record 0
	LD	(FCB+35),HL
	LD	DE,BUF
	LD	C,1AH		; set DTA
	CALL	BDOS
	LD	DE,FCB
	LD	HL,1000
	LD	C,27H		; random block read
	CALL	BDOS
	CALL	SHOW
	LD	DE,FCB
	LD	HL,10
	LD	C,27H		; at the end of the file
	CALL	BDOS
	CALL	SHOW
	LD	DE,FCB
	LD	C,10H		; close
	JP	BDOS

SHOW:	PUSH	HL
	PUSH	AF
	LD	DE,MSG
	LD	C,9
	CALL	BDOS
	POP	AF
	CALL	HEX
	LD	E,' '
	LD	C,2
	CALL	BDOS
	POP	HL
	PUSH	HL
	LD	A,H
	CALL	HEX
	POP	HL
	LD	A,L
	CALL	HEX
	LD	DE,CRLF
	LD	C,9
	JP	BDOS

HEX:	PUSH	AF
	RRCA
	RRCA
	RRCA
	RRCA
	CALL	DIGIT
	POP	AF
DIGIT:	AND	0FH
	ADD	A,'0'
	CP	'9'+1
	JR	C,DIG1
	ADD	A,7
DIG1:	LD	E,A
	LD	C,2
	JP	BDOS

MSG:	DEFB	"RDBLK: $"
CRLF:	DEFB	13,10,"$"
FCB:	DEFB	0,"BLKTEST DAT"
	DEFS	25,0
BUF:
