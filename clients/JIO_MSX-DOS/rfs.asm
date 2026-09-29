; ------------------------------------------------------------------------------
; rfs.asm
; JIO remote file system for the DOS 2 kernel
;
; Copyright (C) 2025 All rights reserved
; JIO remote file system by Louthrax
; 115K2 transmit/receive routines based on code by Nyyrikki
; ------------------------------------------------------------------------------
; All file and directory functions are sent to the JIO server (COMMAND_BDOS),
; there is no FAT, sector or disk buffer code left in the kernel.
; Device files (CON, AUX, PRN, LST, NUL) are still handled by the kernel.
;
; HYBRID build (p0_hybrid.asm): the drives of the JIO disk interface (first
; drives) are served by the JIO server, the other drives use the FAT code of
; the kernel. The X_ functions route each call to the JIO (R_) or local (F_)
; implementation. The FCB functions use the kernel file handle functions and
; work on both kinds of drives.
;
; This file is included in p0_kernel.asm or p0_hybrid.asm (page 0 code segment, kernel RAM).
; The communication variables are also located in the code segment, so they
; stay available while a TPA segment is mapped in page 2 for data transfers.
; ------------------------------------------------------------------------------

RFS_COMMAND	EQU	22		; COMMAND_BDOS (common/drv_jio.inc)
RFS_FIBSZ	EQU	50		; FIB size on the wire (see common/msxdos2.h)
RFS_FIBRES	EQU	RFS_FIBSZ-1	; FIB result byte
RFS_RESET	EQU	1DH		; reset server file system state

; ---------------------------------------------------------
; Variables
; ---------------------------------------------------------
RFS_HDR:	DEFB	"JIO",0,RFS_COMMAND
RFS_FUNC:	DEFB	0
RFS_BUF:	DEFS	8,0		; command parameters / result
RFS_TXB:	DEFB	0		; single byte to transmit
	IFNDEF HYBRID
RFS_MODE:	DEFB	0		; open mode
RFS_RH:		DEFB	0		; remote file handle
	ENDIF
RFS_OP:		DEFB	0		; b0 = read, b2 = segment type
RFS_TURBO:	DEFB	0		; turbo R flag
RFS_CPU:	DEFB	0		; saved CPU mode
	IFNDEF HYBRID
RFS_LOGIN:	DEFB	0		; drives served by the server (bit 0 = A:)
	ENDIF
RFS_ADDR:	DEFW	0		; transfer address
RFS_LEFT:	DEFW	0		; bytes left
RFS_DONE:	DEFW	0		; bytes done
RFS_CHUNK:	DEFW	0		; bytes in current chunk
RFS_PTR:	DEFW	0		; page 2 transfer address of current chunk
	IFNDEF HYBRID
RFS_OFS:	DEFS	4,0		; FCB file offset
RFS_MA:		DEFS	4,0		; FCB multiply operand
RFS_REQ:	DEFW	0		; FCB block records requested
RFS_PBUF:	DEFS	16,0		; FCB path "D:NAME.EXT"
	ENDIF
	IFDEF HYBRID
; Work area of the removed FCB code in the data segment (page 2, cleared at boot): only for variables that are
; not used while a TPA segment is mapped in page 2 (RFS_RW)
RFS_DPB		EQU	I_B975		; unopened FCB for _SFIRST (37 bytes)
RFS_PBUF	EQU	I_B975		; FCB path, not used at the same time as RFS_DPB
RFS_FIB		EQU	I_B99A		; FCB search FIB (RFS_FIBSZ bytes of the 64 bytes area)
RFS_OFS		EQU	RFS_FIB+RFS_FIBSZ	; FCB file offset (4 bytes)
RFS_MA		EQU	RFS_OFS+4	; FCB multiply operand (4 bytes)
RFS_REQ		EQU	RFS_MA+4	; FCB block records requested (2 bytes)
RFS_MODE	EQU	RFS_REQ+2	; open mode
RFS_RH		EQU	RFS_MODE+1	; remote file handle
RFS_NJIO	EQU	RFS_RH+1	; number of JIO drives (first physical drives)
RFS_WPJIO	EQU	RFS_NJIO+1	; last entry found is on a JIO drive (_WPATH)
	IF RFS_WPJIO >= I_B9DA
		ERROR	"RFS variables overflow the FCB work area"
	ENDIF
RFS_DRVP:	DEFB	"A:"		; drive prefix of paths without drive
	ELSE
RFS_FIB:	DEFS	RFS_FIBSZ,0	; FCB search FIB
	ENDIF
	IFNDEF HYBRID
RFS_DPB:	DEFS	36,0		; unopened FCB for _SFIRST, dummy DPB for _ALLOC
	ENDIF

; ---------------------------------------------------------
; *** Communication ***
; ---------------------------------------------------------

; ---------------------------------------------------------
; Subroutine initialize remote file system
; Called once by K_INIT
; ---------------------------------------------------------
RFS_INIT:	LD	A,(EXPTBL)
		LD	HL,IDBYT2
		CALL	C000C			; RDSLT
		CP	3			; turbo R?
		LD	A,0
		JR	NZ,J_RI1
		DEC	A
J_RI1:		LD	(RFS_TURBO),A
	IFDEF HYBRID
		LD	HL,DRVTBL		; JIO drives: drives of the first disk interface, if it is this one
		LD	A,(MASTER)
		INC	HL
		CP	(HL)
		DEC	HL
		LD	A,0
		JR	NZ,J_RI6
		LD	A,(HL)
J_RI6:		LD	(RFS_NJIO),A
		OR	A
		RET	Z			; no JIO drive (no server)
		LD	A,RFS_RESET		; reset server state (no answer)
		CALL	RFS_CMD
		JR	RFS_END
	ELSE
		LD	A,RFS_RESET		; reset server state (no answer)
		CALL	RFS_CMD
		CALL	RFS_END
		LD	A,18H			; get drives served
		CALL	RFS_CMD
		LD	HL,RFS_LOGIN
		LD	BC,1
		CALL	RFS_RX
		CALL	RFS_END
		LD	A,(RFS_LOGIN)		; number of drives = highest drive served
		LD	B,0
J_RI4:		OR	A
		JR	Z,J_RI5
		INC	B
		SRL	A
		JR	J_RI4
J_RI5:		LD	A,B
		LD	(SNUMDR),A
		LD	E,0FFH			; default drive = first drive served
J_RI2:		INC	E
		LD	A,E
		CP	8
		JR	NC,J_RI3
		CALL	RFS_TSTDRV
		JR	Z,J_RI2
		JP	R_SELDSK
J_RI3:		XOR	A
		RET
	ENDIF

; ---------------------------------------------------------
; Subroutine start remote command
; Input:  A = BDOS function
; Output: interrupts disabled, Z80 mode on turbo R
; May corrupt: AF,BC,DE,HL
; ---------------------------------------------------------
RFS_CMD:	LD	(RFS_FUNC),A
		LD	A,(RFS_TURBO)
		OR	A
		JR	Z,J_RC1
		PUSH	IX
		LD	IX,GETCPU
		CALL	K_BIOS
		LD	(RFS_CPU),A
		XOR	A			; Z80 mode
		LD	IX,CHGCPU
		CALL	K_BIOS
		POP	IX
J_RC1:		DI
		LD	HL,RFS_HDR
		LD	BC,6
		JR	RFS_TX

; ---------------------------------------------------------
; Subroutine end remote command
; Restore CPU mode and enable interrupts
; ---------------------------------------------------------
RFS_END:	PUSH	HL
		PUSH	DE
		PUSH	BC
		LD	A,(RFS_TURBO)
		OR	A
		JR	Z,J_RE1
		PUSH	IX
		LD	A,(RFS_CPU)
		LD	IX,CHGCPU
		CALL	K_BIOS
		POP	IX
J_RE1:		POP	BC
		POP	DE
		POP	HL
		EI
		RET

; ---------------------------------------------------------
; Subroutine transmit path or FIB
; Input:  DE = pointer to ASCIIZ path or FIB
; ---------------------------------------------------------
RFS_TXPATH:	EX	DE,HL
		LD	A,(HL)
		INC	A
	IFDEF HYBRID
		JR	NZ,J_TP1
		LD	BC,RFS_FIBSZ
		JR	RFS_TX

J_TP1:		DEC	A			; path without drive: send current drive first
		JR	Z,J_TP2
		INC	HL
		LD	A,(HL)
		DEC	HL
		CP	':'
		JR	Z,RFS_TXSTR
J_TP2:		PUSH	HL
		LD	A,(CUR_DRV)
		ADD	A,'A'-1
		LD	(RFS_DRVP),A
		LD	HL,RFS_DRVP
		LD	BC,2
		CALL	RFS_TX
		POP	HL
	ELSE
		JR	NZ,RFS_TXSTR
		LD	BC,RFS_FIBSZ
		JR	RFS_TX
	ENDIF

; ---------------------------------------------------------
; Subroutine transmit ASCIIZ string (including the zero)
; Input:  HL = pointer to string
; ---------------------------------------------------------
RFS_TXSTR:	PUSH	HL
		LD	BC,0
J_TS1:		LD	A,(HL)
		INC	HL
		INC	BC
		OR	A
		JR	NZ,J_TS1
		POP	HL

; ---------------------------------------------------------
; Subroutine transmit data
; Input:  HL = pointer to data
;         BC = size
; May corrupt: AF,BC,HL
; ---------------------------------------------------------
RFS_TX:		LD	A,B
		OR	C
		RET	Z
		EXX
		PUSH	BC
		PUSH	DE
		EXX
		CALL	J_TX0
		EXX
		POP	DE
		POP	BC
		EXX
		RET

J_TX0:		INC	BC
		EXX
		LD	A,15
		OUT	(0A0H),A
		IN	A,(0A2H)
		OR	4
		LD	E,A
		XOR	4
		LD	D,A
		LD	C,0A1H

		DEFB	3EH			; LD A,n: skip RET NZ
J_TXLOOP:	RET	NZ
		OUT	(C),E
		EXX
		LD	A,(HL)
		CPI
		RET	PO
		EXX
		RRCA
		OUT	(C),D			; =0
		RET	NZ
		JP	C,J_TX10
		OUT	(C),D			; -0
		RRCA
		JP	C,J_TX11

J_TX01:		OUT	(C),D			; -1
		RRCA
		JR	C,J_TX12
		NOP

J_TX02:		OUT	(C),D			; -0
		RRCA
		JP	C,J_TX13

J_TX03:		OUT	(C),D			; -1
		RRCA
		JR	C,J_TX14
		NOP

J_TX04:		OUT	(C),D			; -0
		RRCA
		JP	C,J_TX15

J_TX05:		OUT	(C),D			; -1
		RRCA
		JR	C,J_TX16
		NOP

J_TX06:		OUT	(C),D			; -0
		RRCA
		JP	C,J_TX17

J_TX07:		OUT	(C),D			; -1
		JP	J_TXLOOP

J_TX10:		OUT	(C),E			; -0
		RRCA
		JP	NC,J_TX01

J_TX11:		OUT	(C),E			; -1
		RRCA
		JR	NC,J_TX02
		NOP

J_TX12:		OUT	(C),E			; -0
		RRCA
		JP	NC,J_TX03

J_TX13:		OUT	(C),E			; -1
		RRCA
		JR	NC,J_TX04
		NOP

J_TX14:		OUT	(C),E			; -0
		RRCA
		JP	NC,J_TX05

J_TX15:		OUT	(C),E			; -1
		RRCA
		JR	NC,J_TX06
		NOP

J_TX16:		OUT	(C),E			; -0
		RRCA
		JP	NC,J_TX07

J_TX17:		OUT	(C),E			; -1
		JP	J_TXLOOP

; ---------------------------------------------------------
; Subroutine receive one answer packet, wait until received
; Input:  HL = pointer to buffer
;         BC = size
; May corrupt: AF,BC,DE,HL
; ---------------------------------------------------------
RFS_RX:		LD	A,B
		OR	C
		RET	Z
		PUSH	IX
J_RX0:		PUSH	HL
		PUSH	BC
		LD	D,B
		LD	E,C
		CALL	J_RX1
		POP	BC
		POP	HL
		OR	A
		JR	Z,J_RX0			; time-out: server not ready yet
		POP	IX
		RET

; Input:  HL = buffer, DE = size
; Output: A = 1 received, 0 time-out
J_RX1:		PUSH	DE
		LD	DE,0
		DEC	HL
		LD	C,0A2H
		LD	IX,0
		ADD	IX,SP
		LD	A,15
		OUT	(0A0H),A
		IN	A,(0A2H)
		OR	64
		OUT	(0A1H),A
		LD	A,14
		OUT	(0A0H),A
		IN	A,(0A2H)
		OR	1
		JP	PE,J_RXPE

J_RXPO:		DEC	DE			;  7
		LD	A,D			;  5
		OR	E			;  5
		JR	Z,J_RXTO		;  8
		IN	F,(C)			; 14
		JP	PO,J_RXPO		; 11	LOOP=50 (2-CLOCKS)
		RLC	A			; 10
		IN	F,(C)			; 14
		JP	PO,J_RXPO		; 11	At least 2 clocks needed to be down

J_WUPO:		IN	F,(C)			; 14
		JP	PE,J_WUPO		; 11	LOOP=25
		POP	DE			; 11
		PUSH	DE			; 11

J_RBPO:		IN	F,(C)			; 14
		JP	PO,J_RBPO		; 11	LOOP=25
		LD	B,(HL)			;  8 = 33 CYCLES

		IN	A,(C)			; 14	Bit 0
		NOP				;  5
		RRCA				;  5
		DEC	DE			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 1
		XOR	B			;  5
		RRCA				;  5
		INC	HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 2
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 3
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 4
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 5
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 6
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 7
		XOR	B			;  5
		RRCA				;  5

		LD	(HL),A			;  8

		LD	A,D			;  5
		OR	E			;  5
		JP	NZ,J_RBPO		; 11

J_RXOK:		LD	SP,IX
		POP	DE
		LD	A,1
		RET

J_RXTO:		POP	DE
		XOR	A
		RET

J_RXPE:		DEC	DE			;  7
		LD	A,D			;  5
		OR	E			;  5
		JR	Z,J_RXTO		;  8
		IN	F,(C)			; 14
		JP	PE,J_RXPE		; 11	LOOP= 50 (2-CLOCKS)
		RLC	A			; 10
		IN	F,(C)			; 14
		JP	PE,J_RXPE		; 11	At least 2 clocks needed to be down

J_WUPE:		IN	F,(C)			; 14
		JP	PO,J_WUPE		; 11	LOOP=25
		POP	DE			; 11
		PUSH	DE			; 11

J_RBPE:		IN	F,(C)			; 14
		JP	PE,J_RBPE		; 11	LOOP=25
		LD	B,(HL)			;  8 = 33 CYCLES

		IN	A,(C)			; 14	Bit 0
		CPL				;  5
		RRCA				;  5
		DEC	DE			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 1
		XOR	B			;  5
		RRCA				;  5
		INC	HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 2
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 3
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 4
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 5
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 6
		XOR	B			;  5
		RRCA				;  5
		LD	SP,HL			;  7 = 31 CYCLES

		IN	B,(C)			; 14	Bit 7
		XOR	B			;  5
		RRCA				;  5

		LD	(HL),A			;  8	Instruction uses data bus write

		LD	A,D			;  5
		OR	E			;  5
		JP	NZ,J_RBPE		; 11

		JR	J_RXOK

; ---------------------------------------------------------
; Subroutine receive 1 byte result and end command
; Output: A = result
; ---------------------------------------------------------
RFS_RXERR:	LD	BC,1

; ---------------------------------------------------------
; Subroutine receive result in RFS_BUF and end command
; Input:  BC = size
; Output: A = first byte of RFS_BUF
; ---------------------------------------------------------
RFS_RXBUF:	LD	HL,RFS_BUF
		CALL	RFS_RX
		CALL	RFS_END
		LD	A,(RFS_BUF)
		RET

; ---------------------------------------------------------
; Subroutine start command with a 1 byte parameter
; Input:  A = parameter
;         C = BDOS function
; ---------------------------------------------------------
RFS_CMD1:	LD	(RFS_BUF),A
		LD	A,C
		LD	BC,1

; ---------------------------------------------------------
; Subroutine start command with RFS_BUF parameters
; Input:  A  = BDOS function
;         BC = size of parameters in RFS_BUF
; ---------------------------------------------------------
RFS_CMDBUF:	PUSH	BC
		CALL	RFS_CMD
		POP	BC
		LD	HL,RFS_BUF
		JP	RFS_TX

	IFNDEF HYBRID
; ---------------------------------------------------------
; Subroutine test if drive is served
; Input:  E = physical drive (0 = A:)
; Output: Zx = reset if served
; ---------------------------------------------------------
RFS_TSTDRV:	PUSH	BC
		LD	B,E
		INC	B
		LD	A,(RFS_LOGIN)
J_TD1:		RRCA
		DJNZ	J_TD1
		SBC	A,A
		POP	BC
		RET

	ENDIF

; ---------------------------------------------------------
; Subroutine check if path is a device
; Input:  DE = pointer to ASCIIZ path or FIB
; Output: A  = 0 remote file, 1 device, else error code
;         I_B9DA = parsed FIB (ASCIIZ path)
;         I_B926 = last item (ASCIIZ path)
; ---------------------------------------------------------
	IFNDEF HYBRID
RFS_CHKDEV:	PUSH	HL
		PUSH	DE
		PUSH	BC
		LD	A,(DE)
		INC	A
		JR	NZ,J_CD1
		LD	HL,30			; FIB: device flag
		ADD	HL,DE
		LD	A,(HL)
		JR	J_CD2

J_CD1:		PUSH	IX
		LD	IX,I_B9DA
		XOR	A
		LD	B,A
		LD	C,4			; parse only, no disk access
		CALL	C12C3
		JR	NZ,J_CD4		; parse error
		LD	A,(IX+30)
J_CD4:		POP	IX
		JR	NZ,J_CD3
J_CD2:		RLCA
		AND	1
J_CD3:		POP	BC
		POP	DE
		POP	HL
		RET

; Device result of RFS_CHKDEV to error code
RFS_DEVERR:	CP	1
		RET	NZ
		LD	A,_IDEV
		RET
	ENDIF

; ---------------------------------------------------------
; Subroutine get remote file of file handle
; Input:  B  = file handle
; Output: IX = pointer to FAB
;         Zx = set if remote file, A = remote file handle
;         Zx = reset if error (A = error) or device (A = 0)
; ---------------------------------------------------------
RFS_GETRH:	CALL	C2136
	IFNDEF HYBRID
		RET	NC
		JR	Z,J_GR1
		BIT	7,(IX+30)
		LD	A,0
		RET	NZ
	ENDIF
		LD	A,(IX+32)
		CP	A
		RET
	IFNDEF HYBRID

J_GR1:		OR	A
		RET
	ENDIF

; ---------------------------------------------------------
; Subroutine close remote file of FAB (last reference)
; Input:  IX = pointer to FAB
; ---------------------------------------------------------
RFS_FCLOSE:
	IFDEF HYBRID
		BIT	6,(IX+49)		; remote file ?
		RET	Z
	ELSE
		BIT	7,(IX+30)
		RET	NZ
	ENDIF
		LD	A,(IX+32)
		OR	A
		RET	Z
		PUSH	HL
		PUSH	DE
		PUSH	BC
		LD	C,45H
		CALL	RFS_CMD1
		CALL	RFS_RXERR
		POP	BC
		POP	DE
		POP	HL
		RET

	IFDEF HYBRID
; ---------------------------------------------------------
; Subroutine read or write remote file of FAB (from C275B)
; Input:  IX = pointer to FAB, others see RFS_RW
; ---------------------------------------------------------
RFS_RWFAB:	PUSH	AF
		LD	A,(IX+32)
		LD	(RFS_RH),A
		POP	AF
	ENDIF

; ---------------------------------------------------------
; Subroutine read or write remote file
; Input:  A  = b0 set for read, b2 = segment type
;         DE = transfer address
;         BC = size
;         RFS_RH = remote file handle
; Output: A  = error
;         BC = bytes transferred
; ---------------------------------------------------------
RFS_RW:		LD	(RFS_OP),A
		LD	A,B
		OR	C
		RET	Z
		LD	(RFS_ADDR),DE
		LD	(RFS_LEFT),BC
		LD	HL,0
		LD	(RFS_DONE),HL
J_RW1:		LD	HL,(RFS_LEFT)
		LD	A,H
		OR	L
		JP	Z,J_RW7
		LD	DE,(RFS_ADDR)		; chunk = min(left, room in 16K page)
		LD	A,D
		AND	3FH
		LD	B,A
		LD	C,E
		PUSH	HL
		LD	HL,4000H
		OR	A
		SBC	HL,BC
		POP	BC
		PUSH	HL
		SBC	HL,BC
		POP	HL
		JR	C,J_RW2
		LD	H,B
		LD	L,C
J_RW2:		LD	(RFS_CHUNK),HL
		LD	(RFS_BUF+1),HL
		LD	A,(RFS_RH)
		LD	(RFS_BUF),A
		PUSH	DE
		LD	A,(RFS_OP)
		RRCA
		LD	A,49H
		SBC	A,0			; 48H = read, 49H = write
		LD	BC,3
		CALL	RFS_CMDBUF
		POP	DE
		LD	A,(RFS_OP)
		AND	04H
		CALL	C2731			; A = segment, DE = page 0 based address
		SET	7,D
		LD	(RFS_PTR),DE
		CALL	PUT_P2			; transfer segment in page 2
		DI
		LD	A,(RFS_OP)
		RRCA
		JR	C,J_RW3
		LD	HL,(RFS_PTR)		; write
		LD	BC,(RFS_CHUNK)
		CALL	RFS_TX
		LD	HL,RFS_BUF
		LD	BC,3
		CALL	RFS_RX
		JR	J_RW4

J_RW3:		LD	HL,RFS_BUF		; read
		LD	BC,3
		CALL	RFS_RX
		LD	HL,(RFS_PTR)
		LD	BC,(RFS_BUF+1)
		CALL	RFS_RX
J_RW4:		LD	A,(DATA_S)		; data segment back in page 2 before interrupts are enabled
		CALL	PUT_P2
		CALL	RFS_END
		LD	A,(RFS_BUF)
		OR	A
		JR	NZ,J_RW8
		LD	BC,(RFS_BUF+1)
		LD	HL,(RFS_DONE)
		ADD	HL,BC
		LD	(RFS_DONE),HL
		LD	HL,(RFS_ADDR)
		ADD	HL,BC
		LD	(RFS_ADDR),HL
		LD	HL,(RFS_LEFT)
		OR	A
		SBC	HL,BC
		LD	(RFS_LEFT),HL
		LD	HL,(RFS_CHUNK)
		SBC	HL,BC
		JP	Z,J_RW1			; full chunk, continue
J_RW7:		LD	HL,(RFS_DONE)
		LD	A,H
		OR	L
		JR	NZ,J_RW9
		LD	A,(RFS_OP)		; nothing read: end of file
		RRCA
		LD	A,_EOF
		JR	C,J_RW8
J_RW9:		XOR	A
J_RW8:		LD	BC,(RFS_DONE)
		OR	A
		RET

; ---------------------------------------------------------
; Subroutine move remote file pointer
; Input:  A     = method code
;         C     = remote file handle
;         DE:HL = offset
; Output: A     = error
;         DE:HL = new file pointer
; ---------------------------------------------------------
RFS_RSEEK:	LD	(RFS_BUF+1),A
		LD	A,C
		LD	(RFS_BUF),A
		LD	(RFS_BUF+2),HL
		LD	(RFS_BUF+4),DE
		LD	A,4AH
		LD	BC,6
		CALL	RFS_CMDBUF
		LD	BC,5
		CALL	RFS_RXBUF
		LD	HL,(RFS_BUF+1)
		LD	DE,(RFS_BUF+3)
		RET

; ---------------------------------------------------------
; *** Functions: 0D,0E,18,1B,2F-31,5F,67-69,6A,6E ***
; ---------------------------------------------------------

	IFNDEF HYBRID
; ---------------------------------------------------------
; Function $0D _DSKRST
; ---------------------------------------------------------
R_DSKRST:	LD	A,(CUR_DRV)
		DEC	A
		LD	E,A
		CALL	R_SELDSK		; keep server in sync
		LD	HL,DBUF
		LD	(DTA_AD),HL
		XOR	A
		LD	H,A
		LD	L,A
		RET

; ---------------------------------------------------------
; Function $0E _SELDSK
; Input:  E = drive number (0 = A:)
; ---------------------------------------------------------
R_SELDSK:	LD	A,E
		CP	8
		JR	NC,J_SD1
		CALL	RFS_TSTDRV
		JR	Z,J_SD1
		LD	A,E
		INC	A
		LD	(CUR_DRV),A
		LD	A,E
		LD	C,0EH
		CALL	RFS_CMD1
		CALL	RFS_RXERR
		LD	C,0
		DEFB	21H			; LD HL,n: skip next instruction
J_SD1:		LD	C,_IDRV
		LD	HL,(SNUMDR)
		LD	H,0
		LD	A,C
		RET

; ---------------------------------------------------------
; Function $18 _LOGIN
; ---------------------------------------------------------
R_LOGIN:	LD	A,(RFS_LOGIN)
		LD	L,A
		XOR	A
		LD	H,A
		RET

	ENDIF

; ---------------------------------------------------------
; Function $1B _ALLOC
; Input:  E  = drive number (0 = current, 1 = A: etc)
; Output: C  = sectors per cluster
;         DE = total clusters on disk
;         HL = free clusters on disk
;         IX = pointer to DPB
;         IY = pointer to first FAT sector (dummy)
; ---------------------------------------------------------
R_ALLOC:	LD	A,E
	IFDEF HYBRID
		OR	A
		JR	NZ,J_AL1
		LD	A,(CUR_DRV)		; current drive
J_AL1:
	ENDIF
		LD	C,1BH
		CALL	RFS_CMD1
		LD	BC,5
		CALL	RFS_RXBUF		; sectors per cluster, total and free clusters
		LD	C,A
		LD	DE,(RFS_BUF+1)
		LD	HL,(RFS_BUF+3)
		LD	IX,RFS_DPB
		LD	IY,RFS_DPB
		OR	A
		LD	A,0
		RET	NZ
		LD	C,0FFH			; invalid drive
		LD	A,_IDRV
		RET

	IFNDEF HYBRID
; ---------------------------------------------------------
; Function $5F _FLUSH
; Function $69 _BUFFER
; Function $6E _DSKCHK
; ---------------------------------------------------------
R_FLUSH:
R_BUFFER:
R_DSKCHK:	XOR	A
		LD	B,A
		RET

	ENDIF

; ---------------------------------------------------------
; Function $68 _RAMD
; RAM disk H: served by the server (no MSX memory used)
; Input:  B = 0 destroy, 1-0FEH create (16 KB segments), 0FFH get size
; Output: A = error, B = RAM disk size (segments)
; ---------------------------------------------------------
R_RAMD:
	IFDEF HYBRID
		LD	A,(RFS_NJIO)
		OR	A
		JR	Z,J_RD2			; no server: no RAM disk
	ENDIF
		LD	A,B
		LD	C,68H
		CALL	RFS_CMD1
	IFDEF HYBRID
		LD	BC,3
		CALL	RFS_RXBUF		; error, size, drives served (not used)
		LD	HL,(RFS_BUF+1)
		LD	B,L
		LD	HL,0			; H: DPB entry: DPB of the first JIO drive when the RAM disk exists
		INC	B
		DEC	B
		JR	Z,J_RD1
		LD	HL,(I_BA25)
J_RD1:		LD	(D_BA33),HL
		RET

J_RD2:		LD	A,B
		INC	A
		LD	B,0
		LD	A,_NORAM
		RET	NZ
		XOR	A
		RET
	ELSE
		LD	BC,3
		CALL	RFS_RXBUF		; error, size, drives served
		LD	HL,(RFS_BUF+1)
		LD	B,L
		LD	L,A
		LD	A,H
		LD	(RFS_LOGIN),A
		LD	A,L
		RET
	ENDIF

	IFNDEF HYBRID
; ---------------------------------------------------------
; Function $6A _ASSIGN
; ---------------------------------------------------------
R_ASSIGN:	LD	D,B
		XOR	A
		RET

	ENDIF

; ---------------------------------------------------------
; *** Functions: 40-46,48-4A,4C-56,59,5A,5E ***
; ---------------------------------------------------------

; ---------------------------------------------------------
; Function $43 _OPEN
; Input:  DE = pointer to ASCIIZ path or FIB
;         A  = open mode
; Output: B  = file handle
; ---------------------------------------------------------
R_OPEN:		LD	(RFS_MODE),A
		LD	C,43H
	IFDEF HYBRID
		JR	J_OP2
	ELSE
		CALL	RFS_CHKDEV
		OR	A
		JR	Z,J_OP2
		CP	1
		RET	NZ
J_OPDEV:	XOR	A			; device: open it locally
		LD	(RFS_RH),A
	ENDIF
J_OP1:		LD	HL,I_B9DA
		LD	A,(DE)
		INC	A
		JR	NZ,RFS_NEWFAB
		EX	DE,HL
		JR	RFS_NEWFAB

J_OP2:		LD	A,(RFS_MODE)		; remote file
		LD	(RFS_TXB),A
		PUSH	DE
		LD	A,C
		CALL	RFS_CMD
		POP	DE
		PUSH	DE
		CALL	RFS_TXPATH
		LD	HL,RFS_TXB
		LD	BC,1
		CALL	RFS_TX
		LD	BC,2
		CALL	RFS_RXBUF
		POP	DE
		OR	A
		RET	NZ
		LD	A,(RFS_BUF+1)
		LD	(RFS_RH),A
		JR	J_OP1

; ---------------------------------------------------------
; Function $44 _CREATE
; Input:  DE = pointer to ASCIIZ path
;         A  = open mode
;         B  = attributes (b7 = create new)
; Output: B  = file handle (0FFH if sub-directory)
; ---------------------------------------------------------
R_CREATE:	LD	(RFS_MODE),A
		BIT	3,B
		LD	A,_IATTR
		RET	NZ
	IFNDEF HYBRID
		CALL	RFS_CHKDEV
		OR	A
		JR	Z,J_CR1
		CP	1
		RET	NZ
		JR	J_OPDEV			; device
	ENDIF

J_CR1:		PUSH	BC
		PUSH	DE
		LD	A,44H
		CALL	RFS_CMD
		POP	DE
		PUSH	DE
		CALL	RFS_TXPATH
		POP	DE
		POP	BC
		PUSH	BC
		LD	A,(RFS_MODE)
		LD	(RFS_BUF),A
		LD	A,B
		LD	(RFS_BUF+1),A
		LD	HL,RFS_BUF
		LD	BC,2
		CALL	RFS_TX
		LD	BC,2
		CALL	RFS_RXBUF
		POP	BC
		OR	A
		RET	NZ
		LD	A,(RFS_BUF+1)
		LD	(RFS_RH),A
		BIT	4,B
		LD	B,0FFH
		JR	NZ,J_CR2		; sub-directory: no file handle
		LD	HL,I_B9DA
		JR	RFS_NEWFAB

J_CR2:		XOR	A
		RET

; ---------------------------------------------------------
; Subroutine allocate file handle and FAB
; Input:  HL = pointer to FIB (first 32 bytes copied into FAB)
;         RFS_MODE = open mode
;         RFS_RH   = remote file handle (not used for devices)
; Output: A  = error
;         B  = file handle
;         IX = pointer to FAB
; ---------------------------------------------------------
RFS_NEWFAB:	PUSH	HL
		CALL	C2121			; B = free file handle, HL = pointer in handle table
		JR	NZ,J_NF2
		PUSH	HL
		CALL	C21A3			; HL = new FAB
		JR	NZ,J_NF1
		EX	DE,HL
		POP	HL
		LD	(HL),E
		INC	HL
		LD	(HL),D
		PUSH	DE
		POP	IX
		POP	HL
		PUSH	BC
		LD	BC,32
		LDIR
		POP	BC
		LD	(IX+31),06H
		LD	A,(RFS_MODE)
		AND	07H
	IFDEF HYBRID
		OR	40H			; b6 = remote file
		LD	(IX+21),0FFH		; size FFFFFFFFH: _IOCTL never reports end of file
		LD	(IX+22),0FFH
		LD	(IX+23),0FFH
		LD	(IX+24),0FFH
	ENDIF
		LD	(IX+49),A
		LD	A,(RFS_RH)
		LD	(IX+32),A
		CALL	C20E8
		XOR	A
		RET

J_NF1:		POP	HL
J_NF2:		POP	HL
		PUSH	AF			; out of handles or memory: close remote file
		LD	A,(RFS_RH)
		OR	A
		JR	Z,J_NF3
		LD	C,45H
		CALL	RFS_CMD1
		CALL	RFS_RXERR
J_NF3:		POP	AF
		RET

	IFNDEF HYBRID
; ---------------------------------------------------------
; Function $45 _CLOSE
; ---------------------------------------------------------
R_CLOSE:	CALL	C2136
		RET	NC
		RET	Z
		CALL	C22CD			; closes remote file on last reference
		CALL	C20E8
		XOR	A
		RET

; ---------------------------------------------------------
; Function $46 _ENSURE
; ---------------------------------------------------------
R_ENSURE:	CALL	C2136
		RET	NC
		RET	Z
		XOR	A
		RET

	ENDIF
; ---------------------------------------------------------
; Function $4A _SEEK
; Input:  B     = file handle
;         A     = method code
;         DE:HL = signed offset
; Output: DE:HL = new file pointer
; ---------------------------------------------------------
R_SEEK:		EX	AF,AF'
		PUSH	DE
		PUSH	HL
		CALL	RFS_GETRH
		POP	HL
		POP	DE
	IFNDEF HYBRID
		JR	NZ,J_SK1
	ENDIF
		LD	C,A
		EX	AF,AF'
		JP	RFS_RSEEK

	IFNDEF HYBRID

J_SK1:		OR	A			; device: pointer always 0
		RET	NZ
		LD	H,A
		LD	L,A
		LD	D,A
		LD	E,A
		RET
	ENDIF

; ---------------------------------------------------------
; Function $4C _HTEST
; ---------------------------------------------------------
R_HTEST:	XOR	A
		LD	B,A
		RET

; ---------------------------------------------------------
; Function $4D _DELETE
; Function $5A _CHDIR
; Input:  DE = pointer to ASCIIZ path or FIB
; ---------------------------------------------------------
R_DELETE:	LD	C,4DH
		DEFB	21H			; LD HL,n: skip next instruction
R_CHDIR:	LD	C,5AH
	IFNDEF HYBRID
		CALL	RFS_CHKDEV
		OR	A
		JP	NZ,RFS_DEVERR
	ENDIF
		PUSH	DE
		LD	A,C
		CALL	RFS_CMD
		POP	DE
		CALL	RFS_TXPATH
		JP	RFS_RXERR

; ---------------------------------------------------------
; Function $4E _RENAME
; Function $4F _MOVE
; Input:  DE = pointer to ASCIIZ path or FIB
;         HL = pointer to new name / new path
; ---------------------------------------------------------
R_RENAME:	LD	C,4EH
		DEFB	0DDH,21H		; LD IX,n: skip next instruction
R_MOVE:		LD	C,4FH
	IFNDEF HYBRID
		CALL	RFS_CHKDEV
		OR	A
		JP	NZ,RFS_DEVERR
	ENDIF
		PUSH	HL
		PUSH	DE
		LD	A,C
		CALL	RFS_CMD
		POP	DE
		CALL	RFS_TXPATH
		POP	HL
		CALL	RFS_TXSTR
		JP	RFS_RXERR

; ---------------------------------------------------------
; Function $50 _ATTR
; Input:  DE = pointer to ASCIIZ path or FIB
;         A  = 0 get, 1 set
;         L  = new attributes
; Output: L  = attributes
; ---------------------------------------------------------
R_ATTR:		LD	(RFS_BUF),A
		LD	A,L
		LD	(RFS_BUF+1),A
		LD	C,50H
		LD	HL,2
		CALL	RFS_PATHBUF
J_AT1:		LD	A,(RFS_BUF+1)
		LD	L,A
		LD	A,(RFS_BUF)
		RET

; ---------------------------------------------------------
; Function $51 _FTIME
; Input:  DE = pointer to ASCIIZ path or FIB
;         A  = 0 get, 1 set
;         IX = new time
;         HL = new date
; Output: DE = time
;         HL = date
; ---------------------------------------------------------
R_FTIME:	LD	(RFS_BUF),A
		LD	(RFS_BUF+3),HL
		PUSH	IX
		POP	HL
		LD	(RFS_BUF+1),HL
		LD	C,51H
		LD	HL,5
		CALL	RFS_PATHBUF
J_FT1:		LD	DE,(RFS_BUF+1)
		LD	HL,(RFS_BUF+3)
		LD	A,(RFS_BUF)
		RET

; Subroutine send path and RFS_BUF parameters, receive result in RFS_BUF
; Input:  DE = pointer to ASCIIZ path or FIB
;         C  = function
;         HL = size of parameters and result
; Output: RFS_BUF = result (first byte = error)
RFS_PATHBUF:
	IFNDEF HYBRID
		CALL	RFS_CHKDEV
		OR	A
		JR	Z,J_PB1
		CALL	RFS_DEVERR
		LD	(RFS_BUF),A
		RET
	ENDIF

J_PB1:		PUSH	HL
		PUSH	DE
		LD	A,C
		CALL	RFS_CMD
		POP	DE
		CALL	RFS_TXPATH
		POP	BC
		PUSH	BC
		LD	HL,RFS_BUF
		CALL	RFS_TX
		POP	BC
		JP	RFS_RXBUF

; ---------------------------------------------------------
; Function $52 _HDELETE
; Input:  B = file handle
; ---------------------------------------------------------
R_HDELETE:	CALL	RFS_GETRH
	IFNDEF HYBRID
		JR	NZ,J_HD1
	ENDIF
		SET	3,(IX+49)		; handle is dead after delete
		LD	C,52H
		CALL	RFS_CMD1
		JP	RFS_RXERR

	IFNDEF HYBRID
J_HD1:		OR	A
		RET	NZ
		LD	A,_IDEV
		RET
	ENDIF

; ---------------------------------------------------------
; Function $53 _HRENAME
; Function $54 _HMOVE
; Input:  B  = file handle
;         HL = pointer to new name / new path
; ---------------------------------------------------------
R_HRENAME:	LD	C,53H
		DEFB	0DDH,21H		; LD IX,n: skip next instruction
R_HMOVE:	LD	C,54H
		PUSH	HL
		CALL	RFS_GETRH
		POP	HL
	IFNDEF HYBRID
		JR	NZ,J_HD1
	ENDIF
		PUSH	HL
		CALL	RFS_CMD1
		POP	HL
		CALL	RFS_TXSTR
		JP	RFS_RXERR

; ---------------------------------------------------------
; Function $55 _HATTR
; Input:  B = file handle
;         A = 0 get, 1 set
;         L = new attributes
; Output: L = attributes
; ---------------------------------------------------------
R_HATTR:	LD	(RFS_BUF+1),A
		LD	A,L
		LD	(RFS_BUF+2),A
		LD	C,55H
		LD	HL,3
		CALL	RFS_HBUF
	IFDEF HYBRID
		JR	J_AT1
	ELSE
		JP	J_AT1			; out of JR range
	ENDIF

; ---------------------------------------------------------
; Function $56 _HFTIME
; Input:  B  = file handle
;         A  = 0 get, 1 set
;         IX = new time
;         HL = new date
; Output: DE = time
;         HL = date
; ---------------------------------------------------------
R_HFTIME:	LD	(RFS_BUF+1),A
		LD	(RFS_BUF+4),HL
		PUSH	IX
		POP	HL
		LD	(RFS_BUF+2),HL
		LD	C,56H
		LD	HL,6
		CALL	RFS_HBUF
	IFDEF HYBRID
		JR	J_FT1
	ELSE
		JP	J_FT1			; out of JR range
	ENDIF

; Subroutine send file handle and RFS_BUF parameters, receive result in RFS_BUF
; Input:  B  = file handle
;         C  = function
;         HL = size of parameters (including file handle)
; Output: RFS_BUF = result (first byte = error), zeros for devices
RFS_HBUF:	PUSH	HL
		CALL	RFS_GETRH
		POP	HL
	IFNDEF HYBRID
		JR	NZ,J_HB1
	ENDIF
		LD	(RFS_BUF),A
		LD	A,C
		LD	B,H
		LD	C,L
		PUSH	BC
		CALL	RFS_CMDBUF
		POP	BC
		DEC	BC
		JP	RFS_RXBUF

	IFNDEF HYBRID
J_HB1:		LD	HL,RFS_BUF
		LD	B,5
J_HB2:		LD	(HL),0
		INC	HL
		DJNZ	J_HB2
		LD	(RFS_BUF),A
		RET
	ENDIF

; ---------------------------------------------------------
; Function $40 _FFIRST
; Input:  DE = pointer to ASCIIZ path or FIB
;         HL = pointer to ASCIIZ file name (only if DE = FIB)
;         B  = search attributes
;         IX = pointer to new FIB
; ---------------------------------------------------------
R_FFIRST:	LD	C,40H
	IFNDEF HYBRID
		CALL	RFS_CHKDEV
		OR	A
		JR	Z,J_FF1
		CP	1
		RET	NZ
		JP	RFS_DEVFIB
	ENDIF

J_FF1:		CALL	RFS_FFCMD
		JR	RFS_RXFIB

; ---------------------------------------------------------
; Function $42 _FNEW
; Input:  DE = pointer to ASCIIZ path or FIB
;         HL = pointer to ASCIIZ file name (only if DE = FIB)
;         B  = create attributes
;         IX = pointer to template FIB and new FIB
; ---------------------------------------------------------
R_FNEW:		LD	C,42H
	IFNDEF HYBRID
		CALL	RFS_CHKDEV
		OR	A
		JP	NZ,RFS_DEVERR
	ENDIF
		CALL	RFS_FFCMD
		PUSH	IX
		POP	HL
		INC	HL
		LD	BC,13
		CALL	RFS_TX
		JR	RFS_RXFIB

; Subroutine start find command: send path or FIB (+ file name) and attributes
; Input:  C = function
RFS_FFCMD:	LD	A,B
		LD	(RFS_TXB),A
		PUSH	HL
		PUSH	DE
		LD	A,C
		CALL	RFS_CMD
		POP	DE
		PUSH	DE
		CALL	RFS_TXPATH
		POP	DE
		POP	HL
		LD	A,(DE)
		INC	A
		CALL	Z,RFS_TXSTR
		LD	HL,RFS_TXB
		LD	BC,1
		JP	RFS_TX

; ---------------------------------------------------------
; Function $41 _FNEXT
; Input:  IX = pointer to FIB
; ---------------------------------------------------------
R_FNEXT:
	IFNDEF HYBRID
		BIT	7,(IX+30)
		LD	A,_NOFIL
		RET	NZ
	ENDIF
		LD	A,41H
		CALL	RFS_CMD
		PUSH	IX
		POP	HL
		LD	BC,RFS_FIBSZ
		CALL	RFS_TX

; Subroutine receive FIB and end command
; Input:  IX = pointer to FIB
RFS_RXFIB:	PUSH	IX
		POP	HL
		LD	BC,RFS_FIBSZ
		CALL	RFS_RX
		CALL	RFS_END
	IFDEF HYBRID
		LD	A,1
		LD	(RFS_WPJIO),A
	ENDIF
		LD	A,(IX+RFS_FIBRES)
		OR	A
		RET

	IFNDEF HYBRID
; Subroutine setup FIB for device
; Input:  IX = pointer to FIB, I_B9DA and I_B926 = parsed device
RFS_DEVFIB:	LD	A,(DE)
		INC	A
		LD	A,_IDEV
		RET	Z			; FIB of device given
		LD	(IX+0),0FFH
		PUSH	IX
		POP	HL
		INC	HL
		LD	DE,I_B926
		LD	A,(DE)
		PUSH	IX
		LD	IX,I_B9DA
		CALL	C173A
		POP	IX
		PUSH	IX
		POP	HL
		LD	DE,14
		ADD	HL,DE
		LD	(HL),80H		; attribute = device
		LD	B,10
J_DF1:		INC	HL
		LD	(HL),0
		DJNZ	J_DF1
		INC	HL
		EX	DE,HL
		LD	HL,I_B9DA+25		; drive and device info
		LD	BC,7
		LDIR
		XOR	A
		RET

	ENDIF
; ---------------------------------------------------------
; Function $59 _GETCD
; Input:  B  = drive number (0 = current, 1 = A: etc)
;         DE = pointer to 64 byte buffer
; ---------------------------------------------------------
R_GETCD:	LD	A,B
	IFDEF HYBRID
		OR	A
		JR	NZ,J_GC1
		LD	A,(CUR_DRV)		; current drive
J_GC1:
	ENDIF
		PUSH	DE
		LD	C,59H
		CALL	RFS_CMD1
		LD	HL,RFS_BUF
		LD	BC,1
		CALL	RFS_RX
		POP	HL
		PUSH	HL
		LD	A,(RFS_BUF)
		LD	C,A
		LD	B,0
		CALL	RFS_RX
		CALL	RFS_END
		POP	DE
		XOR	A
		RET

; ---------------------------------------------------------
; Function $5E _WPATH
; Input:  DE = pointer to 64 byte buffer
; Output: HL = pointer to start of last item
; ---------------------------------------------------------
R_WPATH:	PUSH	DE
		LD	A,5EH
		CALL	RFS_CMD
		LD	HL,RFS_BUF
		LD	BC,3
		CALL	RFS_RX
		POP	HL
		PUSH	HL
		LD	A,(RFS_BUF+2)
		LD	C,A
		LD	B,0
		CALL	RFS_RX
		CALL	RFS_END
		POP	DE
		LD	A,(RFS_BUF+1)
		LD	L,A
		LD	H,0
		ADD	HL,DE
		LD	A,(RFS_BUF)
		RET

; ---------------------------------------------------------
; *** Functions: 0F-17,21-24,26-28 ***
; FCB functions, implemented with the remote file handle functions
; FCB+24 = remote file handle
; ---------------------------------------------------------

; ---------------------------------------------------------
; Function $0F _FOPEN
; Function $16 _FMAKE
; Input:  DE = pointer to FCB
; ---------------------------------------------------------
	IFDEF HYBRID
R_FOPEN:	PUSH	DE
		CALL	RFS_FCBPATH
		EX	DE,HL			; DE = path
		XOR	A			; open mode = read and write
		PUSH	DE
		CALL	X_OPEN
		POP	DE
		OR	A
		JR	Z,J_FO3
		LD	A,1			; read only file: open for read only
		CALL	X_OPEN
		JR	J_FO3

R_FMAKE:	PUSH	DE
		CALL	RFS_FCBPATH
		EX	DE,HL			; DE = path
		XOR	A			; open mode
		LD	B,A			; attributes
		CALL	X_CREATE
J_FO3:		POP	IX
		OR	A
		JR	NZ,RFS_FERR
		PUSH	IX
		PUSH	BC
		CALL	C2136			; IX = pointer to FAB
		SET	5,(IX+49)		; b5 = FCB file: not locked (see C22D5)
		POP	BC
		POP	IX
		INC	B
		LD	(IX+24),B		; file handle + 1
		DEC	B
		XOR	A
		LD	(IX+13),A		; attributes
		LD	(IX+14),A		; extent high byte
		LD	H,A			; get file size
		LD	L,A
		LD	D,A
		LD	E,A
		LD	A,2
		PUSH	IX
		CALL	X_SEEK
		POP	IX
	ELSE
R_FOPEN:	LD	A,43H
		DEFB	21H			; LD HL,n: skip next instruction
R_FMAKE:	LD	A,44H
		PUSH	DE
		PUSH	AF
		CALL	RFS_FCBPATH
		POP	AF
		PUSH	AF
		PUSH	HL
		CALL	RFS_CMD
		POP	HL
		CALL	RFS_TXSTR
		XOR	A
		LD	(RFS_BUF),A		; open mode
		LD	(RFS_BUF+1),A		; attributes
		POP	AF
		SUB	42H			; 1 = open, 2 = create
		LD	C,A
		LD	B,0
		LD	HL,RFS_BUF
		CALL	RFS_TX
		LD	BC,2
		CALL	RFS_RXBUF
		POP	IX
		OR	A
		JR	NZ,RFS_FERR
		LD	A,(RFS_BUF+1)
		LD	C,A
		LD	(IX+24),A		; remote file handle
		XOR	A
		LD	(IX+13),A		; attributes
		LD	(IX+14),A		; extent high byte
		LD	H,A			; get file size
		LD	L,A
		LD	D,A
		LD	E,A
		LD	A,2
		CALL	RFS_RSEEK
	ENDIF
		LD	(IX+16),L
		LD	(IX+17),H
		LD	(IX+18),E
		LD	(IX+19),D
		LD	BC,127			; record count in extent = (size + 127) / 128 - extent * 128 (0..128)
		ADD	HL,BC
		JR	NC,J_FO1
		INC	DE
J_FO1:		ADD	HL,HL
		EX	DE,HL
		ADC	HL,HL			; records = D (low), L, H
		LD	A,H
		OR	A
		LD	A,128
		JR	NZ,J_FO2
		LD	H,L
		LD	L,D
		LD	A,(IX+12)
		LD	D,A
		SRL	D
		LD	A,0
		RRA
		LD	E,A			; DE = extent * 128
		OR	A
		SBC	HL,DE
		LD	A,0
		JR	C,J_FO2
		LD	A,H
		OR	A
		LD	A,128
		JR	NZ,J_FO2
		LD	A,L
		CP	129
		JR	C,J_FO2
		LD	A,128
J_FO2:		LD	(IX+15),A

RFS_FOK:	XOR	A
		LD	H,A
		LD	L,A
		RET

RFS_FERR:	OR	A
		JR	NZ,J_FE1
		LD	A,_NOFIL
J_FE1:		LD	HL,00FFH
		RET

; ---------------------------------------------------------
; Function $10 _FCLOSE
; Input:  DE = pointer to FCB
; ---------------------------------------------------------
R_FCLOSE:	PUSH	DE
		POP	IX
		LD	A,(IX+24)
		OR	A
		JR	Z,RFS_FOK
		LD	(IX+24),0
	IFDEF HYBRID
		DEC	A			; file handle
		LD	B,A
		CALL	F_CLOSE
	ELSE
		LD	C,45H
		CALL	RFS_CMD1
		CALL	RFS_RXERR
	ENDIF
		OR	A
		JR	NZ,RFS_FERR
		JR	RFS_FOK

; ---------------------------------------------------------
; Function $11 _SFIRST
; Function $12 _SNEXT
; Input:  DE = pointer to FCB (_SFIRST)
; Output: DTA = unopened FCB of the entry found
; ---------------------------------------------------------
R_SFIRST:	PUSH	DE
		POP	IX
		CALL	RFS_FCBFIND
		JR	J_SF1

R_SNEXT:	LD	A,(RFS_FIB)
		INC	A
		JR	NZ,RFS_FERR
		LD	IX,RFS_FIB
	IFDEF HYBRID
		CALL	X_FNEXT
		OR	A
	ELSE
		CALL	R_FNEXT
	ENDIF
J_SF1:		JR	NZ,RFS_FERR
		LD	HL,RFS_DPB		; build unopened FCB (33 bytes)
		LD	B,33
J_SF2:		LD	(HL),0
		INC	HL
		DJNZ	J_SF2
		LD	A,(RFS_FIB+25)
		LD	(RFS_DPB),A		; drive
		LD	HL,RFS_FIB+1
		LD	DE,RFS_DPB+1
		CALL	RFS_TO11
		LD	A,(RFS_FIB+14)
		LD	(RFS_DPB+13),A		; attributes
		LD	HL,RFS_FIB+15		; time, date
		LD	DE,RFS_DPB+23
		LD	BC,4
		LDIR
		LD	HL,RFS_FIB+21		; size
		LD	DE,RFS_DPB+29
		LD	C,4
		LDIR
		LD	HL,RFS_DPB		; copy to DTA
		LD	DE,(DTA_AD)
		LD	B,33
		CALL	RFS_TODTA
		JR	RFS_FOK

; Subroutine find first entry of FCB, result in RFS_FIB
; Input:  IX = pointer to FCB
RFS_FCBFIND:	PUSH	IX
		POP	DE
		CALL	RFS_FCBPATH
		EX	DE,HL
		LD	B,0
	IFNDEF HYBRID
		LD	C,40H
	ENDIF
		PUSH	IX
		LD	IX,RFS_FIB
	IFDEF HYBRID
		CALL	X_FFIRST
		OR	A
	ELSE
		CALL	RFS_FFCMD
		CALL	RFS_RXFIB
	ENDIF
		POP	IX
		RET

; Subroutine copy ASCIIZ file name to 11 character FCB name
; Input:  HL = pointer to ASCIIZ file name
;         DE = pointer to FCB name
RFS_TO11:	PUSH	DE
		LD	B,11
J_TE1:		LD	A,' '
		LD	(DE),A
		INC	DE
		DJNZ	J_TE1
		POP	DE
		PUSH	DE
		LD	B,8
J_TE2:		LD	A,(HL)
		OR	A
		JR	Z,J_TE6
		INC	HL
		CP	'.'
		JR	Z,J_TE3
		INC	B
		DEC	B
		JR	Z,J_TE2			; name too long: skip character
		LD	(DE),A
		INC	DE
		DEC	B
		JR	J_TE2

J_TE3:		POP	DE			; extension
		PUSH	HL
		LD	HL,8
		ADD	HL,DE
		EX	DE,HL
		POP	HL
		LD	B,3
J_TE5:		LD	A,(HL)
		OR	A
		RET	Z
		INC	HL
		LD	(DE),A
		INC	DE
		DJNZ	J_TE5
		RET

J_TE6:		POP	DE
		RET

; ---------------------------------------------------------
; Function $13 _FDEL
; Input:  DE = pointer to FCB (wildcards allowed)
; ---------------------------------------------------------
R_FDEL:		PUSH	DE
		POP	IX
		LD	C,0FFH			; no file deleted yet
J_FD1:		PUSH	BC
		CALL	RFS_FCBFIND
		POP	BC
		JR	NZ,J_FD2
		PUSH	BC
		LD	DE,RFS_FIB
	IFDEF HYBRID
		CALL	X_DELETE
	ELSE
		CALL	R_DELETE
	ENDIF
		POP	BC
		OR	A
		JR	NZ,J_FD2
		LD	C,A
		JR	J_FD1

J_FD2:		LD	A,C
		OR	A
		JP	NZ,RFS_FERR
		JP	RFS_FOK

; ---------------------------------------------------------
; Function $17 _FREN
; Input:  DE = pointer to FCB with old name at +1, new name at +17
; ---------------------------------------------------------
R_FREN:		PUSH	DE
		LD	HL,16			; new name as path "D:NAME.EXT"
		ADD	HL,DE
		EX	DE,HL
		CALL	RFS_FCBPATH
		LD	DE,RFS_FIB
		LD	BC,16
		LDIR
		POP	DE
		CALL	RFS_FCBPATH		; old name
		EX	DE,HL
		LD	HL,RFS_FIB+2		; new name without drive
	IFDEF HYBRID
		CALL	X_RENAME
	ELSE
		CALL	R_RENAME
	ENDIF
		OR	A
		JP	NZ,RFS_FERR
		JP	RFS_FOK

; ---------------------------------------------------------
; Function $14 _RDSEQ
; Function $15 _WRSEQ
; Input:  DE = pointer to FCB
; ---------------------------------------------------------
R_RDSEQ:	LD	A,1
		DEFB	0FEH			; CP n: skip next instruction
R_WRSEQ:	XOR	A
		PUSH	DE
		POP	IX
		PUSH	AF
		LD	A,(IX+32)		; offset = ((extent) * 128 + current record) * 128
		LD	C,A
		RRCA
		AND	80H
		LD	(RFS_OFS),A
		LD	A,C
		SRL	A
		LD	C,A
		LD	L,(IX+12)
		LD	H,(IX+14)
		LD	A,L
		RRCA
		RRCA
		AND	0C0H
		OR	C
		LD	(RFS_OFS+1),A
		SRL	H
		RR	L
		SRL	H
		RR	L
		LD	(RFS_OFS+2),HL
		POP	AF
		CALL	RFS_REC
		RET	NZ
		INC	(IX+32)			; next record
		BIT	7,(IX+32)
		RET	Z
		LD	(IX+32),A
		INC	(IX+12)
		RET	NZ
		INC	(IX+14)
		RET

; Subroutine read or write one 128 bytes record at RFS_OFS
; Input:  IX = pointer to FCB
;         A  = 1 read, 0 write
; Output: A = L = 0 ok, 1 end of file / disk full
;         Zx = set if ok
RFS_REC:	LD	BC,128
		CALL	RFS_FCBRW
		JR	NZ,J_RR2
		LD	A,B
		OR	C
		JR	Z,J_RR2
		LD	A,(RFS_OP)
		RRCA
		JR	NC,J_RR1
		LD	HL,(DTA_AD)		; read: fill rest of record with zeros
		ADD	HL,BC
		EX	DE,HL
		LD	HL,128
		OR	A
		SBC	HL,BC
		JR	Z,J_RR1
		LD	B,L
		LD	HL,RFS_DPB+35		; zero byte
		LD	(HL),0
J_RR3:		PUSH	BC
		PUSH	DE
		PUSH	HL
		LD	B,1
		CALL	RFS_TODTA
		POP	HL
		POP	DE
		POP	BC
		INC	DE
		DJNZ	J_RR3
J_RR1:		XOR	A
		LD	H,A
		LD	L,A
		RET

J_RR2:		LD	HL,1
		LD	A,L
		OR	A
		RET

; ---------------------------------------------------------
; Function $21 _RDRND
; Function $22 _WRRND
; Function $28 _WRZER
; Input:  DE = pointer to FCB
; ---------------------------------------------------------
R_RDRND:	LD	A,1
		DEFB	0FEH			; CP n: skip next instruction
R_WRRND:
R_WRZER:	XOR	A
		PUSH	DE
		POP	IX
		PUSH	AF
		LD	A,(IX+33)		; current record and extent from random record
		LD	L,(IX+34)
		LD	H,(IX+35)
		LD	C,A
		AND	7FH
		LD	(IX+32),A
		LD	A,C
		RLA
		ADC	HL,HL
		LD	(IX+12),L
		LD	(IX+14),H
		XOR	A			; offset = random record * 128
		LD	(RFS_OFS),A
		LD	HL,RFS_OFS+1
		LD	A,(IX+33)
		LD	(HL),A
		INC	HL
		LD	A,(IX+34)
		LD	(HL),A
		INC	HL
		LD	A,(IX+35)
		LD	(HL),A
		SRL	(HL)
		DEC	HL
		RR	(HL)
		DEC	HL
		RR	(HL)
		DEC	HL
		RR	(HL)
		POP	AF
		JP	RFS_REC

; ---------------------------------------------------------
; Function $23 _FSIZE
; Input:  DE = pointer to FCB
; Output: FCB random record = file size in 128 bytes records
; ---------------------------------------------------------
R_FSIZE:	PUSH	DE
		POP	IX
		CALL	RFS_FCBFIND
		JP	NZ,RFS_FERR
		LD	HL,(RFS_FIB+21)
		LD	DE,(RFS_FIB+23)
		LD	BC,127
		ADD	HL,BC
		JR	NC,J_FS1
		INC	DE
J_FS1:		ADD	HL,HL
		EX	DE,HL
		ADC	HL,HL
		LD	(IX+33),D
		LD	(IX+34),L
		LD	(IX+35),H
		JP	RFS_FOK

; ---------------------------------------------------------
; Function $24 _SETRND
; Input:  DE = pointer to FCB
; ---------------------------------------------------------
R_SETRND:	PUSH	DE
		POP	IX
		LD	A,(IX+32)
		LD	C,(IX+12)
		LD	B,(IX+14)
		ADD	A,A
		SRL	B
		RR	C
		RRA
		LD	(IX+33),A
		LD	(IX+34),C
		LD	(IX+35),B
		JP	RFS_FOK

; ---------------------------------------------------------
; Function $27 _RDBLK
; Function $26 _WRBLK
; Input:  DE = pointer to FCB
;         HL = number of records
; Output: HL = number of records transferred
; ---------------------------------------------------------
R_RDBLK:	LD	A,1
		DEFB	0FEH			; CP n: skip next instruction
R_WRBLK:	XOR	A
		PUSH	DE
		POP	IX
		LD	(RFS_REQ),HL
		PUSH	AF
		LD	C,(IX+14)		; record size
		LD	B,(IX+15)
		LD	A,B
		OR	C
		JR	NZ,J_BK1
		LD	C,128
J_BK1:		PUSH	BC
		LD	(RFS_MA),HL		; bytes = records * record size
		LD	HL,0
		LD	(RFS_MA+2),HL
		CALL	RFS_MUL
		LD	HL,(RFS_OFS+2)
		LD	A,H
		OR	L
		JR	NZ,J_BK9		; more than 64K
		LD	HL,(RFS_OFS)
		PUSH	HL
		LD	HL,IX_33		; offset = random record * record size
		PUSH	IX
		POP	DE
		ADD	HL,DE
		LD	DE,RFS_MA
		LD	BC,4
		LDIR
		POP	HL
		POP	BC
		PUSH	BC
		PUSH	HL
		LD	HL,63
		OR	A
		SBC	HL,BC
		JR	NC,J_BK2
		XOR	A
		LD	(RFS_MA+3),A		; record size >= 64: 3 bytes random record
J_BK2:		CALL	RFS_MUL
		POP	BC			; bytes
		POP	DE			; record size
		POP	AF
		PUSH	DE
		CALL	RFS_FCBRW
		POP	DE
		PUSH	AF
		LD	H,B			; records = (bytes + record size - 1) / record size
		LD	L,C
		ADD	HL,DE
		DEC	HL
		CALL	RFS_DIV
		PUSH	HL
		LD	C,(IX+33)		; update random record
		LD	B,(IX+34)
		ADD	HL,BC
		LD	(IX+33),L
		LD	(IX+34),H
		JR	NC,J_BK3
		INC	(IX+35)
		JR	NZ,J_BK3
		INC	(IX+36)
J_BK3:		POP	HL
		POP	AF
		JR	NZ,J_BK4
		LD	DE,(RFS_REQ)
		PUSH	HL
		OR	A
		SBC	HL,DE
		POP	HL
		RET	Z
J_BK4:		LD	A,1
		RET

J_BK9:		POP	BC
		POP	AF
		LD	HL,0
		LD	A,1
		RET

IX_33		EQU	33

; ---------------------------------------------------------
; Subroutine read or write FCB data at file offset
; Input:  IX     = pointer to FCB
;         RFS_OFS = file offset
;         BC     = size
;         A      = 1 read, 0 write
; Output: A      = error, Zx set if ok
;         BC     = bytes transferred
; ---------------------------------------------------------
RFS_FCBRW:	LD	(RFS_OP),A
	IFDEF HYBRID
		PUSH	IX
		PUSH	BC
		LD	A,(IX+24)
		DEC	A
		LD	B,A			; file handle
		PUSH	BC
		XOR	A
		LD	HL,(RFS_OFS)
		LD	DE,(RFS_OFS+2)
		CALL	X_SEEK
		POP	BC			; B = file handle
		POP	HL			; HL = size
		OR	A
		JR	Z,J_GW0
		LD	BC,0
		JR	J_GW3

J_GW0:		LD	DE,(DTA_AD)
		LD	A,(RFS_OP)
		RRCA
		JR	NC,J_GW1
		CALL	F_READ
		JR	J_GW2

J_GW1:		CALL	F_WRITE
J_GW2:		LD	B,H
		LD	C,L
J_GW3:		POP	IX
		OR	A
		RET	NZ
	ELSE
		LD	A,(IX+24)
		LD	(RFS_RH),A
		PUSH	BC
		LD	C,A
		XOR	A
		LD	HL,(RFS_OFS)
		LD	DE,(RFS_OFS+2)
		CALL	RFS_RSEEK
		POP	BC
		OR	A
		RET	NZ
		LD	A,(RFS_OP)
		LD	DE,(DTA_AD)
		CALL	RFS_RW
		OR	A
		RET	NZ
	ENDIF
		LD	A,(RFS_OP)		; write: update file size
		RRCA
		JR	C,J_FW2
		LD	HL,(RFS_OFS)		; end = offset + bytes
		ADD	HL,BC
		EX	DE,HL
		LD	HL,(RFS_OFS+2)
		JR	NC,J_FW1
		INC	HL
J_FW1:		PUSH	BC
		LD	C,(IX+18)		; end > size?
		LD	B,(IX+19)
		OR	A
		SBC	HL,BC
		ADD	HL,BC
		JR	C,J_FW3
		JR	NZ,J_FW4
		EX	DE,HL
		LD	C,(IX+16)
		LD	B,(IX+17)
		SBC	HL,BC
		ADD	HL,BC
		EX	DE,HL
		JR	C,J_FW3
J_FW4:		LD	(IX+16),E
		LD	(IX+17),D
		LD	(IX+18),L
		LD	(IX+19),H
J_FW3:		POP	BC
J_FW2:		XOR	A
		RET

; Subroutine RFS_OFS = RFS_MA * BC (32 bits)
RFS_MUL:	LD	HL,0
		LD	(RFS_OFS),HL
		LD	(RFS_OFS+2),HL
		LD	A,16
J_MU1:		LD	HL,(RFS_OFS)		; RFS_OFS <<= 1
		ADD	HL,HL
		LD	(RFS_OFS),HL
		LD	HL,(RFS_OFS+2)
		ADC	HL,HL
		LD	(RFS_OFS+2),HL
		SLA	C
		RL	B
		JR	NC,J_MU2
		LD	HL,(RFS_OFS)		; RFS_OFS += RFS_MA
		LD	DE,(RFS_MA)
		ADD	HL,DE
		LD	(RFS_OFS),HL
		LD	HL,(RFS_OFS+2)
		LD	DE,(RFS_MA+2)
		ADC	HL,DE
		LD	(RFS_OFS+2),HL
J_MU2:		DEC	A
		JR	NZ,J_MU1
		RET

; Subroutine HL = HL / DE
RFS_DIV:	LD	B,H
		LD	C,L
		LD	HL,0
		LD	A,16
J_DV1:		SLA	C
		RL	B
		ADC	HL,HL
		SBC	HL,DE
		JR	NC,J_DV2
		ADD	HL,DE
		JR	J_DV3
J_DV2:		INC	C
J_DV3:		DEC	A
		JR	NZ,J_DV1
		LD	H,B
		LD	L,C
		RET

; Subroutine build path from FCB
; Input:  DE = pointer to FCB
; Output: HL = pointer to ASCIIZ path "D:NAME.EXT" (all "?" name or extension becomes "*")
RFS_FCBPATH:	PUSH	DE
		LD	HL,RFS_PBUF
		LD	A,(DE)
		OR	A
		JR	NZ,J_FP1
		LD	A,(CUR_DRV)
J_FP1:		ADD	A,'A'-1
		LD	(HL),A
		INC	HL
		LD	(HL),':'
		INC	HL
		INC	DE
		LD	B,8
		CALL	J_FP3
		LD	(HL),'.'
		INC	HL
		LD	B,3
		CALL	J_FP3
		DEC	HL
		LD	A,(HL)
		CP	'.'
		JR	Z,J_FP2			; no extension
		INC	HL
J_FP2:		LD	(HL),0
		POP	DE
		LD	HL,RFS_PBUF
		RET

J_FP3:		PUSH	DE
		PUSH	BC
J_FP4:		LD	A,(DE)
		CP	'?'
		JR	NZ,J_FP5
		INC	DE
		DJNZ	J_FP4
		POP	BC			; all "?"
		POP	DE
		LD	(HL),'*'
		INC	HL
		JR	J_FP7

J_FP5:		POP	BC
		POP	DE
		PUSH	DE
		PUSH	BC
J_FP6:		LD	A,(DE)
		CP	' '
		JR	Z,J_FP8
		LD	(HL),A
		INC	HL
		INC	DE
		DJNZ	J_FP6
J_FP8:		POP	BC
		POP	DE
J_FP7:		EX	DE,HL
		LD	C,B
		LD	B,0
		ADD	HL,BC
		EX	DE,HL
		RET

; Subroutine copy data to DTA (TPA segments)
; Input:  HL = source
;         DE = destination (TPA address)
;         B  = size
RFS_TODTA:	PUSH	BC
		PUSH	DE
		PUSH	HL
		XOR	A
		CALL	C2731			; A = segment, DE = page 0 based address
		LD	C,A
		POP	HL
		LD	A,(HL)
		INC	HL
		PUSH	HL
		EX	DE,HL
		LD	E,A
		LD	A,C
		CALL	WR_SEG
		EI
		POP	HL
		POP	DE
		POP	BC
		INC	DE
		DJNZ	RFS_TODTA
		RET

	IFDEF HYBRID
; ---------------------------------------------------------
; *** Routing of the functions to the JIO or local implementation ***
; The CALL of a router is followed by the address of the JIO implementation
; and the address of the local implementation. All registers are kept.
; ---------------------------------------------------------

; Subroutine route on the drive of a path or FIB
; Input:  DE = pointer to ASCIIZ path or FIB
RFS_ROUTE:	EX	(SP),HL
		PUSH	DE
		PUSH	BC
		PUSH	AF
		PUSH	HL
		PUSH	IX
		CALL	RFS_ISJIOP
J_RO1:		POP	IX
J_RO2:		POP	HL
		JR	C,J_RO3
		INC	HL
		INC	HL
J_RO3:		LD	A,(HL)
		INC	HL
		LD	H,(HL)
		LD	L,A
		POP	AF
		POP	BC
		POP	DE
		EX	(SP),HL
		RET

; Subroutine route on the drive of the FIB in IX
RFS_ROUTEF:	EX	(SP),HL
		PUSH	DE
		PUSH	BC
		PUSH	AF
		PUSH	HL
		PUSH	IX
		POP	DE
		PUSH	IX
		CALL	RFS_ISJIOP
		JR	J_RO1

; Subroutine route on a drive number
; Input:  A = drive number (0 = current, 1 = A: etc)
RFS_ROUTED:	EX	(SP),HL
		PUSH	DE
		PUSH	BC
		PUSH	AF
		PUSH	HL
		CALL	RFS_ISJIOD
		JR	J_RO2

; Subroutine route on a file handle (remote file)
; Input:  B = file handle
RFS_ROUTEH:	EX	(SP),HL
		PUSH	DE
		PUSH	BC
		PUSH	AF
		PUSH	HL
		PUSH	IX
		CALL	C2136
		JR	NC,J_RH1		; invalid file handle: local
		JR	Z,J_RH1			; not opened: local
		BIT	6,(IX+49)
		JR	Z,J_RH1
		SCF				; remote file
		JR	J_RO1

J_RH1:		OR	A
		JR	J_RO1

; Subroutine is path or FIB on a JIO drive ?
; Input:  DE = pointer to ASCIIZ path or FIB
; Output: Cx = set if JIO drive (devices are local)
RFS_ISJIOP:	LD	A,(DE)
		INC	A
		JR	NZ,J_IJ1
		LD	HL,30			; FIB
		ADD	HL,DE
		OR	A
		BIT	7,(HL)			; device ?
		RET	NZ
		LD	HL,25
		ADD	HL,DE
		LD	A,(HL)			; drive
		JR	RFS_ISJIOD

J_IJ1:		LD	IX,I_B9DA		; ASCIIZ path: parse only, no disk access
		XOR	A
		LD	B,A
		LD	C,4
		CALL	C12C3
		JR	Z,J_IJ2
		OR	A			; invalid path: local (reports the error)
		RET

J_IJ2:		BIT	7,(IX+30)		; device ?
		RET	NZ
		LD	A,(IX+25)		; drive

; Subroutine is drive a JIO drive ?
; Input:  A  = drive number (0 = current, 1 = A: etc)
; Output: Cx = set if JIO drive
RFS_ISJIOD:	CALL	C3606			; A = physical drive (1 = first), HL = DPB entry
		CP	8
		JR	Z,J_IJ3			; H: RAM disk of the server when it has a DPB entry
		DEC	A
		LD	HL,RFS_NJIO
		CP	(HL)
		RET

J_IJ3:		LD	A,(HL)
		INC	HL
		OR	(HL)
		RET	Z
		SCF
		RET

; ---------------------------------------------------------
; Routed functions
; ---------------------------------------------------------
X_FFIRST:	CALL	RFS_ROUTE
		DEFW	R_FFIRST,L_FFIRST
X_FNEXT:	CALL	RFS_ROUTEF
		DEFW	R_FNEXT,L_FNEXT
X_FNEW:		CALL	RFS_ROUTE
		DEFW	R_FNEW,L_FNEW
X_OPEN:		CALL	RFS_ROUTE
		DEFW	R_OPEN,F_OPEN
X_CREATE:	CALL	RFS_ROUTE
		DEFW	R_CREATE,F_CREATE
X_SEEK:		CALL	RFS_ROUTEH
		DEFW	R_SEEK,F_SEEK
X_HTEST:	CALL	RFS_ROUTE
		DEFW	R_HTEST,F_HTEST
X_DELETE:	CALL	RFS_ROUTE
		DEFW	R_DELETE,F_DELETE
X_RENAME:	CALL	RFS_ROUTE
		DEFW	R_RENAME,F_RENAME
X_MOVE:		CALL	RFS_ROUTE
		DEFW	R_MOVE,F_MOVE
X_ATTR:		CALL	RFS_ROUTE
		DEFW	R_ATTR,F_ATTR
X_FTIME:	CALL	RFS_ROUTE
		DEFW	R_FTIME,F_FTIME
X_HDELETE:	CALL	RFS_ROUTEH
		DEFW	R_HDELETE,F_HDELETE
X_HRENAME:	CALL	RFS_ROUTEH
		DEFW	R_HRENAME,F_HRENAME
X_HMOVE:	CALL	RFS_ROUTEH
		DEFW	R_HMOVE,F_HMOVE
X_HATTR:	CALL	RFS_ROUTEH
		DEFW	R_HATTR,F_HATTR
X_HFTIME:	CALL	RFS_ROUTEH
		DEFW	R_HFTIME,F_HFTIME
X_CHDIR:	CALL	RFS_ROUTE
		DEFW	R_CHDIR,F_CHDIR
X_GETCD:	LD	A,B
		CALL	RFS_ROUTED
		DEFW	R_GETCD,F_GETCD
X_ALLOC:	LD	A,E
		CALL	RFS_ROUTED
		DEFW	R_ALLOC,F_ALLOC
X_DPARM:	LD	A,L
		CALL	RFS_ROUTED
		DEFW	R_NODISK,F_DPARM
X_RDABS:	LD	A,L
		INC	A
		CALL	RFS_ROUTED
		DEFW	R_NODISK,F_RDABS
X_WRABS:	LD	A,L
		INC	A
		CALL	RFS_ROUTED
		DEFW	R_NODISK,F_WRABS

; Whole path of the last entry found
X_WPATH:	LD	A,(RFS_WPJIO)
		OR	A
		JP	NZ,R_WPATH
		JP	F_WPATH

; Local find functions: last entry found is local
L_FFIRST:	CALL	L_WPLOC
		JP	F_FFIRST
L_FNEXT:	CALL	L_WPLOC
		JP	F_FNEXT
L_FNEW:		CALL	L_WPLOC
		JP	F_FNEW
L_WPLOC:	PUSH	AF
		XOR	A
		LD	(RFS_WPJIO),A
		POP	AF
		RET

; ---------------------------------------------------------
; Function $2F _RDABS, $30 _WRABS, $31 _DPARM (JIO drive: no sectors)
; ---------------------------------------------------------
R_NODISK:	LD	A,_IDRV
		RET
	ENDIF
