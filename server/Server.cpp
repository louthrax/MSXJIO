#include <QDateTime>
#include <QThread>

#include "Server.h"
#include "InterfaceSerialPort.h"
#include "InterfaceBluetoothSocket.h"
#include "Pack.h"

#include "../common/drv_jio.inc"

static_assert(sizeof(tdReadWriteHeader) == 7, "tdReadWriteHeader must be 7 bytes");
std::coroutine_handle<> ByteReader::	m_soHandle = nullptr;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
quint16 Server::uiXModemCRC16(const void *_pucData, size_t _uiSize, quint16 _uiCRC)
{
    /*~~~~~~~~~~~~*/
    size_t	uiIndex;
    /*~~~~~~~~~~~~*/

    for(uiIndex = 0; uiIndex < _uiSize; uiIndex++)
    {
        _uiCRC ^= ((quint8 *) _pucData)[uiIndex] << 8;

        for(int iIndex = 0; iIndex < 8; iIndex++)
        {
            if(_uiCRC & 0x8000)
                _uiCRC = (_uiCRC << 1) ^ 0x1021;
            else
                _uiCRC <<= 1;
        }
    }

    return _uiCRC;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
quint16 Server::uiTransmit
    (
        const void		*_pvAddress,
        unsigned int	_uiLength,
        unsigned char	_ucFlags,
        quint16			_uiCRC,
        bool			_bLast,
        int				_iDelay
        )
{
    vTransmitData(QByteArray((const char *) _pvAddress, _uiLength), _iDelay);

    if(_ucFlags & FLAG_RX_CRC)
    {
        _uiCRC = uiXModemCRC16(_pvAddress, _uiLength, _uiCRC);

        if(_bLast) vTransmitData(QByteArray((const char *) &_uiCRC, sizeof(_uiCRC)), _iDelay);
    }

    return _uiCRC;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
#define vReceive(_pvAddress, _uiSize, _ucFlags, _uiCRC)                                \
{                                                                                      \
    QByteArray _acRead = co_await oRead(_uiSize);                                      \
    if(m_bRecordRequest) m_oRequest.m_acRequest += _acRead;                            \
    memcpy(_pvAddress, _acRead.constData(), _uiSize);                                  \
    if(_ucFlags & FLAG_TX_CRC)                                                         \
    {                                                                                  \
            _uiCRC = uiXModemCRC16(_pvAddress, _uiSize, _uiCRC);                       \
    }                                                                                  \
}


// ASCIIZ path, or FIB (first byte = 0FFH)
#define vReceivePathOrFIB(oFIB, szPath, uiCRC)                       \
do                                                                   \
{                                                                    \
    unsigned char _ucChar;                                           \
    memset(&(oFIB), 0, sizeof(oFIB));                                \
    (szPath).clear();                                                \
    vReceive(&_ucChar, sizeof(_ucChar), 0, (uiCRC));                 \
    if (_ucChar == 0xFF)                                             \
    {                                                                \
        (oFIB).m_ucFF = 0xFF;                                        \
        vReceive((oFIB).m_acFileName, sizeof(oFIB) - 1, 0, (uiCRC)); \
    }                                                                \
    else                                                             \
    {                                                                \
        while (_ucChar)                                              \
        {                                                            \
            (szPath) += static_cast<char>(_ucChar);                  \
            vReceive(&_ucChar, sizeof(_ucChar), 0, (uiCRC));         \
        }                                                            \
    }                                                                \
}                                                                    \
while (0)


#define vReceiveString(szString, uiCRC)                                             \
do {                                                                                \
    unsigned char _ucChar;                                                          \
    (szString).clear();                                                             \
    vReceive(&_ucChar, sizeof(_ucChar), 0, (uiCRC));                                \
    while (_ucChar) {                                                               \
        (szString) += static_cast<char>(_ucChar);                                   \
        vReceive(&_ucChar, sizeof(_ucChar), 0, (uiCRC));                            \
    }                                                                               \
} while (0)

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString Server::szGetServerInfo()
{
    /*~~~~~~~~~~~*/
    QString oText;
    QString oFlags;
    /*~~~~~~~~~~~*/

    // only "Read only" is used for the served directories (COMMAND_BDOS)
    bool bImage = m_eServeMode == eServeDiskImage;

    if(m_bRxCRC && bImage) oFlags += "RxCRC ";
    if(m_bTxCRC && bImage) oFlags += "TxCRC ";
    if(m_bTimeout && bImage) oFlags += "Timeout ";
    if(m_bAutoRetry && bImage) oFlags += "AutoRetry ";
    if(m_bReadOnly || (bImage && m_oDrive.bIsMediaWriteProtected())) oFlags += "ReadOnly ";
    if(m_bSlowTx && bImage) oFlags += "SlowTx";

    if(m_eServeMode == eServeDirectories)
    {
        /*~~~~~~~~~~~~~*/
        QString oDrives;
        /*~~~~~~~~~~~~~*/

        for(int i = 0; i < 8; i++)
        {
            if(bIsDriveServed(i)) oDrives += QString("%1: %2\r\n").arg(QChar('A' + i)).arg(szRootDir(i));
        }

        oText = QString::asprintf
            (
                "\r\nDirectories :\r\n%s\r\nFlags : %s\r\n",
                qPrintable(oDrives.isEmpty() ? "None\r\n" : oDrives),
                qPrintable(oFlags)
                );
    }
    else
    {
        oText = QString::asprintf
            (
                "\r\nDrive :\r\n%s\r\nFlags : %s\r\nFile  : %s\r\n",
                qPrintable(m_oDrive.szDescription()),
                qPrintable(oFlags),
                qPrintable(m_oDrive.oMediaPath())
                );
    }

    return oText;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

QString Server::szGetFIBDescription(tdFileInfoBlock &_roFIB)
{
    if (_roFIB.m_ucFF == 0xFF)
        return QString(_roFIB.m_acFileName) + " | " + m_oFindEntries.value(_roFIB.m_uiFindId, "**NULL**");
    else
        return "";
}

QString Server::szGetFileHandleDescription(unsigned char _ucFileHandle)
{
    if (m_apoOpenedFiles[_ucFileHandle])
        return QString(m_apoOpenedFiles[_ucFileHandle]->fileName());
    else
        return "**NULL**";
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

#define ENUM_CASE(e) case e: return #e;

const char *tdFunctionToString(tdFunction func)
{
    switch (func)
    {
        ENUM_CASE(DOS_PROGRAM_TERMINATE)
        ENUM_CASE(DOS_CONSOLE_INPUT)
        ENUM_CASE(DOS_CONSOLE_OUTPUT)
        ENUM_CASE(DOS_AUXILIARY_INPUT)
        ENUM_CASE(DOS_AUXILIARY_OUTPUT)
        ENUM_CASE(DOS_PRINTER_OUTPUT)
        ENUM_CASE(DOS_DIRECT_CONSOLE_IO)
        ENUM_CASE(DOS_DIRECT_CONSOLE_INPUT)
        ENUM_CASE(DOS_CONSOLE_INPUT_WITHOUT_ECHO)
        ENUM_CASE(DOS_STRING_OUTPUT)
        ENUM_CASE(DOS_BUFFERED_LINE_INPUT)
        ENUM_CASE(DOS_CONSOLE_STATUS)
        ENUM_CASE(DOS_RETURN_VERSION_NUMBER)
        ENUM_CASE(DOS_DISK_RESET)
        ENUM_CASE(DOS_SELECT_DISK)
        ENUM_CASE(DOS_OPEN_FILE_FCB)
        ENUM_CASE(DOS_CLOSE_FILE_FCB)
        ENUM_CASE(DOS_SEARCH_FOR_FIRST_ENTRY_FCB)
        ENUM_CASE(DOS_SEARCH_FOR_NEXT_ENTRY_FCB)
        ENUM_CASE(DOS_DELETE_FILE_FCB)
        ENUM_CASE(DOS_SEQUENTIAL_READ_FCB)
        ENUM_CASE(DOS_SEQUENTIAL_WRITE_FCB)
        ENUM_CASE(DOS_CREATE_FILE_FCB)
        ENUM_CASE(DOS_RENAME_FILE_FCB)
        ENUM_CASE(DOS_GET_LOGIN_VECTOR)
        ENUM_CASE(DOS_GET_CURRENT_DRIVE)
        ENUM_CASE(DOS_SET_DISK_TRANSFER_ADDRESS)
        ENUM_CASE(DOS_GET_ALLOCATION_INFORMATION)
        ENUM_CASE(CHECK_NFS)
        ENUM_CASE(RESET_NFS)
        ENUM_CASE(DOS_RANDOM_READ_FCB)
        ENUM_CASE(DOS_RANDOM_WRITE_FCB)
        ENUM_CASE(DOS_GET_FILE_SIZE_FCB)
        ENUM_CASE(DOS_SET_RANDOM_RECORD_FCB)
        ENUM_CASE(DOS_RANDOM_BLOCK_WRITE_FCB)
        ENUM_CASE(DOS_RANDOM_BLOCK_READ_FCB)
        ENUM_CASE(DOS_RANDOM_WRITE_ZERO_FILL_FCB)
        ENUM_CASE(DOS_GET_DATE)
        ENUM_CASE(DOS_SET_DATE)
        ENUM_CASE(DOS_GET_TIME)
        ENUM_CASE(DOS_SET_TIME)
        ENUM_CASE(DOS_SET_RESET_VERIFY_FLAG)
        ENUM_CASE(DOS_ABSOLUTE_SECTOR_READ)
        ENUM_CASE(DOS_ABSOLUTE_SECTOR_WRITE)
        ENUM_CASE(DOS_GET_DISK_PARAMETERS)
        ENUM_CASE(DOS_FIND_FIRST_ENTRY)
        ENUM_CASE(DOS_FIND_NEXT_ENTRY)
        ENUM_CASE(DOS_FIND_NEW_ENTRY)
        ENUM_CASE(DOS_OPEN_FILE_HANDLE)
        ENUM_CASE(DOS_CREATE_FILE_HANDLE)
        ENUM_CASE(DOS_CLOSE_FILE_HANDLE)
        ENUM_CASE(DOS_ENSURE_FILE_HANDLE)
        ENUM_CASE(DOS_DUPLICATE_FILE_HANDLE)
        ENUM_CASE(DOS_READ_FROM_FILE_HANDLE)
        ENUM_CASE(DOS_WRITE_TO_FILE_HANDLE)
        ENUM_CASE(DOS_MOVE_FILE_HANDLE_POINTER)
        ENUM_CASE(DOS_IO_CONTROL_FOR_DEVICES)
        ENUM_CASE(DOS_TEST_FILE_HANDLE)
        ENUM_CASE(DOS_DELETE_FILE_OR_SUBDIRECTORY)
        ENUM_CASE(DOS_RENAME_FILE_OR_SUBDIRECTORY)
        ENUM_CASE(DOS_MOVE_FILE_OR_SUBDIRECTORY)
        ENUM_CASE(DOS_GET_SET_FILE_ATTRIBUTES)
        ENUM_CASE(DOS_GET_SET_FILE_DATE_AND_TIME)
        ENUM_CASE(DOS_DELETE_FILE_HANDLE)
        ENUM_CASE(DOS_RENAME_FILE_HANDLE)
        ENUM_CASE(DOS_MOVE_FILE_HANDLE)
        ENUM_CASE(DOS_GET_SET_FILE_HANDLE_ATTRIBUTES)
        ENUM_CASE(DOS_GET_SET_FILE_HANDLE_DATE_AND_TIME)
        ENUM_CASE(DOS_GET_DISK_TRANSFER_ADDRESS)
        ENUM_CASE(DOS_GET_VERIFY_FLAG_SETTING)
        ENUM_CASE(DOS_GET_CURRENT_DIRECTORY)
        ENUM_CASE(DOS_CHANGE_CURRENT_DIRECTORY)
        ENUM_CASE(DOS_PARSE_PATHNAME)
        ENUM_CASE(DOS_PARSE_FILENAME)
        ENUM_CASE(DOS_CHECK_CHARACTER)
        ENUM_CASE(DOS_GET_WHOLE_PATH_STRING)
        ENUM_CASE(DOS_FLUSH_DISK_BUFFERS)
        ENUM_CASE(DOS_FORK_A_CHILD_PROCESS)
        ENUM_CASE(DOS_REJOIN_PARENT_PROCESS)
        ENUM_CASE(DOS_TERMINATE_WITH_ERROR_CODE)
        ENUM_CASE(DOS_DEFINE_ABORT_ROUTINE)
        ENUM_CASE(DOS_DEFINE_DISK_ERROR_HANDLER_ROUTINE)
        ENUM_CASE(DOS_GET_PREVIOUS_ERROR_CODE)
        ENUM_CASE(DOS_EXPLAIN_ERROR_CODE)
        ENUM_CASE(DOS_FORMAT_A_DISK)
        ENUM_CASE(DOS_CREATE_OR_DESTROY_RAMDISK)
        ENUM_CASE(DOS_ALLOCATE_SECTOR_BUFFERS)
        ENUM_CASE(DOS_LOGICAL_DRIVE_ASSIGNMENT)
        ENUM_CASE(DOS_GET_ENVIRONMENT_ITEM)
        ENUM_CASE(DOS_SET_ENVIRONMENT_ITEM)
        ENUM_CASE(DOS_FIND_ENVIRONMENT_ITEM)
        ENUM_CASE(DOS_GET_SET_DISK_CHECK_STATUS)
        ENUM_CASE(DOS_GET_MSX_DOS_VERSION_NUMBER)
        ENUM_CASE(DOS_GET_SET_REDIRECTION_STATUS)
    default: return "Unhandled";
    }
}

/*
 =======================================================================================================================
    BDOS functions modifying the served directories (logged in orange). The functions that get or set (attributes,
    date and time, RAM disk) are logged when their parameters are received.
 =======================================================================================================================
 */
static bool bIsModifyFunction(unsigned char _ucFunction)
{
    switch(_ucFunction)
    {
    case DOS_FIND_NEW_ENTRY:
    case DOS_CREATE_FILE_HANDLE:
    case DOS_WRITE_TO_FILE_HANDLE:
    case DOS_DELETE_FILE_OR_SUBDIRECTORY:
    case DOS_RENAME_FILE_OR_SUBDIRECTORY:
    case DOS_MOVE_FILE_OR_SUBDIRECTORY:
    case DOS_DELETE_FILE_HANDLE:
    case DOS_RENAME_FILE_HANDLE:
    case DOS_MOVE_FILE_HANDLE:
        return true;
    default:
        return false;
    }
}

static bool bIsGetSetFunction(unsigned char _ucFunction)
{
    switch(_ucFunction)
    {
    case DOS_GET_SET_FILE_ATTRIBUTES:
    case DOS_GET_SET_FILE_DATE_AND_TIME:
    case DOS_GET_SET_FILE_HANDLE_ATTRIBUTES:
    case DOS_GET_SET_FILE_HANDLE_DATE_AND_TIME:
    case DOS_CREATE_OR_DESTROY_RAMDISK:
        return true;
    default:
        return false;
    }
}

void Server::vLogBDOSFunction(unsigned char _ucFunction, bool _bModify)
{
    m_bLogModify = _bModify;
    vLog(_bModify ? eLogBDOSModify : eLogBDOS, "%s\n", tdFunctionToString((tdFunction) _ucFunction));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
Task Server::oParser()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char		*szSignature = "JIO";
    const size_t	uiSignatureLength = strlen(szSignature);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    while(true)
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        quint8				ucFlags;
        quint8				ucCommand;
        quint8				ucChar;
        tdReadWriteHeader	oHeader;
        size_t				iSigPos;
        quint16				uiCRC;
        bool				bCRCOK;
        quint16				uiReceivedCRC;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        iSigPos = 0;
        uiCRC = 0;
        m_bInRequest = false;

        while(iSigPos < uiSignatureLength)
        {
            vReceive(&ucChar, sizeof(ucChar), FLAG_TX_CRC, uiCRC);

            if(ucChar == szSignature[iSigPos])
            {
                iSigPos++;
            }
            else if(ucChar == szSignature[0])
            {
                iSigPos = 1;
            }
            else
            {
                iSigPos = 0;
                uiCRC = 0;
            }
        }

        m_bInRequest = true;            // "JIO" received: the rest of the request is expected (onDeviceReadyRead)
        vReceive(&ucFlags, sizeof(ucFlags), FLAG_TX_CRC, uiCRC);
        vReceive(&ucCommand, sizeof(ucCommand), FLAG_TX_CRC, uiCRC);

        switch(ucCommand)
        {
        case COMMAND_BDOS:
        {
            unsigned char         ucFunction;
            unsigned char         ucFileHandle;
            unsigned char         ucByte1;
            unsigned char         ucByte2;
            unsigned short int    uiWord1;
            unsigned short int    uiWord2;
            qint32                iOffset;
            tdFileInfoBlock       oFIB;
            QString               szPath;
            QString               szString;
            QByteArray            acData;
            char                  acTemplate[14];

            // request sent again by the MSX (same number, same bytes): previous answers sent again
            if (ucFlags && m_oRequests.contains(ucFlags))
            {
                const tdRequest &roPrevious = m_oRequests[ucFlags];
                QByteArray      acRead;
                bool            bSame = true;

                while (bSame && (acRead.size() < roPrevious.m_acRequest.size()))
                {
                    acRead += (QByteArray) co_await oRead(1);
                    bSame = acRead.back() == roPrevious.m_acRequest[acRead.size() - 1];
                }

                if (bSame)
                {
                    vLog(eLogWarning, "Request %d sent again by the MSX (answer late or lost): answer sent again\n", ucFlags);
                    m_uiReceiveErrors++;        // the MSX did not receive the answer in time (time-out)
                    emit statisticsChanged();
                    for (const QPair<QByteArray, int> &roAnswer : roPrevious.m_aoAnswers)
                        uiTransmit(roAnswer.first.constData(), roAnswer.first.size(), 0, 0, false, roAnswer.second);
                    break;
                }

                m_acBuffer.prepend(acRead);     // another request with this number: executed
            }

            m_oRequest = tdRequest();
            m_bRecordRequest = ucFlags != 0;

            vReceive(&ucFunction, sizeof(ucFunction), 0, uiCRC);
            if (!bIsGetSetFunction(ucFunction))
                vLogBDOSFunction(ucFunction, bIsModifyFunction(ucFunction));

            switch(ucFunction)
            {
            case RESET_NFS:
                vResetNFS();
                break;

            case DOS_SELECT_DISK:
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vLog(eLogBDOSDetails, "Drive %c:\n", 'A' + ucByte1);
                vDOS_SELECT_DISK(ucByte1);
                break;

            case DOS_GET_LOGIN_VECTOR:
                vDOS_GET_LOGIN_VECTOR();
                break;

            case DOS_CREATE_OR_DESTROY_RAMDISK:
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vLogBDOSFunction(ucFunction, ucByte1 != 0xFF);      // FFh = get size
                vLog(eLogBDOSDetails, "Segments %02Xh\n", ucByte1);
                vDOS_CREATE_OR_DESTROY_RAMDISK(ucByte1);
                break;

            case DOS_GET_ALLOCATION_INFORMATION:
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vDOS_GET_ALLOCATION_INFORMATION(ucByte1);
                break;

            case DOS_FIND_FIRST_ENTRY:
            case DOS_FIND_NEW_ENTRY:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                szString.clear();
                if (oFIB.m_ucFF == 0xFF)
                    vReceiveString(szString, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vLog(eLogBDOSDetails, "Path %s | FIB %s | Name %s | Attributes %02Xh\n",
                     qPrintable(szPath), qPrintable(szGetFIBDescription(oFIB)), qPrintable(szString), ucByte1);

                if (ucFunction == DOS_FIND_FIRST_ENTRY)
                    vDOS_FIND_FIRST_ENTRY(oFIB, szPath, szString, ucByte1);
                else
                {
                    memset(acTemplate, 0, sizeof(acTemplate));
                    vReceive(acTemplate, 13, 0, uiCRC);
                    vDOS_FIND_NEW_ENTRY(oFIB, szPath, szString, ucByte1, acTemplate);
                }
                break;

            case DOS_FIND_NEXT_ENTRY:
                vReceive(&oFIB, sizeof(oFIB), 0, uiCRC);
                vLog(eLogBDOSDetails, "FIB %s\n", qPrintable(szGetFIBDescription(oFIB)));
                vDOS_FIND_NEXT_ENTRY(oFIB);
                break;

            case DOS_OPEN_FILE_HANDLE:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vLog(eLogBDOSDetails, "Path %s | FIB %s | Mode %02Xh\n", qPrintable(szPath), qPrintable(szGetFIBDescription(oFIB)), ucByte1);
                vDOS_OPEN_FILE_HANDLE(oFIB, szPath, ucByte1);
                break;

            case DOS_CREATE_FILE_HANDLE:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vReceive(&ucByte2, sizeof(ucByte2), 0, uiCRC);
                vLog(eLogBDOSDetails, "Path %s | Mode %02Xh | Attributes %02Xh\n", qPrintable(szPath), ucByte1, ucByte2);
                vDOS_CREATE_FILE_HANDLE(oFIB, szPath, ucByte1, ucByte2);
                break;

            case DOS_CLOSE_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vLog(eLogBDOSDetails, "Handle %s\n", qPrintable(szGetFileHandleDescription(ucFileHandle)));
                vDOS_CLOSE_FILE_HANDLE(ucFileHandle);
                break;

            case DOS_READ_FROM_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&uiWord1, sizeof(uiWord1), 0, uiCRC);
                vLog(eLogBDOSDetails, "Handle %s | Size %d\n", qPrintable(szGetFileHandleDescription(ucFileHandle)), uiWord1);
                vDOS_READ_FROM_FILE_HANDLE(ucFileHandle, uiWord1);
                break;

            case DOS_WRITE_TO_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&uiWord1, sizeof(uiWord1), 0, uiCRC);
                vLog(eLogBDOSDetails, "Handle %s | Size %d\n", qPrintable(szGetFileHandleDescription(ucFileHandle)), uiWord1);
                acData.resize(uiWord1);
                if (uiWord1)
                    vReceive(acData.data(), uiWord1, 0, uiCRC);
                vDOS_WRITE_TO_FILE_HANDLE(ucFileHandle, acData);
                break;

            case DOS_MOVE_FILE_HANDLE_POINTER:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vReceive(&iOffset, sizeof(iOffset), 0, uiCRC);
                vLog(eLogBDOSDetails, "Handle %s | Method %d | Offset %d\n", qPrintable(szGetFileHandleDescription(ucFileHandle)), ucByte1, iOffset);
                vDOS_MOVE_FILE_HANDLE_POINTER(ucFileHandle, ucByte1, iOffset);
                break;

            case DOS_DELETE_FILE_OR_SUBDIRECTORY:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                vLog(eLogBDOSDetails, "Path %s | FIB %s\n", qPrintable(szPath), qPrintable(szGetFIBDescription(oFIB)));
                vDOS_DELETE_FILE_OR_SUBDIRECTORY(oFIB, szPath);
                break;

            case DOS_RENAME_FILE_OR_SUBDIRECTORY:
            case DOS_MOVE_FILE_OR_SUBDIRECTORY:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                vReceiveString(szString, uiCRC);
                vLog(eLogBDOSDetails, "Path %s | FIB %s | New %s\n", qPrintable(szPath), qPrintable(szGetFIBDescription(oFIB)), qPrintable(szString));
                vDOS_RENAME_OR_MOVE(oFIB, szPath, szString, ucFunction == DOS_MOVE_FILE_OR_SUBDIRECTORY);
                break;

            case DOS_GET_SET_FILE_ATTRIBUTES:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vReceive(&ucByte2, sizeof(ucByte2), 0, uiCRC);
                vLogBDOSFunction(ucFunction, ucByte1 != 0);
                vLog(eLogBDOSDetails, "Path %s | FIB %s | Set %d | Attributes %02Xh\n", qPrintable(szPath), qPrintable(szGetFIBDescription(oFIB)), ucByte1, ucByte2);
                vDOS_GET_SET_FILE_ATTRIBUTES(oFIB, szPath, ucByte1, ucByte2);
                break;

            case DOS_GET_SET_FILE_DATE_AND_TIME:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vReceive(&uiWord1, sizeof(uiWord1), 0, uiCRC);
                vReceive(&uiWord2, sizeof(uiWord2), 0, uiCRC);
                vLogBDOSFunction(ucFunction, ucByte1 != 0);
                vLog(eLogBDOSDetails, "Path %s | FIB %s | Set %d\n", qPrintable(szPath), qPrintable(szGetFIBDescription(oFIB)), ucByte1);
                vDOS_GET_SET_FILE_DATE_AND_TIME(oFIB, szPath, ucByte1, uiWord1, uiWord2);
                break;

            case DOS_DELETE_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vLog(eLogBDOSDetails, "Handle %s\n", qPrintable(szGetFileHandleDescription(ucFileHandle)));
                vDOS_DELETE_FILE_HANDLE(ucFileHandle);
                break;

            case DOS_RENAME_FILE_HANDLE:
            case DOS_MOVE_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceiveString(szString, uiCRC);
                vLog(eLogBDOSDetails, "Handle %s | New %s\n", qPrintable(szGetFileHandleDescription(ucFileHandle)), qPrintable(szString));
                vDOS_RENAME_OR_MOVE_FILE_HANDLE(ucFileHandle, szString, ucFunction == DOS_MOVE_FILE_HANDLE);
                break;

            case DOS_GET_SET_FILE_HANDLE_ATTRIBUTES:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vReceive(&ucByte2, sizeof(ucByte2), 0, uiCRC);
                vLogBDOSFunction(ucFunction, ucByte1 != 0);
                vLog(eLogBDOSDetails, "Handle %s | Set %d | Attributes %02Xh\n", qPrintable(szGetFileHandleDescription(ucFileHandle)), ucByte1, ucByte2);
                vDOS_GET_SET_FILE_HANDLE_ATTRIBUTES(ucFileHandle, ucByte1, ucByte2);
                break;

            case DOS_GET_SET_FILE_HANDLE_DATE_AND_TIME:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vReceive(&uiWord1, sizeof(uiWord1), 0, uiCRC);
                vReceive(&uiWord2, sizeof(uiWord2), 0, uiCRC);
                vLogBDOSFunction(ucFunction, ucByte1 != 0);
                vLog(eLogBDOSDetails, "Handle %s | Set %d\n", qPrintable(szGetFileHandleDescription(ucFileHandle)), ucByte1);
                vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(ucFileHandle, ucByte1, uiWord1, uiWord2);
                break;

            case DOS_GET_CURRENT_DIRECTORY:
                vReceive(&ucByte1, sizeof(ucByte1), 0, uiCRC);
                vDOS_GET_CURRENT_DIRECTORY(ucByte1);
                break;

            case DOS_CHANGE_CURRENT_DIRECTORY:
                vReceivePathOrFIB(oFIB, szPath, uiCRC);
                vLog(eLogBDOSDetails, "Path %s | FIB %s\n", qPrintable(szPath), qPrintable(szGetFIBDescription(oFIB)));
                vDOS_CHANGE_CURRENT_DIRECTORY(oFIB, szPath);
                break;

            case DOS_GET_WHOLE_PATH_STRING:
                vDOS_GET_WHOLE_PATH_STRING();
                break;

            default:
                vLog(eLogWarning, "Unsupported BDOS function %02Xh\n", ucFunction);
                break;
            }

            vEndRequest(ucFlags);
        }
        break;

        case COMMAND_DRIVE_DISK_CHANGED:
            vLog(eLogInfo, "Disk changed: %s", m_bDiskChanged ? "Yes" : "No");

            bCRCOK = true;
            if(ucFlags & FLAG_TX_CRC)
            {
                vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
                bCRCOK = uiReceivedCRC == uiCRC;
            }
            vLog(eLogInfo, ucFlags & FLAG_RX_CRC ? (bCRCOK ? "✓\n" : "❌\n") : "\n");

            if(bCRCOK)
            {
                /*~~~~~~~~~~~~~*/
                quint16 uiAnswer;
                /*~~~~~~~~~~~~~*/

                uiAnswer = m_bDiskChanged ? DRIVE_ANSWER_DISK_CHANGED : DRIVE_ANSWER_DISK_UNCHANGED;

                uiTransmit(&uiAnswer, sizeof(uiAnswer), 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
                m_bDiskChanged = false;
            }
            break;

        case COMMAND_LOG:
        {
            /*~~~~~~~~~~~~~~~~~~~*/
            QString szText;
            /*~~~~~~~~~~~~~~~~~~~*/

            // ASCIIZ text of the MSX (debugging), no answer
            vReceiveString(szText, uiCRC);
            vLog(eLogClient, "MSX: %s\n", qPrintable(szText.left(64)));
        }
        break;

        case COMMAND_DATE_TIME:
        {
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            QDateTime	oNow = QDateTime::currentDateTime();
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            vLog(eLogInfo, "Date and time: %s", qPrintable(oNow.toString("yyyy-MM-dd HH:mm:ss")));

            bCRCOK = true;
            if(ucFlags & FLAG_TX_CRC)
            {
                vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
                bCRCOK = uiReceivedCRC == uiCRC;
            }
            vLog(eLogInfo, ucFlags & FLAG_RX_CRC ? (bCRCOK ? "✓\n" : "❌\n") : "\n");

            if(bCRCOK)
            {
                PACK_PUSH
                struct
                {
                    quint16 uiYear;
                    quint8  ucMonth;
                    quint8  ucDay;
                    quint8  ucHour;
                    quint8  ucMinute;
                    quint8  ucSecond;
                } s;
                PACK_POP

                s.uiYear = oNow.date().year();
                s.ucMonth = oNow.date().month();
                s.ucDay = oNow.date().day();
                s.ucHour = oNow.time().hour();
                s.ucMinute = oNow.time().minute();
                s.ucSecond = oNow.time().second();

                uiTransmit(&s, sizeof(s), ucFlags, 0, true, TRANSMIT_DELAY_ACKNOWLEDGE);
            }
        }
        break;

        case COMMAND_DRIVE_INFO:
        {
            vLog(eLogRead, "Info");

            m_bDiskChanged = false;
            bCRCOK = true;
            if(ucFlags & FLAG_TX_CRC)
            {
                vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
                bCRCOK = uiReceivedCRC == uiCRC;
            }
            vLog(eLogRead, ucFlags & FLAG_RX_CRC ? (bCRCOK ? "✓\n" : "❌\n") : "\n");

            if(bCRCOK)
            {
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
                QByteArray	oInfoData;
                quint8		W_FLAGS =
                    (m_bRxCRC     ? FLAG_RX_CRC : 0)         |
                    (m_bTxCRC     ? FLAG_TX_CRC : 0)         |
                    (m_bTimeout   ? FLAG_TIMEOUT : 0)      |
                    (m_bAutoRetry ? FLAG_AUTO_RETRY : 0) |
                    (m_bSlowTx    ? FLAG_SLOW_TX : 0)    |
                    // Bluetooth: long transmissions of the MSX can be lost by the serial module (no flow control),
                    // JIO.COM sends its large writes in blocks
                    ((m_eInterface == eInterfaceBluetooth) ? FLAG_TX_BLOCKS : 0);
                bool        bImage = m_eServeMode == eServeDiskImage;
                quint8		W_DRIVES = bImage ? m_oDrive.uiPartitionCount() : 0;
                quint8		W_BOOTDRV = bImage ? m_oDrive.uiFirstActivePartition() : 0;
                QByteArray	acPayload = szGetServerInfo().toUtf8().left(509);
                /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                oInfoData = QByteArray(1, W_FLAGS) +
                            QByteArray(1, W_DRIVES) +
                            QByteArray(1, W_BOOTDRV) +
                            acPayload.leftJustified(509, '\0');
                uiTransmit(oInfoData.constData(), oInfoData.size(), ucFlags, 0, true, TRANSMIT_DELAY_ACKNOWLEDGE);
            }
            else
            {
                m_uiTransmitErrors++;
                emit statisticsChanged();
            }
        }
        break;

        case COMMAND_DRIVE_READ:
        {
            /*~~~~~~~~~~~~~~~~~~~~~~~~*/
            unsigned char	ucPartition;
            unsigned int	uiSector;
            /*~~~~~~~~~~~~~~~~~~~~~~~~*/

            vLog(eLogRead, "Read  ");
            vReceive(&oHeader, sizeof(oHeader), ucFlags, uiCRC);
            ucPartition = oHeader.m_uiSector >> 24;

            uiSector = oHeader.m_uiSector & 0xFFFFFF;
            if(uiSector & 0x800000) uiSector &= 0xFFFF;     // bits 16-23 = media descriptor (DOS 2), not a sector

            vLog
                (
                    eLogRead,
                    "%2d sec. at P%c: %10d to   0x%04X",
                    oHeader.m_ucLength,
                    ucPartition + '0',
                    uiSector,
                    oHeader.m_uiAddress
                    );

            bCRCOK = true;
            if(ucFlags & FLAG_TX_CRC)
            {
                vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
                bCRCOK = uiReceivedCRC == uiCRC;
            }

            vLog(eLogRead, ucFlags & FLAG_RX_CRC ? (bCRCOK ? "✓\n" : "❌\n") : "\n");

            if(bCRCOK)
            {
                /*~~~~~~~~~~~~~~~~~~*/
                QByteArray	oFileData;
                /*~~~~~~~~~~~~~~~~~~*/

                if(m_eServeMode != eServeDiskImage)
                {
                    vLog(eLogError, "No disk image served (directories mode) !\n");
                }
                else if(m_oDrive.eReadSectors(ucPartition, uiSector, oHeader.m_ucLength, oFileData) == eDriveErrorOK)
                    uiTransmit
                        (
                            oFileData.constData(),
                            oFileData.size(),
                            ucFlags,
                            0,
                            true,
                            TRANSMIT_DELAY_ACKNOWLEDGE
                            );
                else
                {
                    vLog(eLogError, "Error reading from file !\n");
                }
            }
            else
            {
                vLog(eLogError, "Transmission error !\n");
                m_uiTransmitErrors++;
                emit statisticsChanged();
            }
        }
        break;

        case COMMAND_DRIVE_WRITE:
        {
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            char			*acData;
            quint16			uiAcknowledge = DRIVE_ANSWER_WRITE_OK;
            unsigned char	ucPartition;
            unsigned int	uiSector;
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            vLog(eLogWrite, "Write ");

            vReceive(&oHeader, sizeof(oHeader), ucFlags, uiCRC);
            ucPartition = oHeader.m_uiSector >> 24;
            uiSector = oHeader.m_uiSector & 0xFFFFFF;
            if(uiSector & 0x800000) uiSector &= 0xFFFF;     // bits 16-23 = media descriptor (DOS 2), not a sector

            vLog
                (
                    eLogWrite,
                    "%2d sec. at P%c: %10d from 0x%04X",
                    oHeader.m_ucLength,
                    ucPartition + '0',
                    uiSector,
                    oHeader.m_uiAddress
                    );

            acData = (char *) malloc(oHeader.m_ucLength * 512);

            vReceive(acData, oHeader.m_ucLength * 512, ucFlags, uiCRC);

            bCRCOK = true;
            if(ucFlags & FLAG_TX_CRC)
            {
                vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
                bCRCOK = uiReceivedCRC == uiCRC;
            }

            vLog(eLogWrite, ucFlags & FLAG_RX_CRC ? (bCRCOK ? "✓\n" : "❌\n") : "\n");

            if(bCRCOK)
            {
                if(m_eServeMode != eServeDiskImage)
                {
                    vLog(eLogError, "No disk image served (directories mode) !\n");
                    uiAcknowledge = DRIVE_ANSWER_WRITE_FAILED;
                }
                else if(m_bReadOnly)
                {
                    uiAcknowledge = DRIVE_ANSWER_WRITE_PROTECTED;
                }
                else
                {
                    switch(m_oDrive.eWriteSectors(ucPartition, uiSector, oHeader.m_ucLength, acData))
                    {
                    case eDriveErrorOK:
                        break;

                    case eDriveErrorNoMedia:
                        vLog(eLogError, "No media !\n");
                        uiAcknowledge = DRIVE_ANSWER_WRITE_FAILED;
                        break;

                    case eDriveErrorReadError:
                    case eDriveErrorWriteError:
                        vLog(eLogError, "Error writing to file !\n");
                        uiAcknowledge = DRIVE_ANSWER_WRITE_FAILED;
                        break;

                    case eDriveErrorWriteProtected:
                        vLog(eLogError, "Media write-protected !\n");
                        uiAcknowledge = DRIVE_ANSWER_WRITE_PROTECTED;
                        break;
                    }
                }
            }
            else
            {
                uiAcknowledge = DRIVE_ANSWER_WRITE_FAILED;
                vLog(eLogError, "Transmission error !\n");
                m_uiTransmitErrors++;
                emit statisticsChanged();
            }

            uiTransmit(&uiAcknowledge, sizeof(uiAcknowledge), 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
            free(acData);
        }
        break;

        case COMMAND_DRIVE_REPORT_CRC_ERROR:
            vLog(eLogError, "CRC error !\n");
            m_uiReceiveErrors++;
            emit statisticsChanged();
            break;

        case COMMAND_DRIVE_REPORT_WRITE_FAULT:
            vLog(eLogError, "Write fault error !\n");
            m_uiTransmitErrors++;
            emit statisticsChanged();
            break;

        case COMMAND_DRIVE_REPORT_DRIVE_NOT_READY:
            vLog(eLogError, "Timeout error !\n");
            m_uiReceiveErrors++;
            emit statisticsChanged();
            break;

        case COMMAND_DRIVE_REPORT_WRITE_PROTECTED:
            vLog(eLogError, "Write protected error !\n");
            break;

        default:
            vLog(eLogError, "Unknown command: %d\n", ucCommand);
            break;
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
Server::Server(QObject *_poParent) :
    QObject(_poParent),
    m_poRetryTimer(new QTimer(this)),
    m_poUnlockTimer(new QTimer(this))
{
    m_poRetryTimer->setSingleShot(true);
    connect(m_poRetryTimer, &QTimer::timeout, this, &Server::onRetryTimer);
    connect(m_poUnlockTimer, &QTimer::timeout, this, &Server::onUnlockTimer);

    oParser();
}

/*
 =======================================================================================================================
    Parser restarted: request being received and data received abandoned, next request looked for ("JIO")
 =======================================================================================================================
 */
void Server::vRestartParser()
{
    ByteReader::vDestroy();
    m_poCurrentByteReader.reset();
    m_acBuffer.clear();
    m_bInRequest = false;
    m_bRecordRequest = false;
    oParser();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
Server::~Server()
{
    blockSignals(true);         // the user interface may be partly destroyed
    m_bWantConnected = false;
    delete m_poInterface;
    m_poInterface = nullptr;
    vResetNFS();                // files closed, RAM disk directory removed
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vSetInterface(tdInterface _eInterface)
{
    m_bWantConnected = false;
    m_bConnectedOnce = false;
    m_poRetryTimer->stop();
    m_poUnlockTimer->stop();

    delete m_poInterface;

    m_eInterface = _eInterface;

    switch(m_eInterface)
    {
    case eInterfaceSerial:
        vLog(eLogInfo, "Switched to USB mode\n");
        m_poInterface = new InterfaceSerialPort(this);
        break;

    case eInterfaceBluetooth:
        vLog(eLogInfo, "Switched to Bluetooth mode\n");
        m_poInterface = new InterfaceBluetoothSocket(this);
        break;
    }

    connect(m_poInterface, &Interface::deviceDiscovered, this, &Server::deviceDiscovered);
    connect(m_poInterface, &Interface::deviceConnected, this, &Server::onDeviceConnected);
    connect(m_poInterface, &Interface::deviceReadyRead, this, &Server::onDeviceReadyRead);
    connect(m_poInterface, &Interface::log, this, &Server::onInterfaceLog);
    connect(m_poInterface, &Interface::deviceDisconnected, this, &Server::onDeviceDisconnected);

    vSetState(eCStateDisconnected);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vScanDevices()
{
    if(m_poInterface) m_poInterface->vScanDevices();
}

/*
 =======================================================================================================================
    Connection to the device (_szDeviceID, or given by the resolver if empty)
 =======================================================================================================================
 */
void Server::vConnect(const QString &_szDeviceID)
{
    m_szDeviceID = _szDeviceID;
    m_bWantConnected = true;
    vSetState(eCStateConnecting);
    onRetryTimer();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vDisconnect()
{
    m_bWantConnected = false;
    m_bConnectedOnce = false;
    m_poRetryTimer->stop();
    m_poUnlockTimer->stop();
    if(m_poInterface) m_poInterface->vDisconnectDevice();
    vSetState(eCStateDisconnected);
}

/*
 =======================================================================================================================
    Attempt to connect. The serial port is opened at once (deviceConnected, or nothing if it failed), the Bluetooth
    connection is asynchronous: the next attempt is later.
 =======================================================================================================================
 */
void Server::onRetryTimer()
{
    if(!m_bWantConnected || (m_eConnectionState == eCStateConnected) || !m_poInterface) return;

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QString szID = !m_szDeviceID.isEmpty() ? m_szDeviceID : (m_oDeviceResolver ? m_oDeviceResolver() : QString());
    bool    bRetry = m_bRetryAlways || m_bConnectedOnce;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if(!szID.isEmpty())
    {
        m_poInterface->vConnectDevice(szID);
    }

    if(m_eConnectionState == eCStateConnected) return;

    if(bRetry)
    {
        m_poRetryTimer->start(m_eInterface == eInterfaceBluetooth ? 15000 : 1000);
    }
    else if(m_eInterface == eInterfaceSerial)
    {
        // first connection failed (serial port: the result is known)
        m_bWantConnected = false;
        vSetState(eCStateDisconnected);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::onDeviceConnected()
{
    m_poRetryTimer->stop();
    m_poUnlockTimer->stop();

    vLog(eLogConnected, "Connected to " + m_poInterface->oGetName() + "\n");
    m_bConnectedOnce = true;
    vSetState(eCStateConnected);
    vRestartParser();               // data of a previous connection abandoned

    // Unlock: the opening of the port may disturb the line, the MSX may then wait for the data of an answer that is
    // never sent (no time-out in the data of an answer). FFH bytes complete such a reception (its CRC fails, or FFH =
    // error code); they are never taken as the start of an answer (sync byte F0H).
    QByteArray acUnlock(1024, (char) 0xFF);
    m_poInterface->vWrite(acUnlock);
}

/*
 =======================================================================================================================
    Also emitted by the interfaces before each attempt to connect (previous connection closed)
 =======================================================================================================================
 */
void Server::onDeviceDisconnected()
{
    m_poUnlockTimer->stop();

    if(m_eConnectionState == eCStateConnected)
    {
        vLog(eLogError, "Device disconnected\n");

        if(m_bWantConnected)
        {
            vLog(eLogInfo, "Attempting reconnection...\n");
            vSetState(eCStateConnecting);
            m_poRetryTimer->start(m_eInterface == eInterfaceBluetooth ? 1000 : 500);
        }
        else
        {
            vSetState(eCStateDisconnected);
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::onDeviceReadyRead()
{
    m_poUnlockTimer->stop();

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	acData = m_poInterface->acReadAll();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    // request incomplete, no data for a while:
    // - the new data is the start of another request ("JIO"): bytes of the request lost on the link, request abandoned
    // - otherwise the rest of the request, late (link stalled, e.g. Bluetooth): request continued
    if(m_bInRequest && m_oLastData.isValid() && (m_oLastData.elapsed() > REQUEST_TIMEOUT))
    {
        if(acData.startsWith("JIO"))
        {
            vLog(eLogError, "Incomplete request abandoned (no data for %.1f s): %d of %d bytes received, bytes lost on the link\n",
                 m_oLastData.elapsed() / 1000.0, (int) m_acBuffer.size(), m_poCurrentByteReader ? m_poCurrentByteReader->iGetSize() : 0);
            m_uiReceiveErrors++;
            vRestartParser();
        }
        else
        {
            vLog(eLogWarning, "Data late (no data for %.1f s): %d of %d bytes received, %d more bytes now, request continued\n",
                 m_oLastData.elapsed() / 1000.0, (int) m_acBuffer.size(), m_poCurrentByteReader ? m_poCurrentByteReader->iGetSize() : 0,
                 (int) acData.size());
        }
    }
    m_oLastData.start();

    m_acBuffer.append(acData);

    if(m_poCurrentByteReader)
    {
        m_poCurrentByteReader->tryResume();
    }

    m_uiBytesReceived += acData.size();
    emit dataReceived(acData.size());
    emit statisticsChanged();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::onInterfaceLog(tdLogType _eLogType, const QString &_szMessage)
{
    vLog(_eLogType, _szMessage + "\n");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString Server::szDeviceName()
{
    return m_poInterface ? m_poInterface->oGetName() : QString();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vSetState(tdConnectionState _eState)
{
    if(m_eConnectionState == _eState) return;
    m_eConnectionState = _eState;
    emit stateChanged(_eState);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vSetUnlock(bool _bUnlock)
{
    if(_bUnlock)
        m_poUnlockTimer->start(10);
    else
        m_poUnlockTimer->stop();
}

void Server::onUnlockTimer()
{
    vTransmitData(QByteArray(10, (char) 0xAA), 1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vTransmitData(const QByteArray &_roData, int _iDelay)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	acDataToTransmit = QByteArray(_iDelay, (char) 0xFF) + QByteArray(1, (char) 0xF0) + _roData;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    m_uiBytesTransmitted += _roData.size();
    emit dataTransmitted(_roData.size());
    emit statisticsChanged();

    if(m_poInterface) m_poInterface->vWrite(acDataToTransmit);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vResetStatistics()
{
    m_uiBytesReceived = 0;
    m_uiBytesTransmitted = 0;
    m_uiReceiveErrors = 0;
    m_uiTransmitErrors = 0;
    emit statisticsChanged();
}

/*
 =======================================================================================================================
    Disk image or directories: the other mode is not served.
 =======================================================================================================================
 */
void Server::vSetServeMode(tdServeMode _eServeMode)
{
    m_eServeMode = _eServeMode;

    // the files opened on the served directories are closed, the MSX sees a disk change
    vResetNFS();
    m_bDiskChanged = true;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool Server::bInsertMedia(const QString &_szPath)
{
    m_bDiskChanged = true;

    if(m_oDrive.bInsertMedia(_szPath))
    {
        vLog(eLogInfo, "Media opened successfully\n");
        vLog(eLogInfo, szGetServerInfo() + "\n");
        return true;
    }

    if(!_szPath.isEmpty()) vLog(eLogError, "Media not found\n");
    return false;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vEjectMedia()
{
    if(!m_oDrive.oMediaPath().isEmpty())
    {
        m_oDrive.vEjectMedia();
        vLog(eLogInfo, "Media ejected\n");
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vLog(tdLogType _eLogType, QString fmt, ...)
{
    /*~~~~~~~~~*/
    va_list args;
    /*~~~~~~~~~*/

    va_start(args, fmt);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QString message = QString::vasprintf(fmt.toUtf8(), args);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    va_end(args);

    if(_eLogType == eLogBDOSDetails) message = "  " + message;

    emit log(_eLogType, message, m_bLogModify);
}
