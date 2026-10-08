
#include "driver.h"
#include "stub.h"
#include "../../common/drv_jio.inc"

extern char driver_start;
extern char driver_end;
extern char driver_reloc_start;
extern char driver_reloc_end;

extern char stub_start;
extern char stub_end;
extern char stub_reloc_start;
extern char stub_reloc_end;

extern char jumper_start;
extern char jumper_end;
extern char jumper_reloc_start;
extern char jumper_reloc_end;

typedef unsigned char bool;
#define true 1
#define false 0

int     main(int argc, char **argv);
void	cputs(const char *str);
void    vError(const char * _szErrorMessage, int _iErrorCode);
__at (0xF349) char * HIMSAV;


/*
 =======================================================================================================================
 =======================================================================================================================
 */
void start() __naked
{
__asm
        ld      hl,80h      ; Argument buffer
        ld      c,(hl)
        inc     hl
        ld      b,0
        add     hl,bc
        ld      (hl),0      ; Zero terminate it
        ld      de,81h
        ld      bc,nularg

        pop     hl          ; Return address
        exx

        ld      a,(80h)     ; Get buffer length
        inc     a           ; Allow for null byte
        neg
        ld      l,a
        ld      h,-1
        add     hl,sp
        ld      sp,hl       ; Allow space for args
        ld      bc,0        ; Flag end of args
        push	bc
        ld      hl,80h      ; Address of argument buffer
        ld      c,(hl)
        ld      b,0
        add     hl,bc
        ld      b,c
        ex      de,hl
        ld      hl,(6)
        ld      c,1
        dec     hl
        ld      (hl),0
        inc     b

l9:     ld      a,(de)
        cp      ' '
        jr      nz,l3
        dec     de
        djnz	l9
        jr      l6

l2:     ld      a,(de)
        cp      ' '
        dec     de
        jr      nz,l1
        push	hl
        inc     c

l4:     ld      a,(de)      ; Remove extra spaces
        cp      ' '
        jr      nz,l5
        dec 	de
        jr      l4

l5:     xor 	a
l1:     dec     hl
        ld      (hl),a

l3:     djnz	l2

l6:     ld      d,b
        ld  	e,c
        ld  	hl,nularg
        push	hl
        ld  	hl,0
        add     hl,sp

        ex      de,hl
        call	_main
        jp      0

nularg:	defw	0
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char * jumper_target = (char*)0x8100;
unsigned char g_aucDrivesToChange [8] = { 0 };
bool *g_pbHandledDrives = 0;
int    g_iResult = 0;
unsigned char *g_pucMapper = 0;             // mapper support routines (jump table)
unsigned char g_ucDriverSegment = 0;
bool g_bVerbose = false;                    // V option: steps of the install

/*
 =======================================================================================================================
    Verbose output (V option): BDOS _CONOUT, the strings are not changed (puts ends them with a '$')
 =======================================================================================================================
 */
void vPutChar(char _cChar) __naked
{
    _cChar;
__asm
        ld      e,a
        ld      c,2
        jp      5
__endasm;
}

void vPrint(const char *_szText)
{
    while (*_szText)
        vPutChar(*_szText++);
}

void vVerbose(const char *_szLabel, unsigned int _uiValue, unsigned char _ucDigits)
{
    if (!g_bVerbose)
        return;
    vPrint(_szLabel);
    while (_ucDigits--)
        vPutChar("0123456789ABCDEF"[(_uiValue >> (_ucDigits * 4)) & 15]);
    vPrint("\r\n");
}

void vVerboseText(const char *_szText)
{
    if (g_bVerbose)
        vPrint(_szText);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void str_to_upper(char *s)
{
    if (!s) return;

    while (*s)
    {
        if (*s >= 'a' && *s <= 'z')
            *s = *s - 'a' + 'A';
        s++;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void puts(char * _szString) __naked
{
    _szString;

__asm
        ld      d,h
        ld      e,l
        dec     hl

b1: 	inc 	hl
        ld      a,(hl)
        cp      0x24
        jr      z,dollar
        or      a
        jr      nz,b1

        ld      (hl),0x24

        push	hl
        ld      c,9
        call	5
        pop     hl
        ret

dollar:	push	hl
        ld      c,9
        call	5
        ld      e,0x24
        ld      c,2
        call	5
        pop     hl

        ld      d,h
        ld      e,l
        inc     de
        jr      b1
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vJumpTo( char *_pcAddress) __naked
{
    _pcAddress;

__asm
        nop
        jp (hl)
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void memcopy(void *dest, const void *src, unsigned int n)
{
    dest;
    src;
    n;

__asm
        ex      de,hl
        ld      c,(ix+4)
        ld      b,(ix+5)
        ldir
__endasm;
}
/*
 =======================================================================================================================
 =======================================================================================================================
 */
// Code copied at _pcCopy, relocated for the address _uiBase (MSX-DOS 1: the stub is prepared in a buffer)
void vRelocateFor(char * _pcCopy, unsigned int _uiBase, unsigned int * _puiRelocationStart, unsigned int _uiSize)
{
    while (_uiSize)
    {
        *((unsigned int*)(_pcCopy + *_puiRelocationStart)) += _uiBase;
        _uiSize--;
        _puiRelocationStart++;
    }
}

void vRelocate(char * _pcCodeStart, unsigned int * _puiRelocationStart, unsigned int _uiSize)
{
    vRelocateFor(_pcCodeStart, (unsigned int) _pcCodeStart, _puiRelocationStart, _uiSize);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
// HIMSAV lowered by the size of the stub. The stack is not moved: the memory below HIMSAV is still the resident part
// of MSX-DOS until it is restarted (a stack there overwrites it), the stack of JIO.COM is in the TPA.
void vReserveMemory() __naked
{
__asm
        ld      hl,(_HIMSAV)
        ld      de,_stub_end
        or      a
        sbc     hl,de
        ld      de,_stub_start
        add     hl,de
        ld      (_HIMSAV),hl
        ret
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
/*
 =======================================================================================================================
    Mapper support routines (MSX-DOS 2): jump table, 0 if not available
 =======================================================================================================================
 */
// IX (frame pointer of the C code) and IY kept: EXTBIO or ALL_SEG change IX on Nextor 2.1.0 alpha 2
unsigned char *pucMapperTable() __naked
{
__asm
        push    ix
        push    iy
        xor     a
        ld      hl,0
        ld      de,0x0402
        call    0xFFCA      ; EXTBIO
        pop     iy
        pop     ix
        ex      de,hl
        ret
__endasm;
}

// ALL_SEG: system segment of the primary mapper (low byte) and its slot (high byte), 0xFFFF if none is free
// (IX and IY kept)
unsigned int uiAllocateSegment() __naked
{
__asm
        push    ix
        push    iy
        ld      hl,(_g_pucMapper)
        ld      de,allseg_ret
        push    de
        ld      a,1         ; system segment (not freed when the program ends)
        ld      b,0         ; primary mapper
        jp      (hl)
allseg_ret:
        pop     iy
        pop     ix
        ld      d,b
        ld      e,a
        ret     nc
        ld      de,0xFFFF
        ret
__endasm;
}

/*
 =======================================================================================================================
    MSX-DOS 1: no mapper support routines. The mapper of the TPA (slot of page 2) is accessed with its I/O ports
    (FCH..FFH, page 2: FEH), MSX-DOS 1 keeps the segments of the boot (page 0..3: 3, 2, 1, 0).
 =======================================================================================================================
 */
bool          g_bDos1 = false;
unsigned char g_aucMapperSave[256] = { 0 };

// Major version of MSX-DOS (_DOSVER): MSX-DOS 1 has no _DOSVER, MSXDOS.SYS answers A = B = 0 to a function it does not
// know (IX and IY kept)
unsigned char ucDosVersion() __naked
{
__asm
        push    ix
        push    iy
        ld      b,0
        ld      c,0x6F      ; _DOSVER
        call    5
        pop     iy
        pop     ix
        ld      a,b
        ret
__endasm;
}

// Number of segments of the mapper of page 2 (0 = 256): segment number written at 8000H of each segment, segment 0
// then holds the last number written to a segment aliased with it (256 - size). The bytes are restored.
unsigned char ucMapperSegments() __naked
{
__asm
        di
        ld      hl,_g_aucMapperSave
        ld      bc,0                ; b = 256 segments, c = segment
mapsave:
        ld      a,c
        out     (0xFE),a
        ld      a,(0x8000)
        ld      (hl),a
        inc     hl
        inc     c
        djnz    mapsave
mapwrite:
        ld      a,c
        out     (0xFE),a
        ld      (0x8000),a
        inc     c
        djnz    mapwrite
        xor     a
        out     (0xFE),a
        ld      a,(0x8000)
        neg
        ld      e,a                 ; size (0 = 256)
        ld      hl,_g_aucMapperSave
maprestore:
        ld      a,c
        out     (0xFE),a
        ld      a,(hl)
        ld      (0x8000),a
        inc     hl
        inc     c
        djnz    maprestore
        ld      a,1                 ; segment of page 2 of the TPA
        out     (0xFE),a
        ei
        ld      a,e
        ret
__endasm;
}

void vOutP2(unsigned char _ucSegment) __naked
{
    _ucSegment;
__asm
        out     (0xFE),a
        ret
__endasm;
}

unsigned char ucGetP2() __naked
{
__asm
        ld      hl,(_g_pucMapper)
        ld      de,0x27     ; GET_P2
        add     hl,de
        jp      (hl)
__endasm;
}

void vPutP2(unsigned char _ucSegment) __naked
{
    _ucSegment;
__asm
        ld      hl,(_g_pucMapper)
        ld      de,0x24     ; PUT_P2
        add     hl,de
        jp      (hl)
__endasm;
}

/*
 =======================================================================================================================
    The driver is copied to a system segment of the mapper (at DRIVER_BASE, page 2), only the stub (stub.asm) is
    resident at HIMSAV
 =======================================================================================================================
 */
bool bInstallDriver()
{
    unsigned int  uiSegment;
    unsigned char ucTpaSegment;

    vVerbose("Mapper routines: ", (unsigned int) g_pucMapper, 4);
    if (g_bDos1)
    {
        // MSX-DOS 1: last segment of the mapper of the TPA (not used by MSX-DOS 1: segments 0..3)
        unsigned char ucSegments = ucMapperSegments();

        vVerbose("Mapper segments (MSX-DOS 1): ", ucSegments, 2);
        if (ucSegments && (ucSegments < 5))
        {
            vError("No memory mapper with a free segment (MSX-DOS 1: 128 KB needed).\r\n", 1);
            return false;
        }
        g_ucDriverSegment = ucSegments - 1;
        vVerbose("Driver segment: ", g_ucDriverSegment, 2);
        vOutP2(g_ucDriverSegment);
        memcopy((char *) DRIVER_BASE, &driver_start, &driver_end - &driver_start);
        vRelocate((char *) DRIVER_BASE, &driver_reloc_start, (&driver_reloc_end - &driver_reloc_start) >> 1);
        vOutP2(1);
        vVerboseText("Driver copied\r\n");
        return true;
    }

    uiSegment = uiAllocateSegment();
    if (uiSegment == 0xFFFF)
    {
        vError("No free memory mapper segment.\r\n", 1);
        return false;
    }
    g_ucDriverSegment = uiSegment;
    vVerbose("Driver segment: ", g_ucDriverSegment, 2);
    vVerbose("Driver segment slot: ", uiSegment >> 8, 2);

    ucTpaSegment = ucGetP2();
    vVerbose("TPA page 2 segment: ", ucTpaSegment, 2);
    vPutP2(g_ucDriverSegment);
    memcopy((char *) DRIVER_BASE, &driver_start, &driver_end - &driver_start);
    vRelocate((char *) DRIVER_BASE, &driver_reloc_start, (&driver_reloc_end - &driver_reloc_start) >> 1);
    vPutP2(ucTpaSegment);
    vVerboseText("Driver copied\r\n");

    return true;
}

// Jumper: restart of MSX-DOS through BASIC (CALL SYSTEM), MSX-DOS is then below HIMSAV
void vInstallJumper()
{
    memcopy(jumper_target, &jumper_start, &jumper_end - &jumper_start);
    vRelocate(jumper_target, &jumper_reloc_start, (&jumper_reloc_end - &jumper_reloc_start) >> 1);
}

// Stub at HIMSAV (memory reserved before the restart of MSX-DOS) and hook
/*
 =======================================================================================================================
    Server information (COMMAND_DRIVE_INFO, as the JIO ROM at boot), before anything is installed: flags of the
    server ("Auto retry") and description. While the server does not answer: "Waiting for server, press [ESC] to
    cancel" and a dot for each time-out of the receive routine (about 0.9 s), [ESC] cancels the install.
 =======================================================================================================================
 */
static const unsigned char g_aucInfoCommand[] = { 'J', 'I', 'O', FLAG_RX_CRC, COMMAND_DRIVE_INFO };
unsigned char g_aucInfo[512] = { 0 };       // answer: flags, drives, boot drive, description (never on the stack)
unsigned char g_aucInfoCRC[2] = { 0 };      // CRC of the answer (separate packet), low byte first
unsigned char g_ucAutoRetry = 0;
unsigned char g_ucTxBlocks = 0;             // large writes sent in blocks (FLAG_TX_BLOCKS: Bluetooth link)
unsigned char JioPort = 0xFF;               // serial line (transmit.asm, receive.asm, stub STUB_PORT): 0xFF = joystick
                                            // port 2, 0xFE = joystick port 1 (J1 option), else I/O register of the
                                            // JIO cartridge (C option)
bool g_bAutoLine = true;                    // no J1, J2 or C option: serial line found at install (bGetServerInfo)

// Serial routines of the driver (transmit.asm, receive.asm): HL = data, DE = size, interrupts disabled.
// receive.asm changes IX (frame pointer of the C code): kept by bInfoReceive
static void vInfoTransmit(const void *_pvSource, unsigned int _uiSize) __naked
{
    _pvSource;
    _uiSize;
__asm
#include "transmit.asm"
__endasm;
}

static bool bInfoReceive(void *_pvDestination, unsigned int _uiSize) __naked
{
    _pvDestination;
    _uiSize;
__asm
    push    ix
    push    iy
    call    bJIOReceive
    pop     iy
    pop     ix
    ret
#include "receive.asm"
__endasm;
}

// I/O port of the JIO cartridge: probe of the ports of the IOSEL switches (00H, 20H, 30H), 0xFF = not found
// (detection routine of herraa1, as the JIO ROM)
static unsigned char ucDetectCartPort() __naked
{
__asm
        ld      hl,jio_ports
jio_probe:
        ld      a,(hl)
        cp      0xff
        ret     z
        ld      c,a
        ld      a,0x2f
        out     (c),a
        in      a,(c)
        and     0xfc
        cp      0xcc
        jr      nz,jio_next
        ld      a,0xdb
        out     (c),a
        in      a,(c)
        and     0xfc
        cp      0x88
        jr      nz,jio_next
        ld      a,0xf7
        out     (c),a
        in      a,(c)
        and     0xfc
        cp      0x44
        ld      a,c
        ret     z
jio_next:
        inc     hl
        jr      jio_probe
jio_ports:
        .db     0x00,0x20,0x30,0xff
__endasm;
}

// Main ROM through CALSLT, IX (frame pointer of the C code) and IY kept: GETCPU, CHGCPU (turbo R), SNSMAT
static char cGetCPU() __naked
{
__asm
        push    ix
        push    iy
        ld      ix,0x0183   ; GETCPU
        ld      iy,(0xFCC1-1)
        call    0x001C      ; CALSLT
        pop     iy
        pop     ix
        ret
__endasm;
}

static void vSetCPU(char _cCPUMode) __naked
{
    _cCPUMode;
__asm
        push    ix
        push    iy
        ld      ix,0x0180   ; CHGCPU
        ld      iy,(0xFCC1-1)
        call    0x001C      ; CALSLT
        pop     iy
        pop     ix
        ret
__endasm;
}

static bool bEscPressed() __naked
{
__asm
        push    ix
        push    iy
        ld      a,7         ; row 7: [ESC] = bit 2 (0 = pressed)
        ld      ix,0x0141   ; SNSMAT
        ld      iy,(0xFCC1-1)
        call    0x001C      ; CALSLT
        pop     iy
        pop     ix
        and     4
        ld      a,0
        ret     nz
        inc     a
        ret
__endasm;
}

static void vDI() __naked
{
__asm
        di
        ret
__endasm;
}

static void vEI() __naked
{
__asm
        ei
        ret
__endasm;
}

bool bHasTurbo();

// CRC-16 XModem of the answer: a reception completed by noise (or by the FFH bytes sent by the server when it
// connects) is not taken as the answer
static bool bInfoCRCOK()
{
    unsigned int  uiCRC = 0;
    unsigned int  uiIndex;
    unsigned char ucBit;

    for (uiIndex = 0; uiIndex < sizeof(g_aucInfo); uiIndex++)
    {
        uiCRC ^= ((unsigned int) g_aucInfo[uiIndex]) << 8;
        for (ucBit = 0; ucBit < 8; ucBit++)
            uiCRC = (uiCRC & 0x8000) ? ((uiCRC << 1) ^ 0x1021) : (uiCRC << 1);
    }
    return (g_aucInfoCRC[0] == (unsigned char) uiCRC) && (g_aucInfoCRC[1] == (unsigned char) (uiCRC >> 8));
}

// Request to the server and its answer (and the CRC of the answer if _pvCRC), Z80 mode (turbo R), interrupts
// disabled. false: time-out.
bool bServerExchange(const void *_pvRequest, unsigned int _uiRequestSize, void *_pvAnswer, unsigned int _uiAnswerSize, void *_pvCRC)
{
    bool    bTurbo = bHasTurbo();
    bool    bReceived;
    char    cCPU = 0;

    if (bTurbo)
    {
        cCPU = cGetCPU();
        vSetCPU(0);                         // Z80 mode: timings of the serial routines
    }
    vDI();
    vInfoTransmit(_pvRequest, _uiRequestSize);
    bReceived = bInfoReceive(_pvAnswer, _uiAnswerSize) && (!_pvCRC || bInfoReceive(_pvCRC, 2));
    vEI();
    if (bTurbo)
        vSetCPU(cCPU);
    return bReceived;
}

/*
 =======================================================================================================================
    Drives served by the server ("JIO +"): BDOS _LOGIN, as the hybrid JIO ROM at boot (bit 0 = A:). Answer without
    CRC: asked just after the server information, a few times.
 =======================================================================================================================
 */
static const unsigned char g_aucLoginCommand[] = { 'J', 'I', 'O', 0, COMMAND_BDOS, 0x18 };
unsigned char g_ucLogin = 0;                // answer (never on the stack)

bool bGetServedDrives()
{
    unsigned char ucTry;

    for (ucTry = 0; ucTry < 5; ucTry++)
    {
        if (bServerExchange(g_aucLoginCommand, sizeof(g_aucLoginCommand), &g_ucLogin, sizeof(g_ucLogin), 0))
            return true;
    }
    vError("No answer from the server.\r\n", 1);
    return false;
}

// "JIO +": all the drives served by the server are handled (false: none or no answer)
bool bAddServedDrives()
{
    char            acDrive[] = "  :";
    unsigned char   ucDrive;

    if (!bGetServedDrives())
        return false;
    if (!g_ucLogin)
    {
        vError("No drive served by the server (disk image mode ?).\r\n", 1);
        return false;
    }
    vPrint("Drives served:");
    for (ucDrive = 0; ucDrive < 8; ucDrive++)
    {
        if (g_ucLogin & (1 << ucDrive))
        {
            g_aucDrivesToChange[ucDrive] = 1;
            acDrive[1] = 'A' + ucDrive;
            vPrint(acDrive);
        }
    }
    vPrint("\r\n");
    return true;
}

void vPrintSerialLine()
{
    if (JioPort == 0xFF)
        vPrint("Serial line: joystick port 2\r\n");
    else if (JioPort == 0xFE)
        vPrint("Serial line: joystick port 1\r\n");
    else
    {
        vPrint("Serial line: JIO cartridge, port ");
        vPutChar("0123456789ABCDEF"[JioPort >> 4]);
        vPutChar("0123456789ABCDEF"[JioPort & 15]);
        vPrint("H\r\n");
    }
}

// false if cancelled with [ESC]. Serial line not given (g_bAutoLine): one request on each line in turn, JIO cartridge
// (if its I/O register is found), joystick port 2, joystick port 1 (as JIOTIME), the line of the answer is kept
bool bGetServerInfo()
{
    bool            bWaiting = false;
    bool            bReceived;
    unsigned char   aucLines[3];
    unsigned char   ucLines = 0;
    unsigned char   ucLine = 0;

    if (g_bAutoLine)
    {
        aucLines[0] = ucDetectCartPort();
        if (aucLines[0] != 0xFF)
            ucLines++;
        aucLines[ucLines++] = 0xFF;
        aucLines[ucLines++] = 0xFE;
    }

    for (;;)
    {
        if (g_bAutoLine)
        {
            JioPort = aucLines[ucLine];
            if (++ucLine == ucLines)
                ucLine = 0;
        }
        bReceived = bServerExchange(g_aucInfoCommand, sizeof(g_aucInfoCommand), g_aucInfo, sizeof(g_aucInfo), g_aucInfoCRC);
        if (bReceived && bInfoCRCOK())
            break;
        bReceived = false;

        if (bEscPressed())
        {
            while (bEscPressed());          // [ESC] released: not read by COMMAND2
            break;
        }
        if (bWaiting)
            vPutChar('.');
        else
        {
            vPrint("Waiting for server, press [ESC] to cancel");
            bWaiting = true;
        }
    }

    if (bWaiting)
        vPrint("\r\n");
    if (!bReceived)
        return false;

    if (g_bAutoLine)
        vPrintSerialLine();
    g_aucInfo[sizeof(g_aucInfo) - 1] = 0;
    g_ucAutoRetry = (g_aucInfo[0] & FLAG_AUTO_RETRY) ? 1 : 0;
    g_ucTxBlocks = (g_aucInfo[0] & FLAG_TX_BLOCKS) ? 1 : 0;
    puts((char *) g_aucInfo + 3);           // description of the server (as the JIO ROM)
    vVerbose("Auto retry: ", g_ucAutoRetry, 2);
    vVerbose("Writes in blocks: ", g_ucTxBlocks, 2);
    return true;
}

// Stub copied at _pucCopy, relocated for the address _pucBase (MSX-DOS 2: both HIMSAV, MSX-DOS 1: buffer prepared
// before it is copied below MSX-DOS), with the address of GET_P2 and of the previous hook
void vFillStub(unsigned char *_pucCopy, unsigned char *_pucBase, unsigned int _uiGetP2, unsigned int _uiHookOriginal)
{
    memcopy(_pucCopy, &stub_start, &stub_end - &stub_start);
    vRelocateFor(_pucCopy, (unsigned int) _pucBase, &stub_reloc_start, (&stub_reloc_end - &stub_reloc_start) >> 1);

    _pucCopy[STUB_SEGMENT] = g_ucDriverSegment;
    _pucCopy[STUB_AUTO_RETRY] = g_ucAutoRetry;
    _pucCopy[STUB_PORT] = JioPort;
    _pucCopy[STUB_TX_BLOCKS] = g_ucTxBlocks;
    *((unsigned int*)(_pucCopy + STUB_ENTRY + 1)) = DRIVER_BASE + driver__vDriverEntry;
    *((unsigned int*)(_pucCopy + STUB_GET_P2 + 1)) = _uiGetP2;
    *((unsigned int*)(_pucCopy + STUB_HOOK_ORIGINAL + 1)) = _uiHookOriginal;
    g_pbHandledDrives = (bool *) (_pucCopy + STUB_DRIVES);
}

// MSX-DOS 2: stub at HIMSAV (memory reserved before the restart of MSX-DOS), GO_BDOS hook (F37AH)
void vInstallStub()
{
    vFillStub((unsigned char *) HIMSAV, (unsigned char *) HIMSAV, (unsigned int) g_pucMapper + 0x27, *((unsigned int*)0xF37B));

    *((unsigned char*)0xF37A) = (unsigned char)0xC3;
    *((unsigned int*)0xF37B) = (unsigned int) (HIMSAV + STUB_HOOK);
}

/*
 =======================================================================================================================
    MSX-DOS 1: no GO_BDOS hook. The BDOS jump (0005H) goes to the entry of MSXDOS.SYS (address in 0006H, also the top
    of the TPA): the stub is put just below it, 0005H jumps to the stub, which calls MSXDOS.SYS for the functions it
    does not handle. The warm boot of MSXDOS.SYS (after each program) sets 0005H again from a constant of its code
    (LD HL,<entry> / LD (0006H),HL): set to the stub. Before the stub: JP to its hook, and its GET_P2 (MSX-DOS 1:
    segment 1 in page 2). COMMAND.COM keeps a copy of itself and its data (batch file being run...) below the top of
    the TPA: the stub is put below this copy (address read by the warm boot), which stays as it is, above the new top
    of the TPA. If this address is not found: stub just below MSXDOS.SYS, and copy of COMMAND.COM made invalid (its
    checksum), the warm boot reloads it below the stub.
    The stub is prepared in a buffer, copied at the end (the stack of JIO.COM is at the top of the TPA).
 =======================================================================================================================
 */
#define DOS1_PRESTUB 6                      // JP hook, LD A,1 / RET (GET_P2)
unsigned char  g_aucStubImage[1024] = { 0 };
unsigned char  g_aucDos1Stack[128] = { 0 };
unsigned char *g_pucDos1Top = 0;            // new top of the TPA (0006H): stub image
unsigned int   g_uiStubImageSize = 0;
unsigned char *g_pucWarmBootConstant = 0;   // <entry> of LD HL,<entry> / LD (0006H),HL of the warm boot
unsigned char *g_pucCommandChecksum = 0;    // checksum of the copy of COMMAND.COM, made invalid (0: copy kept)

bool bPrepareStubDos1()
{
    unsigned int  uiEntry = *((unsigned int*)6);
    unsigned char *pucBase;
    unsigned char *p, *q;

    g_uiStubImageSize = (&stub_end - &stub_start) + DOS1_PRESTUB;
    if (g_uiStubImageSize > sizeof(g_aucStubImage))
    {
        vError("Stub too big.\r\n", 1);
        return false;
    }
    unsigned int  uiTop = uiEntry;

    // Warm boot of MSXDOS.SYS: LD HL,<entry> / LD (0006H),HL, then the check of the copy of COMMAND.COM:
    // LD HL,(<start>) / LD BC,(<size>) ... LD HL,(<checksum>) / SBC HL,DE
    for (p = (unsigned char *) uiEntry; p < (unsigned char *) uiEntry + 0x1000; p++)
    {
        if ((p[0] == 0x21) && (p[1] == (uiEntry & 0xFF)) && (p[2] == (uiEntry >> 8)) && (p[3] == 0x22) && (p[4] == 6) && (p[5] == 0))
        {
            g_pucWarmBootConstant = p + 1;
            for (q = p + 6; q < p + 80; q++)
            {
                if ((q[0] == 0x2A) && (q[3] == 0xED) && (q[4] == 0x4B) && (uiTop == uiEntry))
                    uiTop = **((unsigned int **) (q + 1));      // start of the copy of COMMAND.COM
                if ((q[0] == 0xED) && (q[1] == 0x52) && (q[-3] == 0x2A))
                {
                    g_pucCommandChecksum = *((unsigned char **) (q - 2));
                    break;
                }
            }
            break;
        }
    }
    if ((uiTop >= uiEntry) || (uiTop < 0x8000))
        uiTop = uiEntry;                    // copy of COMMAND.COM not found: reloaded below the stub
    else
        g_pucCommandChecksum = 0;           // copy of COMMAND.COM kept, above the stub

    g_pucDos1Top = (unsigned char *) (uiTop - g_uiStubImageSize);
    pucBase = g_pucDos1Top + DOS1_PRESTUB;
    vFillStub(g_aucStubImage + DOS1_PRESTUB, pucBase, (unsigned int) (g_pucDos1Top + 3), uiEntry);
    g_aucStubImage[0] = 0xC3;               // JP hook
    *((unsigned int*)(g_aucStubImage + 1)) = (unsigned int) (pucBase + STUB_HOOK);
    g_aucStubImage[3] = 0x3E;               // LD A,1 / RET: segment of page 2 of the TPA
    g_aucStubImage[4] = 1;
    g_aucStubImage[5] = 0xC9;

    vVerbose("MSXDOS.SYS entry: ", uiEntry, 4);
    vVerbose("Warm boot constant: ", (unsigned int) g_pucWarmBootConstant, 4);
    vVerbose("Copy of COMMAND.COM: ", uiTop, 4);
    vVerbose("COMMAND.COM reloaded, checksum: ", (unsigned int) g_pucCommandChecksum, 4);
    vVerbose("Stub: ", (unsigned int) pucBase, 4);
    if (!g_pucWarmBootConstant)
    {
        vError("Warm boot of MSXDOS.SYS not found (unknown version).\r\n", 1);
        return false;
    }
    return true;
}

// Stub copied below MSXDOS.SYS, BDOS jump and warm boot set to it, COMMAND.COM reloaded, reset of the server
// through the stub, then warm boot (no return: the stack of JIO.COM was at the top of the TPA)
void vFinishDos1() __naked
{
__asm
        ld      sp,_g_aucDos1Stack+128
        ld      hl,_g_aucStubImage
        ld      de,(_g_pucDos1Top)
        ld      bc,(_g_uiStubImageSize)
        ldir
        ld      de,(_g_pucDos1Top)
        ld      (6),de                      ; BDOS jump: stub
        ld      hl,(_g_pucWarmBootConstant)
        ld      (hl),e
        inc     hl
        ld      (hl),d
        ld      hl,(_g_pucCommandChecksum)
        ld      a,h
        or      l
        jr      z,dos1_reset
        inc     (hl)                        ; copy of COMMAND.COM invalid: reloaded by the warm boot
dos1_reset:
        ld      c,0x1D                      ; reset of the server (through the stub)
        call    5
        jp      0
__endasm;
}

/*
 =======================================================================================================================
    PROGRAM environment item set back to SHELL. COMMAND2 sets PROGRAM to the path of JIO.COM, which does not end (it
    restarts MSX-DOS): after the restart, COMMAND2 would take it as its own path (SHELL), and load JIO.COM instead of
    COMMAND2.COM when it reloads itself ("Wrong version of command"). An empty SHELL deletes PROGRAM.
 =======================================================================================================================
 */
char g_acEnvValue[256] = { 0 };

/*
 =======================================================================================================================
    Value of an environment item (_GENV) in g_acEnvValue, empty if not found
 =======================================================================================================================
 */
void vGetEnv(const char *_szItem) __naked
{
    _szItem;

__asm
        push    ix
        push    iy
        ld      de,_g_acEnvValue
        ld      b,255
        ld      c,0x6B      ; _GENV
        call    5
        or      a
        jr      z,genv_ok
        xor     a
        ld      (_g_acEnvValue),a
genv_ok:
        pop     iy
        pop     ix
        ret
__endasm;
}

void vRestoreProgramItem() __naked
{
__asm
        push    ix
        push    iy
        ld      hl,shell_item
        call    _vGetEnv
        ld      hl,program_item
        ld      de,_g_acEnvValue
        ld      c,0x6C      ; _SENV
        call    5
        pop     iy
        pop     ix
        ret
shell_item:
        .ascii  "SHELL"
        .db     0
program_item:
        .ascii  "PROGRAM"
        .db     0
__endasm;
}

bool bCheckRFS(unsigned int * _puiBase) __naked
{
    _puiBase;

__asm
        push    hl
        ld      c,0x1C
        call    5
        ld      a,l
        cp      'R'
        pop     hl
        jr      z,installed
        xor     a
        ret

installed:
        ld      (hl),e
        inc     hl
        ld      (hl),d
        ld      a,1
        ret
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vApplyDriveChanges()
{
    for(unsigned int iIndex = 0; iIndex < 8; iIndex++)
    {
        if (g_aucDrivesToChange[iIndex] == 1)
        {
            g_pbHandledDrives[iIndex] = true;

        }
        else
        if (g_aucDrivesToChange[iIndex] == 2)
        {
            g_pbHandledDrives[iIndex] = false;
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static void vExample(const char *_szName, const char *_szText)
{
    unsigned char   ucLength = 0;

    vPrint("  ");
    vPrint(_szName);
    while (_szName[ucLength])
        ucLength++;
    for (; ucLength < 3; ucLength++)
        vPutChar(' ');                      // columns aligned as for a 3 letter name
    vPrint(_szText);
}

void vUsage(void)
{
    char    acName[9];
    char    *pcName = acName;

    // name of the program: PROGRAM environment item (path of the program, set by COMMAND2), without path and extension
    vGetEnv("PROGRAM");
    for (char *pcPath = g_acEnvValue; *pcPath; pcPath++)
    {
        if ((*pcPath == '\\') || (*pcPath == ':'))
            pcName = acName;
        else if (*pcPath == '.')
            break;
        else if (pcName < acName + 8)
            *pcName++ = *pcPath;
    }
    if (pcName == acName)
    {
        acName[0] = 'J';
        acName[1] = 'I';
        acName[2] = 'O';
        pcName = acName + 3;
    }
    *pcName = 0;

    vPrint("\r\nUsage: ");
    vPrint(acName);
    vPrint(" [+] [+A|-A] [+B|-B] ... [J1|J2|C[<port>]] [S] [V] [H]\r\n");
    vPrint("  +          Add / handle all the drives served by the server\r\n");
    vPrint("  +<drive>   Add / handle drive (A..H)\r\n");
    vPrint("  -<drive>   Remove / unhandle drive (A..H)\r\n");
    vPrint("  J1, J2     Joystick port 1 or 2 (default: JIO\r\n");
    vPrint("             cartridge, joystick 2, joystick 1)\r\n");
    vPrint("  C[<port>]  JIO cartridge instead of joystick port 2,\r\n");
    vPrint("             I/O port in hex (00, 20 or 30, detected\r\n");
    vPrint("             if not given)\r\n");
    vPrint("  S          Show currently handled drives\r\n");
    vPrint("  V          Show the steps of the install\r\n");
    vPrint("  H          Show this help (also without parameters)\r\n");
    vPrint("\r\nExamples:\r\n");
    vExample(acName, " +           ; install and handle the drives served\r\n");
    vExample(acName, " +A +B       ; install and handle drives A and B\r\n");
    vExample(acName, " -C          ; remove drive C\r\n");
    vExample(acName, " C +         ; JIO cartridge (port detected)\r\n");
    vExample(acName, " S           ; list handled drives\r\n");
    vExample(acName, " H           ; show this message\r\n");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vError(const char * _szErrorMessage, int _iErrorCode)
{
    puts(_szErrorMessage);
    g_iResult = _iErrorCode;
}


bool bHasTurbo() __naked
{
__asm

  ld a,(0xFCC1)
  ld hl,0x180
  call  0x000C
  cp 0xC3
  jr nz,noTurbo

  ld a,(0xFCC1)
  ld hl,0x183
  call  0x000C
  cp 0xC3
  jr nz,noTurbo
  ld a,1
  ret
noTurbo:
  xor a
  ret
__endasm;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int main(int argc, char **argv)
{
    bool            bInstalled;
    bool            bAddRequired;
    bool            bRemoveRequired;
    bool            bAddAll = false;
    unsigned char * pcBase;
    unsigned char * szArg;

    bAddRequired = false;
    bRemoveRequired = false;

    if (argc < 2)
    {
        vUsage();                           // no parameter: help
        return 0;
    }

    bInstalled = bCheckRFS(&pcBase);

    g_pbHandledDrives = pcBase + STUB_DRIVES;
    if (bInstalled)
        JioPort = pcBase[STUB_PORT];        // serial line of the installed driver

    for (int iIndex = 1; iIndex < argc; iIndex++)
    {
        szArg = argv[iIndex];

        if (szArg[0])
        {
            str_to_upper(szArg);

            if ((szArg[0] == 'J') && ((szArg[1] == '1') || (szArg[1] == '2')) && !szArg[2])
            {
                // joystick port 1 or 2
                JioPort = (szArg[1] == '1') ? 0xFE : 0xFF;
                g_bAutoLine = false;
                if (bInstalled)
                    pcBase[STUB_PORT] = JioPort;
            }
            else if (szArg[0] == 'C')
            {
                // JIO cartridge: I/O port (hex, 2 digits), detected by default
                unsigned char   ucPort = 0;
                bool            bValid = true;

                if (!szArg[1])
                {
                    ucPort = ucDetectCartPort();
                    if (ucPort == 0xFF)
                    {
                        vError("JIO cartridge not found (IOSEL switches: 00, 20 or 30).\r\n", 1);
                        return g_iResult;
                    }
                }
                else if (szArg[2] && !szArg[3])
                {
                    for (unsigned char ucIndex = 1; ucIndex < 3; ucIndex++)
                    {
                        char    cDigit = szArg[ucIndex];

                        ucPort <<= 4;
                        if ((cDigit >= '0') && (cDigit <= '9'))
                            ucPort += cDigit - '0';
                        else if ((cDigit >= 'A') && (cDigit <= 'F'))
                            ucPort += cDigit - 'A' + 10;
                        else
                            bValid = false;
                    }
                }
                else
                    bValid = false;

                if (!bValid || (ucPort >= 0xFE))
                    vError("Invalid port. Use C or C<port> (hex, e.g. C20).", 1);
                else
                {
                    JioPort = ucPort;
                    g_bAutoLine = false;
                    if (bInstalled)
                        pcBase[STUB_PORT] = ucPort;
                }
            }
            else if ((szArg[0] == '+') && !szArg[1])
            {
                bAddAll = true;             // all the drives served by the server
                bAddRequired = true;
            }
            else if (szArg[1])
            {
                if (!szArg[2])
                {
                    if((szArg[1] >= 'A') && (szArg[1] <= 'A' + 7))
                    {
                        switch(szArg[0])
                        {
                        case '+':
                            g_aucDrivesToChange[szArg[1] - 'A'] = 1;
                            bAddRequired = true;
                            break;
                        case '-':
                            g_aucDrivesToChange[szArg[1] - 'A'] = 2;
                            bRemoveRequired = true;
                            break;
                        default:
                            vError("Invalid operator. Use +<drive> or -<drive> (e.g., +A, -C).", 1);
                            break;
                        }
                    }
                    else
                    {
                        vError("Invalid drive letter. Use A..H.", 1);
                    }
                }
                else
                {
                    vError("Too many characters. Use +<drive> or -<drive> only (e.g., +A).", 1);
                }
            }
            else if (szArg[0] == 'S')
            {
                if (bInstalled)
                {
                    vPrintSerialLine();
                    vPrint("Disks handled:");
                    for(int iIndex = 0; iIndex < 8; iIndex++)
                    {
                        if (g_pbHandledDrives[iIndex])
                        {
                            vPutChar(' ');
                            vPutChar('A' + iIndex);
                            vPutChar(':');
                        }
                    }
                    vPrint("\r\n");
                }
                else
                    puts("RFS not installed.\r\n");
            }
            else if (szArg[0] == 'V')
            {
                g_bVerbose = true;
            }
            else if (szArg[0] == 'H')
            {
                vUsage();
            }
            else
            {
                vError("Unknown option. Use 'H' for help.", 1);
            }
        }
    }

    if (!bInstalled)
    {
        if (bAddRequired)
        {
            if (!bGetServerInfo() || (bAddAll && !bAddServedDrives()))
            {
                puts("RFS not installed.\r\n");
                return g_iResult;
            }
            puts("Installing RFS and drives...\r\n");
            g_bDos1 = ucDosVersion() < 2;   // MSX-DOS 1: mapper accessed with its I/O ports, even if mapper support
                                            // routines are found (memory manager)
            g_pucMapper = g_bDos1 ? 0 : pucMapperTable();
            if (!g_bDos1)
            {
                vRestoreProgramItem();
                if (!g_pucMapper)
                {
                    vError("No memory mapper support routines.\r\n", 1);
                    return g_iResult;
                }
            }
            if (!bInstallDriver())
                return g_iResult;
            if (g_bDos1)
            {
                if (!bPrepareStubDos1())
                    return g_iResult;
                g_aucStubImage[DOS1_PRESTUB + STUB_HAS_TURBO] = bHasTurbo();
                g_aucStubImage[DOS1_PRESTUB + STUB_DOS1] = 1;
                vApplyDriveChanges();
                vVerboseText("Restarting COMMAND.COM...\r\n");
                vFinishDos1();
            }
            vReserveMemory();
            vVerbose("Memory reserved, HIMSAV: ", (unsigned int) HIMSAV, 4);
            vInstallJumper();
            vInstallStub();
            HIMSAV[STUB_HAS_TURBO] = bHasTurbo();
            vVerbose("Stub: ", (unsigned int) HIMSAV, 4);
            vVerboseText("Testing the hook (RESET)...\r\n");
__asm
  ld c,0x1D
  call 0xF37A
__endasm;
            vVerboseText("OK\r\n");
            vApplyDriveChanges();
            vVerboseText("Restarting MSX-DOS...\r\n");
            vJumpTo(jumper_target);
        }
        else
        {
            if (bRemoveRequired)
                puts("RFS not installed, nothing to remove.\r\n");
        }
    }
    else
    {
        if (bAddAll && !bAddServedDrives())
            return g_iResult;
        if (bAddRequired || bRemoveRequired)
        {
            vApplyDriveChanges();
            puts("Changes applied.");
        }
    }

    return g_iResult;
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

_stub_start:
    INCBIN "stub"
_stub_end:

_stub_reloc_start:
    INCBIN "stub.reloc"
_stub_reloc_end:

_jumper_start:
    INCBIN "jumper"
_jumper_end:

_jumper_reloc_start:
    INCBIN "jumper.reloc"
_jumper_reloc_end:

__endasm;
}
