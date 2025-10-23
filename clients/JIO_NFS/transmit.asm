;********************************************************************************************************************************
; IN:  HL = DATA
;      BC = LENGTH
;********************************************************************************************************************************

vJIOTransmit:
                                exx
                                push                            bc
                                push                            de
                                exx

                                call                            vJIOTransmit2

                                exx
                                pop                             de
                                pop                             bc
                                exx
                                ret

vJIOTransmit2:
                                ex                              de,hl
                                inc	bc
                                exx
                                ld	a,15
                                out	($a0),a
                                in	a,($a2)
                                or	4
                                ld	e,a
                                xor	4
                                ld	d,a
                                ld	c,$a1

                                db	$3e
JIOTransmitLoop:
                                ret	nz
                                out	(c),e
                                exx
                                ld	a,(hl)
                                cpi
                                ret	po
                                exx
                                rrca
                                out	(c),d	; =0
                                ret	nz
                                jp	c,TRANSMIT10
                                out	(c),d	; -0
                                rrca
                                jp	c,TRANSMIT11
;________________________________________________________________________________________________________________________________

TRANSMIT01:	
                                out	(c),d	; -1
                                rrca
                                jr	c,TRANSMIT12
                                nop

TRANSMIT02:	
                                out	(c),d	; -0
                                rrca
                                jp	c,TRANSMIT13

TRANSMIT03:	
                                out	(c),d	; -1
                                rrca
                                jr	c,TRANSMIT14
                                nop

TRANSMIT04:	
                                out	(c),d	; -0
                                rrca
                                jp	c,TRANSMIT15

TRANSMIT05:	
                                out	(c),d	; -1
                                rrca
                                jr	c,TRANSMIT16
                                nop

TRANSMIT06:	
                                out	(c),d	; -0
                                rrca
                                jp	c,TRANSMIT17

TRANSMIT07:	
                                out	(c),d	; -1
                                jp	JIOTransmitLoop
;________________________________________________________________________________________________________________________________

TRANSMIT10:
	out	(c),e	; -0
                                rrca
                                jp	nc,TRANSMIT01

TRANSMIT11:
	out	(c),e	; -1
                                rrca
                                jr	nc,TRANSMIT02
                                nop

TRANSMIT12:	
                                out	(c),e	; -0
                                rrca
                                jp	nc,TRANSMIT03

TRANSMIT13:	
                                out	(c),e	; -1
                                rrca
                                jr	nc,TRANSMIT04
                                nop

TRANSMIT14:	
                                out	(c),e	; -0
                                rrca
                                jp	nc,TRANSMIT05

TRANSMIT15:	
                                out	(c),e	; -1
                                rrca
                                jr	nc,TRANSMIT06
                                nop

TRANSMIT16:	
                                out	(c),e	; -0
                                rrca
                                jp	nc,TRANSMIT07

TRANSMIT17:	
                                out	(c),e	; -1
                                jp	JIOTransmitLoop
