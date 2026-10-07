vJIOTransmit:
                                ld                              b,d
                                ld                              c,e

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

; _JioPort (defined by the program): 0xFF = joystick port 2, 0xFE = joystick port 1 (PSG), else I/O register of the
; JIO cartridge
vJIOTransmit2:
                                inc	bc
                                exx
                                ld	a,(_JioPort)
                                ld	c,a
                                ld	b,4	; bit 2: joystick port 2 pin 6, JIO cartridge
                                inc	a
                                jr	z,TransmitPSG
                                inc	a
                                jr	nz,TransmitCart
                                ld	b,1	; bit 0: joystick port 1 pin 6
TransmitPSG:
                                ld	a,15	; PSG register 15
                                out	(0xa0),a
                                in	a,(0xa2)
                                ld	c,0xa1
                                jr	TransmitLevels
TransmitCart:
                                in	a,(c)	; I/O register of the JIO cartridge
TransmitLevels:
                                or	b
                                ld	e,a
                                xor	b
                                ld	d,a

                                defb	0x3e
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
