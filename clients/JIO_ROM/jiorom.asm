; ------------------------------------------------------------------------------
; jiorom.asm
; JIO-ROM.COM: starts MSX-DOS 2 with the JIO MSX-DOS 2 ROM on a MSX-DOS 1 computer, without the ROM cartridge
;
; The JIO MSX-DOS 2 ROM built with RAMROM (see p3_paging.asm of clients/JIO_MSX-DOS) is copied to the top segment
; of the memory mapper of page 3 (M-1, M = number of segments as counted by the MSX-DOS 2 kernel, 255 at most), its
; kernel to the segment of the kernel code (M-3, CODE_S of the ROM, which hides the top segment). The ROM runs in
; page 1 from this segment, with the RAM slot as slot (bit 6 set in its slot id).
; No reset: the system is put back as after the initialization of the BIOS (as SofaRunIt does for MSX-DOS 1), the
; INIT of the ROM is called (first disk interface), then the INIT of the disk ROMs of MSX-DOS 1 (local drives, e.g.
; the floppy drive) and the H_RUNC hook of the ROM, which ends the initialization and boots MSXDOS2.SYS.
; Needs a memory mapper of 128 KB (7 segments at least) in the RAM slot of pages 1 to 3.
; ------------------------------------------------------------------------------

BDOS		equ	0005h
RDSLT		equ	000Ch
CALSLT		equ	001Ch

VARWRK		equ	0F380h		; start of the system variables (HIMEM after the BIOS initialization)
RAMAD0		equ	0F341h		; slot ids of the RAM of pages 0 to 3 (MSX-DOS)
RAMAD1		equ	0F342h
RAMAD2		equ	0F343h
RAMAD3		equ	0F344h
MEMSIZ		equ	0F672h
STKTOP		equ	0F674h
NULBUF		equ	0F862h
HOKVLD		equ	0FB20h
DRVTBL		equ	0FB21h		; disk interfaces: number of drives, slot id (4 entries)
HIMEM		equ	0FC4Ah
CSRSW		equ	0FCA9h		; cursor shown by CHPUT (set by COMMAND.COM of MSX-DOS 1)
EXPTBL		equ	0FCC1h
SLTTBL		equ	0FCC5h
DISKID		equ	0FD99h
H_KEYI		equ	0FD9Ah		; first hook
H_RUNC		equ	0FECBh
HOOKEND		equ	0FFD8h		; last byte of the hooks (ENAINT)

ROM_INIT	equ	4002h		; INIT handler of a ROM (header)

MIN_SEGMENTS	equ	7		; 4 TPA, kernel code and data (MSX-DOS 2), ROM

BOOT_ADR	equ	0C400h		; boot code (page 3), above the temporary stack of the ROM (TMPSTK, C200H)
BOOT_SP		equ	0F088h		; stack of the boot (below STKTOP)
SETP0_ADR	equ	0A000h		; page 0 slot switching (page 2: page 3 switched)

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
		ld	(BootSlot-BOOT_ADR+BootCode),a

		; number of segments of the memory mapper, as counted by the MSX-DOS 2 kernel (255 at most)
		ld	a,(Segments)
		or	a
		jr	nz,SegmentsOk
		dec	a			; 256 segments: 255
SegmentsOk:	cp	MIN_SEGMENTS
		ld	de,MSG_MAPPER
		jp	c,PrintString
		dec	a
		ld	(BootRomSeg-BOOT_ADR+BootCode),a	; top segment: ROM
		sub	2
		ld	(KernelSeg),a		; CODE_S of the ROM: kernel code

		; disk interfaces of MSX-DOS 1 (DRVTBL): their INIT is called after the INIT of the ROM
		ld	hl,DRVTBL
		ld	de,BootDisks-BOOT_ADR+BootCode
		ld	bc,4*256+0		; c = number of disk interfaces
DiskLoop:	ld	a,(hl)
		inc	hl
		or	a
		jr	z,DiskNext
		ld	a,(hl)
		ld	(de),a
		inc	de
		inc	c
DiskNext:	inc	hl
		djnz	DiskLoop
		ld	a,c
		ld	(BootNDisks-BOOT_ADR+BootCode),a

		ld	de,MSG_START
		call	PrintString

		; ROM and kernel copied to their segments (no MSX-DOS call after: the segment of JIO.COM could be used)
		ld	hl,RomImage
		ld	a,(BootRomSeg-BOOT_ADR+BootCode)
		call	CopySegment
		ld	hl,RomImage+4000h
		ld	a,(KernelSeg)
		call	CopySegment

		; boot code in page 3 (MSX-DOS 1 is not used anymore), page 0 slot switching in page 2
		di
		ld	hl,BootCode
		ld	de,BOOT_ADR
		ld	bc,BootCodeEnd-BootCode
		ldir
		ld	hl,SetPage0Code
		ld	de,SETP0_ADR
		ld	bc,SetPage0End-SetPage0Code
		ldir
		jp	Boot

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

; Slot id A to the screen: primary slot, -secondary slot if expanded
PrintSlot:	push	af
		and	03h
		add	a,'0'
		call	PrintChar
		pop	af
		bit	7,a
		ret	z
		push	af
		ld	a,'-'
		call	PrintChar
		pop	af
		rrca
		rrca
		and	03h
		add	a,'0'
		jr	PrintChar

; A to the screen in decimal (0 = 256)
PrintDec:	or	a
		jr	nz,PrintDec1
		ld	a,'2'
		call	PrintChar
		ld	a,'5'
		call	PrintChar
		ld	a,'6'
		jr	PrintChar
PrintDec1:	ld	c,0			; leading zeros not shown
		ld	b,100
		call	PrintDigit
		ld	b,10
		call	PrintDigit
		inc	c
		ld	b,1
PrintDigit:	ld	d,'0'-1
PrintDigit1:	inc	d
		sub	b
		jr	nc,PrintDigit1
		add	a,b
		push	af
		ld	a,d
		cp	'0'
		jr	nz,PrintDigit2
		inc	c
		dec	c
		jr	z,PrintDigit3
PrintDigit2:	ld	c,1
		call	PrintChar
PrintDigit3:	pop	af
		ret

; ------------------------------------------------------------------------------
; Number of segments of the memory mapper of page 2 (0 = 256): segment number written at 8000H of each segment,
; segment 0 then holds the last number written to a segment aliased with it (256 - size). The bytes are restored.
; (as JIO.COM, clients/JIO_NFS/main.c). MSX-DOS 1 keeps the segments of the boot: 3, 2, 1, 0 in pages 0 to 3.
; ------------------------------------------------------------------------------
MapperSegments:	di
		ld	hl,Buffer
		ld	bc,0			; b = 256 segments, c = segment
MapSave:	ld	a,c
		out	(0FEh),a
		ld	a,(8000h)
		ld	(hl),a
		inc	hl
		inc	c
		djnz	MapSave
MapWrite:	ld	a,c
		out	(0FEh),a
		ld	(8000h),a
		inc	c
		djnz	MapWrite
		xor	a
		out	(0FEh),a
		ld	a,(8000h)
		neg
		ld	e,a			; size (0 = 256)
		ld	hl,Buffer
MapRestore:	ld	a,c
		out	(0FEh),a
		ld	a,(hl)
		ld	(8000h),a
		inc	hl
		inc	c
		djnz	MapRestore
		ld	a,1			; segment of page 2 of the TPA
		out	(0FEh),a
		ei
		ld	a,e
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
Segments:	defb	0
KernelSeg:	defb	0

; ------------------------------------------------------------------------------
; Boot (page 3, interrupts disabled): system as after the initialization of the BIOS, INIT of the disk ROMs, H_RUNC
; ------------------------------------------------------------------------------
BootCode:
		PHASE	BOOT_ADR
Boot:		ld	sp,BOOT_SP
		; memory and hooks as after the BIOS initialization (the INIT of the first disk interface checks HIMEM)
		ld	hl,VARWRK
		ld	(HIMEM),hl
		ld	hl,0F168h
		ld	(MEMSIZ),hl
		ld	hl,0F0A0h
		ld	(STKTOP),hl
		ld	hl,0F177h
		ld	(NULBUF),hl
		xor	a
		ld	(CSRSW),a		; no cursor shown when printing (as after the BIOS initialization)
		ld	(DISKID),a		; no disk interface initialized
		ld	(HOKVLD),a		; EXTBIO not initialized
		ld	hl,H_KEYI
		ld	de,H_KEYI+1
		ld	bc,HOOKEND-H_KEYI
		ld	(hl),0C9h
		ldir
		; page 0: main ROM, page 1: ROM (RAM slot, ROM segment)
		ld	a,(EXPTBL)
		call	SetPage0
		ld	a,(BootRomSeg)
		out	(0FDh),a
		; INIT of the ROM: first disk interface, MSX-DOS 2 kernel
		ld	a,(BootSlot)
		ld	ix,(ROM_INIT)
		call	CallSlot
		; INIT of the disk ROMs of MSX-DOS 1
		ld	hl,BootDisks
		ld	a,(BootNDisks)
		or	a
		jr	z,BootRunc
		ld	b,a
BootDisk:	push	bc
		push	hl
		ld	a,(hl)
		push	af
		ld	hl,ROM_INIT
		call	RDSLT
		ld	e,a
		pop	af
		push	af
		push	de
		inc	hl
		call	RDSLT
		pop	de
		ld	d,a
		push	de
		pop	ix			; INIT handler
		pop	af
		call	CallSlot
		pop	hl
		inc	hl
		pop	bc
		djnz	BootDisk
		; end of the initialization: H_RUNC of the ROM, boots MSXDOS2.SYS (or Disk BASIC)
BootRunc:	call	H_RUNC
		; initialization canceled (SHIFT key, no memory mapper): MSX reset
		di
		rst	0

; CALSLT of the BIOS: A = slot id, IX = address
CallSlot:	push	af
		pop	iy
		jp	CALSLT

BootSlot:	defb	0			; RAM slot of pages 1 to 3
BootRomSeg:	defb	0			; segment of the ROM
BootNDisks:	defb	0			; disk interfaces of MSX-DOS 1
BootDisks:	defs	4,0			; their slot id
		DEPHASE
BootCodeEnd:

; ------------------------------------------------------------------------------
; Page 0 set to the slot id A, run in page 2: page 3 is switched to change the secondary slot register
; ------------------------------------------------------------------------------
SetPage0Code:
		PHASE	SETP0_ADR
SetPage0:	ld	c,a			; slot id
		and	03h
		ld	b,a			; primary slot
		in	a,(0A8h)
		and	0FCh
		or	b
		ld	d,a			; primary slot register: page 0 = primary slot
		bit	7,c
		jr	z,SetPage0Prim
		ld	a,b
		rrca
		rrca
		ld	e,a
		ld	a,d
		and	3Fh
		or	e
		out	(0A8h),a		; page 3: primary slot (no stack)
		ld	a,(0FFFFh)
		cpl
		and	0FCh
		ld	e,a
		ld	a,c
		rrca
		rrca
		and	03h
		or	e			; secondary slot register: page 0 = secondary slot
		ld	(0FFFFh),a
		ld	e,a
		ld	a,d
		out	(0A8h),a		; page 3 back
		ld	c,b
		ld	b,0
		ld	hl,SLTTBL
		add	hl,bc
		ld	(hl),e
		ret
SetPage0Prim:	ld	a,d
		out	(0A8h),a
		ret
		DEPHASE
SetPage0End:

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
RomImage:	BINARY	"0_Temp/jio_dos2_ram.rom"
