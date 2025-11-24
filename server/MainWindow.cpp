#include <QTimer>
#include <QStandardPaths>
#include <QFileDialog>
#include <QScrollBar>
#include <QButtonGroup>
#include <QThread>
#include <QPixmap>
#include <QBluetoothPermission>

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "InterfaceSerialPort.h"
#include "InterfaceBluetoothSocket.h"

#include "../common/drv_jio.inc"

static_assert(sizeof(tdReadWriteHeader) == 7, "tdReadWriteHeader must be 7 bytes");
std::coroutine_handle<> ByteReader::	m_soHandle = nullptr;

#define TRANSMIT_DELAY_NORMAL		3
#define TRANSMIT_DELAY_ACKNOWLEDGE	7

/*
 =======================================================================================================================
 =======================================================================================================================
 */
quint16 MainWindow::uiXModemCRC16(const void *_pucData, size_t _uiSize, quint16 _uiCRC)
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
quint16 MainWindow::uiTransmit
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
    memcpy(_pvAddress, (((QByteArray) co_await oRead(_uiSize)).constData()), _uiSize); \
    if(_ucFlags & FLAG_TX_CRC)                                                         \
    {                                                                                  \
            _uiCRC = uiXModemCRC16(_pvAddress, _uiSize, _uiCRC);                       \
    }                                                                                  \
}


#define vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC)      \
do                                                                   \
{                                                                    \
    unsigned char _ucChar;                                           \
    memset(&(oFIB), 0, sizeof(oFIB));                                \
    (szPath).clear();                                                \
    vReceive(&_ucChar, sizeof(_ucChar), 0, (uiCRC));                 \
    if (_ucChar == 0xFF)                                             \
    {                                                                \
        vReceive((oFIB).m_acFileName, sizeof(oFIB) - 1, 0, (uiCRC)); \
        szPath = QFileInfo(*oFIB.m_poFile).absoluteFilePath();       \
        ucPhysicalDrive = (oFIB).m_ucDrive ? (oFIB).m_ucDrive - 1 : m_ucCurrentPhysicalDrive; \
    }                                                                \
    else                                                             \
    {                                                                \
        while (_ucChar)                                              \
        {                                                            \
            (szPath) += static_cast<char>(_ucChar);                  \
            vReceive(&_ucChar, sizeof(_ucChar), 0, (uiCRC));         \
        }                                                            \
        ucPhysicalDrive = BDOSToQt(szPath);                          \
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
QString MainWindow::szGetServerInfo()
{
    /*~~~~~~~~~~~*/
    QString oText;
    QString oFlags;
    /*~~~~~~~~~~~*/

    if(m_bRxCRC) oFlags += "RxCRC ";
    if(m_bTxCRC) oFlags += "TxCRC ";
    if(m_bTimeout) oFlags += "Timeout ";
    if(m_bAutoRetry) oFlags += "AutoRetry ";
    if(m_bReadOnly | m_oDrive.bIsMediaWriteProtected()) oFlags += "ReadOnly ";
    if(m_bSlowTx) oFlags += "SlowTx";

    oText = QString::asprintf
        (
            "\r\nDrive :\r\n%s\r\nDate  : %s\r\nFlags : %s\r\nFile  : %s\r\n",
            qPrintable(m_oDrive.szDescription()),
            qPrintable(m_oDrive.oMediaLastModified()),
            qPrintable(oFlags),
            qPrintable(m_oDrive.oMediaPath())
            );

    return oText;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */

QString MainWindow::szGetFIBDescription(tdFileInfoBlock &_roFIB)
{
    if (_roFIB.m_ucFF == 0xFF)
        return QString(_roFIB.m_acFileName) + " | " + QString(_roFIB.m_poFile ? _roFIB.m_poFile->fileName(): "**NULL**");
    else
        return "";
}

QString MainWindow::szGetFileHandleDescription(unsigned char _ucFileHandle)
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
 =======================================================================================================================
 */
Task MainWindow::oParser()
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

        vReceive(&ucFlags, sizeof(ucFlags), FLAG_TX_CRC, uiCRC);
        vReceive(&ucCommand, sizeof(ucCommand), FLAG_TX_CRC, uiCRC);

        switch(ucCommand)
        {
        case COMMAND_BDOS:
        {
            unsigned char         ucFunction;
            unsigned char         ucFileHandle;
            unsigned char         ucSearchAttributes;
            unsigned char         ucAttributes;
            unsigned char         ucOpenMode;
            unsigned char         ucDriveNumber;
            unsigned char         ucMethodCode;
            unsigned char         ucGetOrSet;
            unsigned char         ucPhysicalDrive;
            unsigned char         ucNewAttributes;
            unsigned char         ucDiskToSelect;
            int                   iSignedOffset;
            unsigned short int    uiSize;
            unsigned short int    uiNewTime;
            unsigned short int    uiNewDate;
            char*                 pcData;

            tdFileInfoBlock       oFIB;
            tdFileControlBlock    oFCB;
            QString               szPath;
            QString               szWildcard;

            vReceive(&ucFunction, sizeof(ucFunction), 0, uiCRC);
            if (ucFunction != 0xFF)
                vLog(eLogBDOS, "%s\n", tdFunctionToString((tdFunction)ucFunction));

            switch(ucFunction)
            {
            case RESET_NFS:
                vResetNFS();
                break;

            case DOS_FIND_FIRST_ENTRY:
               vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);
               if (oFIB.m_ucFF == 0xFF)
               {
                   vReceiveString(szWildcard, uiCRC);
                   szPath += "/" + szWildcard;
               }
               else
                   szWildcard.clear();
               vReceive(&ucSearchAttributes, sizeof(ucSearchAttributes), 0, uiCRC);

               vLog(eLogBDOSDetails, "ucSearchAttributes %d\n", ucSearchAttributes);
               vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());
               vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());

               vDOS_FIND_FIRST_ENTRY(ucSearchAttributes, ucPhysicalDrive, szPath, oFIB);

               vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());
               break;

            case DOS_FIND_NEW_ENTRY:
                vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);
                vReceive(&ucSearchAttributes, sizeof(ucSearchAttributes), 0, uiCRC);
                vReceive(oFIB.m_acFileName, 13, 0, uiCRC);
                if (oFIB.m_ucFF == 0xFF)
                {
                    vReceiveString(szWildcard, uiCRC);
                    szPath += "/" + szWildcard;
                }
                else
                    szWildcard.clear();

                vLog(eLogBDOSDetails, "ucSearchAttributes %d\n", ucSearchAttributes);
                vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());

                vDOS_FIND_NEW_ENTRY(ucSearchAttributes, ucPhysicalDrive, szPath, oFIB);

                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());
                break;

            case DOS_FIND_NEXT_ENTRY:
                vReceive(&oFIB, sizeof(oFIB), 0, uiCRC);

                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());
                vDOS_FIND_NEXT_ENTRY(oFIB);
                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());
                break;

            case DOS_CHANGE_CURRENT_DIRECTORY:
                vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);

                vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());

                vDOS_CHANGE_CURRENT_DIRECTORY(ucPhysicalDrive, szPath);
                break;

            case DOS_OPEN_FILE_HANDLE:
                vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);
                vReceive(&ucOpenMode, sizeof(ucOpenMode), 0, uiCRC);

                vLog(eLogBDOSDetails, "ucOpenMode %d\n", ucOpenMode);
                vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());

                vDOS_OPEN_FILE_HANDLE(ucOpenMode, szPath);
                break;

            case DOS_READ_FROM_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&uiSize, sizeof(uiSize), 0, uiCRC);

                vLog(eLogBDOSDetails, "ucFileHandle %s\n", szGetFileHandleDescription(ucFileHandle).toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "uiSize %d\n", uiSize);

                vDOS_READ_FROM_FILE_HANDLE(ucFileHandle, uiSize);
                break;

            case DOS_WRITE_TO_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&uiSize, sizeof(uiSize), 0, uiCRC);

                vLog(eLogBDOSDetails, "ucFileHandle %s\n", szGetFileHandleDescription(ucFileHandle).toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "uiSize %d\n", uiSize);

                pcData = uiSize ? (char *) malloc(uiSize) : NULL;
                vReceive(pcData, uiSize, ucFlags, uiCRC);
                vDOS_WRITE_TO_FILE_HANDLE(ucFileHandle, uiSize, pcData);
                if (pcData)
                    free(pcData);
                break;

            case DOS_CLOSE_FILE_HANDLE:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);

                vLog(eLogBDOSDetails, "ucFileHandle %s\n", szGetFileHandleDescription(ucFileHandle).toLocal8Bit().constData());

                vDOS_CLOSE_FILE_HANDLE(ucFileHandle);
                break;

            case DOS_MOVE_FILE_HANDLE_POINTER:
                vReceive(&ucFileHandle, sizeof(ucFileHandle), 0, uiCRC);
                vReceive(&ucMethodCode, sizeof(ucMethodCode), 0, uiCRC);
                vReceive(&iSignedOffset, sizeof(iSignedOffset), 0, uiCRC);

                vLog(eLogBDOSDetails, "ucFileHandle %s\n", szGetFileHandleDescription(ucFileHandle).toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "ucMethodCode %d\n", ucMethodCode);
                vLog(eLogBDOSDetails, "iSignedOffset %d\n", iSignedOffset);

                vDOS_MOVE_FILE_HANDLE_POINTER(ucFileHandle, ucMethodCode, iSignedOffset);
                break;

            case DOS_GET_CURRENT_DIRECTORY:
                vReceive(&ucDriveNumber, sizeof(ucDriveNumber), 0, uiCRC);
                vDOS_GET_CURRENT_DIRECTORY(ucDriveNumber);
                break;

            case DOS_CREATE_FILE_HANDLE:
                vReceiveString(szPath, uiCRC);
                vReceive(&ucOpenMode, sizeof(ucOpenMode), 0, uiCRC);
                vReceive(&ucAttributes, sizeof(ucAttributes), 0, uiCRC);

                vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "ucOpenMode %d\n", ucOpenMode);
                vLog(eLogBDOSDetails, "ucAttributes %d\n", ucAttributes);

                vDOS_CREATE_FILE_HANDLE(szPath, ucOpenMode, ucAttributes);
                break;

            case DOS_GET_WHOLE_PATH_STRING:
                vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);

                vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());

                vDOS_GET_WHOLE_PATH_STRING(ucPhysicalDrive, szPath);
                break;

            case DOS_DELETE_FILE_OR_SUBDIRECTORY:
                vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);

                vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());

                vDOS_DELETE_FILE_OR_SUBDIRECTORY(szPath);
                break;

            case DOS_GET_SET_FILE_ATTRIBUTES:
                vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);
                vReceive(&ucGetOrSet, sizeof(ucGetOrSet), 0, uiCRC);
                vReceive(&ucNewAttributes, sizeof(ucNewAttributes), 0, uiCRC);

                vLog(eLogBDOSDetails, "szPath %s\n", szPath.toLocal8Bit().constData());
                vLog(eLogBDOSDetails, "oFIB %s\n", szGetFIBDescription(oFIB).toLocal8Bit().constData());

                vDOS_GET_SET_FILE_ATTRIBUTES(szPath, ucGetOrSet, ucNewAttributes);
                break;

            case DOS_GET_SET_FILE_HANDLE_DATE_AND_TIME:
                vReceive(&ucGetOrSet, sizeof(ucGetOrSet), 0, uiCRC);
                vReceive(&uiNewDate, sizeof(uiNewDate), 0, uiCRC);
                vReceivePathOrFIB(ucPhysicalDrive, szPath, oFIB, uiCRC);
                vReceive(&uiNewTime, sizeof(uiNewTime), 0, uiCRC);
                vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(szPath, ucGetOrSet, uiNewDate, uiNewTime);
                break;

            case DOS_SELECT_DISK:
                vReceive(&ucDiskToSelect, sizeof(ucDiskToSelect), 0, uiCRC);

                vLog(eLogBDOSDetails, "ucDiskToSelect %d\n", ucDiskToSelect);

                vDOS_SELECT_DISK(ucDiskToSelect);
                break;


            case DOS_OPEN_FILE_FCB:
                vReceive(&oFCB, sizeof(oFCB), 0, uiCRC);
                vDOS_OPEN_FILE_FCB(oFCB);
                break;

            case DOS_CLOSE_FILE_FCB:
                vReceive(&oFCB, sizeof(oFCB), 0, uiCRC);
                vDOS_CLOSE_FILE_FCB(oFCB);
                break;

            case DOS_RANDOM_BLOCK_READ_FCB:
                vReceive(&oFCB, sizeof(oFCB), 0, uiCRC);

                vDOS_RANDOM_BLOCK_READ_FCB(oFCB);
                break;

            case 0xFF:
                vReceive(&ucFunction, sizeof(ucFunction), 0, uiCRC);
                if ((ucFunction != DOS_CONSOLE_OUTPUT) &&
                    (ucFunction != DOS_CONSOLE_INPUT_WITHOUT_ECHO) &&
                    (ucFunction != DOS_CONSOLE_STATUS) &&
                    (ucFunction != DOS_CHECK_CHARACTER))
                {
                    vLog(eLogWarning, "%s\n", tdFunctionToString((tdFunction)ucFunction));
                }
                break;
            }
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
                    (m_bSlowTx    ? FLAG_SLOW_TX : 0);
                quint8		W_DRIVES = m_oDrive.uiPartitionCount();
                quint8		W_BOOTDRV = m_oDrive.uiFirstActivePartition();
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
                vUpdateLights();
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

                if(uiSector & 0x800000) uiSector &= 0xFFFF;

                if(m_oDrive.eReadSectors(ucPartition, uiSector, oHeader.m_ucLength, oFileData) == eDriveErrorOK)
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
                vUpdateLights();
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
                if(uiSector & 0x800000) uiSector &= 0xFFFF;

                if(m_bReadOnly)
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
                vUpdateLights();
            }

            uiTransmit(&uiAcknowledge, sizeof(uiAcknowledge), 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
            free(acData);
        }
        break;

        case COMMAND_DRIVE_REPORT_CRC_ERROR:
            vLog(eLogError, "CRC error !\n");
            m_uiReceiveErrors++;
            vUpdateLights();
            break;

        case COMMAND_DRIVE_REPORT_WRITE_FAULT:
            vLog(eLogError, "Write fault error !\n");
            m_uiTransmitErrors++;
            vUpdateLights();
            break;

        case COMMAND_DRIVE_REPORT_DRIVE_NOT_READY:
            vLog(eLogError, "Timeout error !\n");
            m_uiReceiveErrors++;
            vUpdateLights();
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

#ifdef Q_OS_ANDROID

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vRequestAndroidPermissionsAndSetInterface(QObject *parent)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QBluetoothPermission	oBluetoothPermission;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    qApp->requestPermission(oBluetoothPermission, parent, [this] (const QPermission &perm)
                            {
                                if(perm.status() != Qt::PermissionStatus::Granted)
                                {
                                    vLog(eLogError, "Bluetooth permission denied\n");
                                }
                                else
                                {
                                    vLog(eLogInfo, "Bluetooth permission granted\n"); vSetInterface(m_eSelectedInterface);
                                }
                            }
                            );
}
#endif

/*
 =======================================================================================================================
 =======================================================================================================================
 */
MainWindow::MainWindow() :
    m_poUI(new Ui::MainWindow),
    m_poRedLightOffTimer(new QTimer(this)),
    m_poGreenLightOffTimer(new QTimer(this)),
    m_poUnlockTimer(new QTimer(this))
{
    m_poSettings = new QSettings();

    m_poUI->setupUi(this);

    connect(m_poUI->unlockPushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->timeout, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->slowTx, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->refreshPushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->readOnly, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->namesListWidget, &QListWidget::itemClicked, this, &MainWindow::onItemActivated);
    connect(m_poUI->fileSelectPushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectPushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->connectPushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->clearPushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->bluetoothButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->autoRetry, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->USBButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->TxCRC, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->RxCRC, &QPushButton::clicked, this, &MainWindow::onButtonClicked);

    connect(m_poUI->fileEjectDriveA_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectDriveB_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectDriveC_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectDriveD_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectDriveE_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectDriveF_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectDriveG_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileEjectDriveH_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);


    connect(m_poUI->fileSelectDriveA_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileSelectDriveB_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileSelectDriveC_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileSelectDriveD_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileSelectDriveE_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileSelectDriveF_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileSelectDriveG_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->fileSelectDriveH_PushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);


    connect(m_poUI->directoryPathLineEdit_DriveA, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);
    connect(m_poUI->directoryPathLineEdit_DriveB, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);
    connect(m_poUI->directoryPathLineEdit_DriveC, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);
    connect(m_poUI->directoryPathLineEdit_DriveD, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);
    connect(m_poUI->directoryPathLineEdit_DriveE, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);
    connect(m_poUI->directoryPathLineEdit_DriveF, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);
    connect(m_poUI->directoryPathLineEdit_DriveG, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);
    connect(m_poUI->directoryPathLineEdit_DriveH, &QLineEdit::editingFinished, this, &MainWindow::onDirectoryPathChanged);

    setFixedSize(size());
    setFocusPolicy(Qt::StrongFocus);

    onRedLightTimer();
    m_poRedLightOffTimer->setSingleShot(true);
    connect(m_poRedLightOffTimer, &QTimer::timeout, this, &MainWindow::onRedLightTimer);

    onGreenLightTimer();
    m_poGreenLightOffTimer->setSingleShot(true);
    connect(m_poGreenLightOffTimer, &QTimer::timeout, this, &MainWindow::onGreenLightTimer);

    connect(m_poUnlockTimer, &QTimer::timeout, this, &MainWindow::onUnlockTimer);
    connect(m_poUI->imagePathLineEdit, &QLineEdit::editingFinished, this, &MainWindow::onImagePathValidated);
    connect(m_poUI->addressLineEdit, &QLineEdit::editingFinished, this, &MainWindow::onAddressLineValidated);

    m_poUI->logWidget->setFont(QFont("Ubuntu Mono", LOG_WIDGET_FONT_SIZE));
    m_poUI->namesListWidget->setFont(QFont("Ubuntu", NAMES_LIST_WIDGET_FONT_SIZE));

    vAdjustScrollBars(m_poUI->logWidget);
    vAdjustScrollBars(m_poUI->namesListWidget);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QButtonGroup	*poGroup = new QButtonGroup(this);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    m_bRxCRC = m_poSettings->value("RxCRC", true).toBool();
    m_bTxCRC = m_poSettings->value("TxCRC", true).toBool();
    m_bAutoRetry = m_poSettings->value("AutoRetry", true).toBool();
    m_bTimeout = m_poSettings->value("Timeout", false).toBool();
    m_bReadOnly = m_poSettings->value("ReadOnly", false).toBool();
    m_bSlowTx = m_poSettings->value("SlowTx", false).toBool();

    m_oSelectedSerialID = m_poSettings->value("SelectedSerialID").toString();
    m_oSelectedBlueToothID = m_poSettings->value("SelectedBlueToothID").toString();
    m_poUI->imagePathLineEdit->setText(m_poSettings->value("LastMediaInserted").toString());
    onImagePathValidated();
    m_oDrive.m_oLastPathBrowsed = m_poSettings->value("LastPathBrowsed").toString();

    m_szBDOSRootDir[0] = m_poSettings->value("DrivePathA").toString();
    m_szBDOSRootDir[1] = m_poSettings->value("DrivePathB").toString();
    m_szBDOSRootDir[2] = m_poSettings->value("DrivePathC").toString();
    m_szBDOSRootDir[3] = m_poSettings->value("DrivePathD").toString();
    m_szBDOSRootDir[4] = m_poSettings->value("DrivePathE").toString();
    m_szBDOSRootDir[5] = m_poSettings->value("DrivePathF").toString();
    m_szBDOSRootDir[6] = m_poSettings->value("DrivePathG").toString();
    m_szBDOSRootDir[7] = m_poSettings->value("DrivePathH").toString();


#ifdef Q_OS_ANDROID
    m_eSelectedInterface = eInterfaceBluetooth;
    m_poUI->bluetoothButton->hide();
    m_poUI->USBButton->hide();
#else
    m_eSelectedInterface = (tdInterface) m_poSettings->value("SelectedInterface").toInt();
#endif
    poGroup->setExclusive(true);
    poGroup->addButton(m_poUI->USBButton);
    poGroup->addButton(m_poUI->bluetoothButton);

    m_poUI->bluetoothButton->setChecked(m_eSelectedInterface == eInterfaceBluetooth);
    m_poUI->USBButton->setChecked(m_eSelectedInterface == eInterfaceSerial);

    m_poUI->RxCRC->setChecked(m_bRxCRC);
    m_poUI->TxCRC->setChecked(m_bTxCRC);
    m_poUI->autoRetry->setChecked(m_bAutoRetry);
    m_poUI->timeout->setChecked(m_bTimeout);
    m_poUI->readOnly->setChecked(m_bReadOnly);
    m_poUI->slowTx->setChecked(m_bSlowTx);

    m_poUI->addressLineEdit->setText(roSelectedID());


    m_poUI->fileSelectDriveA_PushButton->setToolTip("Select the directory to serve for drive A:.");
    m_poUI->fileSelectDriveB_PushButton->setToolTip("Select the directory to serve for drive B:.");
    m_poUI->fileSelectDriveC_PushButton->setToolTip("Select the directory to serve for drive C:.");
    m_poUI->fileSelectDriveD_PushButton->setToolTip("Select the directory to serve for drive D:.");
    m_poUI->fileSelectDriveE_PushButton->setToolTip("Select the directory to serve for drive E:.");
    m_poUI->fileSelectDriveF_PushButton->setToolTip("Select the directory to serve for drive F:.");
    m_poUI->fileSelectDriveG_PushButton->setToolTip("Select the directory to serve for drive G:.");
    m_poUI->fileSelectDriveH_PushButton->setToolTip("Select the directory to serve for drive H:.");


    m_poUI->fileEjectDriveA_PushButton->setToolTip("Do not serve drive A:.");
    m_poUI->fileEjectDriveB_PushButton->setToolTip("Do not serve drive B:.");
    m_poUI->fileEjectDriveC_PushButton->setToolTip("Do not serve drive C:.");
    m_poUI->fileEjectDriveD_PushButton->setToolTip("Do not serve drive D:.");
    m_poUI->fileEjectDriveE_PushButton->setToolTip("Do not serve drive E:.");
    m_poUI->fileEjectDriveF_PushButton->setToolTip("Do not serve drive F:.");
    m_poUI->fileEjectDriveG_PushButton->setToolTip("Do not serve drive G:.");
    m_poUI->fileEjectDriveH_PushButton->setToolTip("Do not serve drive H:.");

    m_poUI->fileSelectPushButton->setToolTip("Select the disk image to serve.");
    m_poUI->connectPushButton->setToolTip("Connect to the MSX.");
    m_poUI->addressLineEdit->setToolTip("Address of the communication device to use.");
    m_poUI->imagePathLineEdit->setToolTip("Path to the disk image to serve.");
    m_poUI->redLightLabel->setToolTip("Indicates transmission activity on the MSX.\nFirst line: total bytes transmitted.\nSecond line: total transmission errors.");
    m_poUI->greenLightLabel->setToolTip("Indicates reception activity on the MSX.\nFirst line: total bytes received.\nSecond line: total reception errors.");
    m_poUI->namesListWidget->setToolTip("List of available communication devices.");
    m_poUI->bluetoothButton->setToolTip("Select the Bluetooth interface.");
    m_poUI->USBButton->setToolTip("Select the USB interface.");
    m_poUI->refreshPushButton->setToolTip("Search for available devices again.");
    m_poUI->clearPushButton->setToolTip("Clear the log output.");
    m_poUI->unlockPushButton->setToolTip("Send repeated data to the MSX until it responds.\nUseful when the MSX is stuck waiting for data.");
    m_poUI->RxCRC->setToolTip("Enable CRC checking for incoming data on the MSX.\nApplied at MSX startup.");
    m_poUI->TxCRC->setToolTip("Enable CRC for outgoing data to the MSX.\nApplied at MSX startup.");
    m_poUI->autoRetry->setToolTip("Automatically retry all MSX commands indefinitely.");
    m_poUI->timeout->setToolTip("If enabled, abort the command after a timeout.\nIf disabled, wait indefinitely for a response.");
    m_poUI->readOnly->setToolTip("Prevent writes to the disk image.");
    m_poUI->fileEjectPushButton->setToolTip("Eject disk image.");
    m_poUI->logWidget->setToolTip("Server log.");

    vSetState(m_eConnectionState);

    vUpdateDrivePathsTexts();

#ifdef Q_OS_ANDROID
    vRequestAndroidPermissionsAndSetInterface(this);
#else
    vSetInterface(m_eSelectedInterface);
#endif
    oParser();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
MainWindow::~MainWindow()
{
    m_bLastButtonClickedIsConnect = false;
    m_bConnectedOnce = false;
    vSaveSettings();
    delete m_poSettings;
    delete m_poInterface;
    delete m_poUI;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSetInterface(tdInterface _eInterface)
{
    m_poUI->namesListWidget->clear();

    m_bLastButtonClickedIsConnect = false;
    m_bConnectedOnce = false;

    if(m_poInterface)
    {
        delete m_poInterface;
    }

    m_eSelectedInterface = _eInterface;

    m_poUI->addressLineEdit->setText(roSelectedID());

    switch(m_eSelectedInterface)
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

    connect(m_poInterface, &Interface::deviceDiscovered, this, &MainWindow::onDeviceDiscovered);
    connect(m_poInterface, &Interface::deviceConnected, this, &MainWindow::onDeviceConnected);
    connect(m_poInterface, &Interface::deviceReadyRead, this, &MainWindow::onDeviceReadyRead);
    connect(m_poInterface, &Interface::log, this, &MainWindow::onLog);
    connect(m_poInterface, &Interface::deviceDisconnected, this, &MainWindow::onDeviceDisconnected);

    m_poInterface->vScanDevices();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDeviceDiscovered(const QString &_roName, const QString &_roID)
{
    if(!_roName.isEmpty())
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        bool	bAlreadyExists = false;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        for(int iIndex = 0; iIndex < m_poUI->namesListWidget->count(); iIndex++)
        {
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            QListWidgetItem *_poItem = m_poUI->namesListWidget->item(iIndex);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            if(_poItem->data(Qt::UserRole).toString() == _roID)
            {
                bAlreadyExists = true;
                break;
            }
        }

        if(!bAlreadyExists)
        {
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            QListWidgetItem *_poItem = new QListWidgetItem(_roName);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            _poItem->setData(Qt::UserRole, _roID);
            m_poUI->namesListWidget->addItem(_poItem);
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDeviceConnected()
{
    m_poUnlockTimer->stop();

    vLog(eLogConnected, "Connected to " + m_poInterface->oGetName() + "\n");
    vSaveSettings();
    m_bConnectedOnce = true;
    vSetState(eCStateConnected);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDeviceReadyRead()
{
    m_poUnlockTimer->stop();
    m_poUI->unlockPushButton->setChecked(false);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	acData = m_poInterface->acReadAll();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    m_acBuffer.append(acData);

    if(m_poCurrentByteReader)
    {
        m_poCurrentByteReader->tryResume();
    }

    vSetFrameColor(m_poUI->redLightLabel, 255, 0, 0);

    m_uiBytesReceived += acData.size();

    m_poRedLightOffTimer->start(m_poRedLightOffTimer->remainingTime() + qMax(16, acData.size() / 9));

    vUpdateLights();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onLog(tdLogType _eLogType, const QString &_roMessage)
{
    vLog(_eLogType, _roMessage + "\n");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDeviceDisconnected()
{
    m_poUnlockTimer->stop();

    vLog(eLogError, "Device disconnected\n");

    if(m_bLastButtonClickedIsConnect && m_bConnectedOnce)
    {
        vLog(eLogInfo, "Attempting reconnection...\n");

        QMetaObject::invokeMethod(this, [this]()
                                  {
                                      m_poInterface->vConnectDevice(roSelectedID());
                                  },
                                  Qt::QueuedConnection);

        vSetState(eCStateConnecting);
    }
    else
    {
        vSetState(eCStateDisconnected);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vTransmitData(const QByteArray &_roData, int _iDelay)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	acDataToTransmit = QByteArray(_iDelay, (char) 0xFF) + QByteArray(1, (char) 0xF0) + _roData;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    vSetFrameColor(m_poUI->greenLightLabel, 0, 255, 0);
    m_poGreenLightOffTimer->start(m_poGreenLightOffTimer->remainingTime() + qMax(16, _roData.size() / 9));
    m_uiBytesTransmitted += _roData.size();

    vUpdateLights();

    m_poInterface->vWrite(acDataToTransmit);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSaveSettings()
{
    m_poSettings->setValue("LastMediaInserted", m_oDrive.m_oLastMediaInserted);
    m_poSettings->setValue("LastPathBrowsed", m_oDrive.m_oLastPathBrowsed);
    m_poSettings->setValue("SelectedSerialID", m_oSelectedSerialID);
    m_poSettings->setValue("SelectedBlueToothID", m_oSelectedBlueToothID);
    m_poSettings->setValue("RxCRC", m_bRxCRC);
    m_poSettings->setValue("TxCRC", m_bTxCRC);
    m_poSettings->setValue("AutoRetry", m_bAutoRetry);
    m_poSettings->setValue("Timeout", m_bTimeout);
    m_poSettings->setValue("ReadOnly", m_bReadOnly);
    m_poSettings->setValue("SlowTx", m_bSlowTx);

    m_poSettings->setValue("DrivePathA", m_szBDOSRootDir[0]);
    m_poSettings->setValue("DrivePathB", m_szBDOSRootDir[1]);
    m_poSettings->setValue("DrivePathC", m_szBDOSRootDir[2]);
    m_poSettings->setValue("DrivePathD", m_szBDOSRootDir[3]);
    m_poSettings->setValue("DrivePathE", m_szBDOSRootDir[4]);
    m_poSettings->setValue("DrivePathF", m_szBDOSRootDir[5]);
    m_poSettings->setValue("DrivePathG", m_szBDOSRootDir[6]);
    m_poSettings->setValue("DrivePathH", m_szBDOSRootDir[7]);

#ifndef Q_OS_ANDROID
    m_poSettings->setValue("SelectedInterface", m_eSelectedInterface);
#endif
    m_poSettings->sync();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSetFrameColor(QFrame *_poFrame, int _iR, int _iG, int _iB)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QColor		oColor(_iR, _iG, _iB);
    QPalette	oPalette = _poFrame->palette();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if(_poFrame->palette().color(QPalette::Window) != oColor)
    {
        oPalette.setColor(QPalette::Window, oColor);
        _poFrame->setPalette(oPalette);
        _poFrame->setAutoFillBackground(true);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vAdjustScrollBars(QAbstractScrollArea *_poWidget)
{
    /*$off*/

#ifdef Q_OS_ANDROID
    _poWidget->verticalScrollBar()->setStyleSheet(R"(
                            QScrollBar:vertical {
            width: 20px;
            background: transparent;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #DDF;
            min-height: 40px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: none;
        }
    )");
#else
    _poWidget->verticalScrollBar()->setStyleSheet(R"(
                            QScrollBar:vertical {
            width: 12px;
            background: transparent;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #DDF;
            min-height: 12px;
            border-radius: 4px;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: none;
        }
    )");
#endif

    /*$on*/
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onRedLightTimer()
{
    vSetFrameColor(m_poUI->redLightLabel, 255, 255, 255);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onGreenLightTimer()
{
    vSetFrameColor(m_poUI->greenLightLabel, 255, 255, 255);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onUnlockTimer()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	ba = QByteArray(10, (char) 0xAA);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    vTransmitData(ba, 1);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onItemActivated(QListWidgetItem *_poItem)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QString oAddress = _poItem->data(Qt::UserRole).toString();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    m_poUI->addressLineEdit->setText(oAddress);

    roSelectedID() = oAddress;
    vSetState(m_eConnectionState);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vUpdateLights()
{
    m_poUI->redLightLabel->setText(QLocale().toString(m_uiBytesReceived) + "\n" + QLocale().toString(m_uiTransmitErrors));
    m_poUI->greenLightLabel->setText(QLocale().toString(m_uiBytesTransmitted) + "\n" + QLocale().toString(m_uiReceiveErrors));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vUpdateDrivePathsTexts()
{
    m_poUI->directoryPathLineEdit_DriveA->setText(m_szBDOSRootDir[0]);
    m_poUI->directoryPathLineEdit_DriveB->setText(m_szBDOSRootDir[1]);
    m_poUI->directoryPathLineEdit_DriveC->setText(m_szBDOSRootDir[2]);
    m_poUI->directoryPathLineEdit_DriveD->setText(m_szBDOSRootDir[3]);
    m_poUI->directoryPathLineEdit_DriveE->setText(m_szBDOSRootDir[4]);
    m_poUI->directoryPathLineEdit_DriveF->setText(m_szBDOSRootDir[5]);
    m_poUI->directoryPathLineEdit_DriveG->setText(m_szBDOSRootDir[6]);
    m_poUI->directoryPathLineEdit_DriveH->setText(m_szBDOSRootDir[7]);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDirectoryPathChanged()
{
    /*~~~~~~~~~~~~~~*/
    QLineEdit *poSender;
    /*~~~~~~~~~~~~~~*/

    poSender = (QLineEdit*) QObject::sender();

    if (poSender == m_poUI->directoryPathLineEdit_DriveA) m_szBDOSRootDir[0] = poSender->text(); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveB) m_szBDOSRootDir[1] = poSender->text(); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveC) m_szBDOSRootDir[2] = poSender->text(); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveD) m_szBDOSRootDir[3] = poSender->text(); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveE) m_szBDOSRootDir[4] = poSender->text(); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveF) m_szBDOSRootDir[5] = poSender->text(); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveG) m_szBDOSRootDir[6] = poSender->text(); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveH) m_szBDOSRootDir[7] = poSender->text();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onButtonClicked()
{
    /*~~~~~~~~~~~~~~*/
    QObject *poSender;
    /*~~~~~~~~~~~~~~*/

    poSender = QObject::sender();

    if(poSender == m_poUI->RxCRC)
        m_bRxCRC = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->TxCRC)
        m_bTxCRC = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->autoRetry)
        m_bAutoRetry = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->timeout)
        m_bTimeout = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->readOnly)
        m_bReadOnly = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->slowTx)
        m_bSlowTx = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->unlockPushButton)
    {
        if(m_poUI->unlockPushButton->isChecked())
            m_poUnlockTimer->start(10);
        else
            m_poUnlockTimer->stop();
    }
    else if(poSender == m_poUI->refreshPushButton)
    {
        m_poUI->namesListWidget->clear();
        m_poInterface->vScanDevices();
    }
    else if(poSender == m_poUI->clearPushButton)
    {
        m_uiBytesReceived = 0;
        m_uiBytesTransmitted = 0;
        m_uiReceiveErrors = 0;
        m_uiTransmitErrors = 0;
        vUpdateLights();
        m_poUI->logWidget->clear();
    }

#ifndef Q_OS_ANDROID
    else if(poSender == m_poUI->bluetoothButton)
    {
        vSetInterface(eInterfaceBluetooth);
    }
    else if(poSender == m_poUI->USBButton)
    {
        vSetInterface(eInterfaceSerial);
    }
#endif
    else if(poSender == m_poUI->fileSelectPushButton)
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        QString initialDir;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        if(!m_oDrive.m_oLastPathBrowsed.isEmpty() && QFileInfo::exists(m_oDrive.m_oLastPathBrowsed))
        {
            initialDir = QFileInfo(m_oDrive.m_oLastPathBrowsed).absolutePath();
        }
        else
        {
            initialDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        }

        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        QString oImagePath = QFileDialog::getOpenFileName
            (
                nullptr,
                "Select disk image",
                initialDir,
                "Floppy & Hard disk Images (*.hd *.dsk);;All Files (*)"
                );
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        if(!oImagePath.isEmpty())
        {
            m_poUI->imagePathLineEdit->setText(oImagePath);
            onImagePathValidated();
        }
    }
    else if(
        (poSender == m_poUI->fileSelectDriveA_PushButton) ||
        (poSender == m_poUI->fileSelectDriveB_PushButton) ||
        (poSender == m_poUI->fileSelectDriveC_PushButton) ||
        (poSender == m_poUI->fileSelectDriveD_PushButton) ||
        (poSender == m_poUI->fileSelectDriveE_PushButton) ||
        (poSender == m_poUI->fileSelectDriveF_PushButton) ||
        (poSender == m_poUI->fileSelectDriveG_PushButton) ||
        (poSender == m_poUI->fileSelectDriveH_PushButton))
    {

        QString initialDir;

        if(!m_oDrive.m_oLastPathBrowsed.isEmpty() && QFileInfo::exists(m_oDrive.m_oLastPathBrowsed))
        {
            initialDir = QFileInfo(m_oDrive.m_oLastPathBrowsed).absolutePath();
        }
        else
        {
            initialDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        }
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        QString oDrivePath = QFileDialog::getExistingDirectory(
            nullptr,
            "Select drive directory to serve...",
            initialDir,
            QFileDialog::ShowDirsOnly
                | QFileDialog::DontResolveSymlinks
            );
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        if(!oDrivePath.isEmpty())
        {
            if (poSender == m_poUI->fileSelectDriveA_PushButton) m_szBDOSRootDir[0] = oDrivePath; else
            if (poSender == m_poUI->fileSelectDriveB_PushButton) m_szBDOSRootDir[1] = oDrivePath; else
            if (poSender == m_poUI->fileSelectDriveC_PushButton) m_szBDOSRootDir[2] = oDrivePath; else
            if (poSender == m_poUI->fileSelectDriveD_PushButton) m_szBDOSRootDir[3] = oDrivePath; else
            if (poSender == m_poUI->fileSelectDriveE_PushButton) m_szBDOSRootDir[4] = oDrivePath; else
            if (poSender == m_poUI->fileSelectDriveF_PushButton) m_szBDOSRootDir[5] = oDrivePath; else
            if (poSender == m_poUI->fileSelectDriveG_PushButton) m_szBDOSRootDir[6] = oDrivePath; else
            if (poSender == m_poUI->fileSelectDriveH_PushButton) m_szBDOSRootDir[7] = oDrivePath;

            vUpdateDrivePathsTexts();
        }
    }
    else if(
        (poSender == m_poUI->fileEjectDriveA_PushButton) ||
        (poSender == m_poUI->fileEjectDriveB_PushButton) ||
        (poSender == m_poUI->fileEjectDriveC_PushButton) ||
        (poSender == m_poUI->fileEjectDriveD_PushButton) ||
        (poSender == m_poUI->fileEjectDriveE_PushButton) ||
        (poSender == m_poUI->fileEjectDriveF_PushButton) ||
        (poSender == m_poUI->fileEjectDriveG_PushButton) ||
        (poSender == m_poUI->fileEjectDriveH_PushButton))
    {
        if (poSender == m_poUI->fileEjectDriveA_PushButton) m_szBDOSRootDir[0] = ""; else
        if (poSender == m_poUI->fileEjectDriveB_PushButton) m_szBDOSRootDir[1] = ""; else
        if (poSender == m_poUI->fileEjectDriveC_PushButton) m_szBDOSRootDir[2] = ""; else
        if (poSender == m_poUI->fileEjectDriveD_PushButton) m_szBDOSRootDir[3] = ""; else
        if (poSender == m_poUI->fileEjectDriveE_PushButton) m_szBDOSRootDir[4] = ""; else
        if (poSender == m_poUI->fileEjectDriveF_PushButton) m_szBDOSRootDir[5] = ""; else
        if (poSender == m_poUI->fileEjectDriveG_PushButton) m_szBDOSRootDir[6] = ""; else
        if (poSender == m_poUI->fileEjectDriveH_PushButton) m_szBDOSRootDir[7] = "";

        vUpdateDrivePathsTexts();
    }
    else if(poSender == m_poUI->connectPushButton)
    {
        if(m_eConnectionState == eCStateDisconnected)
        {
            vSetState(eCStateConnecting);
            m_poInterface->vConnectDevice(roSelectedID());
            m_bLastButtonClickedIsConnect = true;
        }
        else if((m_eConnectionState == eCStateConnected) || (m_eConnectionState == eCStateConnecting))
        {
            m_bLastButtonClickedIsConnect = false;
            m_bConnectedOnce = false;
            m_poInterface->vDisconnectDevice();
            vSetState(eCStateDisconnected);
        }
    }
    else if(poSender == m_poUI->fileEjectPushButton)
    {
        if(!m_oDrive.oMediaPath().isEmpty())
        {
            m_oDrive.vEjectMedia();
            m_poUI->imagePathLineEdit->setText("");
            m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/empty.svg"));
            vLog(eLogInfo, "Media ejected\n");
        }
    }

    vSaveSettings();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSetState(tdConnectionState _eCState)
{
    m_eConnectionState = _eCState;

    switch(m_eConnectionState)
    {
    case eCStateConnecting:
        m_poUI->connectPushButton->setIcon(QIcon(":/icons/connecting.svg"));
        m_poUI->connectPushButton->setEnabled(true);
        m_poUI->unlockPushButton->setEnabled(false);
        break;

    case eCStateConnected:
        m_poUI->connectPushButton->setIcon(QIcon(":/icons/connected.svg"));
        m_poUI->connectPushButton->setEnabled(true);
        m_poUI->unlockPushButton->setEnabled(true);
        break;

    case eCStateDisconnected:
        m_poUI->connectPushButton->setIcon(QIcon(":/icons/disconnected.svg"));
        m_poUI->connectPushButton->setEnabled(!roSelectedID().isEmpty());
        m_poUI->unlockPushButton->setEnabled(false);
        break;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vLog(tdLogType _eLogType, QString fmt, ...)
{
    /*~~~~~~~~~*/
    va_list args;
    /*~~~~~~~~~*/

    va_start(args, fmt);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QString message = QString::vasprintf(fmt.toUtf8(), args);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    va_end(args);

    /*~~~~~~~~~~~~~~~~~~~~*/
    QTextCharFormat oFormat;
    /*~~~~~~~~~~~~~~~~~~~~*/

    switch(_eLogType)
    {
    case eLogInfo:		  oFormat.setForeground(QColor(  0,   0,   0)); break;
    case eLogWarning:	  oFormat.setForeground(QColor(192,  64,  64)); break;
    case eLogError:		  oFormat.setBackground(QColor(255,   0,   0)); break;
    case eLogRead:		  oFormat.setForeground(QColor(  0, 192,   0)); break;
    case eLogWrite:		  oFormat.setForeground(QColor(255, 128, 128)); break;
    case eLogBDOS:		  oFormat.setForeground(QColor(  0,   0, 192)); break;
    case eLogBDOSDetails: oFormat.setForeground(QColor( 90,  90, 192)); message = "  " + message; break;
    case eLogConnected:   oFormat.setForeground(QColor(128, 128, 255)); break;
    }

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QScrollBar	*scrollBar = m_poUI->logWidget->verticalScrollBar();
    bool		atBottom = (scrollBar->value() == scrollBar->maximum());
    QTextCursor oCursor(m_poUI->logWidget->document());
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    oCursor.movePosition(QTextCursor::End);
    oCursor.insertText(message, oFormat);

    if(atBottom)
    {
        scrollBar->setValue(scrollBar->maximum());
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onImagePathValidated()
{
    m_bDiskChanged = true;

    if(m_oDrive.bInsertMedia(m_poUI->imagePathLineEdit->text()))
    {
        vLog(eLogInfo, "Media opened successfully\n");
        vLog(eLogInfo, szGetServerInfo() + "\n");
    }
    else
    {
        if(!m_poUI->imagePathLineEdit->text().isEmpty()) vLog(eLogError, "Media not found\n");
    }

    switch(m_oDrive.eMediaType())
    {
    case eMediaEmpty:		m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/empty.svg")); break;
    case eMediaFloppy:		m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/floppy.svg")); break;
    case eMediaHardDisk:	m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/hardDisk.svg")); break;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onAddressLineValidated()
{
    roSelectedID() = m_poUI->addressLineEdit->text();
    vSetState(m_eConnectionState);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString &MainWindow::roSelectedID()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    static QString	soEmptyString;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    switch(m_eSelectedInterface)
    {
    case eInterfaceSerial:		return m_oSelectedSerialID;
    case eInterfaceBluetooth:	return m_oSelectedBlueToothID;
    }

    return soEmptyString;
}
