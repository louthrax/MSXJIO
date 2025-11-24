
#include "../../common/drv_jio.inc"
#include "../../common/msxdos2.h"

#define START_HANDLE 128

typedef unsigned char bool;

typedef void (*tdDosHandler)();
typedef unsigned int size_t;

#define false	0
#define true	1

#define A g_aoRegisters.c.a
#define B g_aoRegisters.c.b
#define C g_aoRegisters.c.c
#define D g_aoRegisters.c.d
#define E g_aoRegisters.c.e
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
#define FCB ((tdFileControlBlock *) g_aoRegisters.p.de)

typedef struct
{
	char			m_acSig1;
	char			m_acSig2;
	char			m_acSig3;
	unsigned char	m_ucFlags;
	unsigned char	m_ucCommand;
    unsigned char   m_ucFunction;
} tdCommonHeader;

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
char *                 g_pcSPSave = 0;
char *                 g_pcDiskTransferAddress = 0x80;
tdRegisters            g_aoRegisters = { { 0,0,0,0,0 } };
bool                   g_bHandledDrives[8] = { 0 };
tdCommonHeader	       g_oCommonHeader = {'J', 'I', 'O', 0, COMMAND_BDOS, 0};
unsigned char          g_ucPreviousErrorCode = 0;
bool                   g_bResult = 0;
const char             g_acDevicesNames[] = "CON\0PRN\0LST\0AUX\0NUL\0";
unsigned char          g_ucCurrentDisk = 0;
unsigned char          g_bHasTurbo = false;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void main(void) __naked
{
__asm
Hook:
    di
    ld      (_g_pcSPSave),sp
    ld      sp,_g_aoRegisters+10

    push    ix
    push    de
    push    hl
    push    af
    push    bc

    ld      sp,(_g_pcSPSave)

    call    _bDoCommand

    or      a
    jr      z,Hook_1
    ld      a,0xC9

Hook_1:
    ld      (RetOrNop),a

    ld      sp,_g_aoRegisters

    pop     bc
    pop     af
    pop     hl
    pop     de
    pop     ix
    ld      sp,(_g_pcSPSave)

RetOrNop:
    ret

Hook_OriginalCode:
    .db     0xC3
Hook_OriginalAddress:
    nop
    nop
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool streq(const char *s1, const char *s2) __naked
{
    s1;
    s2;
__asm
loop:
        ld      a,(de)

        cp      'a'
        jr      c,upper_done
        cp      'z'+1
        jr      nc,upper_done
        sub     32

upper_done:
        cp      (hl)
        jr      nz,diff
        or      a
        jr      z,eq
        inc     de
        inc     hl
        jr      loop
eq:
        inc     a
        ret
diff:
        xor     a
        ret
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static size_t strlen(const char *str) __naked
{
__asm

        ld      c,l
        ld      b,h

loop2:  ld      a,(hl)
        or      a
        inc     hl
        jr      nz,loop2

        dec     hl
        sbc     hl,bc
        ex      de,hl
        ret
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static bool bJIOReceive(void *_pvDestination, unsigned int _uiSize) __naked
{
    _pvDestination;
    _uiSize;
    __asm
#include "receive.asm"
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vJIOTransmit(void *_pvSource, unsigned int _uiSize) __naked
{
    _pvSource;
    _uiSize;
__asm
#include "transmit.asm"
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vTransmitString(char *_pcString)
{
    vJIOTransmit(_pcString, strlen(_pcString) + 1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vReceive(void *_pvAddress, unsigned int _uiLength)
{
    while(!bJIOReceive(_pvAddress, _uiLength));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bIsPhysicalDriveHandled(unsigned char _ucPhysicalDrive)
{
    return g_bHandledDrives[_ucPhysicalDrive];
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bIsLogicalDriveHandled(unsigned char _ucLogicalDrive)
{
    return bIsPhysicalDriveHandled(_ucLogicalDrive ? _ucLogicalDrive - 1 : g_ucCurrentDisk);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bIsDeviceName(const char *s)
{
    char * szName;

    for(szName = g_acDevicesNames; *szName; szName += 4)
    {
        if (streq(szName, s))
            return true;
    }
    return false;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vSendCommonHeader()
{
    vJIOTransmit((void*)&g_oCommonHeader, sizeof(g_oCommonHeader));
    g_bResult = true;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vTransmitPathOrFIB(bool _bAlsoSendHL)
{
    vSendCommonHeader();

    if (DE[0] == 0xFF)
    {
        vJIOTransmit(DE, sizeof(tdFileInfoBlock));
        if (_bAlsoSendHL)
          vTransmitString(HL);
    }
    else
        vTransmitString(DE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bIsPathOrFIBHandled(unsigned char * _pucFIB)
{
    unsigned char ucDrive;
    
    ucDrive = 0;
    if (_pucFIB[0] == 0xFF)
    {
        if (bIsDeviceName(_pucFIB+1) || (((tdFileInfoBlock*)_pucFIB)->m_cAttributes & 128))
            return false;
        ucDrive = ((tdFileInfoBlock*)_pucFIB)->m_ucDrive;
    }
    else
    {
        if (bIsDeviceName(_pucFIB))
            return false;
        if (_pucFIB[0] && (_pucFIB[1] == ':'))
        {
            ucDrive = _pucFIB[0] - 'A' + 1;
            if (ucDrive >= 32)
                ucDrive -= 32;
        }
    }

    return bIsLogicalDriveHandled(ucDrive);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bIsPathOrFIBHandled_DE()
{
    return bIsPathOrFIBHandled(DE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_FIND_FIRST_ENTRY()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(true);
        vJIOTransmit(&B, sizeof(B));

        vReceive(FIB, sizeof(tdFileInfoBlock));

        A = FIB->m_ucResult;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_FIND_NEW_ENTRY()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(true);
        vJIOTransmit(&B, sizeof(B));
        vJIOTransmit(IX+1, 13);
        vReceive(FIB, sizeof(tdFileInfoBlock));

        A = FIB->m_ucResult;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_FIND_NEXT_ENTRY()
{
    if (bIsPathOrFIBHandled(IX))
    {
        vSendCommonHeader();
        vJIOTransmit(FIB, sizeof(*FIB));

        vReceive(FIB, sizeof(*FIB));
        A = FIB->m_ucResult;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_ALLOCATION_INFORMATION()
{
    if (bIsLogicalDriveHandled(E))
    {
        vSendCommonHeader();
        A = 2;
        BCi = 512;
        DEi = 60000;
        HLi = 30000;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_CHANGE_CURRENT_DIRECTORY()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vSendCommonHeader();
        vTransmitString(DE);

        vReceive(&A, sizeof(A));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_OPEN_FILE_HANDLE()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(false);
        vJIOTransmit(&A, sizeof(A));

        vReceive(&BC, sizeof(BC));
        A = C;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_CLOSE_FILE_HANDLE()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));

        vReceive(&A, sizeof(A));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_READ_FROM_FILE_HANDLE()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));
        vJIOTransmit(&HL, sizeof(HL));

        vReceive(&A, sizeof(A) + sizeof(HL));
        if (HL)
            vReceive(DE, (unsigned int)HL);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_WRITE_TO_FILE_HANDLE()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));
        vJIOTransmit(&HL, sizeof(HL));
        if (HL)
            vJIOTransmit(DE, (unsigned int)HL);

        vReceive(&A, sizeof(A) + sizeof(HL));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_MOVE_FILE_HANDLE_POINTER()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));
        vJIOTransmit(&A, sizeof(A));
        vJIOTransmit(&HL, sizeof(HL) + sizeof(DE));

        vReceive(&A, sizeof(A) + sizeof(HL) + sizeof(DE));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_CURRENT_DIRECTORY()
{
    if (bIsLogicalDriveHandled(B))
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));

        vReceive(&L, sizeof(L));
        vReceive(DE, L);
        A = 0;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_CREATE_FILE_HANDLE()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vSendCommonHeader();
        vTransmitString(DE);
        vJIOTransmit(&A, sizeof(A));
        vJIOTransmit(&B, sizeof(B));

        vReceive(&BC, sizeof(BC));
        A = C;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_ENSURE_FILE_HANDLE()
{
    vSendCommonHeader();
    A = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_WHOLE_PATH_STRING()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(false);
        vReceive(&A, sizeof(A) + sizeof(HL));
        vReceive(DE, H);
        HLi = DEi + L;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_DELETE_FILE_OR_SUBDIRECTORY()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(false);
        vReceive(&A, sizeof(A));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_SET_FILE_ATTRIBUTES()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(false);
        vJIOTransmit(&A, sizeof(A) + sizeof(L));

        vReceive(&A, sizeof(A) + sizeof(L));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&A, sizeof(A) + sizeof(HL) + sizeof(DE) + sizeof(IX));
        vReceive(&A, sizeof(A) + sizeof(HL) + sizeof(DE));
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_SET_DISK_TRANSFER_ADDRESS()
{
    g_pcDiskTransferAddress = DE;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GENERIC_FCB_HANDLER()
{
    vSendCommonHeader();
    vJIOTransmit(DE, sizeof(tdFileControlBlock));
    vReceive(DE, sizeof(tdFileControlBlock));

    A = L = ((tdFileControlBlock*)DE)->m_ucResult;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_RANDOM_BLOCK_READ_FCB()
{
    vSendCommonHeader();
    ((tdFileControlBlock*)DE)->m_uiNumberOfRecords = HL;
    ((tdFileControlBlock*)DE)->m_uiDTA = g_pcDiskTransferAddress;
    vJIOTransmit(DE, sizeof(tdFileControlBlock));

    vReceive(DE, sizeof(tdFileControlBlock));
    HL = DE = ((tdFileControlBlock*)DE)->m_uiNumberOfRecords;
    if (HL)
         vReceive(g_pcDiskTransferAddress, (unsigned int)HL);

    A = ((tdFileControlBlock*)DE)->m_ucResult;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_SELECT_DISK()
{
    if (bIsPhysicalDriveHandled(E))
    {
        vSendCommonHeader();
        vJIOTransmit(&E, sizeof(E));
        g_ucCurrentDisk = E;
        vReceive(&A, sizeof(A));
        L = A;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_PREVIOUS_ERROR_CODE()
{
    vSendCommonHeader();
    A = 0;
    B = g_ucPreviousErrorCode;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_LOGIN_VECTOR()
{
    vSendCommonHeader();
    H = 0;
    L = 15;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_CURRENT_DRIVE()
{
    vSendCommonHeader();

    L = A = g_ucCurrentDisk;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_PARSE_PATHNAME()
{
    g_bResult = true;
__asm
    ld    de,(_g_aoRegisters + 6)
    ld    bc,(_g_aoRegisters + 0)

    call  Hook_OriginalCode

    ld    (_g_aoRegisters + 6),de
    ld    (_g_aoRegisters + 0),bc
    ld    (_g_aoRegisters + 4),hl
    ld    (_g_aoRegisters + 3),a

    bit   2,b
    ret   nz

    ld a,(_g_ucCurrentDisk)
    inc a
    ld (_g_aoRegisters + 0),a
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vCheckRFS()
{
    g_bResult = true;
    L = 'R';
    DE = (unsigned char*)main;
}

typedef struct
{
    unsigned char code;    // DOS function number (C)
    tdDosHandler  handler; // function pointer
} tdDosDispatchEntry;

static const tdDosDispatchEntry g_aDosHandlers[] =
{
    { 0x0E, vDOS_SELECT_DISK },
    { 0x0F, vDOS_GENERIC_FCB_HANDLER }, // DOS_OPEN_FILE_FCB
    { 0x10, vDOS_GENERIC_FCB_HANDLER }, // DOS_CLOSE_FILE_FCB
    { 0x18, vDOS_GET_LOGIN_VECTOR },
    { 0x19, vDOS_GET_CURRENT_DRIVE },
    { 0x1A, vDOS_SET_DISK_TRANSFER_ADDRESS },
    { 0x1B, vDOS_GET_ALLOCATION_INFORMATION },
    { 0x1C, vCheckRFS },
    { 0x1D, vSendCommonHeader },
    { 0x27, vDOS_RANDOM_BLOCK_READ_FCB },
    { 0x40, vDOS_FIND_FIRST_ENTRY },
    { 0x41, vDOS_FIND_NEXT_ENTRY },
    { 0x42, vDOS_FIND_NEW_ENTRY },
    { 0x43, vDOS_OPEN_FILE_HANDLE },
    { 0x44, vDOS_CREATE_FILE_HANDLE },
    { 0x45, vDOS_CLOSE_FILE_HANDLE },
    { 0x46, vDOS_ENSURE_FILE_HANDLE },
    { 0x48, vDOS_READ_FROM_FILE_HANDLE },
    { 0x49, vDOS_WRITE_TO_FILE_HANDLE },
    { 0x4A, vDOS_MOVE_FILE_HANDLE_POINTER },
    { 0x4D, vDOS_DELETE_FILE_OR_SUBDIRECTORY },
    { 0x50, vDOS_GET_SET_FILE_ATTRIBUTES },
    { 0x56, vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME },
    { 0x59, vDOS_GET_CURRENT_DIRECTORY },
    { 0x5A, vDOS_CHANGE_CURRENT_DIRECTORY },
    { 0x5B, vDOS_PARSE_PATHNAME },
    { 0x5E, vDOS_GET_WHOLE_PATH_STRING },
    { 0x65, vDOS_GET_PREVIOUS_ERROR_CODE },
    { 0, 0 }
};


#define CHGCPU 0x0180
#define GETCPU 0x0183
#define EXPTBL 0xFCC1
#define CALSLT 0x001C

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char cGetCPU() __naked
{
__asm
        ld	ix,GETCPU
        ld	iy,(EXPTBL-1)
        jp	CALSLT
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vSetCPU(char _cCPUMode) __naked
{
__asm
        ld	ix,CHGCPU
        ld	iy,(EXPTBL-1)
        call	CALSLT
        di
        ret
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static bool bDoCommand()
{
    const tdDosDispatchEntry* pEntry;

    g_bResult = false;

    for (pEntry = g_aDosHandlers; pEntry->code && (pEntry->code != C); pEntry++);

    if (pEntry->code)
    {
__asm
        ld a,(_g_bHasTurbo)
        or  a
        jr  z,noTurbo1

        push    iy

        ex af,af'
        push  af
        ex af,af'

        exx
        push hl
        push de
        push bc
        exx

        call _cGetCPU

        push  af

        xor a
        call _vSetCPU

noTurbo1:
__endasm;

    g_oCommonHeader.m_ucFunction = C;
    pEntry->handler();
    g_ucPreviousErrorCode = A;

__asm
        ld a,(_g_bHasTurbo)
        or  a
        jr  z,noTurbo2

        pop af
        call _vSetCPU

        exx
        pop bc
        pop de
        pop hl
        exx

        ex af,af'
        pop af
        ex af,af'

        pop     iy
noTurbo2:
__endasm;

    }

    return g_bResult;
}
