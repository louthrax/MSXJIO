; ------------------------------------------------------------------------------
; stub.asm
; Resident stub of JIO.COM, copied to HIMSAV at install (relocatable).
;
; The driver (driver.c) is in a system segment of the memory mapper: for each BDOS function, the stub maps it in
; page 2 (DRIVER_BASE), with its own stack in the segment, and restores the TPA segment of page 2 afterwards.
; The data of the program in page 2 are not visible while the driver is mapped: the transfers to or from them and
; the copies of parameters (bounce buffer) are done here, with the TPA segment mapped back in page 2.
; Interrupts are disabled while the driver is mapped. Page 2 must be in the slot of the primary mapper (the slot of
; the TPA), as in MSX-DOS and Disk BASIC.
; Layout (stub.h, shared with driver.c and main.c, no comment there: also read by the assembler):
;   STUB_TRANSMIT       jp vJIOTransmit: HL = data, DE = size
;   STUB_RECEIVE        jp bJIOReceive: HL = buffer, DE = size; A = 1 received, 0 time-out
;   STUB_XFER           jp TpaXfer: same with the TPA segment in page 2, A = 0 transmit, 1 receive
;   STUB_FROM_CALLER    jp FromCaller: HL = TPA address, DE = size (max STUB_BOUNCE_SIZE) -> bounce buffer
;   STUB_TO_CALLER      jp ToCaller: bounce buffer -> HL = TPA address, DE = size (max STUB_BOUNCE_SIZE)
;   STUB_ORIGINAL       jp TpaOriginal: original BDOS, registers in STUB_REGISTERS
;   STUB_ENTRY          jp entry of the driver (set at install)
;   STUB_GET_P2         jp GET_P2 of the mapper support routines (set at install)
;   STUB_HOOK_ORIGINAL  jp previous GO_BDOS hook (set at install)
;   STUB_HOOK           GO_BDOS hook entry
;   STUB_REGISTERS      10 bytes: BC, AF, HL, DE, IX of the BDOS function
;   STUB_DRIVES         8 bytes: drives handled (A: to H:)
;   STUB_HAS_TURBO      turbo R: Z80 mode during the transfers
;   STUB_SEGMENT        mapper segment of the driver
;   STUB_AUTO_RETRY     not 0: "Auto retry" of the server (COMMAND_DRIVE_INFO), the BDOS functions without answer are
;                       done again; 0: they return "Not ready"
;   STUB_BOUNCE         bounce buffer (STUB_BOUNCE_SIZE bytes), to copy TPA data of page 2
;   STUB_DPB            dummy DPB returned in IX by _ALLOC: the kernel only takes the sector size at +2, the rest of
;                       a DPB is not kept (no program uses it on a drive of the server)
;   DRIVER_BASE         address of the driver (page 2), DRIVER_STACK its stack (top of its segment)
; ------------------------------------------------------------------------------

#include "stub.h"

MAPPER_P2	equ	0FEh			; mapper register of page 2

; ------------------------------------------------------------------------------
StubBase:
		jp	vJIOTransmit		; STUB_TRANSMIT
		jp	bJIOReceive		; STUB_RECEIVE
		jp	TpaXfer			; STUB_XFER
		jp	FromCaller		; STUB_FROM_CALLER
		jp	ToCaller		; STUB_TO_CALLER
		jp	TpaOriginal		; STUB_ORIGINAL
DriverEntry:	defb	0C3h			; STUB_ENTRY: jp driver entry
		defw	0
GetP2:		defb	0C3h			; STUB_GET_P2: jp GET_P2
		defw	0
HookOriginal:	defb	0C3h			; STUB_HOOK_ORIGINAL: jp previous hook
		defw	0
		jp	Hook			; STUB_HOOK
Registers:	defs	10,0			; STUB_REGISTERS: BC, AF, HL, DE, IX
Drives:		defs	8,0			; STUB_DRIVES
HasTurbo:	defb	0			; STUB_HAS_TURBO
Segment:	defb	0			; STUB_SEGMENT
AutoRetry:	defb	0			; STUB_AUTO_RETRY
Bounce:		defs	STUB_BOUNCE_SIZE,0	; STUB_BOUNCE
Dpb:		defb	0,0			; STUB_DPB
		defw	512			; sector size

IF Registers <> STUB_REGISTERS
		ERROR	"stub.asm layout does not match stub.h"
ENDIF
IF HookOriginal <> STUB_HOOK_ORIGINAL
		ERROR	"stub.asm layout does not match stub.h"
ENDIF
IF AutoRetry <> STUB_AUTO_RETRY
		ERROR	"stub.asm layout does not match stub.h"
ENDIF
IF Bounce <> STUB_BOUNCE
		ERROR	"stub.asm layout does not match stub.h"
ENDIF
IF Dpb <> STUB_DPB
		ERROR	"stub.asm layout does not match stub.h"
ENDIF

; BDOS functions handled by the driver (bit n = function n, functions 00H to 7FH), same as g_aDosHandlers of
; driver.c. The functions 80H to FFH are never handled.
Functions:
		defb	00h,0C0h,01h,3Fh,80h,00h,00h,00h
		defb	7Fh,0E7h,7Fh,4Eh,20h,00h,00h,00h

SaveSP:		defw	0			; stack of the BDOS function
DriverSP:	defw	0			; stack of the driver
TpaSeg:		defb	0			; TPA segment of page 2

; ------------------------------------------------------------------------------
; GO_BDOS hook: the driver handles the function (A not 0 returned by the driver) or the previous hook is called.
; The functions the driver never handles (Functions) go directly to the previous hook, without mapping the driver.
; ------------------------------------------------------------------------------
Hook:
		push	hl			; function C handled by the driver (bit of Functions) ?
		push	af
		push	bc
		ld	a,c
		cp	80h
		jr	nc,Hook_Test		; functions 80H to FFH: not handled (no carry)
		rrca
		rrca
		rrca
		and	0Fh
		ld	hl,Functions
		add	a,l
		ld	l,a
		ld	a,0
		adc	a,h
		ld	h,a
		ld	a,c
		and	7
		ld	b,a
		inc	b
		ld	a,(hl)
Hook_Bit:	rrca
		djnz	Hook_Bit		; Cx = bit of the function
Hook_Test:	pop	bc
		jr	c,Hook_Driver
		pop	af
		pop	hl
		jp	HookOriginal

Hook_Driver:	pop	af
		pop	hl
		di
		ld	(SaveSP),sp
		ld	sp,Registers+10
		push	ix
		push	de
		push	hl
		push	af
		push	bc
		ld	sp,(SaveSP)
		call	GetP2			; A = TPA segment of page 2
		ld	(TpaSeg),a
		ld	a,(Segment)
		out	(MAPPER_P2),a		; driver in page 2
		ld	sp,DRIVER_STACK
		ld	hl,StubBase
		call	DriverEntry		; A = 0 not handled
		ld	b,a
		ld	a,(TpaSeg)
		out	(MAPPER_P2),a		; TPA in page 2 (stack not used before SP is restored)
		ld	a,b
		or	a
		jr	z,Hook_1
		ld	a,0C9h			; handled: ret
Hook_1:		ld	(RetOrNop),a		; not handled: nop, previous hook
		ld	sp,Registers
		pop	bc
		pop	af
		pop	hl
		pop	de
		pop	ix
		ld	sp,(SaveSP)
RetOrNop:	ret
		jp	HookOriginal

; ------------------------------------------------------------------------------
; Transfer to or from data of the program in page 2: TPA segment in page 2, stack of the BDOS function
; Input: HL = data, DE = size, A = 0 transmit, 1 receive
; Output: A = 1 received, 0 time-out (receive)
; ------------------------------------------------------------------------------
TpaXfer:	ld	(DriverSP),sp
		ld	b,a
		ld	a,(TpaSeg)
		out	(MAPPER_P2),a
		ld	sp,(SaveSP)
		ld	a,b
		or	a
		jr	nz,TpaXfer_Rx
		call	vJIOTransmit
		jr	TpaXfer_End
TpaXfer_Rx:	call	bJIOReceive
TpaXfer_End:	ld	b,a
		ld	a,(Segment)
		out	(MAPPER_P2),a
		ld	sp,(DriverSP)
		ld	a,b
		ret

; ------------------------------------------------------------------------------
; Copy data of the program to the bounce buffer, or the bounce buffer to data of the program (no stack used while
; the TPA segment is in page 2)
; Input: HL = TPA address, DE = size (max STUB_BOUNCE_SIZE)
; ------------------------------------------------------------------------------
FromCaller:	ld	a,(TpaSeg)
		out	(MAPPER_P2),a
		ld	b,d
		ld	c,e
		ld	de,Bounce
		ldir
		ld	a,(Segment)
		out	(MAPPER_P2),a
		ret

ToCaller:	ld	a,(TpaSeg)
		out	(MAPPER_P2),a
		ld	b,d
		ld	c,e
		ex	de,hl
		ld	hl,Bounce
		ldir
		ld	a,(Segment)
		out	(MAPPER_P2),a
		ret

; ------------------------------------------------------------------------------
; Original BDOS function (previous hook) with the registers of STUB_REGISTERS, TPA segment in page 2 and stack of
; the BDOS function. The results are stored in STUB_REGISTERS.
; ------------------------------------------------------------------------------
TpaOriginal:	ld	(DriverSP),sp
		ld	a,(TpaSeg)
		out	(MAPPER_P2),a
		ld	sp,Registers
		pop	bc
		pop	af
		pop	hl
		pop	de
		pop	ix
		ld	sp,(SaveSP)
		call	HookOriginal
		di
		ld	sp,Registers+10
		push	ix
		push	de
		push	hl
		push	af
		push	bc
		ld	a,(Segment)
		out	(MAPPER_P2),a
		ld	sp,(DriverSP)
		ret

; ------------------------------------------------------------------------------
; Serial routines (HL = data, DE = size; bJIOReceive returns A = 1 received, 0 time-out)
; 115K2 transmit/receive routines based on code by Nyyrikki
; ------------------------------------------------------------------------------
#include "transmit.asm"
#include "receive.asm"
