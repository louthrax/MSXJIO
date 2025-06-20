
; Original 38400 bauds communication routine used by JIO Serial Monitor tool by Tiny Yarou

MSXVER                          equ                             0x002D
INLIN                           equ                             0x00B1
CHPUT                           equ                             0x00A2

CHGCPU                          equ                             0x0180
;Function : Changes CPU mode
;Input    : A = LED 0 0 0 0 0 x x
;                |            0 0 = Z80 (ROM) mode
;                |            0 1 = R800 ROM  mode
;                |            1 0 = R800 DRAM mode
;               LED indicates whether the Turbo LED is switched with the CPU
;Output   : none
;Registers: none


GETCPU                          equ                             0x0183
;Function : Returns current CPU mode
;Input    : none
;Output   : A = 0 0 0 0 0 0 x x
;                           0 0 = Z80 (ROM) mode
;                           0 1 = R800 ROM  mode
;                           1 0 = R800 DRAM mode
;Registers: AF

;##############################################################################

                                org                             0xC800-7

                                defb                            0xFE
                                defw                            BIN_Start
                                defw                            BIN_End
                                defw                            BIN_Start

;##############################################################################

BIN_Start:                      ld                              hl,WELCOME_MESSAGE
                                call                            PrintString

                                call                            GetInitialCPUMode


UserLoop:                       call                            INLIN
                                ret                             c

                                push                            hl

SearchEnd:                      inc                             hl
                                ld                              a,(hl)
                                or                              a
                                jr                              nz,SearchEnd

                                ld                              (hl),13
                                inc                             hl
                                ld                              (hl),10
                                inc                             hl
                                ld                              (hl),0

                                pop                             hl
                                inc                             hl
;______________________________________________________________________________

                                call                            SetZ80CPUMode

                                di

                                call                            SendString

CtrlNotPressed:                 ld                              hl,g_acReceivedMessage
                                call                            ReceiveString
                                ld                              (hl),0
                                jr                              z,NoInterrupted

                                in                              a,(c)
                                bit                             1,a
                                jr                              nz,CtrlNotPressed

NoInterrupted:                  ei

                                call                            RestoreCPUMode
;______________________________________________________________________________

                                ld                              hl,g_acReceivedMessage
                                call                            PrintString

                                ld                              hl,CRLF
                                call                            PrintString
                                ld                              hl,CRLF
                                call                            PrintString

                                jr                              UserLoop

;##############################################################################

PrintString:                    ld                              a,(hl)
                                or                              a
                                ret                             z
                                push                            hl
                                call                            CHPUT
                                pop                             hl
                                inc                             hl
                                jr                              PrintString

;##############################################################################

ReceiveString:                  ld a,15
                                out (0xa0),a
                                in a,(0xa2)
                                or 64
                                out (0xa1),a
                                ld a,14
                                out (0xa0),a

                                in a,(0xAA)
                                and 0xF0
                                add a,6
                                out (0xAA),a

                                ld c,0xA9

ReceiveCharacterLoop:	in f,(c)
                                ret po
                                in a,(0xA2)
	rrca
	jr c,ReceiveCharacterLoop

                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop
                                nop

                                ld d,0
	ld b,8

ReceiveBitLoop:	in a,(0xA2)
	rrca
	rr d

	ld a,2
ReceiveBitDelay:	dec a
	nop
	jr nz,ReceiveBitDelay

	djnz ReceiveBitLoop

	ld a,d
	cp 0x0D
                                ret z

WaitTransmissionEnd:	in a,(0xA2)
	rrca
	jr nc,WaitTransmissionEnd

	ld (hl),d
	inc hl

	jp ReceiveCharacterLoop

;##############################################################################

SendString:                     ld a,(hl)
	or a
	ret z
	call SendCharacter
	inc hl
	jr SendString

;______________________________________________________________________________

SendCharacter:	push hl

	ld l,a

	ld a,15
	out (0xA0),a

	ld h,0xFF
	and a
	rl l
	rl h

	ld b,11

SendBitLoop:	in a,(0xA2)
	rr h
	rr l
	jr nc,l4a70h
	set 2,a
	jr l4a74h

l4a70h:	res 2,a
	jr l4a74h

l4a74h:	out (0xA1),a
	djnz SendBitLoop

	pop hl
	ret

;##############################################################################

GetInitialCPUMode:              ld                              a,(MSXVER)
                                cp                              3
                                ret                             c
                                
                                call                            GETCPU
                                ld                              (InitialCPUMode),a
                                ret

;##############################################################################

RestoreCPUMode:                 ld                              a,(MSXVER)
                                cp                              3
                                ret                             c
                                
                                ld                              a,(InitialCPUMode)
                                jp                              CHGCPU

;##############################################################################

SetZ80CPUMode:                  ld                              a,(MSXVER)
                                cp                              3
                                ret                             c
                                
                                ld                              a,0x80
                                jp                              CHGCPU

InitialCPUMode:                 defb                            0

;##############################################################################

CRLF:                           defb                            13,10,0

WELCOME_MESSAGE:                defb                            12
                                defm                            "JSM - JIO 38400 bauds Serial Monitor v1.0",13,10
                                defm                            "Coded by Louthrax",13,10
                                defm                            "38400 bauds routines by Tiny Yarou",13,10
                                defm                            13,10
                                defm                            "Enter your commands and validate with [RETURN]",13,10
                                defm                            "Press [CTRL] to unlock reception",13,10
                                defm                            "Press [CTRL] + [C] to exit",13,10
                                defm                            13,10
                                defm                            "Useful commands:",13,10
                                defm                            "  Display current UART mode: AT+UART?",13,10
                                defm                            "  Set UART mode for JIO:     AT+UART=115200,0,0",13,10
                                defm                            "  Set device name:           AT+NAME=xxxxx",13,10
                                defm                            "-----------------------------------------",13,10,0

;##############################################################################

BIN_End:

;##############################################################################

g_acReceivedMessage: