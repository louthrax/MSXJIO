bJIOReceive:
	push	ix
	push	de

	ld	de,0

	dec	hl
	ld	b,(hl)	; What if HL=0 ?
	ld	c,0xa2
	ld	ix,0
	add	ix,sp
	ld	a,15
	out	(0xa0),a
	in	a,(0xa2)
	or	64
	out	(0xa1),a
	ld	a,14
	out	(0xa0),a
	in	a,(0xa2)
	or	1
	jp	pe,HeaderPE
;________________________________________________________________________________________________________________________________

HeaderPO:	
	dec	de	;  7
	ld	a,d	;  5
	or	e	;  5
	jr	z,RxTimeOut 	;  8

	in	f,(c)	; 14
	jp	po,HeaderPO	; 11	 LOOP=50 (2-CLOCKS)
	rlc	a	; 10
	in	f,(c)	; 14
	jp	po,HeaderPO	; 11	 At least 2 clocks needed to be down

WU_PO:	in	f,(c)	; 14
	jp	pe,WU_PO	; 11	 LOOP=25
	pop	de	; 11
	push	de	; 11

RX_PO:	in	f,(c)	; 14
	jp	po,RX_PO	; 11	 LOOP=25

	;timing alternatives:
	;ret	po	;  6 = 31 CYCLES
	;ld	sp,hl	;  7	= 32 CYCLES
	ld	b,(hl)	;  8	= 33 CYCLES
	;neg	                                ; 10 =	35 CYCLES
	
	in	a,(c)	; 14	 Bit 0
	nop
                                                                                                ;  5
	rrca	                                ;  5
	dec	de	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 1
	xor	b	;  5
	rrca	                                ;  5
	inc	hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 2
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 3
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 4
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 5
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 6
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 7
	xor	b	;  5
	rrca	                                ;  5

	ld	(hl),a	;  8

	ld	a,d	;  5
	or	e	;  5
	jp	nz,RX_PO	; 11
	 
; ------------------------------------------------------------------------------

ReceiveOK:
	ld	sp,ix
	pop	de
	pop	ix
	ld	a,1
	ret

RxTimeOut:
	pop	de
	pop	ix
	xor	 a
	ret

; ------------------------------------------------------------------------------

HeaderPE:	
	dec	de	;  7
	ld	a,d	;  5
	or	e	;  5
	jr	z,RxTimeOut	;  8

	in	f,(c)	; 14
	jp	pe,HeaderPE	; 11	 LOOP= 50 (2-CLOCKS)
	rlc	a	; 10
	in	f,(c)	; 14
	jp	pe,HeaderPE	; 11	 At least 2 clocks needed to be down

WU_PE:	in	f,(c)	; 14
	jp	po,WU_PE	; 11	 LOOP=25
	pop	de	; 11
	push	de	; 11

RX_PE:	in	f,(c)	; 14
	jp	pe,RX_PE	; 11	 LOOP=25
	
	;timing alternatives:
	;ret	pe	;  6 = 31 CYCLES
	;ld	sp,hl	;  7	= 32 CYCLES
	ld	b,(hl)	;  8	= 33 CYCLES
	;neg	                                ; 10 =	35 CYCLES

	in	a,(c)	; 14	 Bit 0
	cpl	                                ;  5
	rrca	                                ;  5
	dec	de	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 1
	xor	b	;  5
	rrca	                                ;  5
	inc	hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 2
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 3
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 4
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 5
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 6
	xor	b	;  5
	rrca	                                ;  5
	ld	sp,hl	;  7 = 31 CYCLES

	in	b,(c)	; 14	 Bit 7
	xor	b	;  5
	rrca	                                ;  5

	ld	(hl),a	;  8                            Instruction uses data bus write

	ld	a,d	;  5
	or	e	;  5
	jp	nz,RX_PE	; 11

	jr	ReceiveOK 
