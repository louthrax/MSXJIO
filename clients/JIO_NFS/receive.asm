bJIOReceive:
	push	de

	ld	de,0

	dec	hl
	ld	ix,0
	add	ix,sp
	ld	a,(_JioPort)	; 0xFF = joystick port 2, 0xFE = joystick port 1 (PSG), else I/O register of the cartridge
	ld	c,a
	cp	0xfe
	jr	nc,ReceivePSG
	in	a,(c)	; JIO cartridge: bit 0
	jr	ReceiveLevel
ReceivePSG:
	ld	c,0xa2
	ld	a,15	; PSG register 15: bit 6 = joystick port selected (0 = port 1)
	out	(0xa0),a
	in	a,(0xa2)
	jr	z,ReceiveJoy1	; Z (CP 0xFE above, flags kept): joystick port 1
	or	64	; joystick port 2
	jr	ReceiveSelect
ReceiveJoy1:
	and	0xbf
ReceiveSelect:
	out	(0xa1),a
	ld	a,14	; PSG register 14 (pin 1 of the joystick port selected)
	out	(0xa0),a
	in	a,(0xa2)
ReceiveLevel:
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
	ld                              b,(hl)	;  8 = 33 CYCLES
	
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
	ld	a,1
	ret

RxTimeOut:
	pop	de
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
	ld                              b,(hl)                          ;  8 = 33 CYCLES

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
