
#include "flags.inc"

typedef unsigned char bool;

#pragma function=intrinsic(0)
void _opc(unsigned char);
#define cei()	_opc(0xFB)
#define cdi()	_opc(0xF3)


typedef enum
{
    eZ80 = 0,
    eR800_ROM = 1,
    eR800_DRAM = 2
}
tdCPUMode;


typedef struct
{
	char			m_acSig[3];
    unsigned char	m_ucCommand;
    unsigned char	m_aucSector[3];
    unsigned char	m_ucLength;
    void			*m_pvAddress;
} tdReadWriteHeader;

#define false	0
#define true	1

unsigned int	uiXModemCRC16(void *_pvAddress, unsigned int _uiLength);
bool			bJIOReceive(void *_pvDestination, unsigned int _uiSize);
void			vJIOSend(void *_pvSource, unsigned int _uiSize);
tdCPUMode       eGetCPU();
void            vSetCPU(tdCPUMode _eMode);

#define READ(a, b) \
	while(!bJIOReceive(a, b)) \
	{ \
		if(!(_ucFlags & FLAGBITS_RETRY_TIMEOUT)) return 1; \
	}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned char ucReadOrWriteSectors
    (
        unsigned long	_ulSector,
        unsigned char	_ucLength,
        void			*_pvAddress,
        unsigned char	_ucFlags
        )
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    unsigned int		uiTotalLength;
    unsigned int		uiComputedCRC;
    unsigned int		uiReceivedCRC;
	bool				bSuccess;
    tdReadWriteHeader   oHeader;
    tdCPUMode           eCPUMode;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    eCPUMode = eGetCPU();

	oHeader.m_acSig[0] = 'J';
	oHeader.m_acSig[1] = 'I';
    oHeader.m_acSig[2] = 'O';
    oHeader.m_aucSector[0] = (_ulSector >> 0);
    oHeader.m_aucSector[1] = (_ulSector >> 8);
    oHeader.m_aucSector[2] = (_ulSector >> 16);
    oHeader.m_ucLength = _ucLength;
    oHeader.m_pvAddress = _pvAddress;

    uiTotalLength = _ucLength * 512;

    do
    {
        oHeader.m_ucCommand = _ucFlags;

        vSetCPU(eZ80);
        cdi();

        vJIOSend(&oHeader, sizeof(oHeader));

        if ((_ucFlags & 0x0F) == COMMAND_WRITE)
            vJIOSend(_pvAddress, uiTotalLength);
        else
            READ(_pvAddress, uiTotalLength);

        if(((_ucFlags  & FLAGBITS_READ_CRC)  && ((_ucFlags & 0x0F) == COMMAND_READ)) ||
            ((_ucFlags & FLAGBITS_WRITE_CRC) && ((_ucFlags & 0x0F) == COMMAND_WRITE)))
        {
            READ(&uiReceivedCRC, sizeof(uiReceivedCRC));

            vSetCPU(eR800_DRAM);
            uiComputedCRC = uiXModemCRC16(_pvAddress, uiTotalLength);
            bSuccess = uiReceivedCRC == uiComputedCRC;
            if(!bSuccess)
            {
                oHeader.m_ucCommand++;
                vSetCPU(eZ80);
                cdi();
                vJIOSend(&oHeader, sizeof(oHeader));
            }
        }
        else
            bSuccess = true;
	} while(!bSuccess && (_ucFlags & FLAGBITS_RETRY_CRC));

    vSetCPU(eCPUMode);
    cei();

    return bSuccess ? 0 : 1;
}

/*$off*/

/*

CALLING CONVENTION

Up to two parameters can be passed in registers; other parameters are
transferred on the stack.

The compiler assembler interface selects the parameters that can be placed
in the registers as follows:

Parameters, types, and locations

1                   2                   Remaining parameters

Byte                Byte                All types
E                   C                   Pushed

Byte                Word                All types
E                   BC                  Pushed

Byte                3 bytes (pointer)   All types
E                   Pushed              Pushed

Word                Byte                All types
DE                  C                   R7

Word                Word                All types
DE                  BC                  Pushed

3 bytes (pointer)   All types           All types
CDE                 Pushed              Pushed

4 bytes (long etc.) All types           All types
BCDE                Pushed              Pushed

Variable arguments  All types           All types
Pushed              Pushed              Pushed


RETURN VALUES

char                    A (from a non-banked function)
char                    L (from a banked function)
word                    HL
pointer                 HL
banked function pointer CHL
long, float, or double  BCHL


PRESERVING REGISTERS

A and HL are always considered destroyed after a function call. Registers
used for parameters are also considered destroyed after a function call; this
includes the entire 16-bit register. For example, if E is used for a parameter
DE is considered destroyed. All other registers (excluding return value
registers) must be preserved by the called function.

For example, consider the following function prototype:
long foo(int,...)

A and HL are always destroyed, BC is destroyed since it is used for the
return value. Since this is a vararg function it does not take any
parameter in registers which means that IX, IY, and DE must be preserved
as usual.

If the alternative register set is in use by the code generator -ua, HL' is
considered destroyed after each function call, but DE' and BC' must be
preserved.

*/

/*$on*/
