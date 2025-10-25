
extern char driver_start;
extern char driver_end;
extern char driver_reloc_start;
extern char driver_reloc_end;

extern char jumper_start;
extern char jumper_end;
extern char jumper_reloc_start;
extern char jumper_reloc_end;

void main(char * driver_target);

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vSetStack(void * _pvNewStack) __naked
{
__asm
    ld  hl,(0xF349)
    ld  de,_driver_end
    or a
    sbc hl,de
    ld de,_driver_start
    add hl,de
    ld  (0xF349),hl
    ld sp,hl
    jp _main
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vJumpTo( char *_pcAddress) __naked
{
__asm
    nop
    jp (hl)
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vMemCopy(void *dest, const void *src, unsigned int n)
{
__asm
    ex      de,hl
	ld	    c,(ix+4)
	ld	    b,(ix+5)
    ldir
__endasm;
}
/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vRelocate(char * _pcCodeStart, unsigned int * _puiRelocationStart, unsigned int _uiSize)
{
    while (_uiSize)
    {
        *((unsigned int*)(_pcCodeStart + *_puiRelocationStart)) += (unsigned int) _pcCodeStart;
        _uiSize--;
        _puiRelocationStart++;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void main(char * driver_target)
{
    char * jumper_target;

    jumper_target = 0x8100;

    vMemCopy(jumper_target, &jumper_start, &jumper_end - &jumper_start);
    vRelocate(jumper_target, &jumper_reloc_start, (&jumper_reloc_end - &jumper_reloc_start) >> 1);

    vMemCopy(driver_target, &driver_start, &driver_end - &driver_start);
    vRelocate(driver_target, &driver_reloc_start, (&driver_reloc_end - &driver_reloc_start) >> 1);

    vJumpTo(driver_target);
    vJumpTo(jumper_target);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void binaries(void) __naked
{
__asm

_driver_start:
    INCBIN "driver"
_driver_end:

_driver_reloc_start:
    INCBIN "driver.reloc"
_driver_reloc_end:

_jumper_start:
    INCBIN "jumper"
_jumper_end:

_jumper_reloc_start:
    INCBIN "jumper.reloc"
_jumper_reloc_end:

__endasm;
}
