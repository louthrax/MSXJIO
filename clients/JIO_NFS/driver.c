
#include "../../common/drv_jio.inc"
#include "../../common/msxdos2.h"
#include "stub.h"

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
#define IXi g_aoRegisters.i.ix

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
    The driver is in a system segment of the memory mapper, mapped in page 2 (DRIVER_BASE) by the resident stub
    (stub.asm) while a BDOS function is handled. Its variables stay in the segment between the calls.
    The data of the program (TPA) in page 2 are not visible while the driver is mapped: the parameters of the
    program (paths, FIB, FCB, ...) are copied with vFromCaller / vToCaller, its data are transferred with
    vCallerTransmit / vCallerReceive. The other pages are the pages of the program.
 =======================================================================================================================
 */
unsigned char *        g_pucStub = 0;                   // resident stub (stub.h)
char *                 g_pcDiskTransferAddress = 0x80;
tdRegisters            g_aoRegisters = { { 0,0,0,0,0 } };   // registers of the BDOS function (copy of the stub)
tdCommonHeader	       g_oCommonHeader = {'J', 'I', 'O', 0, COMMAND_BDOS, 0};
unsigned char          g_ucPreviousErrorCode = 0;
bool                   g_bResult = 0;
const char             g_acDevicesNames[] = "CON\0PRN\0LST\0AUX\0NUL\0";
unsigned char          g_ucCurrentDisk = 0;
unsigned char          g_bHasTurbo = false;
bool                   g_bLastFindHandled = false;  // last _FFIRST/_FNEXT/_FNEW on a JIO drive (for _WPATH)
// All the variables are initialized: they are then in the code of the driver (an uninitialized variable would be at
// address 0 of the driver, over its code)
unsigned char          g_aucPath[66] = { 0 };       // copy of the path or FIB parameter (DE or IX)
unsigned char          g_aucString[66] = { 0 };     // copy of a string parameter (HL), FIB template
tdFileInfoBlock        g_oFIB = { 0 };              // FIB answered by the server
tdFileControlBlock     g_oFCB = { 0 };              // copy of the FCB parameter (DE)

static bool bDoCommand();

/*
 =======================================================================================================================
    Entry of the driver, called by the stub with HL = stub: registers copied from and to the stub.
    Returns A = 0 if the function is not handled (previous hook called by the stub).
 =======================================================================================================================
 */
void vDriverEntry(void) __naked
{
__asm
    ld      (_g_pucStub),hl
    ld      de,STUB_REGISTERS
    add     hl,de
    push    hl
    ld      de,_g_aoRegisters
    ld      bc,10
    ldir

    call    _bDoCommand

    pop     de
    push    af
    ld      hl,_g_aoRegisters
    ld      bc,10
    ldir
    pop     af
    ret
__endasm;
}

/*
 =======================================================================================================================
    Routines of the stub (stub.h): jump to stub + offset, registers kept
 =======================================================================================================================
 */
static void vStubTransmit(void *_pvSource, unsigned int _uiSize) __naked
{
    _pvSource;
    _uiSize;
__asm
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_TRANSMIT
    add     hl,bc
    ex      (sp),hl
    ret
__endasm;
}

static bool bStubReceive(void *_pvDestination, unsigned int _uiSize) __naked
{
    _pvDestination;
    _uiSize;
__asm
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_RECEIVE
    add     hl,bc
    ex      (sp),hl
    ret
__endasm;
}

static void vStubXferTransmit(void *_pvSource, unsigned int _uiSize) __naked
{
    _pvSource;
    _uiSize;
__asm
    xor     a
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_XFER
    add     hl,bc
    ex      (sp),hl
    ret
__endasm;
}

static bool bStubXferReceive(void *_pvDestination, unsigned int _uiSize) __naked
{
    _pvDestination;
    _uiSize;
__asm
    ld      a,1
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_XFER
    add     hl,bc
    ex      (sp),hl
    ret
__endasm;
}

static void vStubFromCaller(const void *_pvSource, unsigned int _uiSize) __naked
{
    _pvSource;
    _uiSize;
__asm
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_FROM_CALLER
    add     hl,bc
    ex      (sp),hl
    ret
__endasm;
}

static void vStubToCaller(void *_pvDestination, unsigned int _uiSize) __naked
{
    _pvDestination;
    _uiSize;
__asm
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_TO_CALLER
    add     hl,bc
    ex      (sp),hl
    ret
__endasm;
}

static void vStubOriginal(void) __naked
{
__asm
    ld      hl,(_g_pucStub)
    ld      bc,STUB_ORIGINAL
    add     hl,bc
    jp      (hl)
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

static void vCopy(unsigned char *_pucDestination, const unsigned char *_pucSource, unsigned int _uiSize)
{
    while (_uiSize--)
        *_pucDestination++ = *_pucSource++;
}

/*
 =======================================================================================================================
    Memory of the program (TPA)
 =======================================================================================================================
 */

// Data of the program in page 2 (hidden by the driver)
static bool bInPage2(const void *_pvAddress, unsigned int _uiSize)
{
    unsigned int uiStart = (unsigned int) _pvAddress;
    unsigned int uiLast;

    if (!_uiSize)
        return false;

    uiLast = uiStart + _uiSize - 1;
    if (uiLast < uiStart)
        uiLast = 0xFFFF;

    return (uiStart <= 0xBFFF) && (uiLast >= 0x8000);
}

// Copy from the program to the driver
static void vFromCaller(void *_pvDestination, const void *_pvSource, unsigned int _uiSize)
{
    unsigned char       *pucDestination = _pvDestination;
    const unsigned char *pucSource = _pvSource;
    unsigned int        uiSize;

    while (_uiSize)
    {
        uiSize = (_uiSize > STUB_BOUNCE_SIZE) ? STUB_BOUNCE_SIZE : _uiSize;
        if (bInPage2(pucSource, uiSize))
        {
            vStubFromCaller(pucSource, uiSize);
            vCopy(pucDestination, g_pucStub + STUB_BOUNCE, uiSize);
        }
        else
            vCopy(pucDestination, pucSource, uiSize);
        pucDestination += uiSize;
        pucSource += uiSize;
        _uiSize -= uiSize;
    }
}

// Copy from the driver to the program
static void vToCaller(void *_pvDestination, const void *_pvSource, unsigned int _uiSize)
{
    unsigned char       *pucDestination = _pvDestination;
    const unsigned char *pucSource = _pvSource;
    unsigned int        uiSize;

    while (_uiSize)
    {
        uiSize = (_uiSize > STUB_BOUNCE_SIZE) ? STUB_BOUNCE_SIZE : _uiSize;
        if (bInPage2(pucDestination, uiSize))
        {
            vCopy(g_pucStub + STUB_BOUNCE, pucSource, uiSize);
            vStubToCaller(pucDestination, uiSize);
        }
        else
            vCopy(pucDestination, pucSource, uiSize);
        pucDestination += uiSize;
        pucSource += uiSize;
        _uiSize -= uiSize;
    }
}

// Copy a string or FIB parameter of the program (64 bytes, a string is always terminated)
static void vStringFromCaller(unsigned char *_pucDestination, const unsigned char *_pucSource)
{
    unsigned int uiSize = 64;

    if ((unsigned int) _pucSource > 0x10000 - 64)
        uiSize = 0 - (unsigned int) _pucSource;     // up to FFFFH

    vFromCaller(_pucDestination, _pucSource, uiSize);
    _pucDestination[uiSize] = 0;
    _pucDestination[64] = 0;
}

// Transfer data of the program
static void vCallerTransmit(void *_pvSource, unsigned int _uiSize)
{
    if (bInPage2(_pvSource, _uiSize))
        vStubXferTransmit(_pvSource, _uiSize);
    else
        vStubTransmit(_pvSource, _uiSize);
}

static void vCallerReceive(void *_pvDestination, unsigned int _uiSize)
{
    if (bInPage2(_pvDestination, _uiSize))
        while (!bStubXferReceive(_pvDestination, _uiSize));
    else
        while (!bStubReceive(_pvDestination, _uiSize));
}

/*
 =======================================================================================================================
    Data of the driver
 =======================================================================================================================
 */
static void vJIOTransmit(void *_pvSource, unsigned int _uiSize)
{
    vStubTransmit(_pvSource, _uiSize);
}

static void vTransmitString(char *_pcString)
{
    vJIOTransmit(_pcString, strlen(_pcString) + 1);
}

static void vReceive(void *_pvAddress, unsigned int _uiLength)
{
    while(!bStubReceive(_pvAddress, _uiLength));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool bIsPhysicalDriveHandled(unsigned char _ucPhysicalDrive)
{
    return (_ucPhysicalDrive < 8) && g_pucStub[STUB_DRIVES + _ucPhysicalDrive];
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
    Path or FIB parameter (copied by bIsPathOrFIBHandled), and string parameter HL for a FIB if _bAlsoSendHL
 =======================================================================================================================
 */
static void vTransmitPathOrFIB(bool _bAlsoSendHL)
{
    vSendCommonHeader();

    if (g_aucPath[0] == 0xFF)
    {
        vJIOTransmit(g_aucPath, sizeof(tdFileInfoBlock));
        if (_bAlsoSendHL)
        {
            vStringFromCaller(g_aucString, HL);
            vTransmitString(g_aucString);
        }
    }
    else
        vTransmitString(g_aucPath);
}

/*
 =======================================================================================================================
    Copies the path or FIB parameter of the program to g_aucPath
 =======================================================================================================================
 */
bool bIsPathOrFIBHandled(unsigned char * _pucFIB)
{
    unsigned char ucDrive;

    vStringFromCaller(g_aucPath, _pucFIB);
    _pucFIB = g_aucPath;

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

// FIB answered by the server, copied to the FIB of the program (IX)
static void vReceiveFIB()
{
    vReceive(&g_oFIB, sizeof(g_oFIB));
    vToCaller(IX, &g_oFIB, sizeof(g_oFIB));
    A = g_oFIB.m_ucResult;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_FIND_FIRST_ENTRY()
{
    g_bLastFindHandled = bIsPathOrFIBHandled_DE();

    if (g_bLastFindHandled)
    {
        vTransmitPathOrFIB(true);
        vJIOTransmit(&B, sizeof(B));
        vReceiveFIB();
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_FIND_NEW_ENTRY()
{
    g_bLastFindHandled = bIsPathOrFIBHandled_DE();

    if (g_bLastFindHandled)
    {
        vTransmitPathOrFIB(true);
        vJIOTransmit(&B, sizeof(B));
        vFromCaller(g_aucString, IX+1, 13);     // template file name
        vJIOTransmit(g_aucString, 13);
        vReceiveFIB();
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_FIND_NEXT_ENTRY()
{
    g_bLastFindHandled = bIsPathOrFIBHandled(IX);

    if (g_bLastFindHandled)
    {
        vSendCommonHeader();
        vJIOTransmit(g_aucPath, sizeof(tdFileInfoBlock));
        vReceiveFIB();
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_ALLOCATION_INFORMATION()
{
    unsigned char aucAnswer[5];             // sectors per cluster (0 = invalid drive), total clusters, free clusters

    if (bIsLogicalDriveHandled(E))
    {
        vSendCommonHeader();
        vJIOTransmit(&E, sizeof(E));
        vReceive(aucAnswer, sizeof(aucAnswer));
        A = aucAnswer[0];
        BCi = 512;
        DEi = *((unsigned int *) (aucAnswer + 1));
        HLi = *((unsigned int *) (aucAnswer + 3));
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
        vTransmitPathOrFIB(false);
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
            vCallerReceive(DE, (unsigned int)HL);
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
            vCallerTransmit(DE, (unsigned int)HL);

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
        vCallerReceive(DE, L);
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
        vTransmitString(g_aucPath);
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
    g_bResult = true;                       // answered here, nothing sent to the server
    A = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_WHOLE_PATH_STRING()
{
    // No parameter: whole path of the last entry found (on a JIO drive)
    if (g_bLastFindHandled)
    {
        vSendCommonHeader();
        vReceive(&A, sizeof(A) + sizeof(HL));
        vCallerReceive(DE, H);
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
static void vDateTimeAnswer()
{
    unsigned char aucAnswer[5];             // error, time, date

    vReceive(aucAnswer, sizeof(aucAnswer));
    A = aucAnswer[0];
    DEi = *((unsigned int *) (aucAnswer + 1));
    HLi = *((unsigned int *) (aucAnswer + 3));
}

static void vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));
        vJIOTransmit(&A, sizeof(A));
        vJIOTransmit(&IXi, sizeof(IXi));    // new time
        vJIOTransmit(&HLi, sizeof(HLi));    // new date
        vDateTimeAnswer();
    }
}

/*
 =======================================================================================================================
    Function $51 _FTIME: path or FIB, set, time (IX), date (HL)
 =======================================================================================================================
 */
static void vDOS_GET_SET_FILE_DATE_AND_TIME()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(false);
        vJIOTransmit(&A, sizeof(A));
        vJIOTransmit(&IXi, sizeof(IXi));    // new time
        vJIOTransmit(&HLi, sizeof(HLi));    // new date
        vDateTimeAnswer();
    }
}

/*
 =======================================================================================================================
    Function $4E _RENAME, $4F _MOVE: path or FIB, new name or path (HL)
 =======================================================================================================================
 */
static void vDOS_RENAME_OR_MOVE()
{
    if (bIsPathOrFIBHandled_DE())
    {
        vTransmitPathOrFIB(false);
        vStringFromCaller(g_aucString, HL);
        vTransmitString(g_aucString);
        vReceive(&A, sizeof(A));
    }
}

/*
 =======================================================================================================================
    Function $52 _HDELETE
 =======================================================================================================================
 */
static void vDOS_DELETE_FILE_HANDLE()
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
    Function $53 _HRENAME, $54 _HMOVE: file handle, new name or path (HL)
 =======================================================================================================================
 */
static void vDOS_RENAME_OR_MOVE_FILE_HANDLE()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));
        vStringFromCaller(g_aucString, HL);
        vTransmitString(g_aucString);
        vReceive(&A, sizeof(A));
    }
}

/*
 =======================================================================================================================
    Function $55 _HATTR: file handle, set, attributes (L)
 =======================================================================================================================
 */
static void vDOS_GET_SET_FILE_HANDLE_ATTRIBUTES()
{
    if (B >= START_HANDLE)
    {
        vSendCommonHeader();
        vJIOTransmit(&B, sizeof(B));
        vJIOTransmit(&A, sizeof(A) + sizeof(L));
        vReceive(&A, sizeof(A) + sizeof(L));
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
/*
    FCB functions (MSX-DOS 1): the server has no FCB function, they use its file handle functions.
    The file handle of the server is kept in the FCB (byte 18h). The FCB of the program (DE) is copied to g_oFCB,
    and back when it is changed.
    Results as the kernel gives them to the FCB functions layer of the disk ROM, which then sets A = L, B = H
    (and HL = DE for the block functions): result in L, number of records in DE.
 */

// Send the common header with another function than the one called
static void vSendFunction(unsigned char _ucFunction)
{
    g_oCommonHeader.m_ucFunction = _ucFunction;
    vSendCommonHeader();
}

// _ulValue * _uiFactor (no library routine: the library does not use the calling convention of the driver)
static unsigned long ulMultiply(unsigned long _ulValue, unsigned int _uiFactor)
{
    unsigned long ulResult = 0;

    while (_uiFactor)
    {
        if (_uiFactor & 1)
            ulResult += _ulValue;
        _ulValue += _ulValue;
        _uiFactor >>= 1;
    }

    return ulResult;
}

// Function $4A _SEEK, returns the new file pointer
static unsigned long ulSeek(unsigned char _ucHandle, unsigned char _ucMethod, unsigned long _ulOffset)
{
    unsigned char aucAnswer[5];             // error, new file pointer

    vSendFunction(0x4A);
    vJIOTransmit(&_ucHandle, sizeof(_ucHandle));
    vJIOTransmit(&_ucMethod, sizeof(_ucMethod));
    vJIOTransmit(&_ulOffset, sizeof(_ulOffset));
    vReceive(aucAnswer, sizeof(aucAnswer));

    return *((unsigned long *) (aucAnswer + 1));
}

/*
 =======================================================================================================================
    Function $0F _FOPEN
 =======================================================================================================================
 */
// FCB of the program copied to g_oFCB, Zx reset if its drive is handled
static bool bFCBFromCaller()
{
    vFromCaller(&g_oFCB, DE, sizeof(g_oFCB));
    return bIsLogicalDriveHandled(g_oFCB.m_ucDriverNumber);
}

// Copies a name or extension of a FCB (ends at the first space)
static char *pcCopyFCBName(char *_pcDestination, const char *_pcName, unsigned char _ucSize)
{
    while (_ucSize-- && (*_pcName != ' '))
        *_pcDestination++ = *_pcName++;
    return _pcDestination;
}

static void vDOS_OPEN_FILE_FCB()
{
    tdFileControlBlock  *poFCB = &g_oFCB;
    char                acPath[15];         // "D:NAME.EXT"
    char                *pcPath = acPath;
    unsigned char       aucAnswer[2];       // error, file handle
    unsigned char       ucMode = 0;

    if (!bFCBFromCaller())
        return;

    if (poFCB->m_ucDriverNumber)
    {
        *pcPath++ = 'A' - 1 + poFCB->m_ucDriverNumber;
        *pcPath++ = ':';
    }
    pcPath = pcCopyFCBName(pcPath, poFCB->m_acFileName, 8);
    if (poFCB->m_acFileNameExtension[0] != ' ')
    {
        *pcPath++ = '.';
        pcPath = pcCopyFCBName(pcPath, poFCB->m_acFileNameExtension, 3);
    }
    *pcPath = 0;

    vSendFunction(0x43);                    // _OPEN
    vTransmitString(acPath);
    vJIOTransmit(&ucMode, sizeof(ucMode));
    vReceive(aucAnswer, sizeof(aucAnswer));

    if (aucAnswer[0])
    {
        A = L = 0xFF;
        return;
    }

    poFCB->ucNewFileHandle = aucAnswer[1];
    poFCB->m_ulFileSize = ulSeek(aucAnswer[1], 2, 0);
    ulSeek(aucAnswer[1], 0, 0);
    poFCB->m_ucExtentNumber = 0;
    poFCB->u.dos.m_uiRecordSize = 128;
    poFCB->m_ucCurrentRecordWithinExtent = 0;
    vToCaller(DE, poFCB, sizeof(*poFCB));
    A = L = 0;
}

/*
 =======================================================================================================================
    Function $10 _FCLOSE
 =======================================================================================================================
 */
static void vDOS_CLOSE_FILE_FCB()
{
    unsigned char ucError;

    if (!bFCBFromCaller())
        return;

    vSendFunction(0x45);                    // _CLOSE
    vJIOTransmit(&g_oFCB.ucNewFileHandle, sizeof(g_oFCB.ucNewFileHandle));
    vReceive(&ucError, sizeof(ucError));
    A = L = ucError ? 0xFF : 0;
}

/*
 =======================================================================================================================
    Function $27 _RDBLK: HL records of the record size at the random record, to the disk transfer address.
    A last incomplete record is filled with zeros.
 =======================================================================================================================
 */
static void vDOS_RANDOM_BLOCK_READ_FCB()
{
    tdFileControlBlock  *poFCB = &g_oFCB;
    unsigned int        uiRecordSize;
    unsigned long       ulRecord;
    unsigned long       ulBytes;
    unsigned int        uiSize;
    unsigned int        uiLeft;
    unsigned int        uiRecords;
    unsigned int        uiFill;
    unsigned char       aucAnswer[3];       // error, size read

    if (!bFCBFromCaller())
        return;

    uiRecordSize = poFCB->u.dos.m_uiRecordSize ? poFCB->u.dos.m_uiRecordSize : 128;
    ulRecord = poFCB->m_ulRandomRecordNumber;
    if (uiRecordSize >= 64)
        ulRecord &= 0xFFFFFF;               // 3 bytes random record number

    ulSeek(poFCB->ucNewFileHandle, 0, ulMultiply(ulRecord, uiRecordSize));

    ulBytes = ulMultiply(HLi, uiRecordSize);
    uiSize = (ulBytes > 0xFFFF) ? 0xFFFF : (unsigned int) ulBytes;

    vSendFunction(0x48);                    // _READ
    vJIOTransmit(&poFCB->ucNewFileHandle, sizeof(poFCB->ucNewFileHandle));
    vJIOTransmit(&uiSize, sizeof(uiSize));
    vReceive(aucAnswer, sizeof(aucAnswer));
    uiSize = *((unsigned int *) (aucAnswer + 1));
    if (uiSize)
        vCallerReceive(g_pcDiskTransferAddress, uiSize);

    // records read, the last incomplete one is filled with zeros
    uiRecords = 0;
    for (uiLeft = uiSize; uiLeft >= uiRecordSize; uiLeft -= uiRecordSize)
        uiRecords++;
    if (uiLeft)
    {
        uiRecords++;
        uiLeft = uiRecordSize - uiLeft;     // bytes to fill
        for (uiFill = 0; uiFill < sizeof(g_aucString); uiFill++)
            g_aucString[uiFill] = 0;
        while (uiLeft)
        {
            uiFill = (uiLeft > sizeof(g_aucString)) ? sizeof(g_aucString) : uiLeft;
            vToCaller(g_pcDiskTransferAddress + uiSize, g_aucString, uiFill);
            uiSize += uiFill;
            uiLeft -= uiFill;
        }
    }

    poFCB->m_ulRandomRecordNumber = ulRecord + uiRecords;
    vToCaller(DE, poFCB, sizeof(*poFCB));
    A = L = (uiRecords == HLi) ? 0 : 1;
    DEi = uiRecords;
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
        vReceive(&A, sizeof(A));
        L = A;
        g_bResult = false;
    }

    g_ucCurrentDisk = E;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_PREVIOUS_ERROR_CODE()
{
    g_bResult = true;                       // answered here, nothing sent to the server
    A = 0;
    B = g_ucPreviousErrorCode;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_LOGIN_VECTOR()
{
    g_bResult = true;                       // answered here, nothing sent to the server
    H = 0;
    L = 15;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_GET_CURRENT_DRIVE()
{
    g_bResult = true;                       // answered here, nothing sent to the server

    L = A = g_ucCurrentDisk;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_PARSE_PATHNAME()
{
    g_bResult = true;

    // original function (with the TPA mapped), drive of the current disk of the driver if no drive is given
    vCopy(g_pucStub + STUB_REGISTERS, (unsigned char *) &g_aoRegisters, sizeof(g_aoRegisters));
    vStubOriginal();
    vCopy((unsigned char *) &g_aoRegisters, g_pucStub + STUB_REGISTERS, sizeof(g_aoRegisters));

    if (!(B & 4))
        C = g_ucCurrentDisk + 1;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vCheckRFS()
{
    g_bResult = true;
    L = 'R';
    DE = g_pucStub;
}

typedef struct
{
    unsigned char code;    // DOS function number (C)
    tdDosHandler  handler; // function pointer
} tdDosDispatchEntry;

static const tdDosDispatchEntry g_aDosHandlers[] =
{
    { 0x0E, vDOS_SELECT_DISK },
    { 0x0F, vDOS_OPEN_FILE_FCB },
    { 0x10, vDOS_CLOSE_FILE_FCB },
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
    { 0x4E, vDOS_RENAME_OR_MOVE },
    { 0x4F, vDOS_RENAME_OR_MOVE },
    { 0x50, vDOS_GET_SET_FILE_ATTRIBUTES },
    { 0x51, vDOS_GET_SET_FILE_DATE_AND_TIME },
    { 0x52, vDOS_DELETE_FILE_HANDLE },
    { 0x53, vDOS_RENAME_OR_MOVE_FILE_HANDLE },
    { 0x54, vDOS_RENAME_OR_MOVE_FILE_HANDLE },
    { 0x55, vDOS_GET_SET_FILE_HANDLE_ATTRIBUTES },
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
    g_bHasTurbo = g_pucStub[STUB_HAS_TURBO];

    for (pEntry = g_aDosHandlers; pEntry->code && (pEntry->code != C); pEntry++);

    if (pEntry->code)
    {
__asm
        ld a,(_g_bHasTurbo)
        or  a
        jr  z,noTurbo1

        push    iy          ; IX and IY changed by CALSLT (GETCPU, CHGCPU)
        push    ix

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

        pop     ix
        pop     iy
noTurbo2:
__endasm;

    }

    return g_bResult;
}
