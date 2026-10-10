; Segment written to port FDH by a program (without PUT_P1) kept across the interrupts (H_TIMI of the disk ROM)
; Page 1 set to segment 3 (page 0 of the TPA: 4000H shows 0000H), 100 interrupts, then 4000H compared to 0000H
BDOS	EQU	5

	ORG	100H

	XOR	A
	LD	(4000H),A	; page 1 of the TPA (segment 2): not the byte at 0000H (JP)
	DI
	LD	A,3
	OUT	(0FDH),A	; page 1: segment 3
	EI
	LD	B,100
WAIT:	HALT
	DJNZ	WAIT
	LD	A,(4000H)
	LD	HL,0000H
	CP	(HL)
	LD	A,2
	OUT	(0FDH),A	; page 1: segment 2 (TPA)
	LD	DE,MSG_OK
	JR	Z,PRINT
	LD	DE,MSG_FAIL
PRINT:	LD	C,9
	JP	BDOS

MSG_OK:		DEFB	"FDTEST: OK",13,10,"$"
MSG_FAIL:	DEFB	"FDTEST: FAIL",13,10,"$"
