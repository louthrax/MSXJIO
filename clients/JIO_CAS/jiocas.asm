; ------------------------------------------------------------------------------
; jiocas.asm
; Loader of JIO-ROM.CAS: starts MSX-DOS 2 with the JIO MSX-DOS 2 ROM on a MSX without MSX-DOS (no disk drive, or
; disk ROMs disabled with SHIFT at boot), from MSX BASIC: BLOAD"CAS:",R (with MSX-DOS 1: JIO-ROM.COM)
;
; JIO-ROM.CAS (0_Build.sh): this loader (BLOAD file JIOROM, at TAPE_ADR), then two data blocks of 16 KB read by the
; loader with the tape routines of the BIOS: the ROM and its kernel, put in their segments (see
; common/ramrom_boot.asm).
; Needs a memory mapper of 128 KB (7 segments at least) in the RAM slot of pages 2 and 3.
; ------------------------------------------------------------------------------

CHPUT		equ	00A2h
TAPION		equ	00E1h
TAPIN		equ	00E4h
TAPIOF		equ	00E7h

TAPE_ADR	equ	0D000h		; loader (page 3): above the boot code, below the BASIC stack

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

		ld	de,MSG_LOAD
		call	PrintString

		; ROM and kernel read from the tape to their segments
		ld	a,(BootRomSeg-BOOT_ADR+BootCode)
		call	TapeSegment
		jr	c,TapeError
		ld	a,(KernelSeg)
		call	TapeSegment
		jr	c,TapeError
		call	TAPIOF
		jp	StartBoot

TapeError:	call	TAPIOF
		ld	de,MSG_TAPE

PrintString:	ld	a,(de)
		cp	'$'
		ret	z
		call	CHPUT
		inc	de
		jr	PrintString

; Character A to the screen (registers kept)
PrintChar:	jp	CHPUT

; ------------------------------------------------------------------------------
; Block of 16 KB of the tape to a segment, through page 2: A = segment
; Output: Cx set on error (or CTRL-STOP)
; ------------------------------------------------------------------------------
TapeSegment:	ld	(TapeSeg),a
		call	TAPION
		ret	c
		ld	a,(TapeSeg)
		out	(0FEh),a		; interrupts disabled by TAPION
		ld	hl,8000h
TapeLoop:	push	hl
		call	TAPIN
		pop	hl
		jr	c,TapeEnd
		ld	(hl),a
		inc	hl
		ld	a,h
		cp	0C0h
		jr	nz,TapeLoop
		or	a
TapeEnd:	push	af
		ld	a,1			; segment of page 2
		out	(0FEh),a
		pop	af
		ret

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
MSG_TAPE:	defb	13,10,"Tape read error",13,10,"$"

Buffer:		defs	256,0
