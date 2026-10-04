
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

    g_pucMapper = pucMapperTable();
    vVerbose("Mapper routines: ", (unsigned int) g_pucMapper, 4);
    if (!g_pucMapper)
    {
        vError("No memory mapper support routines (MSX-DOS 2 needed).\r\n", 1);
        return false;
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

// false if cancelled with [ESC]
bool bGetServerInfo()
{
    bool    bWaiting = false;
    bool    bReceived;

    for (;;)
    {
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

    g_aucInfo[sizeof(g_aucInfo) - 1] = 0;
    g_ucAutoRetry = (g_aucInfo[0] & FLAG_AUTO_RETRY) ? 1 : 0;
    puts((char *) g_aucInfo + 3);           // description of the server (as the JIO ROM)
    vVerbose("Auto retry: ", g_ucAutoRetry, 2);
    return true;
}

void vInstallStub()
{
    memcopy(HIMSAV, &stub_start, &stub_end - &stub_start);
    vRelocate(HIMSAV, &stub_reloc_start, (&stub_reloc_end - &stub_reloc_start) >> 1);

    HIMSAV[STUB_SEGMENT] = g_ucDriverSegment;
    HIMSAV[STUB_AUTO_RETRY] = g_ucAutoRetry;
    *((unsigned int*)(HIMSAV + STUB_ENTRY + 1)) = DRIVER_BASE + driver__vDriverEntry;
    *((unsigned int*)(HIMSAV + STUB_GET_P2 + 1)) = (unsigned int) g_pucMapper + 0x27;
    HIMSAV[STUB_HOOK_ORIGINAL + 1] = *((unsigned char*)0xF37B);
    HIMSAV[STUB_HOOK_ORIGINAL + 2] = *((unsigned char*)0xF37C);

    *((unsigned char*)0xF37A) = (unsigned char)0xC3;
    *((unsigned int*)0xF37B) = HIMSAV + STUB_HOOK;
    g_pbHandledDrives = HIMSAV + STUB_DRIVES;
}

/*
 =======================================================================================================================
    PROGRAM environment item set back to SHELL. COMMAND2 sets PROGRAM to the path of JIO.COM, which does not end (it
    restarts MSX-DOS): after the restart, COMMAND2 would take it as its own path (SHELL), and load JIO.COM instead of
    COMMAND2.COM when it reloads itself ("Wrong version of command"). An empty SHELL deletes PROGRAM.
 =======================================================================================================================
 */
char g_acEnvValue[256] = { 0 };

void vRestoreProgramItem() __naked
{
__asm
        push    ix
        push    iy
        ld      hl,shell_item
        ld      de,_g_acEnvValue
        ld      b,255
        ld      c,0x6B      ; _GENV
        call    5
        or      a
        jr      z,genv_ok
        xor     a
        ld      (_g_acEnvValue),a
genv_ok:
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
void vUsage(void)
{
    puts("\r\nUsage: RFS [+] [+A|-A] [+B|-B] ... [S] [V] [H]\r\n");
    puts("  +          Add / handle all the drives served by the server\r\n");
    puts("  +<drive>   Add / handle drive (A..H)\r\n");
    puts("  -<drive>   Remove / unhandle drive (A..H)\r\n");
    puts("  S          Show currently handled drives\r\n");
    puts("  V          Show the steps of the install\r\n");
    puts("  H          Show this help\r\n");
    puts("\r\nExamples:\r\n");
    puts("  RFS +           ; install and handle the drives served\r\n");
    puts("  RFS +A +B       ; install and handle drives A and B\r\n");
    puts("  RFS -C          ; remove drive C\r\n");
    puts("  RFS S           ; list handled drives\r\n");
    puts("  RFS H           ; show this message\r\n");
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

    bInstalled = bCheckRFS(&pcBase);

    g_pbHandledDrives = pcBase + STUB_DRIVES;

    for (int iIndex = 1; iIndex < argc; iIndex++)
    {
        szArg = argv[iIndex];

        if (szArg[0])
        {
            str_to_upper(szArg);

            if ((szArg[0] == '+') && !szArg[1])
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
            vRestoreProgramItem();
            if (!bInstallDriver())
                return g_iResult;
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
