; ------------------------------------------------------------------------------
; unpack.asm
; Start of the BLOAD files of JIO-ROM.CAS and JIO-CAS.CAS (BLOAD"CAS:",R): the loader (jiocas.asm), compressed with
; BitBuster, is unpacked to TAPE_ADR and started (shorter file: faster to load from the tape).
; Assembled by 0_Build.sh with TAPE_ADR (loader), PACK_ADR (compressed loader) and STUB_ADR (this code): the BLOAD
; file is the compressed loader then this code, ending just below TAPE_ADR. The depacker writes upwards from
; TAPE_ADR: the compressed data, below, is never overwritten. The stack of BLOAD is kept (return to BASIC).
; ------------------------------------------------------------------------------

		org	STUB_ADR

		di				; alternate registers used by the depacker
		ld	hl,PACK_ADR
		ld	de,TAPE_ADR
		call	Depack
		ei
		jp	TAPE_ADR

INCLUDE "depack.inc"
