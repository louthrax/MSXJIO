
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
// Sequence number of the requests (m_ucFlags of the header, not 0): the server does not execute again a request sent
// again with the same number (requests not sent again for now: "Not ready" error without answer, see vNoAnswer)
unsigned char          g_ucSequence = 0;
unsigned char          g_ucPreviousErrorCode = 0;
bool                   g_bResult = 0;
const char             g_acDevicesNames[] = "CON\0PRN\0LST\0AUX\0NUL\0";
unsigned char          g_ucCurrentDisk = 0xFF;  // current drive if it is a drive of the server, FFH: drive of MSX-DOS
unsigned char          g_ucDosDrive = 0xFF;     // current drive of MSX-DOS (_SELDSK, _CURDRV), FFH: unknown
unsigned char          g_bHasTurbo = false;
bool                   g_bLastFindHandled = false;  // last _FFIRST/_FNEXT/_FNEW on a JIO drive (for _WPATH)
// All the variables are initialized: they are then in the code of the driver (an uninitialized variable would be at
// address 0 of the driver, over its code)
unsigned char          g_aucPath[66] = { 0 };       // copy of the path or FIB parameter (DE or IX)
unsigned char          g_aucString[66] = { 0 };     // copy of a string parameter (HL), FIB template
tdFileInfoBlock        g_oFIB = { 0 };              // FIB answered by the server
tdFileControlBlock     g_oFCB = { 0 };              // copy of the FCB parameter (DE)
// Answers of the server: received in a global, never on the stack (the receive routine of the stub uses SP)
unsigned char          g_aucAnswer[5] = { 0 };

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
    push    ix              ; IX (frame pointer of the C code) and IY changed by receive.asm
    push    iy
    push    hl
    ld      hl,bStubReceive_ret
    ex      (sp),hl
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_RECEIVE
    add     hl,bc
    ex      (sp),hl
    ret
bStubReceive_ret:
    pop     iy
    pop     ix
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
    push    ix              ; IX (frame pointer of the C code) and IY changed by receive.asm
    push    iy
    push    hl
    ld      hl,bStubXferReceive_ret
    ex      (sp),hl
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_XFER
    add     hl,bc
    ex      (sp),hl
    ret
bStubXferReceive_ret:
    pop     iy
    pop     ix
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

// IX (frame pointer of the C code) and IY kept: TpaOriginal loads the IX of the program for the original function,
// and the function returns its own (its results are in STUB_REGISTERS)
static void vStubOriginal(void) __naked
{
__asm
    push    ix
    push    iy
    ld      hl,stub_original_ret
    push    hl
    ld      hl,(_g_pucStub)
    ld      bc,STUB_ORIGINAL
    add     hl,bc
    jp      (hl)
stub_original_ret:
    pop     iy
    pop     ix
    ret
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

/*
 =======================================================================================================================
    Handler of the BDOS function
 =======================================================================================================================
 */
static void (*g_pHandler)(void) = 0;        // handler of the BDOS function (vCallHandler)
unsigned int           g_uiAbortSP = 0;     // stack of vCallHandler: the handler is abandoned there (no answer)

// Handler g_pHandler: IX (frame pointer of the C code) restored
static void vCallHandler() __naked
{
__asm
    push    ix
    ld      (_g_uiAbortSP),sp
    ld      de,call_handler_end
    push    de
    ld      hl,(_g_pHandler)
    jp      (hl)
call_handler_end:
    pop     ix
    ret
__endasm;
}

static void vAbortCommand() __naked
{
__asm
    ld      sp,(_g_uiAbortSP)
    jp      call_handler_end
__endasm;
}

/*
 =======================================================================================================================
    No answer of the server after NO_ANSWER_TRIES time-outs of the receive routine (about 0.9 s each at 3.58 MHz):
    server stopped or disconnected, or bytes of the request lost on the link (the server waits for them). The BDOS
    function returns "Not ready" (FFH for the FCB functions), as for a drive without disk, instead of waiting forever.
    The request is not sent again: the server abandons an incomplete request when the next one arrives.
 =======================================================================================================================
 */
#define NO_ANSWER_TRIES 6

static void vNoAnswer()
{
    if ((C == 0x0F) || (C == 0x10) || (C == 0x27))
        A = L = 0xFF;                       // FCB functions: error
    else
        A = DOS_ERR_NRDY;
    g_bResult = true;
    vAbortCommand();
}

// Transfer data of the program
static void vCallerTransmit(void *_pvSource, unsigned int _uiSize)
{
    if (bInPage2(_pvSource, _uiSize))
        vStubXferTransmit(_pvSource, _uiSize);
    else
        vStubTransmit(_pvSource, _uiSize);
}

// Answer of the server: waited for (time-out of the receive routine: the server is not ready yet), up to
// NO_ANSWER_TRIES time-outs
static void vCallerReceive(void *_pvDestination, unsigned int _uiSize)
{
    unsigned char ucTries = NO_ANSWER_TRIES;

    while (!(bInPage2(_pvDestination, _uiSize) ? bStubXferReceive(_pvDestination, _uiSize) : bStubReceive(_pvDestination, _uiSize)))
    {
        if (!--ucTries)
            vNoAnswer();
    }
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
    unsigned char ucTries = NO_ANSWER_TRIES;

    while (!bStubReceive(_pvAddress, _uiLength))
    {
        if (!--ucTries)
            vNoAnswer();
    }
}

/*
 =======================================================================================================================
    Original BDOS function being handled (TPA mapped), its results in the registers. Only for the function being
    handled: MSX-DOS calls the function it received, whatever C is (another function cannot be called this way).
 =======================================================================================================================
 */
static void vOriginalFunction()
{
    vCopy(g_pucStub + STUB_REGISTERS, (const unsigned char *) &g_aoRegisters, sizeof(g_aoRegisters));
    vStubOriginal();
    vCopy((unsigned char *) &g_aoRegisters, g_pucStub + STUB_REGISTERS, sizeof(g_aoRegisters));
}

// Current drive (0 = A:): drive of the server selected by _SELDSK, or current drive of MSX-DOS (FFH: unknown)
static unsigned char ucCurrentDrive()
{
    return (g_ucCurrentDisk != 0xFF) ? g_ucCurrentDisk : g_ucDosDrive;
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
    return bIsPhysicalDriveHandled(_ucLogicalDrive ? _ucLogicalDrive - 1 : ucCurrentDrive());
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
    if (!++g_ucSequence)
        g_ucSequence = 1;
    g_oCommonHeader.m_ucFlags = g_ucSequence;
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

    if (bIsLogicalDriveHandled(E))
    {
        vSendCommonHeader();
        vJIOTransmit(&E, sizeof(E));
        vReceive(g_aucAnswer, 5);           // sectors per cluster (0 = invalid drive), total clusters, free clusters
        // registers of the GO_BDOS hook (as the JIO ROM): the kernel returns A = C to the program, and BC = sector
        // size taken from the DPB pointed to by IX. MSX-DOS 1 (called from 0005H): A = sectors per cluster,
        // BC = sector size.
        A = 0;
        BCi = g_aucAnswer[0];
        DEi = *((unsigned int *) (g_aucAnswer + 1));
        HLi = *((unsigned int *) (g_aucAnswer + 3));
        IXi = (unsigned int) (g_pucStub + STUB_DPB);
        if (!g_aucAnswer[0])
        {
            BCi = 0xFF;                     // invalid drive
            A = DOS_ERR_IDRV;
        }
        if (g_pucStub[STUB_DOS1])
        {
            A = g_aucAnswer[0] ? g_aucAnswer[0] : 0xFF;
            BCi = 512;
        }
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
    Bluetooth link (STUB_TX_BLOCKS, FLAG_TX_BLOCKS of the server): data written in requests of WRITE_BLOCK_SIZE bytes at
    most, each one answered before the next one is sent, a long continuous transmission can be lost by the Bluetooth
    serial module (no flow control on the MSX side). Otherwise (USB serial) one request. HL returned = total of the bytes
    written, stopped at the first error or short write.
 =======================================================================================================================
 */
#define WRITE_BLOCK_SIZE 8192

static void vDOS_WRITE_TO_FILE_HANDLE()
{
    if (B >= START_HANDLE)
    {
        unsigned char *pucSource = DE;
        unsigned int   uiLeft = HLi;
        unsigned int   uiWritten = 0;
        unsigned int   uiBlock = g_pucStub[STUB_TX_BLOCKS] ? WRITE_BLOCK_SIZE : 0xFFFF;
        unsigned int   uiSize;

        do
        {
            uiSize = (uiLeft > uiBlock) ? uiBlock : uiLeft;
            vSendCommonHeader();
            vJIOTransmit(&B, sizeof(B));
            vJIOTransmit(&uiSize, sizeof(uiSize));
            if (uiSize)
                vCallerTransmit(pucSource, uiSize);

            vReceive(&A, sizeof(A) + sizeof(HL));   // error, bytes written
            uiWritten += HLi;
            pucSource += uiSize;
            uiLeft -= uiSize;
        } while (uiLeft && !A && (HLi == uiSize));

        HLi = uiWritten;
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

    vReceive(g_aucAnswer, 5);               // error, time, date
    A = g_aucAnswer[0];
    DEi = *((unsigned int *) (g_aucAnswer + 1));
    HLi = *((unsigned int *) (g_aucAnswer + 3));
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

    vSendFunction(0x4A);
    vJIOTransmit(&_ucHandle, sizeof(_ucHandle));
    vJIOTransmit(&_ucMethod, sizeof(_ucMethod));
    vJIOTransmit(&_ulOffset, sizeof(_ulOffset));
    vReceive(g_aucAnswer, 5);               // error, new file pointer

    return *((unsigned long *) (g_aucAnswer + 1));
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
    g_oFCB.m_ucDriverNumber &= 0x7F;        // COMMAND.COM of MSX-DOS 1 (DIR): 80H = default drive
    return bIsLogicalDriveHandled(g_oFCB.m_ucDriverNumber);
}

// Copies a name or extension of a FCB (ends at the first space)
static char *pcCopyFCBName(char *_pcDestination, const char *_pcName, unsigned char _ucSize)
{
    while (_ucSize-- && (*_pcName != ' '))
        *_pcDestination++ = *_pcName++;
    return _pcDestination;
}

// Path "D:NAME.EXT" of the FCB g_oFCB (wildcards '?' kept)
static void vFCBPath(char *_pcPath)
{
    if (g_oFCB.m_ucDriverNumber)
    {
        *_pcPath++ = 'A' - 1 + g_oFCB.m_ucDriverNumber;
        *_pcPath++ = ':';
    }
    _pcPath = pcCopyFCBName(_pcPath, g_oFCB.m_acFileName, 8);
    if (g_oFCB.m_acFileNameExtension[0] != ' ')
    {
        *_pcPath++ = '.';
        _pcPath = pcCopyFCBName(_pcPath, g_oFCB.m_acFileNameExtension, 3);
    }
    *_pcPath = 0;
}

static void vDOS_OPEN_FILE_FCB()
{
    tdFileControlBlock  *poFCB = &g_oFCB;
    char                acPath[15];         // "D:NAME.EXT"
    unsigned char       ucMode = 0;

    if (!bFCBFromCaller())
        return;

    vFCBPath(acPath);

    vSendFunction(0x43);                    // _OPEN
    vTransmitString(acPath);
    vJIOTransmit(&ucMode, sizeof(ucMode));
    vReceive(g_aucAnswer, 2);               // error, file handle

    if (g_aucAnswer[0])
    {
        A = L = 0xFF;
        return;
    }

    poFCB->ucNewFileHandle = g_aucAnswer[1];   // g_aucAnswer reused by ulSeek
    poFCB->m_ulFileSize = ulSeek(poFCB->ucNewFileHandle, 2, 0);
    ulSeek(poFCB->ucNewFileHandle, 0, 0);
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
    if (!bFCBFromCaller())
        return;

    vSendFunction(0x45);                    // _CLOSE
    vJIOTransmit(&g_oFCB.ucNewFileHandle, sizeof(g_oFCB.ucNewFileHandle));
    vReceive(g_aucAnswer, 1);               // error
    A = L = g_aucAnswer[0] ? 0xFF : 0;
}

/*
 =======================================================================================================================
    Functions $11 _SFIRST, $12 _SNEXT (MSX-DOS 1: DIR of COMMAND.COM): _FFIRST / _FNEXT of the server, files only.
    Disk transfer address: drive, then the directory entry (name and extension padded with spaces, attributes, time,
    date, cluster 0, size). A = L = 0 found, 0FFH not found.
 =======================================================================================================================
 */
bool g_bLastSearchHandled = false;          // last _SFIRST on a JIO drive (_SNEXT)

// _pucEntry: buffer for the entry (pointer parameter: the driver is relocated by words)
static void vSearchResult(unsigned char *_pucEntry)
{
    unsigned char   *pucEntry = _pucEntry;
    const char      *pcName = g_oFIB.m_acFileName;
    unsigned char   ucI;

    if (g_oFIB.m_ucResult)
    {
        A = L = 0xFF;
        return;
    }

    for (ucI = 0; ucI < 33; ucI++)
        pucEntry[ucI] = (ucI && (ucI < 12)) ? ' ' : 0;
    pucEntry[0] = g_oFIB.m_ucDrive;
    for (ucI = 1; *pcName && (*pcName != '.') && (ucI < 9); ucI++)
        pucEntry[ucI] = *pcName++;
    while (*pcName && (*pcName != '.'))
        pcName++;
    if (*pcName == '.')
    {
        pcName++;
        for (ucI = 9; *pcName && (ucI < 12); ucI++)
            pucEntry[ucI] = *pcName++;
    }
    pucEntry[12] = g_oFIB.m_cAttributes;
    *((unsigned int *) (pucEntry + 23)) = g_oFIB.m_uiLastModificationTime;
    *((unsigned int *) (pucEntry + 25)) = g_oFIB.m_uiLastModificationDate;
    *((unsigned long *) (pucEntry + 29)) = g_oFIB.m_ulFileSize;
    vToCaller(g_pcDiskTransferAddress, pucEntry, 33);
    A = L = 0;
}

static void vDOS_SEARCH_FIRST_FCB()
{
    g_bLastSearchHandled = bFCBFromCaller();
    if (!g_bLastSearchHandled)
        return;

    vFCBPath((char *) g_aucPath);
    vSendFunction(0x40);                    // _FFIRST
    vTransmitString((char *) g_aucPath);
    g_aucAnswer[0] = 0;                     // attributes: files only
    vJIOTransmit(g_aucAnswer, 1);
    vReceive(&g_oFIB, sizeof(g_oFIB));
    vSearchResult(g_aucString);
}

static void vDOS_SEARCH_NEXT_FCB()
{
    if (!g_bLastSearchHandled)
        return;

    vSendFunction(0x41);                    // _FNEXT
    vJIOTransmit(&g_oFIB, sizeof(g_oFIB));
    vReceive(&g_oFIB, sizeof(g_oFIB));
    vSearchResult(g_aucString);
}

/*
 =======================================================================================================================
    Records of the FCB functions: read and write at a record of the file (server file handle in the FCB), results.
    MSX-DOS 1 (called from 0005H): A = L = result, HL = number of records of the block functions. MSX-DOS 2 (GO_BDOS
    hook, FCB layer of the disk ROM): result in L, number of records in DE.
    No library routine for the long divisions, multiplications (calling convention of the driver).
 =======================================================================================================================
 */
static void vFCBRecords(unsigned int _uiRecords)
{
    if (g_pucStub[STUB_DOS1])
        HLi = _uiRecords;
    else
        DEi = _uiRecords;
}

// Sequential record: current block (0CH, 0DH) * 128 + current record (20H)
static unsigned long ulSequentialRecord()
{
    unsigned int uiBlock = g_oFCB.m_ucExtentNumber | ((unsigned int) g_oFCB.m_ucFileAttributes << 8);

    return ulMultiply(uiBlock, 128) + g_oFCB.m_ucCurrentRecordWithinExtent;
}

static void vSetSequentialRecord(unsigned long _ulRecord)
{
    unsigned int uiLow = (unsigned int) _ulRecord;
    unsigned int uiBlock = (uiLow >> 7) | ((unsigned int) (_ulRecord >> 16) << 9);

    g_oFCB.m_ucCurrentRecordWithinExtent = uiLow & 0x7F;
    g_oFCB.m_ucExtentNumber = uiBlock & 0xFF;
    g_oFCB.m_ucFileAttributes = uiBlock >> 8;
}

// _ulValue / _uiDivisor rounded up (binary division)
static unsigned long ulDivideUp(unsigned long _ulValue, unsigned int _uiDivisor)
{
    unsigned long ulDivisor = _uiDivisor;
    unsigned long ulBit = 1;
    unsigned long ulResult = 0;

    _ulValue += _uiDivisor - 1;
    while (!(ulDivisor & 0x80000000) && (ulDivisor < _ulValue))
    {
        ulDivisor <<= 1;
        ulBit <<= 1;
    }
    while (ulBit)
    {
        if (_ulValue >= ulDivisor)
        {
            _ulValue -= ulDivisor;
            ulResult |= ulBit;
        }
        ulDivisor >>= 1;
        ulBit >>= 1;
    }
    return ulResult;
}

// _uiSize zeros to the program at _pcDestination (_pucZeros: buffer, pointer parameter: the driver is relocated by
// words)
static void vCallerZeros(char *_pcDestination, unsigned int _uiSize, unsigned char *_pucZeros)
{
    unsigned int uiSize;

    for (uiSize = 0; uiSize < sizeof(g_aucString); uiSize++)
        _pucZeros[uiSize] = 0;
    while (_uiSize)
    {
        uiSize = (_uiSize > sizeof(g_aucString)) ? sizeof(g_aucString) : _uiSize;
        vToCaller(_pcDestination, _pucZeros, uiSize);
        _pcDestination += uiSize;
        _uiSize -= uiSize;
    }
}

// _uiCount records of _uiRecordSize bytes from the record _ulRecord of the file of g_oFCB to the disk transfer
// address: number of records read, a last incomplete one is filled with zeros
static unsigned int uiFCBRead(unsigned long _ulRecord, unsigned int _uiRecordSize, unsigned int _uiCount)
{
    unsigned long   ulBytes = ulMultiply(_uiCount, _uiRecordSize);
    unsigned int    uiSize = (ulBytes > 0xFFFF) ? 0xFFFF : (unsigned int) ulBytes;
    unsigned int    uiRecords = 0;
    unsigned int    uiLeft;

    ulSeek(g_oFCB.ucNewFileHandle, 0, ulMultiply(_ulRecord, _uiRecordSize));
    vSendFunction(0x48);                    // _READ
    vJIOTransmit(&g_oFCB.ucNewFileHandle, sizeof(g_oFCB.ucNewFileHandle));
    vJIOTransmit(&uiSize, sizeof(uiSize));
    vReceive(g_aucAnswer, 3);               // error, size read
    uiSize = *((unsigned int *) (g_aucAnswer + 1));
    if (uiSize)
        vCallerReceive(g_pcDiskTransferAddress, uiSize);

    for (uiLeft = uiSize; uiLeft >= _uiRecordSize; uiLeft -= _uiRecordSize)
        uiRecords++;
    if (uiLeft)
    {
        uiRecords++;
        vCallerZeros(g_pcDiskTransferAddress + uiSize, _uiRecordSize - uiLeft, g_aucString);
    }
    return uiRecords;
}

// _uiCount records of _uiRecordSize bytes from the disk transfer address to the record _ulRecord of the file of
// g_oFCB (blocks of WRITE_BLOCK_SIZE bytes on a Bluetooth link): number of records written, file size of the FCB
// updated
static unsigned int uiFCBWrite(unsigned long _ulRecord, unsigned int _uiRecordSize, unsigned int _uiCount)
{
    unsigned long   ulPosition = ulMultiply(_ulRecord, _uiRecordSize);
    unsigned long   ulBytes = ulMultiply(_uiCount, _uiRecordSize);
    unsigned int    uiLeft = (ulBytes > 0xFFFF) ? 0xFFFF : (unsigned int) ulBytes;
    unsigned int    uiBlock = g_pucStub[STUB_TX_BLOCKS] ? WRITE_BLOCK_SIZE : 0xFFFF;
    char            *pcSource = g_pcDiskTransferAddress;
    unsigned int    uiWritten = 0;
    unsigned int    uiSize;
    unsigned int    uiRecords = 0;

    ulSeek(g_oFCB.ucNewFileHandle, 0, ulPosition);
    while (uiLeft)
    {
        uiSize = (uiLeft > uiBlock) ? uiBlock : uiLeft;
        vSendFunction(0x49);                // _WRITE
        vJIOTransmit(&g_oFCB.ucNewFileHandle, sizeof(g_oFCB.ucNewFileHandle));
        vJIOTransmit(&uiSize, sizeof(uiSize));
        vCallerTransmit(pcSource, uiSize);
        vReceive(g_aucAnswer, 3);           // error, size written
        uiWritten += *((unsigned int *) (g_aucAnswer + 1));
        if (g_aucAnswer[0] || (*((unsigned int *) (g_aucAnswer + 1)) != uiSize))
            break;
        pcSource += uiSize;
        uiLeft -= uiSize;
    }

    ulPosition += uiWritten;
    if (ulPosition > g_oFCB.m_ulFileSize)
        g_oFCB.m_ulFileSize = ulPosition;
    while (uiWritten >= _uiRecordSize)
    {
        uiWritten -= _uiRecordSize;
        uiRecords++;
    }
    return uiRecords;
}

static unsigned int uiFCBRecordSize()
{
    return g_oFCB.u.dos.m_uiRecordSize ? g_oFCB.u.dos.m_uiRecordSize : 128;
}

static unsigned long ulRandomRecord()
{
    unsigned long ulRecord = g_oFCB.m_ulRandomRecordNumber;

    if (uiFCBRecordSize() >= 64)
        ulRecord &= 0xFFFFFF;               // 3 bytes random record number
    return ulRecord;
}

/*
 =======================================================================================================================
    Function $14 _RDSEQ, $15 _WRSEQ: record of 128 bytes at the current block and record, which are increased.
    A = 0 done, 1 end of file (nothing read) or not written.
 =======================================================================================================================
 */
static void vDOS_SEQUENTIAL_READ_FCB()
{
    unsigned long ulRecord;

    if (!bFCBFromCaller())
        return;

    ulRecord = ulSequentialRecord();
    if (uiFCBRead(ulRecord, 128, 1))
    {
        vSetSequentialRecord(ulRecord + 1);
        A = L = 0;
    }
    else
        A = L = 1;
    vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
}

static void vDOS_SEQUENTIAL_WRITE_FCB()
{
    unsigned long ulRecord;

    if (!bFCBFromCaller())
        return;

    ulRecord = ulSequentialRecord();
    if (uiFCBWrite(ulRecord, 128, 1))
    {
        vSetSequentialRecord(ulRecord + 1);
        A = L = 0;
    }
    else
        A = L = 1;
    vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
}

/*
 =======================================================================================================================
    Function $21 _RDRND, $22 _WRRND, $28 _WRZER: record of 128 bytes at the random record, the current block and
    record are set to it. A = 0 done, 1 end of file (nothing read) or not written.
 =======================================================================================================================
 */
static void vDOS_RANDOM_READ_FCB()
{
    unsigned long ulRecord;

    if (!bFCBFromCaller())
        return;

    ulRecord = g_oFCB.m_ulRandomRecordNumber & 0xFFFFFF;
    vSetSequentialRecord(ulRecord);
    A = L = uiFCBRead(ulRecord, 128, 1) ? 0 : 1;
    vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
}

static void vDOS_RANDOM_WRITE_FCB()
{
    unsigned long ulRecord;

    if (!bFCBFromCaller())
        return;

    ulRecord = g_oFCB.m_ulRandomRecordNumber & 0xFFFFFF;
    vSetSequentialRecord(ulRecord);
    A = L = uiFCBWrite(ulRecord, 128, 1) ? 0 : 1;
    vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
}

/*
 =======================================================================================================================
    Function $26 _WRBLK, $27 _RDBLK: HL records of the record size (0 = 128) at the random record, which is increased.
    A last incomplete record read is filled with zeros. A = 0 all done, 1 end of file or not all written; number of
    records done (vFCBRecords).
 =======================================================================================================================
 */
static void vDOS_RANDOM_BLOCK_READ_FCB()
{
    unsigned int uiRecords;

    if (!bFCBFromCaller())
        return;

    uiRecords = HLi ? uiFCBRead(ulRandomRecord(), uiFCBRecordSize(), HLi) : 0;
    A = L = (uiRecords == HLi) ? 0 : 1;
    g_oFCB.m_ulRandomRecordNumber = ulRandomRecord() + uiRecords;
    vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
    vFCBRecords(uiRecords);
}

static void vDOS_RANDOM_BLOCK_WRITE_FCB()
{
    unsigned int uiRecords;

    if (!bFCBFromCaller())
        return;

    uiRecords = HLi ? uiFCBWrite(ulRandomRecord(), uiFCBRecordSize(), HLi) : 0;
    A = L = (uiRecords == HLi) ? 0 : 1;
    g_oFCB.m_ulRandomRecordNumber = ulRandomRecord() + uiRecords;
    vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
    vFCBRecords(uiRecords);
}

/*
 =======================================================================================================================
    Function $16 _FMAKE: file created (emptied if it exists) and opened, FCB as _FOPEN does.
    A = L = 0 done, 0FFH error.
 =======================================================================================================================
 */
static void vDOS_CREATE_FILE_FCB()
{
    char acPath[15];                        // "D:NAME.EXT"

    if (!bFCBFromCaller())
        return;

    vFCBPath(acPath);
    vSendFunction(0x44);                    // _CREATE
    vTransmitString(acPath);
    g_aucAnswer[0] = 0;                     // open mode
    g_aucAnswer[1] = 0;                     // attributes
    vJIOTransmit(g_aucAnswer, 2);
    vReceive(g_aucAnswer, 2);               // error, file handle

    if (g_aucAnswer[0])
    {
        A = L = 0xFF;
        return;
    }

    g_oFCB.ucNewFileHandle = g_aucAnswer[1];
    g_oFCB.m_ulFileSize = 0;
    g_oFCB.m_ucExtentNumber = 0;
    g_oFCB.u.dos.m_uiRecordSize = 128;
    g_oFCB.m_ucCurrentRecordWithinExtent = 0;
    vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
    A = L = 0;
}

/*
 =======================================================================================================================
    Functions $13 _FDEL, $17 _FREN, $23 _FSIZE: files of the FCB name (wildcards '?'), found with _FFIRST / _FNEXT of
    the server (files only), deleted or renamed through their FIB.
 =======================================================================================================================
 */
// First (_bFirst) or next file of the FCB name in g_oFIB: false if none
static bool bFCBFind(bool _bFirst)
{
    if (_bFirst)
    {
        vFCBPath((char *) g_aucPath);
        vSendFunction(0x40);                // _FFIRST
        vTransmitString((char *) g_aucPath);
        g_aucAnswer[0] = 0;                 // attributes: files only
        vJIOTransmit(g_aucAnswer, 1);
    }
    else
    {
        vSendFunction(0x41);                // _FNEXT
        vJIOTransmit(&g_oFIB, sizeof(g_oFIB));
    }
    vReceive(&g_oFIB, sizeof(g_oFIB));
    return !g_oFIB.m_ucResult;
}

static void vDOS_DELETE_FILE_FCB()
{
    bool bFound;

    if (!bFCBFromCaller())
        return;

    A = L = 0xFF;                           // no file deleted
    for (bFound = bFCBFind(true); bFound; bFound = bFCBFind(false))
    {
        vSendFunction(0x4D);                // _DELETE (FIB)
        vJIOTransmit(&g_oFIB, sizeof(g_oFIB));
        vReceive(g_aucAnswer, 1);
        if (!g_aucAnswer[0])
            A = L = 0;
    }
}

// New name "NAME.EXT" of the found file g_oFIB: FCB template _pcTemplate (11 characters, '?' = character of the old
// name) to _pcNewName
static void vRenamedName(const char *_pcTemplate, char *_pcNewName)
{
    char            acOld[11];
    const char      *pcName = g_oFIB.m_acFileName;
    unsigned char   ucI;

    for (ucI = 0; ucI < 11; ucI++)
        acOld[ucI] = ' ';
    for (ucI = 0; *pcName && (*pcName != '.') && (ucI < 8); ucI++)
        acOld[ucI] = *pcName++;
    while (*pcName && (*pcName != '.'))
        pcName++;
    if (*pcName == '.')
    {
        pcName++;
        for (ucI = 8; *pcName && (ucI < 11); ucI++)
            acOld[ucI] = *pcName++;
    }

    for (ucI = 0; ucI < 11; ucI++)
    {
        char c = (_pcTemplate[ucI] == '?') ? acOld[ucI] : _pcTemplate[ucI];

        if (ucI == 8)
            *_pcNewName++ = '.';
        if (c != ' ')
            *_pcNewName++ = c;
    }
    if (_pcNewName[-1] == '.')
        _pcNewName--;
    *_pcNewName = 0;
}

static void vDOS_RENAME_FILE_FCB()
{
    bool bFound;

    if (!bFCBFromCaller())
        return;

    A = L = 0xFF;                           // no file renamed
    for (bFound = bFCBFind(true); bFound; bFound = bFCBFind(false))
    {
        vRenamedName(((const char *) &g_oFCB) + 17, (char *) g_aucString);
        vSendFunction(0x4E);                // _RENAME (FIB, new name)
        vJIOTransmit(&g_oFIB, sizeof(g_oFIB));
        vTransmitString((char *) g_aucString);
        vReceive(g_aucAnswer, 1);
        if (!g_aucAnswer[0])
            A = L = 0;
    }
}

// Function $23 _FSIZE: random record = size of the file in records of the record size (0 = 128)
static void vDOS_GET_FILE_SIZE_FCB()
{
    if (!bFCBFromCaller())
        return;

    if (bFCBFind(true))
    {
        g_oFCB.m_ulRandomRecordNumber = ulDivideUp(g_oFIB.m_ulFileSize, uiFCBRecordSize());
        vToCaller(DE, &g_oFCB, sizeof(g_oFCB));
        A = L = 0;
    }
    else
        A = L = 0xFF;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
// Drive of the server: current drive of the driver. Other drive: selected by MSX-DOS (not changed if it does not
// exist, as programs check with _CURDRV).
static void vDOS_SELECT_DISK()
{
    if (bIsPhysicalDriveHandled(E))
    {
        vSendCommonHeader();
        vJIOTransmit(&E, sizeof(E));
        vReceive(&A, sizeof(A));
        if (g_pucStub[STUB_DOS1])
        {
            // MSX-DOS 1 only knows its own drives: A = number of drives including this one, MSX-DOS not called
            if (A <= E)
                A = E + 1;
        }
        else
            g_bResult = false;
        L = A;
        g_ucCurrentDisk = E;
    }
    else
    {
        // A = number of drives: the drive does not exist after them (current drive of MSX-DOS not changed)
        g_bResult = true;
        vOriginalFunction();
        if (E < A)
        {
            // drive of MSX-DOS selected (an invalid drive, e.g. E = 0FFH to get the number of drives, changes nothing)
            g_ucDosDrive = E;
            g_ucCurrentDisk = 0xFF;
        }
        if (g_pucStub[STUB_DOS1])
        {
            // MSX-DOS 1: number of drives including the drives of the server (COMMAND.COM checks a drive letter with
            // it, _SELDSK with E = 0FFH)
            unsigned char ucDrive;

            for (ucDrive = 0; ucDrive < 8; ucDrive++)
                if (bIsPhysicalDriveHandled(ucDrive) && (A <= ucDrive))
                    A = ucDrive + 1;
            L = A;
        }
    }
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
// Drives of MSX-DOS and drives of the server
static void vDOS_GET_LOGIN_VECTOR()
{
    unsigned char ucDrive;

    g_bResult = true;
    vOriginalFunction();
    for (ucDrive = 0; ucDrive < 8; ucDrive++)
        if (bIsPhysicalDriveHandled(ucDrive))
            L |= 1 << ucDrive;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
// Drive of the server selected: answered here. Otherwise: MSX-DOS (current drive of MSX-DOS known).
static void vDOS_GET_CURRENT_DRIVE()
{
    g_bResult = true;                       // nothing sent to the server
    if (g_ucCurrentDisk != 0xFF)
        L = A = g_ucCurrentDisk;
    else
    {
        vOriginalFunction();
        g_ucDosDrive = A;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vDOS_PARSE_PATHNAME()
{
    g_bResult = true;

    // original function (with the TPA mapped), drive of the current disk of the driver if no drive is given
    vOriginalFunction();

    if (!(B & 4) && (g_ucCurrentDisk != 0xFF))
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
    { 0x11, vDOS_SEARCH_FIRST_FCB },
    { 0x12, vDOS_SEARCH_NEXT_FCB },
    { 0x13, vDOS_DELETE_FILE_FCB },
    { 0x14, vDOS_SEQUENTIAL_READ_FCB },
    { 0x15, vDOS_SEQUENTIAL_WRITE_FCB },
    { 0x16, vDOS_CREATE_FILE_FCB },
    { 0x17, vDOS_RENAME_FILE_FCB },
    { 0x18, vDOS_GET_LOGIN_VECTOR },
    { 0x19, vDOS_GET_CURRENT_DRIVE },
    { 0x1A, vDOS_SET_DISK_TRANSFER_ADDRESS },
    { 0x1B, vDOS_GET_ALLOCATION_INFORMATION },
    { 0x1C, vCheckRFS },
    { 0x1D, vSendCommonHeader },
    { 0x21, vDOS_RANDOM_READ_FCB },
    { 0x22, vDOS_RANDOM_WRITE_FCB },
    { 0x23, vDOS_GET_FILE_SIZE_FCB },
    { 0x26, vDOS_RANDOM_BLOCK_WRITE_FCB },
    { 0x27, vDOS_RANDOM_BLOCK_READ_FCB },
    { 0x28, vDOS_RANDOM_WRITE_FCB },
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
    g_pHandler = pEntry->handler;
    vCallHandler();
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
