; ------------------------------------------------------------------------------
; depack.inc
; BitBuster depacker, included by jiocas.asm (ROM and kernel blocks of JIO-ROM.CAS) and unpack.asm (loader of the
; tape files, compressed)
; ------------------------------------------------------------------------------
; BitBuster depacker (Arjan Bakker, as in SofaCas): HL = compressed data, DE = destination. Interrupts disabled
; (alternate registers used). Changes AF, BC, DE, HL, BC', DE', HL'.
; ------------------------------------------------------------------------------
GETBIT		MACRO				; next bit of the bit stream in Cx (A: bits, HL: compressed data)
		LOCAL	GetBitEnd
		add	a,a
		jr	nz,GetBitEnd
		ld	a,(hl)			; 8 more bits
		inc	hl
		rla				; (bit 0 set: end of these 8 bits)
GetBitEnd:
		ENDM

Depack:		ld	a,128
		exx
		ld	de,1
		exx
DepackLoop:	GETBIT
		jr	c,DepackMatch
		ldi				; literal byte
		jr	DepackLoop
DepackMatch:	ld	c,(hl)			; bits 6-0 of offset-1, bit 7: offset of 11 bits
		inc	hl
		ld	b,0
		bit	7,c
		jr	z,DepackOffset
		GETBIT				; bits 10-8 of offset-1
		rl	b
		GETBIT
		rl	b
		GETBIT
		rl	b
		GETBIT				; bit 7
		jr	c,DepackOffset
		res	7,c
DepackOffset:	inc	bc			; offset
		exx				; length-2 (Elias gamma) in HL'
		ld	h,d
		ld	l,e
		ld	b,e			; number of bits
DepackSize:	exx
		GETBIT
		exx
		jr	nc,DepackSizeEnd
		inc	b
		jr	DepackSize
DepackBits:	exx
		GETBIT
		exx
		adc	hl,hl
DepackSizeEnd:	djnz	DepackBits
		inc	hl			; length
		exx
		ret	c			; length of 16 bits + 1: end
		push	hl
		exx
		push	hl
		exx
		ld	h,d
		ld	l,e
		sbc	hl,bc			; source of the match
		pop	bc
		ldir
		pop	hl
		jr	DepackLoop
