

#include "flags.inc"

typedef unsigned char bool;

#pragma function = intrinsic(0)
void _opc (unsigned char);
#define cei()	_opc(0xFB)
#define cdi()	_opc(0xF3)

typedef enum { eZ80 = 0, eR800_ROM = 1, eR800_DRAM = 2 } tdCPUMode;

typedef struct
{
	char			m_acSig[3];
	unsigned char	m_ucCommand;
} tdCommonHeader;

typedef struct
{
	unsigned char	m_aucSector[3];
	unsigned char	m_ucLength;
	void			*m_pvAddress;
} tdReadWriteData;

#define false	0
#define true	1

unsigned int	uiXModemCRC16(void *_pvAddress, unsigned int _uiLength, unsigned int _uiCRC);
bool			bJIOReceive(void *_pvDestination, unsigned int _uiSize);
void			vJIOTransmit(void *_pvSource, unsigned int _uiSize);
tdCPUMode		eGetCPU();
void			vSetCPU(tdCPUMode _eMode);

/*
 =======================================================================================================================
 =======================================================================================================================
 */

unsigned int uiTransmit
(
	void			*_pvAddress,
	unsigned int	_uiLength,
	unsigned char	_ucFlags,
	unsigned int	_uiCRC,
	bool			_bLast
)
{
	if(_ucFlags & FLAGBITS_TX_CRC)
	{
        vSetCPU(eR800_DRAM);
        _uiCRC = uiXModemCRC16(_pvAddress, _uiLength, _uiCRC);
	}

    vSetCPU(eZ80);
    cdi();
    vJIOTransmit(_pvAddress, _uiLength);

	if(_bLast && (_ucFlags & FLAGBITS_TX_CRC))
	{
        vJIOTransmit(&_uiCRC, sizeof(_uiCRC));
    }

	return _uiCRC;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned char ucReceive(void *_pvAddress, unsigned int _uiLength, unsigned char _ucFlags, unsigned int *_puiCRC)
{
    /* Avoid interrupt before reading acknowledge or checksum */
    if (_uiLength != 2)
    {
        vSetCPU(eZ80);
        cdi();
    }

    while(!bJIOReceive(_pvAddress, _uiLength))
	{
		if(!(_ucFlags & FLAGBITS_RETRY_TIMEOUT))
		{
            return COMMAND_REPORT_TIMEOUT;
		}
	}

	if(_ucFlags & FLAGBITS_RX_CRC)
	{
        vSetCPU(eR800_DRAM);
        *_puiCRC = uiXModemCRC16(_pvAddress, _uiLength, *_puiCRC);
	}

	return COMMAND_OK;
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
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	unsigned int	uiTotalLength;
    unsigned int	uiReceivedCRC;
    unsigned int	uiComputedCRC;
    unsigned int	uiTransmitCRC;
	tdCommonHeader	oCommonHeader;
	tdReadWriteData oReadWriteHeader;
	tdCPUMode		eCPUMode;
	unsigned char	ucResult;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define ucCommand (_ucFlags & 0x0F)

	eCPUMode = eGetCPU();

	oCommonHeader.m_acSig[0] = 'J';
	oCommonHeader.m_acSig[1] = 'I';
	oCommonHeader.m_acSig[2] = 'O';

    oReadWriteHeader.m_aucSector[0] = (_ulSector >> 0);
    oReadWriteHeader.m_aucSector[1] = (_ulSector >> 8);
    oReadWriteHeader.m_aucSector[2] = (_ulSector >> 16);
	oReadWriteHeader.m_ucLength = _ucLength;
	oReadWriteHeader.m_pvAddress = _pvAddress;

	uiTotalLength = _ucLength * 512;

	do
	{
		ucResult = COMMAND_OK;
		oCommonHeader.m_ucCommand = _ucFlags;

		uiTransmitCRC = 0;
		uiTransmitCRC = uiTransmit(&oCommonHeader, sizeof(oCommonHeader), _ucFlags, 0, ucCommand == COMMAND_INFO);

		if(ucCommand == COMMAND_WRITE)
		{
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			unsigned int	uiAcknowledge;
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

			uiTransmitCRC = uiTransmit(&oReadWriteHeader, sizeof(oReadWriteHeader), _ucFlags, uiTransmitCRC, false);
			uiTransmitCRC = uiTransmit(_pvAddress, uiTotalLength, _ucFlags, uiTransmitCRC, true);

			if(_ucFlags & FLAGBITS_TX_CRC)
			{
				uiReceivedCRC = 0;
				ucResult = ucReceive(&uiAcknowledge, sizeof(uiAcknowledge), _ucFlags, &uiReceivedCRC);
				if(ucResult == COMMAND_OK)
				{
					switch(uiAcknowledge)
					{
					case 0x1111:	ucResult = COMMAND_REPORT_BAD_TX_CRC; break;
					case 0x2222:	break;
					default:		ucResult = COMMAND_REPORT_BAD_ACKNOWLEDGE; break;
					}
				}
			}
		}
		else
		{
			if(ucCommand == COMMAND_READ)
			{
                uiTransmit(&oReadWriteHeader, sizeof(oReadWriteHeader), _ucFlags, uiTransmitCRC, true);
			}

            uiComputedCRC = 0;
            ucResult = ucReceive(_pvAddress, uiTotalLength, _ucFlags, &uiComputedCRC);

            if((ucResult == COMMAND_OK) && (_ucFlags & FLAGBITS_RX_CRC))
			{
                ucResult = ucReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, 0);
                if ((ucResult == COMMAND_OK) && (uiReceivedCRC != uiComputedCRC))
                {
                    ucResult = COMMAND_REPORT_BAD_RX_CRC;
                }
			}
		}

		if(ucResult != COMMAND_OK)
		{
			oCommonHeader.m_ucCommand = ucResult;
            uiTransmit(&oCommonHeader, sizeof(oCommonHeader), _ucFlags, 0, true);
        }
    } while((ucResult != COMMAND_OK) && (_ucFlags & FLAGBITS_RETRY_CRC));

	vSetCPU(eCPUMode);
	cei();

    return ucResult;
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
