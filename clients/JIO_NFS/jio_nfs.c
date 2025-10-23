
#include "../JIO_MSX-DOS/drv_jio.inc"
#include "jio_nfs.h"

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

#pragma function=intrinsic(0)

void _opc(unsigned char);
#define cei()	{ _opc(0xFB); }
#define cdi()	{ _opc(0xF3); }
#define halt()	{ _opc(0x76); }
#define nop()	{ _opc(0x00); }

typedef struct
{
	char			m_acSig1;
	char			m_acSig2;
	char			m_acSig3;
	unsigned char	m_ucFlags;
	unsigned char	m_ucCommand;
    unsigned char   m_ucFunction;
} tdCommonHeader;

typedef void (*tdDosHandler)();

typedef unsigned char bool;

typedef unsigned int size_t;

extern bool		bJIOReceive(void *_pvDestination, unsigned int _uiSize);
extern void		vJIOTransmit(void *_pvSource, unsigned int _uiSize);
extern size_t   strlen(const char *);

extern tdRegisters g_aoRegisters;
extern tdCommonHeader	g_oCommonHeader;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void vTransmitString(char *_pcString)
{
    vJIOTransmit(_pcString, strlen(_pcString) + 1);
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
void vDOS_FIND_FIRST_ENTRY()
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
    {
        vJIOTransmit(DE, sizeof(tdFileInfoBlock));
}
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
        g_oCommonHeader.m_ucFunction = C;
        vJIOTransmit((void*)&g_oCommonHeader, sizeof(g_oCommonHeader));
        g_aDosHandlers[C]();

        return true;
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
