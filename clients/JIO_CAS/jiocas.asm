; ------------------------------------------------------------------------------
; jiocas.asm
; Loader of JIO-ROM.CAS and JIO-CAS.CAS: starts MSX-DOS 2 with the JIO MSX-DOS 2 ROM on a MSX without MSX-DOS (no disk
; drive, or disk ROMs disabled with SHIFT at boot), from MSX BASIC: BLOAD"CAS:",R (with MSX-DOS 1: JIO-ROM.COM)
;
; JIO-ROM.CAS (0_Build.sh): this loader (BLOAD file JIOROM, at TAPE_ADR), then two data blocks read by the loader
; with the tape routines of the BIOS: the ROM and its kernel (16 KB each, compressed with BitBuster as in SofaCas),
; put in their segments (see common/ramrom_boot.asm).
; JIO-CAS.CAS (assembled with SERVER): this loader only (BLOAD file JIOCAS), the ROM and its kernel are read from the
; JIO server (COMMAND_BIN: BIN chosen on the server, JIO-CAS.BIN built in by default), much faster. Served whatever
; the server serves (disk image and/or directories).
; Needs a memory mapper of 128 KB (7 segments at least) in the RAM slot of pages 2 and 3.
; ------------------------------------------------------------------------------

CHGET		equ	009Fh
CHPUT		equ	00A2h
KILBUF		equ	0156h
TAPION		equ	00E1h
TAPIN		equ	00E4h
TAPIOF		equ	00E7h

TAPE_ADR	equ	0D000h		; loader (page 3): above the boot code, below the BASIC stack
	IFDEF SERVER
TRIES		equ	6		; time-outs of bJIOReceive (about 0.8 s each) for an answer of the server
COMMAND_BIN	equ	25		; JIO protocol (common/drv_jio.inc, msxjio_protocol_specification.md)
	ENDIF
PACK_END	equ	0C040h		; end of the compressed data read in a segment (in page 3: room for the decompression
					; in place, checked by 0_Build.sh)

		org	TAPE_ADR

		ld	de,MSG_TITLE
		call	PrintString

		; no MSX-DOS: H_PHYD patched by a disk ROM (the BIOS sets the hooks to RET)
		ld	a,(H_PHYD)
		cp	0C9h
		ld	de,MSG_DOS
		jp	nz,PrintString
		; what is found: RAM slots of pages 2 and 3, memory mapper (page 2) (shown before any error)
		ld	de,MSG_RAM
		call	PrintString
		ld	a,2
		call	PageSlot
		ld	(Slot2),a
		call	PrintSlot
		ld	a,' '
		call	PrintChar
		ld	a,3
		call	PageSlot
		ld	(Slot3),a
		call	PrintSlot
		ld	de,MSG_MAPSIZE
		call	PrintString
		call	MapperSegments
		ld	(Segments),a
		call	PrintDec
		ld	de,MSG_SEGMENTS
		call	PrintString

		; RAM slot of pages 2 and 3: slot of the ROM, memory mapper of MSX-DOS 2
		ld	a,(Slot2)
		ld	hl,Slot3
		cp	(hl)
		ld	de,MSG_SLOTS
		jp	nz,PrintString
		call	SetSlots		; the ROM only (no disk ROM)

		ld	a,(Segments)
		call	SetSegments
		ld	de,MSG_MAPPER
		jp	c,PrintString

	IFDEF SERVER
		; ROM and kernel read from the JIO server: tape player stopped first (cassette input read with the joystick
		; port by JIO)
		call	AskStop
		jp	ServerLoad
	ELSE
		ld	de,MSG_LOAD
		call	PrintString

		; ROM and kernel read from the tape to their segments
		ld	de,MSG_ROM
		call	PrintString
		ld	a,(BootRomSeg-BOOT_ADR+BootCode)
		call	TapeSegment
		jr	c,TapeError
		ld	de,MSG_OK
		call	PrintString
		ld	de,MSG_KERNEL
		call	PrintString
		ld	a,(KernelSeg)
		call	TapeSegment
		jr	c,TapeError
		ld	de,MSG_OK
		call	PrintString
		call	TAPIOF
		call	AskStop
		jp	StartBoot

TapeError:	push	de
		call	TAPIOF
		pop	de			; error message
		jr	PrintString
	ENDIF

; The cassette input, read with the joystick port by JIO (PSG register 14), gets noise from the tape player as long as
; its PLAY key is down (even with the motor stopped by the MSX): STOP pressed on the player, then a key
AskStop:	ld	de,MSG_STOP
		call	PrintString
		call	KILBUF
		jp	CHGET

PrintString:	ld	a,(de)
		cp	'$'
		ret	z
		call	CHPUT
		inc	de
		jr	PrintString

; Character A to the screen (registers kept)
PrintChar:	jp	CHPUT

	IFDEF SERVER
; ------------------------------------------------------------------------------
; ROM and kernel read from the JIO server (BIN: ROM, kernel, 16-bit sums of both, COMMAND_BIN), then boot.
; Serial lines tried as JIOTIME.COM: JIO cartridge (if its I/O register is found), joystick port 2, joystick port 1.
; ------------------------------------------------------------------------------
ServerLoad:	ld	de,MSG_READ
		call	PrintString
		ld	hl,Lines
		push	hl
		call	DetectPort
		pop	hl
		cp	0FFh
		jr	z,LinesJoy
		ld	(hl),a
		inc	hl
LinesJoy:	ld	(hl),0FFh
		inc	hl
		ld	(hl),0FEh
		inc	hl
		ld	de,Lines
		or	a
		sbc	hl,de
		ld	b,l			; number of lines
		ld	hl,8000h		; first request: the sums (end of the BIN), answer: error, size
		ld	(REQ_BIN+5),hl
		ld	hl,4
		ld	(REQ_BIN+7),hl
		ld	hl,Lines
		di
LineLoop:	ld	a,(hl)
		ld	(_JioPort),a
		push	bc
		push	hl
		ld	hl,REQ_BIN
		ld	de,REQ_BIN_END-REQ_BIN
		ld	bc,3
		ld	a,1			; one try on each line
		call	Request
		pop	hl
		pop	bc
		jr	nc,LineFound
		inc	hl
		djnz	LineLoop
		ld	de,MSG_NOANSWER
		jp	ServerError

LineFound:	ld	a,(Answer)
		or	a
		ld	de,MSG_NOFILE
		jp	nz,ServerError		; no BIN on the server
		ld	hl,(Answer+1)
		ld	de,4
		sbc	hl,de			; Cx reset (or a)
		ld	de,MSG_BADFILE
		jp	nz,ServerError		; BIN too short
		ld	hl,FileSums		; sums of the ROM and of the kernel
		ld	de,4
		call	Receive
		jp	c,ServerReadErr
		ld	hl,0
		ld	(REQ_BIN+5),hl
		ld	a,(BootRomSeg-BOOT_ADR+BootCode)
		call	ServerSegment
		jp	c,ServerReadErr
		ld	hl,4000h
		ld	(REQ_BIN+5),hl
		ld	a,(KernelSeg)
		call	ServerSegment
		jp	c,ServerReadErr
		ei
		; checks: ROM header, sums
		ld	a,(BootRomSeg-BOOT_ADR+BootCode)
		out	(0FEh),a
		ld	hl,(8000h)
		ld	a,1
		out	(0FEh),a
		ld	de,'B'*256+'A'
		or	a
		sbc	hl,de
		ld	de,MSG_BADFILE
		jp	nz,ServerError
		ld	a,(BootRomSeg-BOOT_ADR+BootCode)
		call	SegmentSum
		ld	hl,(FileSums)
		sbc	hl,de
		ld	de,MSG_SUM
		jp	nz,ServerError
		ld	a,(KernelSeg)
		call	SegmentSum
		ld	hl,(FileSums+2)
		sbc	hl,de
		ld	de,MSG_SUM
		jp	nz,ServerError
		ld	de,MSG_OK
		call	PrintString
		jp	StartBoot

ServerReadErr:	ld	de,MSG_READERR
ServerError:	ei
		jp	PrintString

; ------------------------------------------------------------------------------
; 16 KB of the BIN (at REQ_BIN+5) to a segment, through page 2: A = segment. Output: Cx set on error
; ------------------------------------------------------------------------------
ServerSegment:	ld	(ReadSeg),a
		ld	hl,4000h
		ld	(REQ_BIN+7),hl
		call	ReadRequest
		ret	c
		ld	a,(ReadSeg)
		out	(0FEh),a
		ld	hl,8000h
		ld	de,4000h
		call	Receive
		ld	a,1			; segment of page 2
		out	(0FEh),a
		ret

; COMMAND_BIN of REQ_BIN+7 bytes: first answer packet (error, size sent) checked. Output: Cx set on error
ReadRequest:	ld	hl,REQ_BIN
		ld	de,REQ_BIN_END-REQ_BIN
		ld	bc,3
		ld	a,TRIES
		call	Request
		ret	c
		ld	a,(Answer)
		or	a
		jr	nz,ReadReqErr		; MSX-DOS error
		ld	hl,(Answer+1)
		ld	de,(REQ_BIN+7)
		sbc	hl,de			; Cx reset (or a)
		ret	z
ReadReqErr:	scf				; error, or BIN too short
		ret

; Request HL (DE bytes) to the server, answer packet (BC bytes) in Answer, A = number of time-outs
; Output: Cx set if no answer
Request:	push	af
		push	bc
		call	vJIOTransmit
		pop	de
		pop	bc
		ld	hl,Answer
		jr	ReceiveTries

; Answer packet of DE bytes at HL. Output: Cx set if no answer
Receive:	ld	b,TRIES
ReceiveTries:	push	bc
		push	de
		push	hl
		call	bJIOReceive
		pop	hl
		pop	de
		pop	bc
		or	a
		ret	nz			; received (Cx reset)
		djnz	ReceiveTries
		scf
		ret

; DE = 16-bit sum of the 16 KB of segment A (Cx reset)
SegmentSum:	out	(0FEh),a
		ld	hl,8000h
		ld	de,0
SegSumLoop:	ld	a,(hl)
		add	a,e
		ld	e,a
		jr	nc,SegSumNext
		inc	d
SegSumNext:	inc	hl
		bit	6,h			; up to C000H
		jr	z,SegSumLoop
		ld	a,1
		out	(0FEh),a
		or	a
		ret

; I/O port of the JIO cartridge: probe of the ports of the IOSEL switches (00H, 20H, 30H)
; (detection routine of herraa1, as the JIO ROM and JIOTIME.COM). Output: A = port, 0FFh = not found
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

; Request (no CRC): part of the BIN
REQ_BIN:	defb	"JIO",0,COMMAND_BIN
		defw	0				; offset
		defw	0				; size
REQ_BIN_END:

_JioPort:	defb	0FFh		; serial line (transmit.asm, receive.asm)
Lines:		defs	3,0
ReadSeg:	defb	0
Answer:		defs	3,0
FileSums:	defs	4,0

INCLUDE "../JIO_NFS/transmit.asm"
INCLUDE "../JIO_NFS/receive.asm"

	ELSE
; ------------------------------------------------------------------------------
; Compressed block of the tape to a segment, through page 2: length of the compressed data, compressed data (BitBuster,
; see bitbuster.py), 16-bit sum of the compressed bytes. The compressed data is read at the end of the segment (up to
; PACK_END, in page 3: room needed by the decompression in place, checked by 0_Build.sh), checked, then unpacked to
; 8000H (bytes read without delay, the sum and the decompression after the block). A = segment
; Output: Cx set on error (read error, CTRL-STOP, wrong sum), DE = error message
; ------------------------------------------------------------------------------
TapeSegment:	ld	(TapeSeg),a
		call	TAPION
		jr	c,TapeErr
		call	TAPIN			; length of the compressed data
		jr	c,TapeErr
		ld	(TapeLen),a
		call	TAPIN
		jr	c,TapeErr
		ld	(TapeLen+1),a
		ld	hl,PACK_END
		ld	de,(TapeLen)
		or	a
		sbc	hl,de
		ld	(TapePack),hl		; start of the compressed data
		ld	a,(TapeSeg)
		out	(0FEh),a		; interrupts disabled by TAPION
TapeLoop:	push	hl
		call	TAPIN
		pop	hl
		jr	c,TapeErr
		ld	(hl),a
		inc	hl
		ld	a,l
		cp	PACK_END % 256
		jr	nz,TapeLoop
		ld	a,h
		cp	PACK_END / 256
		jr	nz,TapeLoop
		call	TAPIN			; sum of the compressed bytes
		jr	c,TapeErr
		ld	(TapeSum),a
		call	TAPIN
		jr	c,TapeErr
		ld	(TapeSum+1),a
		ld	hl,(TapePack)
		ld	de,0
SumLoop:	ld	a,(hl)
		add	a,e
		ld	e,a
		jr	nc,SumNext
		inc	d
SumNext:	inc	hl
		ld	a,l
		cp	PACK_END % 256
		jr	nz,SumLoop
		ld	a,h
		cp	PACK_END / 256
		jr	nz,SumLoop
		ld	hl,(TapeSum)
		or	a
		sbc	hl,de
		ld	de,MSG_SUM
		scf
		jr	nz,TapeEnd
		ld	hl,(TapePack)
		ld	de,8000h
		call	Depack
		or	a
		jr	TapeEnd
TapeErr:	ld	de,MSG_TAPE
TapeEnd:	push	af
		ld	a,1			; segment of page 2
		out	(0FEh),a
		pop	af
		ret

INCLUDE "depack.inc"

	ENDIF

; ------------------------------------------------------------------------------
; Slot id of page A (primary slot register, secondary slot register of SLTTBL)
; ------------------------------------------------------------------------------
PageSlot:	ld	c,a
		in	a,(0A8h)
		call	ShiftPage
		and	03h
		ld	e,a			; primary slot
		ld	d,0
		ld	hl,EXPTBL
		add	hl,de
		bit	7,(hl)
		ret	z			; not expanded
		inc	hl
		inc	hl
		inc	hl
		inc	hl
		ld	a,(hl)			; SLTTBL: secondary slot register
		call	ShiftPage
		and	03h
		rlca
		rlca
		or	e
		or	80h
		ret

; A shifted right by 2 x C bits (bits of page C in bits 1-0)
ShiftPage:	ld	b,c
		inc	b
		jr	ShiftPage2
ShiftPage1:	rrca
		rrca
ShiftPage2:	djnz	ShiftPage1
		ret

TapeSeg:	defb	0
TapeLen:	defw	0
TapePack:	defw	0
TapeSum:	defw	0
Slot2:		defb	0
Slot3:		defb	0

INCLUDE "../../common/ramrom_boot.asm"

; ------------------------------------------------------------------------------
MSG_TITLE:	defb	"JIO-ROM: JIO MSX-DOS 2 ROM in RAM",13,10,"$"
MSG_DOS:	defb	"MSX-DOS is already there: use",13,10
		defb	"JIO-ROM.COM (MSX-DOS 1), or",13,10
		defb	"press SHIFT at boot",13,10,"$"
MSG_RAM:	defb	"RAM of pages 2-3: $"
MSG_MAPSIZE:	defb	13,10,"Mapper of page 2: $"
MSG_SEGMENTS:	defb	" segments",13,10,"$"
MSG_SLOTS:	defb	"The RAM of pages 2-3 is not in the same slot",13,10,"$"
MSG_MAPPER:	defb	"Memory mapper of 128 KB needed",13,10,"$"
MSG_LOAD:	defb	"Loading...",13,10,"$"
MSG_ROM:	defb	"ROM... $"
MSG_KERNEL:	defb	"Kernel... $"
MSG_OK:		defb	"OK",13,10,"$"
MSG_STOP:	defb	"Press STOP on the tape player,",13,10
		defb	"then press a key",13,10,"$"
MSG_TAPE:	defb	"Tape read error",13,10,"$"
	IFDEF SERVER
MSG_READ:	defb	"Reading JIO-CAS.BIN... $"
MSG_NOANSWER:	defb	13,10,"No answer from the JIO server",13,10,"$"
MSG_NOFILE:	defb	"not found",13,10,"$"
MSG_READERR:	defb	"read error",13,10,"$"
MSG_BADFILE:	defb	"not a JIO ROM",13,10,"$"
	ENDIF
MSG_SUM:	defb	"Checksum error",13,10,"$"

; 256 bytes after the loader (not in the tape file: work area of MapperSegments, no initial value)
Buffer:
