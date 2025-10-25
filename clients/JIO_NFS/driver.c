
#include "../../common/drv_jio.inc"
#include "../../common/msxdos2.h"

typedef void (*tdDosHandler)();
typedef unsigned char bool;
typedef unsigned int size_t;

#define false	0
#define true	1

#define A g_aoRegisters.c.a
#define B g_aoRegisters.c.b
#define C g_aoRegisters.c.c
#define H g_aoRegisters.c.h
#define L g_aoRegisters.c.l

#define BC  g_aoRegisters.p.bc
#define DE  g_aoRegisters.p.de
#define HL  g_aoRegisters.p.hl
#define IX  g_aoRegisters.p.ix

#define BCi g_aoRegisters.i.bc
#define DEi g_aoRegisters.i.de
#define HLi g_aoRegisters.i.hl

#define FIB ((tdFileInfoBlock *) g_aoRegisters.p.ix)

typedef struct
{
	char			m_acSig1;
	char			m_acSig2;
	char			m_acSig3;
	unsigned char	m_ucFlags;
	unsigned char	m_ucCommand;
    unsigned char   m_ucFunction;
} tdCommonHeader;


typedef struct
{
    unsigned char m_ucFF;					        /*      0 - Always 0FFh */
    char m_acFileName[13]; 					        /*  1..13 - Filename as an ASCIIZ string */
    unsigned char m_cAttributes;			        /*     14 - File attributes byte */
    unsigned short int m_uiLastModificationTime;	/* 15..16 - Time of last modification */
    unsigned short int m_uiLastModificationDate;	/* 17..18 - Date of last modification */
    unsigned short int m_uiStartCluster;			/* 19..20 - Start cluster */
    unsigned long  m_ulFileSize; 			        /* 21..24 - File size */
    unsigned char m_ucDrive; 				        /*     25 - Logical drive */
    unsigned char m_acQtFile[8];
    char m_acRegExp[13];
    unsigned char m_ucResult;
}
    tdFileInfoBlock;

typedef union
{
    struct {
    unsigned char *bc;
    unsigned char *af;
    unsigned char *hl;
    unsigned char *de;
    unsigned char *ix;
    } p;

    struct {
    unsigned int bc;
    unsigned int af;
    unsigned int hl;
    unsigned int de;
    unsigned int ix;
    } i;

    struct {
    unsigned char c;
    unsigned char b;
    unsigned char f;
    unsigned char a;
    unsigned char l;
    unsigned char h;
    unsigned char e;
    unsigned char d;
    unsigned char ixl;
    unsigned char ixh;
    } c;
}
    tdRegisters;


/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void install(void) __naked
{
__asm
    ld                              hl,(0xF37B)
    ld                              (Hook_OriginalCode),hl

    ld                              a,0xC3
    ld                              (0xF37A),a

    ld                              hl,Hook
    ld                              (0xF37B),hl
    ret
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

 /* All globals need to be initialized in order to be linked at the end of the binary */

unsigned int SPSave = {0};
tdRegisters g_aoRegisters = { { 0,0,0,0,0 } };
tdCommonHeader	g_oCommonHeader = {0};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void main(void) __naked
{
__asm
Hook:
    di
    ld      (_SPSave),sp
    ld      sp,_g_aoRegisters+10

    push    ix
    push    de
    push    hl
    push    af
    push    bc

    ld      sp,(_SPSave)

    call    _bDoCommand

    or      a
    jr      z,OriginalCode

    ld      sp,_g_aoRegisters

    pop     bc
    pop     af
    pop     hl
    pop     de
    pop     ix
    ld      sp,(_SPSave)
    ret

OriginalCode:
    ld      sp,_g_aoRegisters
    pop     bc
    pop     af
    pop     hl
    pop     de
    pop     ix
    ld      sp,(_SPSave)

    .db     0xC3
Hook_OriginalCode:
    nop
    nop
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
size_t my_strlen(const char *str)
{
    const char *s = str;

    while (*s)
        ++s;

    return (size_t)(s - str);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bJIOReceive(void *_pvDestination, unsigned int _uiSize) __naked
{
__asm

bJIOReceive:
                                ld                              h,d
                                ld                              l,e

                                ld                              d,b
                                ld                              e,c

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
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vJIOTransmit(void *_pvSource, unsigned int _uiSize) __naked
{
__asm
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
                                out	(0xa0),a
                                in	a,(0xa2)
                                or	4
                                ld	e,a
                                xor	4
                                ld	d,a
                                ld	c,0xa1

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
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vTransmitString(char *_pcString)
{
    vJIOTransmit(_pcString, my_strlen(_pcString) + 1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vReceive(void *_pvAddress, unsigned int _uiLength)
{
	while(!bJIOReceive(_pvAddress, _uiLength));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_FIND_FIRST_ENTRY()
{
    vJIOTransmit(&B, sizeof(B));

    if (DE[0] == 0xFF)
    {
        vJIOTransmit(DE, sizeof(tdFileInfoBlock));
        vTransmitString(HL);
    }
    else
        vTransmitString(DE);

    vReceive(FIB, sizeof(tdFileInfoBlock));
    A = FIB->m_ucResult;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_FIND_NEXT_ENTRY()
{
    vJIOTransmit(FIB, sizeof(*FIB));

    vReceive(FIB, sizeof(*FIB));
    A = FIB->m_ucResult;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_GET_ALLOCATION_INFO()
{
    A = 2;
    BCi = 512;
    DEi = 60000;
    HLi = 30000;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_CHANGE_CURRENT_DIRECTORY()
{
    vTransmitString(DE);

    vReceive(&A, sizeof(A));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_OPEN_FILE_HANDLE()
{
    vJIOTransmit(&A, sizeof(A));

    if (DE[0] == 0xFF)
        vJIOTransmit(DE, sizeof(tdFileInfoBlock));
    else
        vTransmitString(DE);

    vReceive(&BC, sizeof(BC));
    A = C;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_CLOSE_FILE_HANDLE()
{
    vJIOTransmit(&B, sizeof(B));

    vReceive(&A, sizeof(A));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_READ_FILE_HANDLE()
{
    vJIOTransmit(&B, sizeof(B));
    vJIOTransmit(&HL, sizeof(HL));

    vReceive(&A, sizeof(A) + sizeof(HL));
    if (HL)
        vReceive(DE, (unsigned int)HL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_WRITE_FILE_HANDLE()
{
    vJIOTransmit(&B, sizeof(B));
    vJIOTransmit(&HL, sizeof(HL));
    if (HL)
        vJIOTransmit(DE, (unsigned int)HL);

    vReceive(&A, sizeof(A) + sizeof(HL));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_MOVE_FILE_POINTER()
{
    vJIOTransmit(&B, sizeof(B));
    vJIOTransmit(&A, sizeof(A));
    vJIOTransmit(&HL, sizeof(HL) + sizeof(DE));

    vReceive(&A, sizeof(A) + sizeof(HL) + sizeof(DE));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_GET_CURRENT_DIRECTORY()
{
    vJIOTransmit(&B, sizeof(B));

    vReceive(&L, sizeof(L));
    vReceive(DE, L);
    A = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_CREATE_FILE_HANDLE()
{
    vTransmitString(DE);
    vJIOTransmit(&A, sizeof(A));
    vJIOTransmit(&B, sizeof(B));

    vReceive(&BC, sizeof(BC));
    A = C;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_ENSURE_FILE_HANDLE()
{
    A = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_GET_WHOLE_PATH()
{
    if (DE[0] == 0xFF)
        vJIOTransmit(DE, sizeof(tdFileInfoBlock));
    else
        vTransmitString(DE);

    vReceive(&A, sizeof(A) + sizeof(HL));
    vReceive(DE, H);
    HLi = DEi + L;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_DELETE_FILE_SUBDIR()
{
    if (DE[0] == 0xFF)
        vJIOTransmit(DE, sizeof(tdFileInfoBlock));
    else
        vTransmitString(DE);

    vReceive(&A, sizeof(A));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_GET_SET_FILE_ATTRBIUTES()
{
    if (DE[0] == 0xFF)
        vJIOTransmit(DE, sizeof(tdFileInfoBlock));
    else
        vTransmitString(DE);

    vJIOTransmit(&A, sizeof(A) + sizeof(L));

    vReceive(&A, sizeof(A) + sizeof(L));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vDOS_FILE_DATE_TIME()
{
    vJIOTransmit(&A, sizeof(A) + sizeof(HL) + sizeof(DE) + sizeof(IX));
    vReceive(&A, sizeof(A) + sizeof(HL) + sizeof(DE));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static const tdDosHandler g_aDosHandlers[] =
{
    /* 0x00 DOS_PROGRAM_TERMINATE          */ 0,
    /* 0x01 DOS_CONSOLE_INPUT              */ 0,
    /* 0x02 DOS_CONSOLE_OUTPUT             */ 0,
    /* 0x03 DOS_AUX_INPUT                  */ 0,
    /* 0x04 DOS_AUX_OUTPUT                 */ 0,
    /* 0x05 DOS_PRINTER_OUTPUT             */ 0,
    /* 0x06 DOS_DIRECT_CONSOLE_IO          */ 0,
    /* 0x07 DOS_DIRECT_CONSOLE_INPUT       */ 0,
    /* 0x08 DOS_CONSOLE_INPUT_WITHOUT_ECHO */ 0,
    /* 0x09 DOS_STRING_OUTPUT              */ 0,
    /* 0x0A DOS_BUFFERED_LINE_INPUT        */ 0,
    /* 0x0B DOS_CONSOLE_STATUS             */ 0,
    /* 0x0C DOS_RETURN_VERSION_NUMBER      */ 0,
    /* 0x0D DOS_DISK_RESET                 */ 0,
    /* 0x0E DOS_SELECT_DISK                */ 0,
    /* 0x0F DOS_OPEN_FILE_FCB              */ 0,
    /* 0x10 DOS_CLOSE_FILE_FCB             */ 0,
    /* 0x11 DOS_SEARCH_FIRST_FCB           */ 0,
    /* 0x12 DOS_SEARCH_NEXT_FCB            */ 0,
    /* 0x13 DOS_DELETE_FILE_FCB            */ 0,
    /* 0x14 DOS_SEQUENTIAL_READ_FCB        */ 0,
    /* 0x15 DOS_SEQUENTIAL_WRITE_FCB       */ 0,
    /* 0x16 DOS_CREATE_FILE_FCB            */ 0,
    /* 0x17 DOS_RENAME_FILE_FCB            */ 0,
    /* 0x18 DOS_GET_LOGIN_VECTOR           */ 0,
    /* 0x19 DOS_GET_CURRENT_DRIVE          */ 0,
    /* 0x1A DOS_SET_DMA_ADDRESS            */ 0,
    /* 0x1B DOS_GET_ALLOCATION_INFO        */ vDOS_GET_ALLOCATION_INFO,
    /* 0x1C                                */ 0,
    /* 0x1D                                */ 0,
    /* 0x1E                                */ 0,
    /* 0x1F                                */ 0,
    /* 0x20                                */ 0,
    /* 0x21 DOS_RANDOM_READ_FCB            */ 0,
    /* 0x22 DOS_RANDOM_WRITE_FCB           */ 0,
    /* 0x23 DOS_GET_FILE_SIZE_FCB          */ 0,
    /* 0x24 DOS_SET_RANDOM_RECORD_FCB      */ 0,
    /* 0x25                                */ 0,
    /* 0x26 DOS_RANDOM_BLOCK_WRITE_FCB     */ 0,
    /* 0x27 DOS_RANDOM_BLOCK_READ_FCB      */ 0,
    /* 0x28 DOS_RANDOM_WRITE_ZERO_FILL_FCB */ 0,
    /* 0x29                                */ 0,
    /* 0x2A DOS_GET_DATE                   */ 0,
    /* 0x2B DOS_SET_DATE                   */ 0,
    /* 0x2C DOS_GET_TIME                   */ 0,
    /* 0x2D DOS_SET_TIME                   */ 0,
    /* 0x2E DOS_SET_VERIFY_FLAG            */ 0,
    /* 0x2F DOS_ABS_SECTOR_READ            */ 0,
    /* 0x30 DOS_ABS_SECTOR_WRITE           */ 0,
    /* 0x31 DOS_GET_DISK_PARAMETERS        */ 0,
    /* 0x32                                */ 0,
    /* 0x33                                */ 0,
    /* 0x34                                */ 0,
    /* 0x35                                */ 0,
    /* 0x36                                */ 0,
    /* 0x37                                */ 0,
    /* 0x38                                */ 0,
    /* 0x39                                */ 0,
    /* 0x3A                                */ 0,
    /* 0x3B                                */ 0,
    /* 0x3C                                */ 0,
    /* 0x3D                                */ 0,
    /* 0x3E                                */ 0,
    /* 0x3F                                */ 0,
    /* 0x40 DOS_FIND_FIRST_ENTRY           */ vDOS_FIND_FIRST_ENTRY,
    /* 0x41 DOS_FIND_NEXT_ENTRY            */ vDOS_FIND_NEXT_ENTRY,
    /* 0x42 DOS_FIND_NEW_ENTRY             */ vDOS_FIND_FIRST_ENTRY,
    /* 0x43 DOS_OPEN_FILE_HANDLE           */ vDOS_OPEN_FILE_HANDLE,
    /* 0x44 DOS_CREATE_FILE_HANDLE         */ vDOS_CREATE_FILE_HANDLE,
    /* 0x45 DOS_CLOSE_FILE_HANDLE          */ vDOS_CLOSE_FILE_HANDLE,
    /* 0x46 DOS_ENSURE_FILE_HANDLE         */ vDOS_ENSURE_FILE_HANDLE,
    /* 0x47 DOS_DUPLICATE_FILE_HANDLE      */ 0,
    /* 0x48 DOS_READ_FILE_HANDLE           */ vDOS_READ_FILE_HANDLE,
    /* 0x49 DOS_WRITE_FILE_HANDLE          */ vDOS_WRITE_FILE_HANDLE,
    /* 0x4A DOS_MOVE_FILE_POINTER          */ vDOS_MOVE_FILE_POINTER,
    /* 0x4B DOS_IO_CONTROL_DEVICES         */ 0,
    /* 0x4C DOS_TEST_FILE_HANDLE           */ 0,
    /* 0x4D DOS_DELETE_FILE_SUBDIR         */ vDOS_DELETE_FILE_SUBDIR,
    /* 0x4E DOS_RENAME_FILE_SUBDIR         */ 0,
    /* 0x4F DOS_MOVE_FILE_SUBDIR           */ 0,
    /* 0x50 DOS_GET_SET_FILE_ATTRIBUTES    */ vDOS_GET_SET_FILE_ATTRBIUTES,
    /* 0x51 DOS_FILE_DATE_TIME             */ 0,
    /* 0x52                                */ 0,
    /* 0x53                                */ 0,
    /* 0x54                                */ 0,
    /* 0x55                                */ 0,
    /* 0x56 DOS_GET_SET_FILE_HANDLE_DATE_AND_TIME */ vDOS_FILE_DATE_TIME,
    /* 0x57 DOS_GET_DMA_ADDRESS            */ 0,
    /* 0x58 DOS_GET_VERIFY_FLAG            */ 0,
    /* 0x59 DOS_GET_CURRENT_DIRECTORY      */ vDOS_GET_CURRENT_DIRECTORY,
    /* 0x5A DOS_CHANGE_CURRENT_DIRECTORY   */ vDOS_CHANGE_CURRENT_DIRECTORY,
    /* 0x5B DOS_PARSE_PATHNAME             */ 0,
    /* 0x5C DOS_PARSE_FILENAME             */ 0,
    /* 0x5D DOS_CHECK_CHARACTER            */ 0,
    /* 0x5E DOS_GET_WHOLE_PATH             */ vDOS_GET_WHOLE_PATH,
    /* 0x5F DOS_FLUSH_DISK_BUFFERS         */ 0,
    /* 0x60 DOS_FORK_CHILD_PROCESS         */ 0,
    /* 0x61 DOS_REJOIN_PARENT_PROCESS      */ 0,
    /* 0x62 DOS_TERMINATE_WITH_ERROR       */ 0,
    /* 0x63 DOS_DEFINE_ABORT_ROUTINE       */ 0,
    /* 0x64 DOS_DEFINE_DISK_ERROR_HANDLER  */ 0,
    /* 0x65 DOS_GET_PREVIOUS_ERROR         */ 0,
    /* 0x66 DOS_EXPLAIN_ERROR              */ 0,
    /* 0x67 DOS_FORMAT_DISK                */ 0,
    /* 0x68 DOS_CREATE_DESTROY_RAMDISK     */ 0,
    /* 0x69 DOS_ALLOCATE_SECTOR_BUFFERS    */ 0,
    /* 0x6A DOS_LOGICAL_DRIVE_ASSIGNMENT   */ 0,
    /* 0x6B DOS_GET_ENVIRONMENT_ITEM       */ 0,
    /* 0x6C DOS_SET_ENVIRONMENT_ITEM       */ 0,
    /* 0x6D DOS_FIND_ENVIRONMENT_ITEM      */ 0,
    /* 0x6E DOS_DISK_CHECK_STATUS          */ 0,
    /* 0x6F DOS_GET_DOS_VERSION            */ 0,
    /* 0x70 DOS_REDIRECTION_STATUS         */ 0
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bDoCommand()
{
    if (g_aDosHandlers[C])
    {
//        g_oCommonHeader.m_ucFunction = C;
//        vJIOTransmit((void*)&g_oCommonHeader, sizeof(g_oCommonHeader));
//        g_aDosHandlers[C]();

        return false;
    }
    else
    {
/*
        g_oCommonHeader.m_ucFunction = 0xFF;
        vJIOTransmit((void*)&g_oCommonHeader, sizeof(g_oCommonHeader));
        vJIOTransmit(&C, sizeof(C));
*/
      return false;
   }
}
