bJIOReceive:
                                out                             (0x2D),a
                                push	ix
                                push	de

                                ld                              de,0

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
                                jr	z,ReceiveTimeOut                ;  8

                                in	f,(c)	; 14
                                jp	po,HeaderPO	; 11   LOOP=50 (2-CLOCKS)
                                rlc	a
                                in	f,(c)	; 14
                                jp	po,HeaderPO	; 11   At least 2 clocks needed to be down

WU_PO:
                                in	f,(c)	; 14
                                jp	pe,WU_PO	; 11   LOOP=25
                                pop	de
                                push	de

RX_PO:
                                in	f,(c)	; 14
                                jp	po,RX_PO	; 11   LOOP=25
                                ld	(hl),b	;  8  = 33 CYCLES

                                in	a,(c)	; 14   Bit 0
                                nop
                                rrca		;  5
                                dec	de	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 1
                                xor	b	;  5
                                rrca		;  5
                                inc	hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 2
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 3
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 4
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 5
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 6
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 7
                                xor	b	;  5
                                rrca		;  5

                                ld	b,a	;  5
                                ld	a,d	;  5
                                or	e	;  5
                                jp	nz,RX_PO	; 11
;________________________________________________________________________________________________________________________________

ReceiveOK:
                                ld	(hl),b
                                ld	sp,ix

                                pop	de
                                pop	ix
                                ld                              a,1
                                ret

ReceiveTimeOut:
                                pop	de
                                pop	ix
                                xor                             a
                                ret
;________________________________________________________________________________________________________________________________

HeaderPE:
                                dec	de	;  7
                                ld	a,d	;  5
                                or	e	;  5
                                jr	z,ReceiveTimeOut	;  8

                                in	f,(c)	; 14
                                jp	pe,HeaderPE	; 11   LOOP= 50 (2-CLOCKS)
                                rlc	a	; 10
                                in	f,(c)	; 14
                                jp	pe,HeaderPE	; 11   At least 2 clocks needed to be down

WU_PE:	
                                in	f,(c)	; 14
                                jp	po,WU_PE	; 11   LOOP=25
                                pop	de
                                push	de

RX_PE:	
                                in	f,(c)	; 14
                                jp	pe,RX_PE	; 11   LOOP=25
                                ld	(hl),b	;  8 = 33 CYCLES

                                in	a,(c)	; 14   Bit 0
                                cpl		;  5
                                rrca		;  5
                                dec	de	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 1
                                xor	b	;  5
                                rrca		;  5
                                inc	hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 2
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 3
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 4
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 5
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 6
                                xor	b	;  5
                                rrca		;  5
                                ld	sp,hl	;  7 = 31 CYCLES

                                in	b,(c)	; 14   Bit 7
                                xor	b	;  5
                                rrca		;  5

                                ld	b,a	;  5
                                ld	a,d	;  5
                                or	e	;  5
                                jp	nz,RX_PE	; 11

                                jr	ReceiveOK
