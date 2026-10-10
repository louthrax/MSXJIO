; ------------------------------------------------------------------------------
; jiorom.asm
; JIO-ROM.COM: starts MSX-DOS 2 with the JIO MSX-DOS 2 ROM on a MSX-DOS 1 computer, without the ROM cartridge
; (see common/ramrom_boot.asm). Needs a memory mapper of 128 KB (7 segments at least) in the RAM slot of pages 1 to 3.
; ------------------------------------------------------------------------------

BDOS		equ	0005h

RAMAD0		equ	0F341h		; slot ids of the RAM of pages 0 to 3 (MSX-DOS)
RAMAD2		equ	0F343h
RAMAD3		equ	0F344h

		org	0100h

		ld	de,MSG_TITLE
		call	PrintString

		; MSX-DOS 1 only (_DOSVER: MSX-DOS 1 answers A = B = 0)
		ld	b,0
		ld	c,6Fh
		call	BDOS
		ld	a,b
		cp	2
		ld	de,MSG_DOS2
		jp	nc,PrintString

		; what is found: RAM slots, memory mapper (page 2), disk interfaces (shown before any error)
		ld	de,MSG_RAM
		call	PrintString
		ld	hl,RAMAD0
		ld	b,4
RamLoop:	ld	a,' '
		call	PrintChar
		ld	a,(hl)
		call	PrintSlot
		inc	hl
		djnz	RamLoop
		ld	de,MSG_MAPSIZE
		call	PrintString
		call	MapperSegments
		ld	(Segments),a
		call	PrintDec
		ld	de,MSG_DISKS
		call	PrintString
		ld	hl,DRVTBL
		ld	b,4
DiskShow:	ld	a,(hl)
		inc	hl
		or	a
		ld	a,(hl)
		inc	hl
		jr	z,DiskShowNext
		push	af
		ld	a,' '
		call	PrintChar
		pop	af
		call	PrintSlot
DiskShowNext:	djnz	DiskShow
		ld	de,MSG_CRLF
		call	PrintString

		; RAM slot of pages 1 to 3: slot of the ROM, memory mapper of MSX-DOS 2
		ld	a,(RAMAD3)
		ld	hl,RAMAD2
		cp	(hl)
		jr	nz,ErrorSlots
		dec	hl
		cp	(hl)
		jr	nz,ErrorSlots
		call	SetSlots		; the ROM, then the disk interfaces of MSX-DOS 1

		ld	a,(Segments)
		call	SetSegments
		ld	de,MSG_MAPPER
		jp	c,PrintString

		ld	de,MSG_START
		call	PrintString

		; ROM and kernel copied to their segments (no MSX-DOS call after: the segment of JIO.COM could be used)
		ld	hl,RomImage
		ld	a,(BootRomSeg-BOOT_ADR+BootCode)
		call	CopySegment
		ld	hl,RomImage+4000h
		ld	a,(KernelSeg)
		call	CopySegment
		jp	StartBoot		; MSX-DOS 1 is not used anymore

ErrorSlots:	ld	de,MSG_SLOTS

PrintString:	ld	c,09h			; _STROUT
		jp	BDOS

; Character A to the screen (registers kept)
PrintChar:	push	af
		push	bc
		push	de
		push	hl
		ld	e,a
		ld	c,02h			; _CONOUT
		call	BDOS
		pop	hl
		pop	de
		pop	bc
		pop	af
		ret

; ------------------------------------------------------------------------------
; Copy of 16 KB to a segment through page 2 (and a buffer in page 0: the source can be in page 2)
; HL = source, A = segment
; ------------------------------------------------------------------------------
CopySegment:	ld	(CopySeg),a
		ld	de,8000h
CopyLoop:	push	de
		ld	de,Buffer
		ld	bc,256
		ldir				; source -> buffer (TPA segment 1 in page 2)
		pop	de
		push	hl
		ld	a,(CopySeg)
		di
		out	(0FEh),a
		ld	hl,Buffer
		ld	bc,256
		ldir				; buffer -> segment
		ld	a,1
		out	(0FEh),a
		ei
		pop	hl
		ld	a,d
		cp	0C0h
		jr	c,CopyLoop
		ret

CopySeg:	defb	0

INCLUDE "../../common/ramrom_boot.asm"

; ------------------------------------------------------------------------------
MSG_TITLE:	defb	"JIO-ROM: JIO MSX-DOS 2 ROM in RAM",13,10,"$"
MSG_DOS2:	defb	"MSX-DOS 2 is already running (JIO.COM serves the JIO drives on MSX-DOS 2)",13,10,"$"
MSG_SLOTS:	defb	"The RAM of pages 1 to 3 is not in the same slot",13,10,"$"
MSG_MAPPER:	defb	"Memory mapper of 128 KB needed",13,10,"$"
MSG_START:	defb	"Starting MSX-DOS 2...",13,10,"$"
MSG_RAM:	defb	"RAM of pages 0-3:$"
MSG_MAPSIZE:	defb	13,10,"Mapper of page 2: $"
MSG_DISKS:	defb	" segments",13,10,"Disk interfaces:$"
MSG_CRLF:	defb	13,10,"$"

Buffer:		defs	256,0

; JIO MSX-DOS 2 ROM built with RAMROM: page 1 (16 KB), kernel (16 KB)
RomImage:	BINARY	"../JIO_MSX-DOS/0_Temp/jio_dos2_ram.rom"
