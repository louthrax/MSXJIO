; ------------------------------------------------------------------------------
; p0_kernel.asm
; DOS 2.20 / DOS 2.31 kernel page 0: BDOS functions
; Based on ASCII DOS 2.20 / DOS 2.31 codebase s2
;
; Code copyrighted by ASCII and others
; Source origin is the msxsyssrc repository by Arjen Zeilemaker
; Restructure, modifications and additional comments by H.J. Berends
; 
; Sourcecode supplied for STUDY ONLY
; Recreation NOT permitted without authorisation of the copyrightholders
; ------------------------------------------------------------------------------
; Modifications:
; 01. The rom bank switching code is removed
; 02. Added FAT16 option, partly based on FAT16 v0.12 by OKEI
; 03. Optimized FAT16 code to free space for format and ramdisk routines
; 04. Use Extended BPB serial on FAT16 partitions (todo: FAT16_EBS)
; 05. Use Microsoft standards to determine if partition is FAT16 or FAT12 (DPBSET)
; 06. Changed the free disk calculation routine for FAT16 partitions
; 07. Optmized code / removed unused code (OPTM)
; 08. Added DOSV231 option
; 09. Undelete flag for DOS1 / FAT16 partitions (DIRTYBIT)
; 10. JIO: FAT, sector, disk buffer, FCB, format and ramdisk code removed.
;     All file functions are served by the JIO server, see rfs.asm.


		INCLUDE "disk.inc"		; Assembler directives
		INCLUDE	"../../common/msx.inc"		; MSX constants and definitions

		SECTION	P0_KERNEL

		ORG	08000H

		PUBLIC	K1_BEGIN		; begin of kernel code
		PUBLIC	K1_END			; end of kernel code

; ------------------------------------------------------------------------------
; Following kernel code is copied to ram in page 0
; ------------------------------------------------------------------------------
K1_BEGIN:
		PHASE  0

; ---------------------------------------------------------
; *** Header ($0000 - $0094) ***
; ---------------------------------------------------------

		JP	K_INIT			; init kernel / BDOS
		DEFS	2,0

C0005:		JP	K_BDOS			; Kernel BDOS handler
		DEFS	4,0
C000C:		JP	SRDSLT			; RDSLT
		DEFS	5,0
C0014:		JP	SWRSLT			; WRSLT
		DEFS	5,0
C001C:		JP	SCALSLT			; CALSLT
		DEFS	5,0
C0024:		JP	SENASLT			; ENASLT
		DEFS	1,0
		JP	CC206			; debugger
		DEFS	5,0
C0030:		JP	SCALLF			; CALLF
		DEFS	5,0
C0038: 		JP	SIRQ			; KEYINT
C003B:		OUT	(0A8H),A		; SSLOT
		LD	A,(DFFFF)
		CPL
		LD	L,A
		AND	H
		OR	D
		JR	J004E
C0046:		OUT	(0A8H),A		; SSLOTL
		LD	A,L
		JR	J004E
C004B:		OUT	(0A8H),A		; SSLOTE
		LD	A,E
J004E:		LD	(DFFFF),A
		LD	A,B
		OUT	(0A8H),A
		RET
		DEFS	7,0
C005C:		JP	K_ALLSEG		; ALL_SEG
C005F:		JP	K_FRESEG		; FRE_SEG

		DEFS	$1E,0
C0080:		JP	KB_CHARIN		; CON input
C0083:		JP	KB_CHAROUTC		; CON output
C0086:		JP	KB_CHARSTAT		; CON check input status
C0089:		JP	KB_LPTOUT		; LPT output
C008C:		JP	KB_LPTSTAT		; LPT check output status
C008F:		JP	KB_AUXOUTC		; AUX output
C0092:		JP	KB_AUXIN		; AUX input

; ---------------------------------------------------------
; *** Initialize kernel / BDOS ***
; ---------------------------------------------------------
K_INIT:		LD	IY,D_BB80		; Base address for IY relative kernel variables
		LD	HL,I012C		; device table
J00CB:		LD	A,(HL)
		OR	A
		JR	Z,J0101
		INC	HL
		PUSH	HL
		LD	HL,43
		CALL	K_ALLOC_P2
		POP	DE
J00D8:		JR	NZ,J012A
		LD	BC,(D_BBF4)
		LD	(D_BBF4),HL		; Update start of device chain
		LD	(HL),C
		INC	HL
		LD	(HL),B
		INC	HL
		EX	DE,HL
		LDI
		LDI
		EX	DE,HL
		LD	BC,6
		ADD	HL,BC
		EX	DE,HL
		LD	BC,12
		LDIR
		LD	A,80H
		LD	(DE),A
		LD	B,14H
		XOR	A
J00FB:		INC	DE
		LD	(DE),A
		DJNZ	J00FB
		JR	J00CB

J0101:		LD	B,00H
		CALL	F_JOIN
		CALL	RFS_INIT		; remote file system
		CALL	K_CON_INIT
	IF OPTM = 0
		; copy cursor on/off escape codes to data segment
		; used in K_FLUSHBUF
		LD	HL,I0178
		LD	DE,I_B066
		LD	BC,6
		LDIR
	ENDIF
		LD	A,1
		LD	(ST_COU),A
		LD	(IY+16),0FFH
		CALL	C10CA			; initialize clockchip
		OR	A
		RET

J012A:		SCF
		RET

I012C:		DEFB	0FFH
		DEFW	I0932
		DEFB	0A3H			; device, ascii mode, console input device, console output device
	        DEFB    "CON        "

		DEFB	0FFH
		DEFW	I09E2
		DEFB	0A0H			; device, ascii mode
		DEFB	"LST        "

		DEFB	0FFH
		DEFW	I09E2
		DEFB	0A0H			; device, ascii mode
		DEFB	"PRN        "

		DEFB	0FFH
		DEFW	I0A03
		DEFB	0A0H			; device, ascii mode
		DEFB	"NUL        "

		DEFB	0FFH
		DEFW	I09BF
		DEFB	0A0H			; device, ascii mode
		DEFB	"AUX        "

		DEFB	0

	IF OPTM = 0
		; cursor on/of escape codes (used in K_FLUSHBUF)
I0178:		DEFB	27,"y5"
		DEFB	27,"x5"
	ENDIF
; ---------------------------------------------------------
; Subroutine allocate BDOS data block
; Input:  HL = size
; ---------------------------------------------------------
K_ALLOC_P2:	PUSH	DE
		PUSH	BC
		INC	HL
		RES	0,L
		LD	B,H
		LD	C,L
		LD	HL,(D_B064)
J01D5:		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		INC	HL
		LD	A,D
		OR	E
		JR	Z,J0203
		BIT	0,E
		JR	NZ,J01E8
		EX	DE,HL
		SBC	HL,BC
		JR	NC,J01ED
		ADD	HL,BC
		EX	DE,HL
J01E8:		RES	0,E
		ADD	HL,DE
		JR	J01D5

J01ED:		EX	DE,HL
		DEC	HL
		DEC	HL
		JR	Z,J0217
		DEC	DE
		DEC	DE
		LD	A,D
		OR	E
		JR	Z,J01FF
		LD	(HL),E
		INC	HL
		LD	(HL),D
		INC	HL
		ADD	HL,DE
		JR	J0217

J01FF:		INC	BC
		INC	BC
		JR	J0217

J0203:		LD	A,_NORAM
		INC	BC
		INC	BC
		LD	HL,(D_B064)
		OR	A
		SBC	HL,BC
		JR	C,J0227
		JP	P,J0227
		LD	(D_B064),HL
		DEC	BC
		DEC	BC
J0217:		LD	(HL),C
		SET	0,(HL)
		INC	HL
		LD	(HL),B
		INC	HL
		PUSH	HL
J021E:		LD	(HL),00H
		INC	HL
		DEC	BC
		LD	A,B
		OR	C
		JR	NZ,J021E
		POP	HL
J0227:		POP	BC
		POP	DE
		OR	A
		RET

; ---------------------------------------------------------
; Subroutine free BDOS data block
; Input:  HL = address of block
; ---------------------------------------------------------
K_FREE_P2:	DEC	HL
		DEC	HL
		RES	0,(HL)
		PUSH	DE
		PUSH	BC
		LD	HL,(D_B064)
J0234:		LD	C,(HL)
		BIT	0,C
		JR	NZ,J023F
		INC	HL
		LD	B,(HL)
		INC	HL
		ADD	HL,BC
		JR	J0234

J023F:		LD	(D_B064),HL
J0242:		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		INC	HL
		LD	A,D
		OR	E
		JR	Z,J026A
		BIT	0,E
		JR	NZ,J0265
J024E:		PUSH	HL
		ADD	HL,DE
		LD	C,(HL)
		INC	HL
		LD	B,(HL)
		POP	HL
		BIT	0,C
		JR	NZ,J025F
		INC	BC
		INC	BC
		EX	DE,HL
		ADD	HL,BC
		EX	DE,HL
		JR	J024E

J025F:		DEC	HL
		LD	(HL),D
		DEC	HL
		LD	(HL),E
		INC	HL
		INC	HL
J0265:		RES	0,E
		ADD	HL,DE
		JR	J0242

J026A:		POP	BC
		POP	DE
		RET

; ---------------------------------------------------------
; *** Kernel BDOS handler ***
; ---------------------------------------------------------
K_BDOS:		EI
		CALL	H_BDOS
		CALL	C0278
		LD	(DSBBFD),A
		RET

; Subroutine BDOS handler (basic)
C0278:		PUSH	HL
		PUSH	BC
		EX	AF,AF'
		LD	A,(CH_COU)
		DEC	A
		CALL	Z,K_CHARFLUSH
		LD	A,C
		CP	71H
		JR	C,J0289
		LD	C,9
J0289:		EX	AF,AF'
		LD	B,00H
		LD	HL,I02A2
		ADD	HL,BC
		ADD	HL,BC
		LD	C,(HL)
		INC	HL
		LD	H,(HL)
		LD	L,C
		LD	IY,D_BB80
J0299:		POP	BC
		EX	(SP),HL
		RET

; ---------------------------------------------------------
; Subroutine to handle invalid BDOS function calls
; ---------------------------------------------------------
K_INVALID:	LD	A,_IBDOS
J029E:		LD	HL,0
		RET

; ---------------------------------------------------------
; MSX-DOS 2 Functions 
; Note: Function $09 _STROUT is implemented in PRTBUF (F1C9),
;	 msxdos2.sys handles the call to this routine.
; ---------------------------------------------------------
; Functions starting with R_ are served by the JIO server (rfs.asm)
I02A2:		DEFW	F_TERM0,F_CONIN,F_CONOUT,F_AUXIN	; 0
		DEFW	F_AUXOUT,F_LSTOUT,F_DIRIO,F_DIRIN	; 4
		DEFW	F_INNOE,K_INVALID,F_BUFIN,F_CONST	; 8
		DEFW	F_CPMVER,R_DSKRST,R_SELDSK,R_FOPEN	; 0C
		DEFW	R_FCLOSE,R_SFIRST,R_SNEXT,R_FDEL	; 10
		DEFW	R_RDSEQ,R_WRSEQ,R_FMAKE,R_FREN		; 14
		DEFW	R_LOGIN,F_CURDRV,F_SETDTA,R_ALLOC	; 18
		DEFW	K_INVALID,K_INVALID,K_INVALID,K_INVALID	; 1C
		DEFW	K_INVALID,R_RDRND,R_WRRND,R_FSIZE	; 20
		DEFW	R_SETRND,K_INVALID,R_WRBLK,R_RDBLK	; 24
		DEFW	R_WRZER,K_INVALID,F_GDATE,F_SDATE	; 28
		DEFW	F_GTIME,F_STIME,F_VERIFY,K_INVALID	; 2C
		DEFW	K_INVALID,K_INVALID,K_INVALID,K_INVALID	; 30
		DEFW	K_INVALID,K_INVALID,K_INVALID,K_INVALID	; 34
		DEFW	K_INVALID,K_INVALID,K_INVALID,K_INVALID	; 38
		DEFW	K_INVALID,K_INVALID,K_INVALID,K_INVALID	; 3C
		DEFW	R_FFIRST,R_FNEXT,R_FNEW,R_OPEN		; 40
		DEFW	R_CREATE,R_CLOSE,R_ENSURE,F_DUP		; 44
		DEFW	F_READ,F_WRITE,R_SEEK,F_IOCTL		; 48
		DEFW	R_HTEST,R_DELETE,R_RENAME,R_MOVE	; 4C
		DEFW	R_ATTR,R_FTIME,R_HDELETE,R_HRENAME	; 50
		DEFW	R_HMOVE,R_HATTR,R_HFTIME,F_GETDTA	; 54
		DEFW	F_GETVFY,R_GETCD,R_CHDIR,F_PARSE	; 58
		DEFW	F_PFILE,F_CHKCHR,R_WPATH,R_FLUSH	; 5C
		DEFW	F_FORK,F_JOIN,F_TERM,K_INVALID		; 60
		DEFW	K_INVALID,F_ERROR,F_EXPLAIN,K_INVALID	; 64
		DEFW	R_RAMD,R_BUFFER,R_ASSIGN,F_GENV		; 68
		DEFW	F_SENV,F_FENV,R_DSKCHK,F_DOSVER		; 6C
		DEFW	F_REDIR					; 70

; ---------------------------------------------------------
; *** Functions: 01-08,0A,0B ***
; *** Console I/O routines	***
; ---------------------------------------------------------

; ---------------------------------------------------------
; Subroutine initialize buffered input history buffer
; ---------------------------------------------------------
K_CON_INIT:	LD	HL,I_B0D0
		LD	(D_BB82),HL
		LD	(D_BB80),HL
		LD	DE,I_B1D0
		EX	DE,HL
		OR	A
		SBC	HL,DE
		EX	DE,HL
J0395:		LD	(HL),0DH
		INC	HL
		DEC	DE
		LD	A,D
		OR	E
		JR	NZ,J0395
		LD	(D_BB7F),A

; ---------------------------------------------------------
; Subroutine clear stored input, console output not duplicated to printer
; ---------------------------------------------------------
K_CON_CLEAR:	XOR	A
		LD	(D_BB8D),A
		LD	(D_BB8A),A
		RET

; ---------------------------------------------------------
; Function $01 _CONIN
; ---------------------------------------------------------
F_CONIN:	CALL	F_INNOE
		PUSH	HL
		LD	A,L
		CALL	C085A
		CALL	NC,C0871
		POP	HL
		XOR	A
		RET

; ---------------------------------------------------------
; Function $02 _CONOUT
; ---------------------------------------------------------
F_CONOUT:	LD	A,E
		CALL	C086C
		XOR	A
		LD	H,A
		LD	L,A
		RET

; ---------------------------------------------------------
; Function $08 _INNOE
; ---------------------------------------------------------
F_INNOE:	BIT	0,(IY+9)
		LD	C,0FFH
		JR	NZ,K_HCONIN
		CALL	C08B2
		LD	L,A
		XOR	A
		LD	H,A
		RET

; ---------------------------------------------------------
; Function $0B _CONST
; ---------------------------------------------------------
F_CONST:	CALL	C0897
		LD	L,A
		XOR	A
		LD	H,A
		RET

; ---------------------------------------------------------
; Function $06 _DIRIO
; ---------------------------------------------------------
F_DIRIO:	LD	A,E
		INC	A
		JR	Z,J03E8
		BIT	1,(IY+9)
		LD	A,E
		LD	C,00H
		JR	NZ,K_HCONOUT
		CALL	KB_CHAROUT
		XOR	A
		LD	H,A
		LD	L,A
		RET

J03E8:		BIT	0,(IY+9)
		LD	C,00H
		JR	NZ,K_HCONIN
		LD	HL,D_BB8D
		CP	(HL)
		JR	NZ,J0406
		CALL	KB_CHARSTAT
		JR	NZ,J0406
		LD	L,A
		LD	H,A
		RET

; ---------------------------------------------------------
; Function $07 _DIRIN
; ---------------------------------------------------------
F_DIRIN:	BIT	0,(IY+9)
		LD	C,00H
		JR	NZ,K_HCONIN
J0406:		LD	A,(D_BB8D)
		OR	A
		CALL	Z,KB_CHARIN
		LD	L,A
		XOR	A
		LD	H,A
		LD	(D_BB8D),A
		RET

; ---------------------------------------------------------
; Subroutine character from console input file handle
; ---------------------------------------------------------
K_HCONIN:	LD	B,00H
		PUSH	BC
		CALL	C1D47
		POP	DE
		OR	A
		JR	NZ,J0439
		OR	E
		JR	Z,J0426
		LD	A,B
		SUB	03H
		JR	Z,J0439
J0426:		LD	L,B
		XOR	A
		LD	H,A
		RET

; ---------------------------------------------------------
; Subroutine character to console output file handle
; ---------------------------------------------------------
K_HCONOUT:	LD	B,1
		JR	J0446

; ---------------------------------------------------------
; Function $03 _AUXIN
; ---------------------------------------------------------
F_AUXIN:	LD	B,3
		LD	C,0FFH
		CALL	C1D47
		OR	A
		LD	L,B
		LD	H,A
		RET	Z
J0439:		LD	C,_INERR
		JR	J044F

; ---------------------------------------------------------
; Subroutine $04 _AUXOUT
; ---------------------------------------------------------
F_AUXOUT:	LD	B,3
		JR	J0443

; ---------------------------------------------------------
; Function $05 _LSTOUT
; ---------------------------------------------------------
F_LSTOUT:	LD	B,4
J0443:		LD	C,0FFH
		LD	A,E
J0446:		CALL	C1D22
		OR	A
		LD	L,A
		LD	H,A
		RET	Z
		LD	C,_OUTERR
J044F:		LD	B,A
		LD	A,C
		CALL	C3723
J0454:		JR	J0454

; ---------------------------------------------------------
; Function $0A _BUFIN
; ---------------------------------------------------------
F_BUFIN:	PUSH	DE
		BIT	0,(IY+9)
		JR	NZ,K_HBUFIN
		XOR	A
		CALL	K_CON_BUFIN
		JR	J049E
	
; ---------------------------------------------------------
; console line buffered input (file handle)
; ---------------------------------------------------------
K_HBUFIN:	EX	DE,HL
		LD	B,(HL)
		LD	C,00H
		INC	HL
		PUSH	HL
J0469:		PUSH	HL
		PUSH	BC
		LD	C,0FFH
		CALL	K_HCONIN
		LD	A,L
		POP	BC
		POP	HL
		OR	A
		JR	Z,J0469
		CP	0AH
		JR	Z,J0469
		CP	0DH
		JR	Z,J0499
		LD	E,A
		LD	A,B
		CP	C
		JR	Z,J048E
		INC	C
		INC	HL
		LD	(HL),E
		LD	A,E
		PUSH	HL
		PUSH	BC
		CALL	C086C
		JR	J0495

J048E:		PUSH	HL
		PUSH	BC
		LD	A,7
		CALL	KB_CHAROUT
J0495:		POP	BC
		POP	HL
		JR	J0469

J0499:		POP	HL
		LD	(HL),C
		CALL	C086C
J049E:		POP	HL
		PUSH	HL
		LD	A,(HL)
		INC	HL
		CP	(HL)
		JR	Z,J04AC
		LD	E,(HL)
		LD	D,0
		ADD	HL,DE
		INC	HL
		LD	(HL),0DH
J04AC:		POP	DE
		XOR	A
		LD	L,A
		LD	H,A
		RET

; ---------------------------------------------------------
; Subroutine console line buffered input (keyboard)
; Input:  A = force console output to screen flag
; ---------------------------------------------------------
K_CON_BUFIN:	LD	(D_BB7A),A
		INC	DE
		XOR	A
		LD	(DE),A
		DEC	DE
		LD	(D_BB7C),A

; restart console line input
J04BB:		PUSH	DE
		CALL	K_EDIT_LINE
		POP	DE
		DEC	A
		JR	Z,J050C
		DEC	A
		JR	Z,J052A
		INC	DE
		LD	A,(DE)
		OR	A
		RET	Z
		LD	B,A
		LD	(D_BB7F),A
		LD	A,(D_BB7C)
		OR	A
		JR	Z,J04EA
		PUSH	DE
		PUSH	BC
		LD	HL,(D_BB82)
J04D9:		INC	DE
		LD	A,(DE)
		CP	(HL)
		JR	NZ,J04E6
		CALL	K_CBUF_INC
		DJNZ	J04D9
		LD	A,(HL)
		CP	0DH
J04E6:		POP	BC
		POP	DE
		JR	Z,J04FE
J04EA:		LD	HL,(D_BB80)
J04ED:		INC	DE
		LD	A,(DE)
		LD	(HL),A
		CALL	K_CBUF_INC
		DJNZ	J04ED
		LD	A,(HL)
		LD	(HL),0DH
		CALL	K_CBUF_INC
		LD	(D_BB80),HL
J04FE:		LD	(D_BB82),HL
J0501:		CP	0DH
		RET	Z
		LD	A,(HL)
		LD	(HL),0DH
		CALL	K_CBUF_INC
		JR	J0501

; previous line
J050C:		LD	A,(D_BB7F)
		OR	A
		JR	Z,J04BB
		LD	HL,(D_BB82)
J0515:		CALL	K_CBUF_DEC
		LD	A,(HL)
		CP	0DH
		JR	Z,J0515
J051D:		CALL	K_CBUF_DEC
		LD	A,(HL)
J0521:		CP	0DH
		JR	NZ,J051D
		CALL	K_CBUF_INC
		JR	J0544

; next line
J052A:		LD	A,(D_BB7F)
		OR	A
		JR	Z,J04BB
		LD	HL,(D_BB82)
J0533:		LD	A,(HL)
		CP	0DH
		CALL	K_CBUF_INC
		JR	NZ,J0533
		SCF
J053C:		CALL	NC,K_CBUF_INC
		LD	A,(HL)
		CP	0DH
		JR	Z,J053C

; update current line
J0544:		LD	(D_BB82),HL
		PUSH	DE
		LD	A,(DE)
		LD	B,A
		INC	DE
		INC	DE
		LD	C,0FFH
J054E:		LD	A,(HL)
		LD	(DE),A
		INC	C
		CALL	K_CBUF_INC
		CP	0DH
		INC	DE
		JR	Z,J055C
		DJNZ	J054E
		INC	C
J055C:		POP	DE
		INC	DE
		LD	A,C
		LD	(DE),A
		DEC	DE
		LD	(D_BB7C),A
		JP	J04BB

; ---------------------------------------------------------
; Subroutine next position in history buffer
; Input:  HL = pointer in history buffer
; Output: HL = updated pointer in history buffer
; ---------------------------------------------------------
K_CBUF_INC:	PUSH	AF
		PUSH	DE
		LD	DE,I_B1CF
		OR	A
		SBC	HL,DE
		ADD	HL,DE
		INC	HL
		JR	NZ,J0576
		LD	HL,I_B0D0
J0576:		POP	DE
		POP	AF
		RET

; ---------------------------------------------------------
; Subroutine previous position in history buffer
; Input:  HL = pointer in history buffer
; Output: HL = updated pointer in history buffer
; ---------------------------------------------------------
K_CBUF_DEC:	PUSH	AF
		PUSH	DE
		LD	DE,I_B0D0
		OR	A
		SBC	HL,DE
		ADD	HL,DE
		DEC	HL
		JR	NZ,J0588
		LD	HL,I_B1CF
J0588:		POP	DE
		POP	AF
		RET

; ---------------------------------------------------------
; Subroutine edit line
; Input:  DE = pointer to buffer
; ---------------------------------------------------------
K_EDIT_LINE:	LD	HL,(D_BB8B)
		LD	(D_BB87),HL
		LD	(D_BB7D),HL
		EX	DE,HL
J0595:		LD	C,(HL)
		INC	HL
		LD	(D_BB84),HL
		LD	A,(HL)
		OR	A
		LD	B,A
		JR	Z,J05A5
		INC	HL
		CALL	C0811
		DEC	HL
		LD	A,B
J05A5:		LD	(D_BB86),A
		XOR	A
		CALL	C06BC
I05AC:		LD	DE,I05AC
		PUSH	DE
		PUSH	HL
		LD	HL,D_BB86
		LD	A,(HL)
		CP	B
		JR	NC,J05B9
		LD	(HL),B
J05B9:		POP	HL
		CALL	C08B2
	IF OPTM = 0
		OR	A
		RET	Z
	ENDIF
		CP	0AH
		RET	Z
		CP	0DH
		JP	Z,J07A8
		CP	1DH
		JP	Z,J06F9
		CP	1CH
		JP	Z,J06DB
		CP	7FH
		JP	Z,J0748
		CP	08H
		JP	Z,J0741
		CP	12H
		JP	Z,J06B8
		CP	1BH
		JR	Z,J05EA
		CP	18H
		JR	Z,J05EA
		CP	15H
J05EA:		JP	Z,J07A1
		CP	1EH
		JP	Z,J07BF
		CP	1FH
		JP	Z,J07C6
		CP	0BH
		JP	Z,J06F2
		LD	E,A
		LD	A,(D_BB7B)
		OR	A
		JP	NZ,J0653
		LD	A,(D_BB86)
		CP	B
		JR	Z,J063F
		INC	HL
		LD	A,E
		CALL	C17D6
		JR	NC,J062E
		LD	A,(D_BB86)
		DEC	A
		CP	B
		JR	NZ,J0623
		INC	A
		CP	C
		DEC	HL
		JP	NC,J06AB
		INC	HL
		INC	A
		LD	(D_BB86),A
J0623:		LD	A,(HL)
		CALL	C17D6
		INC	HL
		CALL	NC,C07CD
		DEC	HL
		JR	J0678

J062E:		CALL	C07CD
		JR	C,J06A3
		LD	A,(HL)
		CP	20H		; " "
		JR	C,J06A3
		LD	A,E
		CP	20H		; " "
		JR	C,J06A3
		JR	J0649

J063F:		CP	C
		JR	NC,J06AE
		LD	A,E
		CALL	C17D6
		JR	C,J0659
		INC	HL
J0649:		LD	(HL),E
		LD	A,E
		INC	B
		CALL	C0836
		JP	C0821			; OPTM: call/ret=jp

J0653:		LD	A,E
		CALL	C17D6
		JR	NC,J0687
J0659:		LD	A,(D_BB86)
		INC	A
		CP	C
		JR	NC,J06AB
		INC	A
		LD	(D_BB86),A
		DEC	A
		DEC	A
		SUB	B
		JR	Z,J0677
		PUSH	DE
		PUSH	BC
		LD	C,A
		LD	B,0
		ADD	HL,BC
		LD	D,H
		LD	E,L
		INC	DE
		INC	DE
		LDDR
		POP	BC
		POP	DE
J0677:		INC	HL
J0678:		LD	(HL),E
		INC	HL
		CALL	C08B2
		LD	(HL),A
		DEC	HL
		CALL	C07DC
		INC	B
		INC	B
		JP	C06FD

J0687:		LD	A,(D_BB86)
		CP	C
		JR	NC,J06AE
		INC	A
		LD	(D_BB86),A
		DEC	A
		SUB	B
		JR	Z,J06A2
		PUSH	DE
		PUSH	BC
		LD	C,A
		LD	B,0
		ADD	HL,BC
		LD	D,H
		LD	E,L
		INC	DE
		LDDR
		POP	BC
		POP	DE
J06A2:		INC	HL
J06A3:		LD	(HL),E
		CALL	C07DC
		INC	B
		JP	C06FD

J06AB:		CALL	C08B2
J06AE:		LD	A,7
		PUSH	BC
		PUSH	HL
		CALL	KB_CHAROUT
		POP	HL
		POP	BC
		RET

; Subroutine INS key, flip insert mode and update cursor shape
J06B8:		LD	A,(D_BB7B)
		CPL

; Subroutine set insert mode and update cursor shape
C06BC:		LD	(D_BB7B),A
		OR	A
		LD	A,79H		; "y"
		JR	NZ,J06C5
		DEC	A
J06C5:		PUSH	BC
		PUSH	HL
		PUSH	DE
		PUSH	AF
		LD	A,1BH
		CALL	KB_CHAROUT
		POP	AF
		CALL	KB_CHAROUT
		LD	A,34H
		CALL	KB_CHAROUT
		POP	DE
		POP	HL
		POP	BC
		RET

J06DB:		LD	A,(D_BB86)
		CP	B
		RET	Z
		INC	HL
		INC	B
		LD	A,(HL)
		CALL	C17D6
		JP	NC,C0836
		CALL	C0836
		INC	HL
		INC	B
		LD	A,(HL)
		JP	C0836

J06F2:		LD	A,B
		OR	A
		RET	Z
		LD	B,00H
		JR	C06FD

J06F9:		LD	A,B
		OR	A
		RET	Z
		DEC	B

; Subroutine cursor to position
; Input:  B = position
C06FD:		LD	HL,(D_BB84)
		LD	DE,(D_BB87)
		PUSH	BC
		INC	B
		JR	J0729

J0708:		INC	HL
		LD	A,(HL)
		CALL	C17D6
		JR	NC,J0719
		INC	HL
		DJNZ	J0727
		DEC	HL
		DEC	HL
		POP	BC
		DEC	B
		PUSH	BC
		JR	J072B

J0719:		CP	09H
		JR	NZ,J0723
		LD	A,E
		OR	07H
		LD	E,A
		JR	J0728

J0723:		CP	20H
		JR	NC,J0728
J0727:		INC	DE
J0728:		INC	DE
J0729:		DJNZ	J0708
J072B:		PUSH	HL
		LD	HL,(D_BB8B)
		OR	A
		SBC	HL,DE
		JR	Z,J073E
J0734:		LD	A,8
		CALL	C0836
		DEC	HL
		LD	A,H
		OR	L
		JR	NZ,J0734
J073E:		POP	HL
		POP	BC
		RET

J0741:		LD	A,B
		OR	A
		RET	Z
		DEC	B
		CALL	C06FD
J0748:		LD	A,(D_BB86)
		CP	B
		RET	Z
		DEC	A
		LD	(D_BB86),A
		SUB	B
		JR	Z,J0784
		LD	E,A
		INC	HL
		LD	A,(HL)
		DEC	HL
		CALL	C17D6
		LD	A,E
		JR	NC,J0777
		PUSH	HL
		LD	HL,D_BB86
		DEC	(HL)
		POP	HL
		DEC	A
		JR	Z,J0784
		PUSH	BC
		PUSH	HL
		LD	C,A
		LD	B,0
		INC	HL
		LD	D,H
		LD	E,L
		INC	HL
		INC	HL
		LDIR
		POP	HL
		POP	BC
		JR	J0784

J0777:		PUSH	BC
		PUSH	HL
		LD	C,A
		LD	B,0
		INC	HL
		LD	D,H
		LD	E,L
		INC	HL
		LDIR
		POP	HL
		POP	BC
J0784:		INC	HL
		CALL	C07DC
		DEC	HL
		JP	C06FD

; Subroutine delete line on screen
C078C:		XOR	A
		CP	B
		LD	B,A
		CALL	NZ,C06FD
		CALL	C07D7
		LD	B,00H
		CALL	C06FD
		LD	HL,(D_BB84)
		LD	(HL),00H
		DEC	HL
		RET

J07A1:		CALL	C078C
		POP	DE
		JP	J0595

J07A8:		INC	HL
		CALL	C07DC
		LD	HL,(D_BB84)
		LD	A,(D_BB86)
		LD	(HL),A
		XOR	A
		CALL	C06BC
		LD	A,13
		CALL	C0836
		POP	HL
		XOR	A
		RET

J07BF:		POP	HL
		CALL	C078C
		LD	A,1
		RET

J07C6:		POP	HL
		CALL	C078C
		LD	A,2
		RET

; Subroutine if double byte header character in buffer, replace double byte charcter with space
C07CD:		LD	A,(HL)
		CALL	C17D6
		RET	NC
		INC	HL
		LD	(HL),20H
		DEC	HL
		RET

; Subroutine clear rest of line on screen
C07D7:		PUSH	BC
		PUSH	DE
		PUSH	HL
		JR	J07E7

; Subroutine refresh line on screen
; Input:  B = current position
C07DC:		PUSH	BC
		PUSH	DE
		PUSH	HL
		LD	A,(D_BB86)
		SUB	B
		LD	B,A
		CALL	C0811
J07E7:		LD	DE,(D_BB8B)
		LD	HL,(D_BB7D)
		OR	A
		SBC	HL,DE
		JR	Z,J0803
		JR	C,J0803
J07F5:		LD	A,20H
		CALL	C0836
		DEC	HL
		LD	A,H
		OR	L
		JR	NZ,J07F5
		LD	(D_BB7D),DE
J0803:		LD	A,1BH
		CALL	KB_CHAROUT
		LD	A,4BH
		CALL	KB_CHAROUT
		POP	HL
		POP	DE
		POP	BC
		RET

; Subroutine text to console, update maximum column position
; Input:  HL = pointer to text
;         B  = size of text
C0811:		PUSH	BC
		INC	B
		JR	J081D

J0815:		LD	A,(HL)
		CALL	C0836
		CALL	C0821
		INC	HL
J081D:		DJNZ	J0815
		POP	BC
		RET

; Subroutine update maximum column position
C0821:		PUSH	HL
		PUSH	BC
		LD	HL,(D_BB7D)
		LD	BC,(D_BB8B)
		OR	A
		SBC	HL,BC
		JR	NC,J0833
		LD	(D_BB7D),BC
J0833:		POP	BC
		POP	HL
		RET

; Subroutine character to console (make control character printable, allow force to screen)
C0836:		PUSH	BC
		PUSH	DE
		PUSH	HL
		CALL	C085A
		JR	NC,J0847
		PUSH	AF
		LD	A,5EH
		CALL	C084E
		POP	AF
		ADD	A,40H
J0847:		CALL	C084E
		POP	HL
		POP	DE
		POP	BC
		RET

; Subroutine character to console (allow force to screen)
C084E:		LD	B,A
		LD	A,(D_BB7A)
		OR	A
		LD	A,B
		JP	Z,C0871
		JP	C08EE

; Subroutine is outputable key ?
C085A:		CP	0DH
		RET	Z
		CP	0AH
		RET	Z
		CP	09H
		RET	Z
		CP	08H
		RET	Z
		CP	7FH
		RET	Z
		CP	20H
		RET

; Subroutine character to console (with console status, redirection support and TAB translation)
C086C:		PUSH	AF
		CALL	C0897
		POP	AF

; Subroutine character to console (with redirection support and TAB translation)
C0871:		CP	09H
		JR	NZ,C0882
J0875:		LD	A,20H
		CALL	C0882
		LD	A,(D_BB8B)
		AND	07H
		JR	NZ,J0875
		RET

; Subroutine character to console (with redirection support)
C0882:		LD	HL,(D_BB8B)
		CALL	C0915
		LD	(D_BB8B),HL
		BIT	1,(IY+9)
		JP	Z,J0908
		LD	C,0FFH
		JP	K_HCONOUT

; Subroutine get console status
C0897:		CALL	KB_CHARSTAT
		LD	B,A
		LD	A,(D_BB8D)
		OR	A
		JR	NZ,J08AF
		LD	A,B
		OR	A
		RET	Z
		CALL	KB_CHARIN
		CALL	C08C5
		OR	A
		RET	Z
		LD	(D_BB8D),A
J08AF:		XOR	A
		DEC	A
		RET

; Subroutine ensure input from keyboard
C08B2:		LD	A,(D_BB8D)
		LD	(IY+13),00H
		OR	A
		RET	NZ
J08BB:		CALL	KB_CHARIN
		CALL	C08C5
		OR	A
		JR	Z,J08BB
		RET

; Subroutine handle special keys
C08C5:		CP	10H
		JR	Z,J08E5
		CP	0EH
		JR	Z,J08E8
		CP	03H
		JR	Z,J08DC
		CP	13H
		RET	NZ
		CALL	KB_CHARIN
		CP	03H
		LD	A,00H
		RET	NZ
J08DC:		LD	A,_CTRLC
		LD	B,00H
		CALL	C3723
J08E3:		JR	J08E3

J08E5:		LD	A,0FFH
		DEFB	0FEH
J08E8:		XOR	A
		LD	(D_BB8A),A
		XOR	A
		RET

; Subroutine character to screen (with TAB translation)
C08EE:		CP	09H
		JR	NZ,C08FF
J08F2:		LD	A,20H
		CALL	C08FF
		LD	A,(D_BB8B)
		AND	07H
		JR	NZ,J08F2
		RET

; Subroutine character to screen
C08FF:		LD	HL,(D_BB8B)
		CALL	C0915
		LD	(D_BB8B),HL
J0908:		CALL	KB_CHAROUT
		LD	HL,D_BB8A
		BIT	0,(HL)
		RET	Z
		LD	E,A
		JP	F_LSTOUT

; Subroutine update console column position
C0915:		INC	HL
		CP	7FH
		JR	Z,J091D
		CP	20H
		RET	NC
J091D:		DEC	HL
		LD	B,A
		LD	A,H
		OR	L
		LD	A,B
		RET	Z
		DEC	HL
		CP	08H
		RET	Z
		CP	7FH
		RET	Z
		INC	HL
		CP	0DH
		RET	NZ
		LD	HL,0
		RET

; CON device jumptable
I0932:		JP	J0941
		JP	J098E
		JP	J09A1
		JP	J0A1C
		JP	J09B1

; CON device input handler
J0941:		BIT	5,C
		JR	NZ,J094B
		CALL	KB_CHARIN
		LD	B,A
		XOR	A
		RET

J094B:		LD	HL,(D_BB78)
		LD	A,(HL)
		OR	A
		JR	NZ,J0979
		LD	DE,I_B1D0
		LD	A,0FFH
		LD	(DE),A
		LD	A,0FFH
		CALL	K_CON_BUFIN
		LD	A,10
		CALL	C08EE
		LD	HL,ISB1D1
		LD	E,(HL)
		LD	D,00H
		INC	HL
		EX	DE,HL
		ADD	HL,DE
		LD	(HL),0DH
		INC	HL
		LD	(HL),0AH
		INC	HL
		LD	(HL),00H
		EX	DE,HL
		LD	A,(HL)
		CP	1AH
		JR	Z,J0985
J0979:		INC	HL
		LD	(D_BB78),HL
		LD	B,A
		CP	0AH
		LD	A,_EOL
		RET	Z
		XOR	A
		RET

J0985:		LD	B,A
		LD	(HL),00H
		LD	(D_BB78),HL
		LD	A,_EOF
		RET

; CON device output handler
J098E:		BIT	5,C
		JR	NZ,J0997
		CALL	KB_CHAROUT
		XOR	A
		RET

J0997:		PUSH	AF
		CALL	C0897
		POP	AF
		CALL	C08EE
		XOR	A
		RET

; CON device check if input ready handler
J09A1:		BIT	5,C
		JR	NZ,J09AB
		CALL	KB_CHARSTAT
		LD	E,A
		XOR	A
		RET

J09AB:		CALL	C0897
		LD	E,A
		XOR	A
		RET

; CON device get screen size handler
J09B1:		CALL	KB_SCREENSIZE
		XOR	A
		RET

; Subroutine clear line input buffer
C09B6:		LD	HL,ISB1D2
		LD	(D_BB78),HL
		LD	(HL),00H
		RET

; AUX device jumptable
I09BF:		JP	J09CE
		JP	J09DD
		JP	J0A1C
		JP	J0A1C
		JP	J0A17

; AUX device input handler
J09CE:		CALL	KB_AUXIN
		LD	B,A
		CP	1AH
		JR	Z,J0A14
		CP	0DH
		LD	A,_EOL
		RET	Z
		XOR	A
		RET

; AUX device output handler
J09DD:		CALL	KB_AUXOUT
		XOR	A
		RET

; LST/PRN device jumptable
I09E2:		JP	J0A12
		JP	J09F1
		JP	J0A1C
		JP	J09FD
		JP	J0A17

; LST/PRN device output handler
J09F1:  	CALL	KB_LPTOUT
		JR	NC,J0A1A
		RES	0,(IY+10)
		LD	A,_STOP
		RET

; LST/PRN device check if output ready handler
J09FD:		CALL	KB_LPTSTAT
		LD	E,A
		XOR	A
		RET

; NUL device jumptable
I0A03:		JP	J0A12
		JP	J0A1A
		JP	J0A1C
		JP	J0A1C
		JP	J0A17

J0A12:		LD	B,1AH
J0A14:		LD	A,_EOF
		RET

J0A17:		LD	DE,0
J0A1A:		XOR	A
		RET

J0A1C:		LD	E,0FFH
		XOR	A
		RET

; ---------------------------------------------------------
; *** BIOS calls ***
; ---------------------------------------------------------

; Subroutine get screen size
KB_SCREENSIZE:	LD	A,(LINLEN)
		LD	E,A
		LD	A,(CRTCNT)
		LD	HL,CNSDFG
		ADD	A,(HL)
		LD	D,A
		RET

; Subroutine get character from keyboard
KB_CHARIN:	CALL	H_CHIN
		CALL	K_CHARFLUSH
		PUSH	IX
		LD	IX,CHGET
		CALL	K_BIOS
		CALL	KB_CHECK_STOP
		POP	IX
		RET

; Subroutine get status from keyboard
KB_CHARSTAT:	CALL	H_CHST
		LD	HL,ST_COU
		DEC	(HL)
		JR	NZ,J0A69
		INC	(HL)
		LD	A,(CH_COU)
		DEC	A
		CALL	Z,K_CHARFLUSH
		PUSH	IX
		LD	IX,CHSNS
		CALL	K_BIOS
		CALL	KB_CHECK_STOP
		POP	IX
		LD	A,0FFH
		RET	NZ
		LD	A,65H
		LD	(ST_COU),A
J0A69:		XOR	A
		RET

KB_CHAROUTC:	LD	A,C

; Subroutine output character to screen
KB_CHAROUT:	CALL	H_CHOU
		PUSH	IX
		LD	IX,CHPUT
		CALL	K_BIOS
		POP	IX
		RET

	IF OPTM = 1

; Screen output is not buffered: keep the H.CHFL hook for compatibility
; the rest of the code is never executed because D_BB77 remains 0.
K_CHARFLUSH:	JP	H_CHFL

	ELSE
; Unused code
Q_0A7B: 	LD	E,A
		CP	1BH
		CALL	Z,K_CHARFLUSH
		LD	HL,D_BB76
		BIT	0,(HL)
		RES	0,(HL)
		JR	NZ,J0A91
		CALL	C17D6
		JR	NC,J0A91
		SET	0,(HL)
J0A91:		LD	A,2
		LD	(CH_COU),A
		LD	A,(D_BB77)
		LD	C,A
		LD	B,00H
		LD	HL,D_B06C
		ADD	HL,BC
		LD	(HL),E
		INC	A
		LD	(D_BB77),A
		CP	64H
		JR	Z,K_FLUSHBUF
		LD	A,(ESCCNT)
		OR	A
		JR	NZ,K_FLUSHBUF
		LD	A,E
		CP	0AH
		JR	Z,K_FLUSHBUF
		CP	07H
		JR	Z,K_FLUSHBUF
		RET

; Subroutine flush screen output buffer (if any)
K_CHARFLUSH:	CALL	H_CHFL
		PUSH	AF
		LD	A,(D_BB77)
		OR	A
		JR	Z,J0ACC
		PUSH	BC
		PUSH	DE
		PUSH	HL
		CALL	K_FLUSHBUF
		POP	HL
		POP	DE
		POP	BC
J0ACC:		POP	AF
		RET

; Subroutine flush screen output buffer
K_FLUSHBUF:	EX	AF,AF'
		EXX
		PUSH	AF
		PUSH	BC
		PUSH	DE
		PUSH	HL
		PUSH	IX
		PUSH	IY
		LD	HL,D_B06C
		LD	A,(D_BB76)
		BIT	0,A
		PUSH	AF
		LD	A,(D_BB77)
		JR	Z,J0AE9
		DEC	A
		JR	Z,J0B07
J0AE9:		LD	B,A
		LD	A,(ESCCNT)
		OR	A
		JR	NZ,J0AF6
		LD	HL,ISB069
		INC	B
		INC	B
		INC	B
J0AF6:		CALL	SFLUSH
		PUSH	HL
		LD	HL,I_B066
		LD	B,3
		LD	A,(ESCCNT)
		OR	A
		CALL	Z,SFLUSH
		POP	HL
J0B07:		XOR	A
		LD	(D_BB77),A
		LD	(CH_COU),A
		POP	AF
		JR	Z,J0B1A
		LD	A,(HL)
		LD	(D_B06C),A
		LD	A,1
		LD	(D_BB77),A
J0B1A:		POP	IY
		POP	IX
		POP	HL
		POP	DE
		POP	BC
		POP	AF
		EXX
		EX	AF,AF'
		RET
	ENDIF ; OPTM

; Subroutine output character to printer
KB_LPTOUT:	CALL	H_LSTO
		CALL	K_CHARFLUSH
		PUSH	IX
		LD	IX,LPTOUT
		CALL	K_BIOS
		POP	IX
		RET

; Subroutine get printer status
KB_LPTSTAT:	CALL	H_LSTS
		PUSH	IX
		LD	IX,LPTSTT
		CALL	K_BIOS
		POP	IX
		RET

; Subroutine get character from AUX device
KB_AUXIN:	CALL	K_CHARFLUSH
		LD	HL,SAUXIN
		JP	C3726			; OPTM: call/ret=jp

KB_AUXOUTC:	LD	A,C

; Subroutine output character to AUX device
KB_AUXOUT:	CALL	K_CHARFLUSH
		LD	HL,SAUXOUT
		JP	C3726			; OPTM: call/ret=jp

; Subroutine check and handle CTRL-STOP
KB_CHECK_STOP:	PUSH	AF
		LD	A,(INTFLG)
		SUB	03H
		JR	Z,J0B65
		POP	AF
		RET

J0B65:		LD	(INTFLG),A
		LD	IX,KILBUF
		CALL	K_BIOS
		LD	A,_STOP
		LD	B,00H
		CALL	C3723
J0B76:		JR	J0B76

; ---------------------------------------------------------
; Subroutine call main-bios
; ---------------------------------------------------------
K_BIOS:		EX	AF,AF'
		EXX
		PUSH	AF
		PUSH	BC
		PUSH	DE
		PUSH	HL
		PUSH	IY
		EXX
		EX	AF,AF'
		CALL	P0_CALL
		EX	AF,AF'
		EXX
		POP	IY
		POP	HL
		POP	DE
		POP	BC
		POP	AF
		EXX
		EX	AF,AF'
		RET

; ------------------------------------------------------------
; *** Functions: 00,0C-0E,18-1B,2A-2E,31,57,58,5F,62,65-70 ***
; ------------------------------------------------------------

; ---------------------------------------------------------
; Function $0C _CPMVER
; ---------------------------------------------------------
F_CPMVER:	LD	HL,0022H
		XOR	A
		RET

; ---------------------------------------------------------
; Function $19 _CURDRV
; ---------------------------------------------------------
F_CURDRV:	LD	A,(CUR_DRV)
		DEC	A
		LD	L,A
		XOR	A
		LD	H,A
		RET

; ---------------------------------------------------------
; Function $1A _SETDTA
; Input:  DE = disk transfer address
; ---------------------------------------------------------
F_SETDTA:	LD	(DTA_AD),DE
		XOR	A
		LD	H,A
		LD	L,A
		RET

; ---------------------------------------------------------
; Function $2E _VERIFY
; ---------------------------------------------------------
F_VERIFY:	LD	A,E
		LD	(RAWFLG),A
		XOR	A
		LD	H,A
		LD	L,A
		RET

; ---------------------------------------------------------
; Function $57 _GETDTA
; Output: DE = disk transfer address
; ---------------------------------------------------------
F_GETDTA:	LD	DE,(DTA_AD)
		XOR	A
		RET

; ---------------------------------------------------------
; Function $58 _GETVFY
; ---------------------------------------------------------
F_GETVFY:	LD	A,(RAWFLG)
		OR	A
		JR	Z,J0CDD
		LD	A,0FFH
J0CDD:		LD	B,A
		XOR	A
		RET

; ---------------------------------------------------------
; Function $00 _TERM0
; ---------------------------------------------------------
F_TERM0:	LD	B,00H

; ---------------------------------------------------------
; Function $62 _TERM
; ---------------------------------------------------------
F_TERM:		LD	A,B
		LD	B,00H
		CALL	C3723
J0D03:		JR	J0D03

; ---------------------------------------------------------
; Function $65 _ERROR
; ---------------------------------------------------------
F_ERROR:	LD	B,(IY+125)
		XOR	A
		RET

; ---------------------------------------------------------
; Function $66 _EXPLAIN
; Input:  B = error code
; ---------------------------------------------------------
F_EXPLAIN:	LD	A,B
		PUSH	DE
		PUSH	IY
		LD	IY,(MASTER-1)
		LD	IX,(SERR_M)
		CALL	CALSLT
		EI
		POP	IY
		LD	B,A
		OR	A
		DEC	HL
		CALL	NZ,C0D26
		XOR	A
		LD	(HL),A
		POP	DE
		RET

; Convert byte number to string
; Input:  A  = number (0-99)
;         HL = pointer to string
C0D26:		LD	C,0FFH
J0D28:		INC	C
		SUB	0AH
		JR	NC,J0D28
		ADD	A,3AH
		PUSH	AF
		LD	A,C
		OR	A
		CALL	NZ,C0D26
		POP	AF
		LD	(HL),A
		INC	HL
		RET

; ---------------------------------------------------------
; Function $6F _DOSVER
; ---------------------------------------------------------
F_DOSVER:	LD	B,02H
	IFDEF DOSV231
		LD	C,31H		; MSXDOS version 2.31
	ELSE
		LD	C,20H		; MSXDOS version 2.20
	ENDIF
		XOR	A
		LD	H,A
		LD	L,A
		LD	D,A
		LD	E,A
		RET

; ---------------------------------------------------------
; Function $70 _REDIR
; ---------------------------------------------------------
F_REDIR:	LD	C,(IY+9)
		OR	A
		JR	Z,J0EDB
		LD	(IY+9),B
J0EDB:		LD	B,C
		XOR	A
		RET

; ---------------------------------------------------------
; Function $6B _GENV
; Input:  HL = pointer to ASCIIZ environment name string
;         DE = pointer to buffer for value
;         B  = size of buffer
; Output: A  = error
;         DE = preserved, buffer filled in if A=0
; ---------------------------------------------------------
F_GENV:		XOR	A

; Subroutine get environment
; Input:  A = segment type (0 = TPA, A<>0 = current)
C0EDF:		LD	(D_BBED),A
		XOR	A
		PUSH	BC
		CALL	C0FA2
		POP	BC
		RET	NZ
		PUSH	DE
		PUSH	BC
		LD	DE,D_BBEE
		CALL	C0F8B
		LD	DE,I0F76
		JR	NC,J0EF8
		LD	D,B
		LD	E,C
J0EF8:		POP	BC
		POP	HL
		CALL	C0FC8
		EX	DE,HL
		RET

; ---------------------------------------------------------
; Function $6C _SENV
; Input:  HL = pointer to ASCIIZ environment name string
;         DE = pointer to ASCIIZ value string
; Output: A  = error
; ---------------------------------------------------------
F_SENV:		XOR	A
		LD	(D_BBED),A
		XOR	A
		CALL	C0FA2
		RET	NZ
		LD	A,B
		OR	A
		RET	Z
		EX	AF,AF'
		EX	DE,HL
		LD	A,0FFH
		CALL	C0FA2
		RET	NZ
		LD	A,B
		OR	A
		JR	Z,J0F46
		EX	AF,AF'
		ADD	A,B
		LD	C,A
		LD	A,00H
		ADC	A,A
		LD	B,A
		PUSH	HL
		LD	HL,4
		ADD	HL,BC
		CALL	K_ALLOC_P2
		POP	BC
		RET	NZ
		PUSH	BC
		LD	BC,(D_BBEE)
		LD	(D_BBEE),HL
		LD	(HL),C
		INC	HL
		LD	(HL),B
		INC	HL
		EX	DE,HL
		XOR	A
		CALL	C0FDF
		EX	(SP),HL
		LD	A,0FFH
		CALL	C0FDF
		POP	HL
		LD	DE,(D_BBEE)
		JR	J0F4A

J0F46:		EX	DE,HL
		LD	DE,D_BBEE
J0F4A:		CALL	C0F8B
		LD	HL,D_BBEE
		CALL	C,C2188
		XOR	A
		RET

; ---------------------------------------------------------
; Function $6D _FENV
; Input:  DE = environment item number
;         HL = pointer to buffer for name string
; Output: A  = error
;         HL = preserved, buffer filled in
; ---------------------------------------------------------
F_FENV:		XOR	A
		LD	(D_BBED),A
		PUSH	HL
		PUSH	BC
		LD	B,D
		LD	C,E
		LD	HL,(D_BBEE)
J0F60:		LD	A,H
		OR	L
		LD	DE,I0F76
		JR	Z,J0F71
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		INC	HL
		EX	DE,HL
		DEC	BC
		LD	A,B
		OR	C
		JR	NZ,J0F60
J0F71:		POP	BC
		POP	HL
		JP	C0FC8

I0F76:		DEFW	0

	IF OPTM = 0
		; Unused code
		LD	HL,(D_BBEE)
J0F7B:		LD	A,H
		OR	L
		RET	Z
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		DEC	HL
		CALL	K_FREE_P2
		EX	DE,HL
		LD	(D_BBEE),HL
		JR	J0F7B
	ENDIF

; Subroutine search for environment
; Outputs Cx set if found
C0F8B:		EX	DE,HL
		LD	A,(HL)
		INC	HL
		LD	H,(HL)
		LD	L,A
		OR	H
		EX	DE,HL
		RET	Z
		PUSH	DE
		PUSH	HL
		INC	DE
		INC	DE
		CALL	C0FF1
		LD	B,D
		LD	C,E
		POP	HL
		POP	DE
		JR	NZ,C0F8B
		SCF
		RET

; Subroutine validate environment string
C0FA2:		PUSH	HL
		AND	01H
		LD	C,A
		LD	B,0FFH
J0FA8:		CALL	C1003
		INC	HL
		CALL	C17AE
		JR	Z,J0FC1
		BIT	0,C
		JR	NZ,J0FBB
		BIT	4,C
		LD	A,_IENV
		JR	NZ,J0FC5
J0FBB:		DJNZ	J0FA8
		LD	A,_ELONG
		JR	J0FC5

J0FC1:		DEC	A
		SUB	B
		LD	B,A
		XOR	A
J0FC5:		POP	HL
		OR	A
		RET

; Subroutine copy from environment to buffer
C0FC8:		PUSH	HL
		PUSH	DE
J0FCA:		LD	A,B
		DEC	B
		OR	A
		LD	A,_ELONG
		JR	Z,J0FDB
		LD	A,(DE)
		CALL	C1012
		INC	HL
		LD	A,(DE)
		INC	DE
		OR	A
		JR	NZ,J0FCA
J0FDB:		POP	DE
		POP	HL
		OR	A
		RET

; Subroutine copy from buffer to environment
C0FDF:		PUSH	HL
		AND	01H
		LD	C,A
J0FE3:		CALL	C1003
		INC	HL
		CALL	C17AE
		LD	(DE),A
		INC	DE
		OR	A
		JR	NZ,J0FE3
		POP	HL
		RET

; Subroutine check if environment
C0FF1:		LD	C,00H
J0FF3:		CALL	C1003
		INC	HL
		CALL	C17AE
		LD	B,A
		LD	A,(DE)
		INC	DE
		CP	B
		RET	NZ
		OR	A
		JR	NZ,J0FF3
		RET

; Subroutine read byte (environment)
C1003:		PUSH	HL
		EX	DE,HL
		LD	A,(D_BBED)
		CALL	C2731
		EX	DE,HL
		CALL	RD_SEG
		EI
		POP	HL
		RET

; Subroutine write byte (environment)
C1012:		PUSH	HL
		PUSH	DE
		LD	E,A
		EX	DE,HL
		LD	A,(D_BBED)
		CALL	C2731
		EX	DE,HL
		CALL	WR_SEG
		EI
		POP	DE
		POP	HL
		RET

; ---------------------------------------------------------
; Function $2A _GDATE
; ---------------------------------------------------------
F_GDATE:	CALL	C111B
		LD	C,D
		LD	B,00H
		LD	E,L
		LD	D,H
		LD	HL,1980		; year base is 1980
		ADD	HL,BC
		LD	A,D
		CP	03H
		LD	A,C
		SBC	A,0FCH
		AND	0FCH
		RRCA
		RRCA
		ADD	A,C
		PUSH	HL
		LD	HL,I104D-1
		LD	C,D
		ADD	HL,BC
		ADD	A,(HL)
		POP	HL
		ADD	A,E
J1044:		SUB	07H
		JR	NC,J1044
		ADD	A,7
		LD	C,A
		XOR	A
		RET

I104D:		DEFB	1,4,4,7,9,12,14,17,20,22,25,27

; ---------------------------------------------------------
; Function $2B _SDATE
; ---------------------------------------------------------
F_SDATE:	LD	BC,-1980	; year base is 1980
		ADD	HL,BC
		JR	NC,J1091
		LD	A,H
		OR	A
		JR	NZ,J1091
		LD	A,L
		CP	100		; year < 2080 ?
		JR	NC,J1091
		LD	B,A
		LD	A,D
		DEC	A
		CP	12		; valid month?
		JR	NC,J1091
		LD	HL,I1096
		ADD	A,L
		LD	L,A
		JR	NC,J1077
		INC	H
J1077:		CP	97H
		JR	NZ,J1083
		LD	A,B
		AND	03H
		JR	NZ,J1083
		LD	HL,I10A2
J1083:		LD	A,E
		DEC	A
		CP	(HL)
		JR	NC,J1091
		LD	L,E
		LD	H,D
		LD	D,B
		CALL	C1167
		XOR	A
		LD	C,A
		RET

J1091:		LD	C,0FFH
		LD	A,_IDATE
		RET

I1096:		DEFB	31,28,31,30,31,30,31,31,30,31,30,31
I10A2:		DEFB	29

; ---------------------------------------------------------
; Function $2C _GTIME
; ---------------------------------------------------------
F_GTIME:	CALL	C111B
		LD	H,B
		LD	L,C
		LD	D,E
		LD	E,00H
		XOR	A
		RET

; ---------------------------------------------------------
; Function $2D _STIME
; ---------------------------------------------------------
F_STIME:	LD	A,H
		CP	24			; 24 hours
		JR	NC,J10C5
		LD	A,L
		CP	60			; 60 minutes
		JR	NC,J10C5
		LD	A,D
		CP	60			; 60 seconds
		JR	NC,J10C5
		LD	B,H
		LD	C,L
		LD	E,D
		CALL	C1155
		XOR	A
		LD	C,A
		RET

J10C5:		LD	C,0FFH
		LD	A,_ITIME
		RET

; ---------------------------------------------------------
; Subroutine initialize clockchip
C10CA:		LD	A,13
		OUT	(0B4H),A
		IN	A,(0B5H)
		AND	04H
		LD	B,A
		INC	A
		OUT	(0B5H),A
		LD	A,10
		OUT	(0B4H),A
		LD	A,1
		OUT	(0B5H),A
		LD	A,13
		OUT	(0B4H),A
		LD	A,B
		OUT	(0B5H),A
		LD	BC,00D00H
J10E8:		LD	A,C
		OUT	(0B4H),A
		IN	A,(0B5H)
		PUSH	AF
		INC	C
		DJNZ	J10E8
		LD	A,14
		OUT	(0B4H),A
		LD	A,00H
		OUT	(0B5H),A
		LD	B,13
J10FB:		DEC	C
		POP	DE
		LD	A,C
		OUT	(0B4H),A
		LD	A,D
		OUT	(0B5H),A
		DJNZ	J10FB

; Subroutine resume real time clock
J1105:		LD	A,13
		OUT	(0B4H),A
		IN	A,(0B5H)
		OR	08H
		OUT	(0B5H),A
		RET

; Subroutine pause real time clock
C1110:		LD	A,13
		OUT	(0B4H),A
		IN	A,(0B5H)
		AND	04H
		OUT	(0B5H),A
		RET

; Subroutine read time and date from real time clock
C111B:		CALL	C1110
		LD	E,13
		CALL	C113C
		LD	D,A
		CALL	C113C
		LD	H,A
		CALL	C113C
		LD	L,A
		DEC	E
		CALL	C113C
		LD	B,A
		CALL	C113C
		LD	C,A
		CALL	C113C
		LD	E,A
		JP	J1105

; Subroutine read byte (BCD) from real time clock
; Input:  E = register+1
; Output: E = register-1
C113C:		PUSH	BC
		CALL	C114C
		LD	B,A
		ADD	A,A
		ADD	A,A
		ADD	A,B
		ADD	A,A
		LD	B,A
		CALL	C114C
		ADD	A,B
		POP	BC
		RET

; Subroutine read nibble from real time clock
; Input:  E = register+1
; Output: E = register
C114C:		DEC	E
		LD	A,E
		OUT	(0B4H),A
		IN	A,(0B5H)
		AND	0FH
		RET

; Subroutine write hour,minute and second to real time clock
C1155:		LD	L,E
		LD	H,C
		LD	D,B
		CALL	C1110
		LD	A,15
		OUT	(0B4H),A
		LD	A,2
		OUT	(0B5H),A
		LD	E,00H
		JR	J117D

; Subroutine write year,month and day to real time clock
C1167:		CALL	C1110
		OR	01H
		OUT	(0B5H),A
		LD	A,11
		OUT	(0B4H),A
		LD	A,D
		OUT	(0B5H),A
		CALL	C1110
		CALL	C1110
		LD	E,7
J117D:		LD	A,L
		CALL	C118C
		LD	A,H
		CALL	C118C
		LD	A,D
		CALL	C118C
		JP	J1105

; Subroutine convert byte to BCD and write to real time clock
C118C:		LD	C,A
		XOR	A
		LD	B,8
J1190:		RLC	C
		ADC	A,A
		DAA
		DJNZ	J1190
		CALL	C119D
		RRCA
		RRCA
		RRCA
		RRCA

; Subroutine write nibble to real time clock
C119D:		LD	B,A
		LD	A,E
		OUT	(0B4H),A
		LD	A,B
		OUT	(0B5H),A
		INC	E
		RET

; ---------------------------------------------------------
; *** K_ALLSEG ***
; ---------------------------------------------------------
K_ALLSEG:	OR	A
		LD	A,(D_BBFE)
		JR	Z,J11AE
		LD	A,0FFH
J11AE:		EX	AF,AF'
		LD	C,B
		LD	A,C
		AND	8FH
		JR	NZ,J11BA
		LD	A,(RAMAD3)
		OR	C
		LD	C,A
J11BA:		LD	A,C
		AND	70H
		JR	NZ,J11C1
		JR	C1206

J11C1:		LD	B,C
		CP	20H
		JR	NZ,J11CB
		CALL	C1206
		JR	NC,J11FF
J11CB:		XOR	A
		LD	HL,EXPTBL
J11CF:		BIT	7,(HL)
		JR	Z,J11D5
		SET	7,A
J11D5:		LD	C,A
		XOR	B
		AND	8FH
		JR	Z,J11E2
		PUSH	HL
		CALL	C1206
		POP	HL
		JR	NC,J11FF
J11E2:		LD	A,C
		BIT	7,A
		JR	Z,J11ED
		ADD	A,4
		BIT	4,A
		JR	Z,J11D5
J11ED:		INC	HL
		INC	A
		AND	03H
		JR	NZ,J11CF
		LD	A,B
		AND	70H
		CP	30H
		SCF
		JR	NZ,J11FF
		LD	C,B
		CALL	C1206
J11FF:		PUSH	AF
		LD	A,C
		AND	8FH
		LD	B,A
		POP	AF
		RET

; Subroutine allocate segment of the specified slot
C1206:		PUSH	BC
		LD	A,C
		AND	0FH
		ADD	A,A
		ADD	A,A
		LD	E,A
		LD	D,00H
		LD	HL,I_BA35
		ADD	HL,DE
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		INC	HL
		LD	A,(HL)
		INC	HL
		LD	H,(HL)
		LD	L,A
		OR	H
		JR	Z,J1253
		LD	A,(DE)
		INC	DE
		LD	C,A
		EX	AF,AF'
		LD	B,A
		EX	AF,AF'
		INC	B
		JR	Z,J123B
		LD	B,00H
J1229:		LD	A,(HL)
		OR	A
		JR	Z,J1234
		INC	B
		INC	HL
		DEC	C
		JR	NZ,J1229
		JR	J1253

J1234:		EX	DE,HL
		DEC	(HL)
		INC	HL
		INC	HL
		INC	(HL)
		JR	J124C

J123B:		ADD	HL,BC
J123C:		DEC	HL
		LD	A,(HL)
		OR	A
		JR	Z,J1246
		DEC	C
		JR	NZ,J123C
		JR	J1253

J1246:		LD	B,C
		DEC	B
		EX	DE,HL
		DEC	(HL)
		INC	HL
		INC	(HL)
J124C:		EX	AF,AF'
		LD	(DE),A
		EX	AF,AF'
		LD	A,B
		POP	BC
		OR	A
		RET

J1253:		POP	BC
		SCF
		RET

; ---------------------------------------------------------
; *** K_FRESEG ***
; ---------------------------------------------------------
K_FRESEG:	LD	C,A
		LD	A,B
		AND	8FH
		JR	NZ,J125F
		LD	A,(RAMAD3)
J125F:		AND	0FH
		ADD	A,A
		ADD	A,A
		LD	E,A
		LD	D,00H
		LD	HL,I_BA35
		ADD	HL,DE
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		INC	HL
		LD	A,(HL)
		INC	HL
		LD	H,(HL)
		LD	L,A
		OR	H
		JR	Z,J128E
		LD	A,(DE)
		CP	C
		JR	C,J128E
		JR	Z,J128E
		LD	B,0
		ADD	HL,BC
		LD	A,(HL)
		OR	A
		JR	Z,J128E
		LD	(HL),B
		EX	DE,HL
		INC	HL
		INC	(HL)
		INC	HL
		INC	A
		JR	Z,J128B
		INC	HL
J128B:		DEC	(HL)
		OR	A
		RET

J128E:		SCF
		RET

; Subroutine free user segments
; Input:  B = proces id
C1290:		LD	C,10H
		LD	HL,I_BA35
J1295:		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		PUSH	DE
		INC	HL
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		INC	HL
		EX	(SP),HL
		LD	A,H
		OR	L
		JR	Z,J12BE
		PUSH	BC
		LD	C,(HL)
J12A5:		LD	A,(DE)
		INC	A
		JR	Z,J12B9
		DEC	A
		JR	Z,J12B9
		DEC	A
		CP	B
		JR	C,J12B9
		PUSH	HL
		XOR	A
		LD	(DE),A
		INC	HL
		INC	(HL)
		INC	HL
		INC	HL
		DEC	(HL)
		POP	HL
J12B9:		INC	DE
		DEC	C
		JR	NZ,J12A5
		POP	BC
J12BE:		POP	HL
		DEC	C
		JR	NZ,J1295
		RET

; ---------------------------------------------------------
; *** Subroutines: parser  ***
; ---------------------------------------------------------

; ---------------------------------------------------------
; Subroutine parse path
; Input:  A  = drive id
;         B  = attributes
;         C  = parse flag
; ---------------------------------------------------------
C12C3:		LD	(IX+0),0FFH
		LD	(IX+31),B
		LD	(IY+32),C
		LD	(D_BB9E),DE
		OR	A
		JR	NZ,J12D7
		LD	A,(CUR_DRV)
J12D7:		LD	D,A
		CALL	C13BC
		OR	A
		JR	Z,J12E9
		CP	D
		JR	Z,J12E9
		LD	D,A
		BIT	7,(IY+32)
		LD	A,_IDRV
		RET	NZ
J12E9:		LD	(IX+25),D
		BIT	3,(IX+31)
		JR	NZ,J12FF
		CALL	C1782
		JR	Z,J1304
		CP	5CH
		JR	NZ,J1304
		SET	0,B
		SET	1,B
J12FF:		SET	5,(IY+32)
		XOR	A
J1304:		CALL	NZ,C179C
		LD	DE,(D_BB9E)
		LD	(D_BB9C),DE
		CALL	C16BC
		BIT	3,(IX+31)
		JR	Z,J1320
		LD	DE,I_B926
		CALL	C13E9
		JR	J1391

J1320:		LD	DE,I_B926
		CALL	C13FF
		CP	5CH
		JR	NZ,J1366
		SET	1,B
		CALL	C1782
		LD	DE,(D_BB9E)
		LD	(D_BB9C),DE
		LD	DE,I_B926
		CALL	C14CB
		JR	NZ,J13A6
		LD	DE,I_B926
		CALL	C16E7
		JR	Z,J1320
		JR	J13A6

J1366:		LD	A,B
		AND	18H
		JR	NZ,J1383
		BIT	1,(IY+32)
		JR	Z,J1383
		PUSH	HL
		PUSH	BC
		LD	HL,I13B1
		LD	DE,I_B926
		LD	BC,11
		LDIR
		POP	AF
		OR	39H
		LD	B,A
		POP	HL
J1383:		XOR	A
		BIT	0,(IY+32)
		LD	DE,I_B926
		CALL	Z,C14F4
		OR	A
		JR	NZ,J139D
J1391:		SET	4,(IY+32)
		LD	DE,I_B926
		CALL	C14CB
		JR	NZ,J13A6
J139D:		LD	(IX+30),A
		LD	DE,I_B926
		CALL	C16E7
J13A6:		PUSH	AF
		CALL	C1782
		CALL	NZ,C179C
		LD	C,A
		POP	AF
		OR	A
		RET

I13B1:		DEFB	"???????????"

; ---------------------------------------------------------
; Subroutine try to parse drive indicator
; ---------------------------------------------------------
C13BC:		LD	(IY+33),00H
		CALL	C1782
		JR	Z,J13E6
		BIT	1,(IY+33)
		JR	NZ,J13E3
		SUB	41H
		JR	C,J13E3
		CP	1AH
		JR	NC,J13E3
		INC	A
		LD	B,A
		CALL	C1782
		JR	Z,J13E3
		CP	3AH
		LD	A,B
		LD	B,4
		RET	Z
		CALL	C179C
J13E3:		CALL	C179C
J13E6:		XOR	A
		LD	B,A
		RET

; ---------------------------------------------------------
; Subroutine parse volume name
; ---------------------------------------------------------
C13E9:		PUSH	HL
		EX	DE,HL
		LD	A,B
		AND	07H
		LD	B,A
		LD	(IY+33),09H
		LD	C,11
		CALL	C146C
		DEC	D
		JR	NZ,J13FD
		SET	3,B
J13FD:		POP	HL
		RET

; ---------------------------------------------------------
; Subroutine parse file name
; ---------------------------------------------------------
C13FF:		PUSH	HL
		EX	DE,HL
		LD	A,B
		AND	07H
		LD	B,A
		LD	(IY+33),00H
		LD	C,8
		CALL	C1782
		JR	Z,J1454
		CP	2EH
		JR	NZ,J1451
		LD	D,1
		CALL	C1782
		JR	Z,J143A
		BIT	4,(IY+33)
		JR	Z,J144E
		CP	2EH
		JR	NZ,J1437
		SET	7,B
		INC	D
		CALL	C1782
		JR	Z,J143A
		BIT	4,(IY+33)
		JR	Z,J1449
		CP	2EH
		JR	Z,J1449
J1437:		CALL	C179C
J143A:		LD	(HL),2EH
		INC	HL
		DEC	C
		DEC	D
		JR	NZ,J143A
		SET	6,B
		SET	3,B
		SET	0,B
		JR	J1454

J1449:		RES	7,B
		CALL	C179C
J144E:		CALL	C179C
J1451:		CALL	C179C
J1454:		CALL	C146C
		DEC	D
		JR	NZ,J145C
		SET	3,B
J145C:		CP	2EH
		JR	NZ,J1465
		SET	4,B
		CALL	C1782
J1465:		LD	C,3
		CALL	C146C
		POP	HL
		RET

; ---------------------------------------------------------
; Subroutine parse name
; ---------------------------------------------------------
C146C:		LD	D,00H
		INC	C
		CALL	C1782
		JR	Z,J14C4
		CALL	C179C
		CP	20H
		JR	Z,J14C4
		DEC	C
J147C:		INC	C
J147D:		CALL	C1782
		JR	Z,J14C4
		BIT	1,(IY+33)
		JR	Z,J1490
		DEC	C
		DEC	C
		JR	NZ,J148E
		LD	A,20H
J148E:		INC	C
		INC	C
J1490:		BIT	4,(IY+33)
		JR	NZ,J14C1
		BIT	3,(IY+33)
		JR	NZ,J14AC
		BIT	2,(IY+33)
		JR	NZ,J14AC
		CP	2AH
		JR	Z,J14B7
		CP	3FH
		JR	NZ,J14AC
J14AA:		SET	5,B
J14AC:		SET	0,B
		LD	D,1
		DEC	C
		JR	Z,J147C
		LD	(HL),A
		INC	HL
		JR	J147D

J14B7:		LD	A,C
J14B8:		LD	C,A
		DEC	A
		JR	Z,J14AA
		LD	(HL),3FH
		INC	HL
		JR	J14B8

J14C1:		CALL	C179C
J14C4:		DEC	C
		RET	Z
		LD	(HL),20H
		INC	HL
		JR	J14C4

; ---------------------------------------------------------
; Subroutine next item
; ---------------------------------------------------------
C14CB:		XOR	A			; directories are searched by the server
		RET

; ---------------------------------------------------------
; Subroutine check if device and get device flags
; Output: A = device flags
; ---------------------------------------------------------
C14F4:		PUSH	BC
		PUSH	HL
		CALL	C1698
		LD	A,00H
		JR	NC,J1517
		POP	AF
		LD	(IX+26),L
		LD	(IX+27),H
		PUSH	HL
	IF OPTM = 0
		LD	BC,0
		ADD	HL,BC
	ENDIF
		LD	C,(HL)
		INC	HL
		LD	B,(HL)
		LD	(IX+28),C
		LD	(IX+29),B
		LD	BC,7
		ADD	HL,BC
		LD	A,(HL)
J1517:		POP	HL
		POP	BC
		RET

; ---------------------------------------------------------
; Subroutine validate name
; Input:  HL = pointer
;         A  = end marker
;         B  = maximum length
;         C  = character flags
; ---------------------------------------------------------
C1669:		CP	(HL)
		JR	Z,J1694
J166C:		LD	A,(HL)
		CALL	C17AE
		LD	(HL),A
		INC	HL
		BIT	4,C
		JR	NZ,J168C
		BIT	2,C
		JR	NZ,J1686
		BIT	3,C
		JR	NZ,J1686
		CP	3FH
		JR	Z,J1694
		CP	2AH
		JR	Z,J1694
J1686:		DJNZ	J166C
		JR	J1692

J168A:		LD	A,(HL)
		INC	HL
J168C:		CP	20H
		JR	NZ,J1694
		DJNZ	J168A
J1692:		XOR	A
		RET

J1694:		LD	A,_IFNM
		OR	A
		RET

; ---------------------------------------------------------
; Subroutine check if device name
; Input:  DE = pointer to string
; Output: Cx = set if device name, reset if no device name
; ---------------------------------------------------------
C1698:		LD	HL,(D_BBF4)
		PUSH	HL
J169C:		POP	HL
		LD	A,H
		OR	L
		RET	Z
		LD	C,(HL)
		INC	HL
		LD	B,(HL)
		INC	HL
		PUSH	BC
		PUSH	HL
		PUSH	DE
		LD	BC,9
		ADD	HL,BC
		LD	B,8
J16AD:		LD	A,(DE)
		CP	(HL)
		JR	NZ,J16B5
		INC	DE
		INC	HL
		DJNZ	J16AD
J16B5:		POP	DE
		POP	HL
		JR	NZ,J169C
		POP	BC
		SCF
		RET

; ---------------------------------------------------------
; Subroutine initialize whole path buffer
; ---------------------------------------------------------
C16BC:		PUSH	HL
		LD	HL,ISB973
		LD	(HL),02H
		LD	HL,ISB931
		LD	(HL),00H
		INC	HL
		LD	(D_BB9A),HL
		POP	HL
		LD	(IY+25),00H

; ---------------------------------------------------------
; Subroutine terminate whole path buffer
; ---------------------------------------------------------
C16D0:		PUSH	HL
		LD	A,2
		LD	HL,(D_BB9A)
		CP	(HL)
		JR	Z,J16E3
		LD	(HL),00H
		INC	HL
		CP	(HL)
		JR	Z,J16E1
		LD	(HL),00H
J16E1:		LD	A,2AH
J16E3:		ADD	A,0D6H
		POP	HL
		RET

; ---------------------------------------------------------
; Subroutine add item to whole path buffer
; ---------------------------------------------------------
C16E7:		PUSH	BC
		PUSH	HL
		LD	HL,(D_BB9A)
		LD	A,B
		AND	18H
		JR	Z,J172F
		BIT	6,B
		JR	Z,J1706
		BIT	7,B
		JR	Z,J1723
J16F9:		DEC	HL
		LD	A,(HL)
		CP	01H
		JR	Z,J1723
		OR	A
		JR	NZ,J16F9
		LD	A,_IPATH
		JR	J172B

J1706:		PUSH	HL
		LD	HL,ISB901
		PUSH	HL
		LD	(HL),01H
		INC	HL
		LD	A,(DE)
		CALL	C173A
		POP	DE
		POP	HL
J1714:		LD	A,(HL)
		CP	02H
		LD	A,_PLONG
		JR	Z,J172B
		LD	A,(DE)
		LD	(HL),A
		INC	HL
		INC	DE
		OR	A
		JR	NZ,J1714
		DEC	HL
J1723:		LD	(D_BB9A),HL
		CALL	C16D0
		JR	Z,J172F
J172B:		LD	(IY+25),0FFH
J172F:		POP	HL
		POP	BC
		BIT	2,(IY+32)
		JR	Z,J1738
		XOR	A
J1738:		OR	A
		RET

; ---------------------------------------------------------
; Subroutine make ASCIIZ string of file or volume name
; Input:  A  = first character
;         HL = pointer to buffer
; ---------------------------------------------------------
C173A:		PUSH	BC
		PUSH	HL
		LD	B,13
J173E:		LD	(HL),00H
		INC	HL
		DJNZ	J173E
		POP	HL
		LD	BC,0B09H
		BIT	3,(IX+31)
		JR	NZ,J175A
		LD	BC,0800H
		CALL	C176A
		LD	(HL),2EH
		INC	HL
		LD	A,(DE)
		LD	BC,0300H
J175A:		CALL	C176A
		BIT	7,C
		JR	NZ,J1768
		BIT	0,C
		JR	NZ,J1768
		DEC	HL
		LD	(HL),00H
J1768:		POP	BC
		RET

; ---------------------------------------------------------
; Subroutine copy name
; Input:  DE = pointer to string
;         A  = first character
;         HL = buffer
;         B  = maximum length
;         C  = character flags
; ---------------------------------------------------------
C176A:		INC	DE
		CALL	C17AE
		BIT	2,C
		JR	NZ,J177A
		BIT	3,C
		JR	NZ,J177A
		CP	20H
		JR	Z,J177E
J177A:		SET	7,C
		LD	(HL),A
		INC	HL
J177E:		LD	A,(DE)
		DJNZ	C176A
		RET

; ---------------------------------------------------------
; Subroutine get parse string character
; ---------------------------------------------------------
C1782:		PUSH	HL
		LD	HL,(D_BB9E)
		LD	A,(HL)
		OR	A
		JR	Z,J178E
		INC	HL
		LD	(D_BB9E),HL
J178E:		POP	HL
		PUSH	BC
		LD	C,(IY+33)
		CALL	C17AE
		LD	(IY+33),C
		POP	BC
		OR	A
		RET

; ---------------------------------------------------------
; Subroutine undo get parse string character
; ---------------------------------------------------------
C179C:		PUSH	HL
		LD	HL,(D_BB9E)
		DEC	HL
		LD	(D_BB9E),HL
		RES	1,(IY+33)
		RES	2,(IY+33)
		POP	HL
		RET

; ---------------------------------------------------------
; Subroutine check character
; Input:  C  = character flags:
;              b0 = set suppress upcasing
;              b1 = set 1st double byte character
;              b2 = set 2nd double byte character
;              b3 = set volume name
; ---------------------------------------------------------
C17AE:		RES	4,C
		SET	2,C
		BIT	1,C
		RES	1,C
		JR	NZ,J17D1
		RES	2,C
		SET	1,C
		CALL	C17D6X		; if v2.31 call extra routine
		JR	C,J17D1
		RES	1,C
		BIT	0,C
		CALL	Z,C17F3
		BIT	3,C
		CALL	C1803
		JR	NC,J17D1
		SET	4,C
J17D1:		OR	A
		RET	NZ
		SET	4,C
		RET

; ---------------------------------------------------------
; Subroutine check for double byte header character if enabled
; ---------------------------------------------------------
C17D6X:
	IFDEF DOSV231
		; extra routine in DOS v2.31
		PUSH	HL
		LD	HL,I17DC
		JR	J17DD
I17DC:		DB	080H,0A0H
		DB	0E0H,0FDH
	ENDIF

C17D6:		CALL	H_16CH
		PUSH	HL
		LD	HL,KANJTA
J17DD:		CP	(HL)
		INC	HL
		JR	C,J17E4
		CP	(HL)
		JR	C,J17EC
J17E4:		INC	HL
		CP	(HL)
		JR	C,J17F0
		INC	HL
		CP	(HL)
		JR	NC,J17F0
J17EC:		OR	A
		SCF
		POP	HL
		RET

J17F0:		OR	A
		POP	HL
		RET

; ---------------------------------------------------------
; Subroutine make upcase
; ---------------------------------------------------------
C17F3:		PUSH	HL
		LD	HL,I_BA75
		CALL	H_UP
		PUSH	BC
		LD	B,0
		LD	C,A
		ADD	HL,BC
		LD	A,(HL)
		POP	BC
		POP	HL
		RET

; ---------------------------------------------------------
; Subroutine validate character
; Input:  A  = character
;         Zx = set: file name, reset: volume name
; Output: Cx set if illegal character
; ---------------------------------------------------------
C1803:		PUSH	HL
		PUSH	BC
		LD	BC,17
		LD	HL,I181C
		JR	Z,J1810
		LD	BC,6
J1810:		CP	20H
		JR	C,J1818
		CPIR
		JR	NZ,J1819
J1818:		SCF
J1819:		POP	BC
		POP	HL
		RET

I181C:		DEFB	07FH,"|<>/",0FFH," :;.,=+\\\"[]"

; ---------------------------------------------------------
; *** Functions: 40-42,59-5E ***
; ---------------------------------------------------------

; ---------------------------------------------------------
; Function $5B _PARSE
; ---------------------------------------------------------
F_PARSE:	LD	C,4
		LD	IX,I_B9DA
		XOR	A
		CALL	C12C3
		LD	C,(IX+25)
		LD	DE,(D_BB9E)
		LD	HL,(D_BB9C)
		RET

; ---------------------------------------------------------
; Function $5C _PFILE
; ---------------------------------------------------------
F_PFILE:	PUSH	HL
		LD	(D_BB9E),DE
		EX	DE,HL
		LD	B,00H
		CALL	C13FF
		LD	DE,(D_BB9E)
		POP	HL
		XOR	A
		RET

; ---------------------------------------------------------
; Function $5D _CHKCHR
; ---------------------------------------------------------
F_CHKCHR:	LD	A,E
		LD	C,D
		CALL	C17AE
		LD	D,C
		LD	E,A
		XOR	A
		RET

; ---------------------------------------------------------
; *** Subroutines: character i/o on file handle ***
; ---------------------------------------------------------

; Subroutine write to file handle
C1D22:		EX	AF,AF'
		CALL	C2136
		RET	NC
		RET	Z
		BIT	7,(IX+30)
		JR	Z,J1D39
		LD	L,(IX+28)
		LD	H,(IX+29)
		INC	HL
		INC	HL
		INC	HL
		EX	AF,AF'

; Subroutine call device handler
C1D38:		JP	(HL)

J1D39:		EX	AF,AF'
		LD	DE,I_BBC5
		LD	(DE),A
		LD	BC,1
		LD	A,0FFH
		JP	C2753			; OPTM: call/ret=jp

; Subroutine read from file handle
C1D47:		CALL	C2136
		RET	NC
		RET	Z
		BIT	7,(IX+30)
		JR	Z,J1D6D
		RES	6,(IX+30)
		LD	L,(IX+28)
		LD	H,(IX+29)
		PUSH	BC
		CALL	C1D38
		POP	DE
		CP	0B9H
		JR	Z,J1D6B
		BIT	5,E
		RET	NZ
		CP	0C7H
		RET	NZ
J1D6B:		XOR	A
		RET

J1D6D:		PUSH	BC
		LD	DE,I_BBC5
		LD	BC,1
		LD	A,0FFH
		CALL	C2757
		LD	HL,I_BBC5
		LD	B,(HL)
		POP	DE
		OR	A
		RET	NZ
		OR	E
		RET	Z
		LD	A,B
		CP	1AH
		LD	A,0C7H
		RET	Z
		XOR	A
		RET

; ---------------------------------------------------------
; *** Functions: 43-56,60,61 ***
; ---------------------------------------------------------

; ---------------------------------------------------------
; Function $47 _DUP
; ---------------------------------------------------------
F_DUP:		CALL	C2136
		RET	NC
		RET	Z
		CALL	C2121
		RET	NZ
		CALL	C215C
		RET	NZ
		LD	(HL),E
		INC	HL
		LD	(HL),D
		CALL	C20E8
		XOR	A
		RET

; ---------------------------------------------------------
; Function $48 _READ
; ---------------------------------------------------------
F_READ:		PUSH	DE
		PUSH	HL
		CALL	C2136
		POP	BC
		POP	DE
		RET	NC
		RET	Z
		XOR	A
		CALL	C2757
		PUSH	BC
		POP	HL
		RET

; ---------------------------------------------------------
; Function $49_WRITE
; ---------------------------------------------------------
F_WRITE:	PUSH	DE
		PUSH	HL
		CALL	C2136
		POP	BC
		POP	DE
		RET	NC
		RET	Z
		XOR	A
		CALL	C2753
		PUSH	BC
		POP	HL
		RET

; ---------------------------------------------------------
; Function $4B _IOCTL
; ---------------------------------------------------------
F_IOCTL:	EX	AF,AF'
		PUSH	DE
		CALL	C2136
		POP	DE
		RET	NC
		RET	Z
		EX	AF,AF'
		LD	L,(IX+28)
		LD	H,(IX+29)
		OR	A
		JR	Z,J1EAA
		DEC	A
		JR	Z,J1E98
		DEC	A
		JR	Z,J1ECB
		DEC	A
		JR	Z,J1EE1
		DEC	A
		JR	Z,J1EFB
J1E95:		LD	A,_ISBFN
		RET

J1E98:		BIT	7,(IX+30)
		JR	Z,J1E95
		LD	A,(IX+30)
		XOR	E
		AND	0DFH
		XOR	E
		RES	6,A
		LD	(IX+30),A
J1EAA:		LD	E,(IX+30)
		XOR	A
		LD	D,A
		BIT	7,E
		RET	NZ

; Subroutine get mode of handle (remote file: end of file is not known)
C1EB2:		LD	E,(IX+25)
		DEC	E
		XOR	A
		LD	D,A
		RET

J1ECB:		BIT	1,(IX+49)
		JR	NZ,J1EF0
		BIT	7,(IX+30)
		JR	NZ,J1EF3
		JR	J1EF0

J1EE1:		BIT	0,(IX+49)
		JR	NZ,J1EF0
		INC	HL
		INC	HL
		INC	HL
		BIT	7,(IX+30)
		JR	NZ,J1EF3
J1EF0:		LD	E,0FFH
		RET

J1EF3:		LD	BC,6
		ADD	HL,BC
		LD	C,(IX+30)
		JP	(HL)
J1EFB:		BIT	7,(IX+30)
		JR	NZ,J1F05
		XOR	A
		LD	E,A
		LD	D,A
		RET

J1F05:		LD	BC,12
		ADD	HL,BC
		JP	(HL)

; ---------------------------------------------------------
; Function $60 _FORK
; ---------------------------------------------------------
F_FORK:		
	IF OPTM = 0
		LD	HL,64
		ADD	HL,HL
	ELSE
		LD	HL,128
	ENDIF
		CALL	K_ALLOC_P2
		RET	NZ
		LD	DE,(D_BBF0)
		LD	(D_BBF0),HL
		LD	(HL),E
		INC	HL
		LD	(HL),D
		LD	A,D
		OR	E
		JR	Z,J2059
		INC	DE
		LD	B,3FH
J2038:		PUSH	BC
		INC	HL
		INC	DE
		LD	A,(DE)
		LD	C,A
		INC	DE
		LD	A,(DE)
		LD	B,A
		OR	C
		JR	Z,J2055
		PUSH	BC
		POP	IX
		BIT	2,(IX+49)
		JR	Z,J2055
		CALL	C215C
		JR	NZ,J2055
		LD	(HL),C
		INC	HL
		LD	(HL),B
		DEC	HL
J2055:		INC	HL
		POP	BC
		DJNZ	J2038
J2059:		LD	A,(D_BBFE)
		LD	B,A
		INC	A
		LD	(D_BBFE),A
		CALL	C20E8
		XOR	A
		RET

; ---------------------------------------------------------
; Function $61 _JOIN
; ---------------------------------------------------------
F_JOIN:		LD	A,B
		OR	A
		JR	Z,J2071
		LD	HL,D_BBFE
		CP	(HL)
		LD	A,_IPROC
		RET	NC
J2071:		CALL	C1290
		LD	HL,(D_BBF0)
		PUSH	HL
J2078:		LD	A,H
		OR	L
		JR	Z,J20A9
		PUSH	BC
		PUSH	HL
		CALL	K_FREE_P2
		LD	B,0FFH
J2083:		INC	B
		CALL	C2136
		JR	NC,J208E
		CALL	NZ,C216B
		JR	J2083

J208E:		POP	HL
		POP	BC
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		EX	DE,HL
		LD	(D_BBF0),HL
		LD	A,(D_BBFE)
		DEC	A
		LD	(D_BBFE),A
		INC	B
		DEC	B
		JR	Z,J2078
		CP	B
		JR	NZ,J2078
		XOR	A
		LD	(DE),A
		DEC	DE
		LD	(DE),A
J20A9:		LD	A,B
		LD	(D_BBFE),A
J20AD:		POP	HL
		LD	A,H
		OR	L
		JR	Z,J20CC
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		PUSH	DE
		LD	B,3FH
J20B8:		INC	HL
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		PUSH	DE
		POP	IX
		LD	A,D
		OR	E
		PUSH	HL
		PUSH	BC
		CALL	NZ,C223A
		POP	BC
		POP	HL
		DJNZ	J20B8
		JR	J20AD

J20CC:		LD	A,(D_BBFE)
		OR	A
		JR	NZ,J20E0
		CALL	F_FORK
		CALL	C21BB
		CALL	K_CON_CLEAR
J20E0:		CALL	C20E8
		CALL	C09B6
		XOR	A
		RET

; ---------------------------------------------------------
; *** Subroutines: file and directory ***
; ---------------------------------------------------------

; Subroutine update redirect status
C20E8:		PUSH	BC
		PUSH	DE
		PUSH	HL
		PUSH	IX
		LD	C,00H
		LD	B,00H
		CALL	C2136
		JR	NC,J2103
		JR	Z,J2103
		LD	A,(IX+30)
		AND	81H
		CP	81H
		JR	Z,J2103
		SET	0,C
J2103:		LD	B,1
		CALL	C2136
		JR	NC,J2117
		JR	Z,J2117
		LD	A,(IX+30)
		AND	82H
		CP	82H
		JR	Z,J2117
		SET	1,C
J2117:		LD	A,C
		LD	(DSBB89),A
		POP	IX
		POP	HL
		POP	DE
		POP	BC
		RET

; Subroutine find free file handle
C2121:		PUSH	DE
		PUSH	IX
		LD	B,0FFH
J2126:		INC	B
		CALL	C2136
		LD	A,0C4H
		JR	NC,J2131
		JR	NZ,J2126
		XOR	A
J2131:		POP	IX
		POP	DE
		OR	A
		RET

; Subroutine get pointer to FIB of file handle
; Input:  B  = file handle
; Output: Cx = reset if invalid file handle, set if valid
;		Zx = reset if fib found
C2136:		LD	A,B
		CP	3FH
		JR	NC,J2158
		LD	HL,(D_BBF0)
		LD	A,H
		OR	L
		JR	Z,J2158
		PUSH	BC
		INC	HL
		INC	HL
		LD	C,B
		LD	B,0
		ADD	HL,BC
		ADD	HL,BC
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		DEC	HL
		POP	BC
		PUSH	DE
		POP	IX
		LD	A,D
		OR	E
		SCF
		LD	A,_NOPEN
		RET

J2158:		LD	A,_IHAND
		OR	A
		RET

; Subroutine increase file handle count of FIB
C215C:		LD	A,(IX-1)
		INC	A
		JR	Z,J2167
		LD	(IX-1),A
		XOR	A
		RET

J2167:		LD	A,_NHAND
		OR	A
		RET

; Subroutine decrease file handle count of FIB and remove FIB if zero count
C216B:		LD	A,(IX-1)
		DEC	A
		LD	(IX-1),A
		RET	NZ
		CALL	RFS_FCLOSE		; last reference: close remote file
		PUSH	DE
		PUSH	BC
		PUSH	IX
		EX	(SP),HL
		LD	BC,-3
		ADD	HL,BC
		EX	DE,HL
		LD	HL,D_BBF2
		CALL	C2188
		POP	HL
		POP	BC
		POP	DE
		XOR	A
		RET

; Subroutine remove element from chain
; Input:  HL = start of chain
;         DE = address of element
C2188:		EX	DE,HL
		LD	B,H
		LD	C,L
		CALL	K_FREE_P2
		EX	DE,HL
J218F:		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		LD	A,D
		OR	E
		RET	Z
		EX	DE,HL
		SBC	HL,BC
		ADD	HL,BC
		JR	NZ,J218F
		DEC	DE
		LD	A,(HL)
		LD	(DE),A
		INC	HL
		INC	DE
		LD	A,(HL)
		LD	(DE),A
		RET

; Subroutine create FIB
C21A3:		LD	HL,54
		CALL	K_ALLOC_P2
		RET	NZ
		PUSH	DE
		LD	DE,(D_BBF2)
		LD	(D_BBF2),HL
		LD	(HL),E
		INC	HL
		LD	(HL),D
		INC	HL
		LD	(HL),01H
		INC	HL
		POP	DE
		RET

; Subroutine open default file handles
C21BB:		LD	B,5
		LD	HL,I21DB
J21C0:		PUSH	BC
		LD	E,(HL)
		INC	HL
		LD	D,(HL)
		INC	HL
		LD	B,(HL)
		INC	HL
		PUSH	HL
		PUSH	BC
		LD	A,B
		CALL	R_OPEN
		POP	BC
		OR	A
		LD	DE,I21F6
		LD	A,B
		CALL	NZ,R_OPEN
		POP	HL
		POP	BC
		DJNZ	J21C0
		RET

I21DB:		DEFW	I21EC
		DEFB	101b		; CON read only
		DEFW	I21EC
		DEFB	110b		; CON write only
		DEFW	I21EC
		DEFB	100b		; CON read & write
		DEFW	I21EE
		DEFB	100b		; AUX read & write
		DEFW	I21F2
		DEFB	110b		; PRN write only

I21EC:		DEFB	"CON",0
I21EE:		DEFB	"AUX",0
I21F2:		DEFB	"PRN",0
I21F6:		DEFB	"NUL",0

; Subroutine ensure directory entry (nothing to do for remote files)
C223A:		XOR	A
		RET

; Subroutine free file handle
C22CD:		CALL	C216B
		XOR	A
		LD	(HL),A
		INC	HL
		LD	(HL),A
		RET
; ---------------------------------------------------------
; *** Subroutines: data transfer ***
; ---------------------------------------------------------

; Subroutine get segment number
; Input:  DE = address
;         A  = segment type (0 = TPA, A<>0 = current)
; Output: DE = page 0 based address
;         A  = segment number
C2731:		PUSH	DE
		PUSH	HL
		LD	HL,P0_TPA
		OR	A
		JR	Z,J273C
		LD	HL,P0_SEG
J273C:		LD	A,D
		AND	0C0H
		RLCA
		RLCA
		LD	E,A
		LD	D,0
		ADD	HL,DE
		LD	A,(HL)
		POP	HL
		POP	DE
		RES	6,D
		RES	7,D
		RET

	IF OPTM = 0
		; Subroutine zero write to FIB
		; Not used
Q_274D:		AND	04H
		OR	02H
		JR	C275B
	ENDIF

; Subroutine write to FIB
; Input:  BC = size
;         DE = transfer address
C2753:		AND	04H
		JR	C275B

; Subroutine read from FIB
; Input:  BC = size
;         DE = transfer address
C2757:		AND	04H
		OR	01H

; Subroutine read/write from FIB
; Input:  BC = size
;         DE = transfer address
;         IX = FIB
;         A  = operation flags
C275B:		BIT	7,(IX+30)
		JR	NZ,J27AA
		PUSH	AF			; remote file
		LD	A,(IX+32)
		LD	(RFS_RH),A
		POP	AF
		JP	RFS_RW

J2783:		LD	L,(IX+45)
		LD	H,(IX+46)
		ADD	HL,BC
		LD	(IX+45),L
		LD	(IX+46),H
		JR	NC,J279A
		INC	(IX+47)
		JR	NZ,J279A
		INC	(IX+48)
J279A:		OR	A
		RET	NZ
		BIT	4,(IY+68)
		JR	NZ,J27A8
		LD	A,B
		OR	C
		LD	A,0C7H
		JR	Z,J279A
J27A8:		XOR	A
		RET

J27AA:		LD	H,A
		AND	04H
		LD	(IX+50),A
		LD	A,B
		OR	C
		RET	Z
		BIT	0,H
		JR	NZ,J27FC
		LD	L,(IX+28)
		LD	H,(IX+29)
		INC	HL
		INC	HL
		INC	HL
		PUSH	BC
J27C1:		PUSH	DE
		LD	A,(IX+50)
		CALL	C2731
		EX	DE,HL
		CALL	RD_SEG
		EI
		EX	DE,HL
		POP	DE
		BIT	5,(IX+30)
		JR	Z,J27D9
		CP	1AH
		JR	Z,J27F1
J27D9:		PUSH	IX
		PUSH	BC
		PUSH	DE
		PUSH	HL
		CALL	C285F
		POP	HL
		POP	DE
		POP	BC
		POP	IX
		OR	A
		JR	NZ,J27F4
		INC	DE
		DEC	BC
		LD	A,B
		OR	C
		JR	NZ,J27C1
		JR	J27F4

J27F1:		XOR	A
		INC	DE
		DEC	BC
J27F4:  POP	HL
		OR	A
		SBC	HL,BC
		LD	B,H
		LD	C,L
J27FA:		JR	J2783

J27FC:		LD	L,(IX+28)
		LD	H,(IX+29)
		PUSH	BC
		RES	6,(IX+30)
J2807:		PUSH	BC
		PUSH	IX
		PUSH	DE
		PUSH	HL
		CALL	C285F
		POP	HL
		POP	DE
		POP	IX
		LD	C,00H
		OR	A
		JR	Z,J2830
		CP	0C7H
		JR	Z,J2827
		CP	0B9H
		JR	NZ,J2850
		BIT	2,(IX+30)
		JR	NZ,J2830
		INC	C
J2827:		INC	C
		BIT	5,(IX+30)
		JR	NZ,J2830
		LD	C,00H
J2830:		PUSH	DE
		PUSH	HL
		LD	A,(IX+50)
		CALL	C2731
		EX	DE,HL
		LD	E,B
		CALL	WR_SEG
		EI
		POP	HL
		POP	DE
		LD	A,C
		POP	BC
		DEC	A
		JR	Z,J2853
		INC	DE
		DEC	BC
		DEC	A
		JR	Z,J2857
		LD	A,B
		OR	C
		JR	NZ,J2807
		JR	J2857

J2850:		POP	BC
		JR	J2857

J2853:		SET	6,(IX+30)
J2857:		POP	HL
		OR	A
		SBC	HL,BC
		LD	B,H
		LD	C,L
		JR	J27FA

; Subroutine call device handler
C285F:		LD	C,(IX+30)
		JP	(HL)

; Subroutine call program abort routine with TPA segments active
C3723:		LD	HL,(KAB_VE)

; Subroutine call routine with TPA segments active
C3726:		PUSH	HL
		LD	HL,PUT_BD
		EX	(SP),HL
		PUSH	HL
		JP	PUT_US


		INCLUDE	"rfs.asm"		; JIO remote file system

	DEPHASE

K1_END:
