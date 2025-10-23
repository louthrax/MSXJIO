	INCLUDE "msx.inc"

                                ORG                             $100

                                ld	hl,installer
                                ld	de,install
                                ld	bc,inst_length
                                ldir
                                jp                              install

;##################################################################################################

installer:
	PHASE	$8100

install:	di

                                ; copy driver code to final destination
                                ld	hl,DRV_CODE		; offset for start of driver code
                                ld	bc,DRV_LENGTH
                                ld                              de,DRV_START
                                ldir

                                ld                              hl,(0xF37B)
                                ld                              (Hook_OriginalCode),hl

                                ld                              a,0xC3
                                ld                              (0xF37A),a
                                ld                              hl,Hook
                                ld                              (0xF37B),hl

                                ld	hl,ENASLT
                                ld	de,my_enaslt
                                ld	bc, 4
                                ldir

                                ld	a,(EXPTBL)		; Main BIOS slot
                                ld	h,$00		; page 0
                                call	my_enaslt

                                ; load BASIC in page 1
                                ld	a,(EXPTBL)		; BASIC slot
                                ld                              h,$40		; page 1
	call                            my_enaslt

                                ld	sp,DRV_START

                                ; allocate memory for the fossil driver
                                ld	hl,DRV_START
                                ld	(HIMSAV),hl		; set new highmem

sethook:
                                ld	hl,call_system
                                ld                              a,(DOSVER)
                                cp                              $22		; DOS version 2.2 or higher?
                                jr	nc,cmd_system		; nc=yes

                                ; DOS 1: set default drive and call system command
                                xor	a
                                ld	(CURDRV),a
                                ld	hl,call_system

                                ; jump to basic and do _system
                                ; use code snippet from ramhelpr.asm by Konamiman
cmd_system:
	push	hl
                                xor	a
                                ld	hl,$f41f
                                ld	($f860),hl
                                ld	hl,$f423		
                                ld	($f41f),hl
                                ld	(hl),a
                                ld	hl,$f52c
                                ld	($f421),hl
                                ld	(hl),a
                                ld	hl,$f42c
                                ld	($f862),hl
                                pop	hl
                                jp	NEWSTT

my_enaslt:	db	0,0,0,0

call_system:	db	":_SYSTEM",$00

	DEPHASE

inst_length:	EQU	$-installer


;##################################################################################################

DRV_CODE:
                                PHASE	$E200

DRV_START:
	INCLUDE "hook.asm"
	INCLUDE "tmp/jio_nfs.asm"
	INCLUDE "transmit.asm"
	INCLUDE "receive.asm"
                                INCLUDE "iar_crt.asm"
DRV_END:

                                DEPHASE

DRV_LENGTH                      EQU DRV_END - DRV_START
