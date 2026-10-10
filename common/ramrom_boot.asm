; ------------------------------------------------------------------------------
; ramrom_boot.asm
; Common part of JIO-ROM.COM (clients/JIO_ROM, from MSX-DOS 1) and JIO-ROM.CAS (clients/JIO_CAS, from BASIC): start of
; MSX-DOS 2 with the JIO MSX-DOS 2 ROM run in RAM (target dos2ram of clients/JIO_MSX-DOS/0_Build.sh, RAMROM: see
; p3_paging.asm), in the memory mapper.
;
; The ROM is put in the top segment of the memory mapper of page 3 (M-1, M = number of segments as counted by the
; MSX-DOS 2 kernel, 255 at most), its kernel in the segment of the kernel code (M-3, CODE_S of the ROM, which hides
; the top segment). The ROM runs in page 1 from this segment, with the RAM slot as slot (bit 6 set in its slot id).
; No reset: the system is put back as after the initialization of the BIOS (as SofaRunIt does for MSX-DOS 1), the
; INIT of the ROM is called (first disk interface), then the INIT of the disk ROMs already there (local drives, e.g.
; the floppy drive) and the H_RUNC hook of the ROM, which ends the initialization and boots MSXDOS2.SYS.
; Needs a memory mapper of 128 KB (7 segments at least) in the RAM slot of pages 2 and 3 (and 1 with MSX-DOS 1).
;
; Defined by the including file: PrintChar (character A to the screen, registers kept), Buffer (256 bytes, not in
; page 2)
; ------------------------------------------------------------------------------

RDSLT		equ	000Ch
CALSLT		equ	001Ch

VARWRK		equ	0F380h		; start of the system variables (HIMEM after the BIOS initialization)
MEMSIZ		equ	0F672h
STKTOP		equ	0F674h
NULBUF		equ	0F862h
HOKVLD		equ	0FB20h
DRVTBL		equ	0FB21h		; disk interfaces: number of drives, slot id (4 entries)
HIMEM		equ	0FC4Ah
CSRSW		equ	0FCA9h		; cursor shown by CHPUT (set by COMMAND.COM of MSX-DOS 1)
CNSDFG		equ	0F3DEh		; function keys shown (MSX BASIC)
EXPTBL		equ	0FCC1h
SLTTBL		equ	0FCC5h
DISKID		equ	0FD99h		; number of disk interfaces initialized (0: none)
H_KEYI		equ	0FD9Ah		; first hook
H_RUNC		equ	0FECBh
H_PHYD		equ	0FFA7h		; physical disk access (patched by the disk ROMs)
HOOKEND		equ	0FFD8h		; last byte of the hooks (ENAINT)

ROM_INIT	equ	4002h		; INIT handler of a ROM (header)

MIN_SEGMENTS	equ	7		; 4 TPA, kernel code and data (MSX-DOS 2), ROM

BOOT_ADR	equ	0C400h		; boot code (page 3), above the temporary stack of the ROM (TMPSTK, C200H)
BOOT_SP		equ	0F088h		; stack of the boot (below STKTOP)
SETP0_ADR	equ	0A000h		; page 0 slot switching (page 2: page 3 switched)

; ------------------------------------------------------------------------------
; Segments of the ROM and of the kernel from the number of segments of the mapper of page 2 (MapperSegments)
; Input: A = number of segments (0 = 256); Output: Cx set if not enough segments
; ------------------------------------------------------------------------------
SetSegments:	or	a
		jr	nz,SetSegments1
		dec	a			; 256 segments: 255 (as the MSX-DOS 2 kernel)
SetSegments1:	cp	MIN_SEGMENTS
		ret	c
		dec	a
		ld	(BootRomSeg-BOOT_ADR+BootCode),a	; top segment: ROM
		sub	2
		ld	(KernelSeg),a		; CODE_S of the ROM: kernel code
		or	a
		ret

; ------------------------------------------------------------------------------
; Slots whose INIT is called by the boot: the ROM (A = RAM slot), then the disk interfaces already initialized
; (DRVTBL, if a disk ROM patched H_PHYD: the BIOS sets the hooks to RET, DRVTBL is not used without disk ROM)
; ------------------------------------------------------------------------------
SetSlots:	ld	de,BootSlots-BOOT_ADR+BootCode
		ld	(de),a
		inc	de
		ld	c,1			; number of slots
		ld	a,(H_PHYD)
		cp	0C9h
		jr	z,SetSlots3
		ld	hl,DRVTBL
		ld	b,4
SetSlots1:	ld	a,(hl)
		inc	hl
		or	a
		jr	z,SetSlots2
		ld	a,(hl)
		ld	(de),a
		inc	de
		inc	c
SetSlots2:	inc	hl
		djnz	SetSlots1
SetSlots3:	ld	a,c
		ld	(BootNSlots-BOOT_ADR+BootCode),a
		ret

; ------------------------------------------------------------------------------
; Number of segments of the memory mapper of page 2 (0 = 256): segment number written at 8000H of each segment,
; segment 0 then holds the last number written to a segment aliased with it (256 - size). The bytes are restored.
; (as JIO.COM, clients/JIO_NFS/main.c). MSX-DOS 1 and the BIOS keep the segments 3, 2, 1, 0 in pages 0 to 3.
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
		ld	a,1			; segment of page 2
		out	(0FEh),a
		ei
		ld	a,e
		ret

; ------------------------------------------------------------------------------
; Boot code in page 3 and page 0 slot switching in page 2, then boot (no return)
; ------------------------------------------------------------------------------
StartBoot:	di
		ld	hl,BootCode
		ld	de,BOOT_ADR
		ld	bc,BootCodeEnd-BootCode
		ldir
		ld	hl,SetPage0Code
		ld	de,SETP0_ADR
		ld	bc,SetPage0End-SetPage0Code
		ldir
		jp	Boot

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
		jp	PrintChar

; A to the screen in decimal (0 = 256)
PrintDec:	or	a
		jr	nz,PrintDec1
		ld	a,'2'
		call	PrintChar
		ld	a,'5'
		call	PrintChar
		ld	a,'6'
		jp	PrintChar
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
		ld	(CNSDFG),a		; function keys not shown (from BASIC), as when MSX-DOS 2 boots from a ROM
		ld	(DISKID),a		; no disk interface initialized
		ld	(HOKVLD),a		; EXTBIO not initialized
		ld	hl,H_KEYI
		ld	de,H_KEYI+1
		ld	bc,HOOKEND-H_KEYI
		ld	(hl),0C9h
		ldir
		; page 0: main ROM, ROM segment in port FDH (page 1 of the RAM slot)
		ld	a,(EXPTBL)
		call	SetPage0
		ld	a,(BootRomSeg)
		out	(0FDh),a
		; INIT of the ROM (first disk interface, MSX-DOS 2 kernel), then of the other disk ROMs
		ld	hl,BootSlots
		ld	a,(BootNSlots)
		ld	b,a
BootInit:	push	bc
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
		push	af
		pop	iy
		call	CALSLT
		pop	hl
		inc	hl
		pop	bc
		djnz	BootInit
		; end of the initialization: H_RUNC of the ROM, boots MSXDOS2.SYS (or Disk BASIC)
		call	H_RUNC
		; initialization canceled (SHIFT key, no memory mapper): MSX reset
		di
		rst	0

BootRomSeg:	defb	0			; segment of the ROM
BootNSlots:	defb	0			; slots whose INIT is called
BootSlots:	defs	5,0			; the ROM (RAM slot), the disk interfaces already there
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
