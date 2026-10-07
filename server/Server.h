#ifndef Server_h
#define Server_h

#include <QObject>
#include <QFile>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <functional>
#include <memory>

#include "ByteReader.h"
#include "Interface.h"
#include "Common.h"
#include "Drive.h"

#define JIO_SERVER
#include "../common/msxdos2.h"

typedef enum {
    eInterfaceSerial,
    eInterfaceBluetooth,
} tdInterface;

typedef enum {
    eCStateConnecting,
    eCStateConnected,
    eCStateDisconnected
} tdConnectionState;

typedef enum {
    eServeDiskImage,        // sectors of a disk image (COMMAND_DRIVE_*)
    eServeDirectories,      // host directories as drives A: to H: (COMMAND_BDOS)
} tdServeMode;

#define TRANSMIT_DELAY_NORMAL		3
#define TRANSMIT_DELAY_ACKNOWLEDGE	7

#pragma pack(push, 1)
typedef struct
{
    quint32 m_uiSector;
    quint8	m_ucLength;
    quint16 m_uiAddress;
} tdReadWriteHeader;
#pragma pack(pop)

static_assert(sizeof(tdReadWriteHeader) == 7, "tdReadWriteHeader must be 7 bytes");

/*
 =======================================================================================================================
    JIO server without user interface: protocol (commands of the MSX), disk image, served directories (BDOS functions,
    Server_BDOS.cpp) and connection to the MSX. Used by the graphical server (MainWindow) and by the command line
    server (JIOServerCLI.pro). The log, the state of the connection and the activity are reported by signals.
 =======================================================================================================================
 */
class Server :
               public QObject
{
    Q_OBJECT

/*
 -----------------------------------------------------------------------------------------------------------------------
 -----------------------------------------------------------------------------------------------------------------------
 */
public:
    explicit Server(QObject *_poParent = nullptr);
    ~Server();

    // Link options (disk image mode, sent to the MSX at its startup) and read only (both modes)
    bool                m_bRxCRC = true;
    bool                m_bTxCRC = true;
    bool                m_bTimeout = false;
    bool                m_bAutoRetry = true;
    bool                m_bReadOnly = false;
    bool                m_bSlowTx = false;

    void                vSetServeMode(tdServeMode _eServeMode);
    tdServeMode         eServeMode() const { return m_eServeMode; }

    bool                bInsertMedia(const QString &_szPath);
    void                vEjectMedia();
    Drive               &roDrive() { return m_oDrive; }

    void                vSetDrivePath(int _iDrive, const QString &_szPath) { m_szBDOSRootDir[_iDrive] = _szPath; }
    QString             szDrivePath(int _iDrive) const { return m_szBDOSRootDir[_iDrive]; }

    QString             szGetServerInfo();

    // Connection: the device is given by its ID (serial port name or Bluetooth address), or by a resolver called at
    // each attempt (automatic detection). After a disconnection, the connection is attempted again if it was
    // connected once, or always with vSetRetryAlways (command line server, device not present yet).
    void                vSetInterface(tdInterface _eInterface);
    tdInterface         eInterface() const { return m_eInterface; }
    void                vScanDevices();
    void                vSetDeviceResolver(std::function<QString()> _oResolver) { m_oDeviceResolver = _oResolver; }
    void                vSetRetryAlways(bool _bRetryAlways) { m_bRetryAlways = _bRetryAlways; }
    void                vConnect(const QString &_szDeviceID = QString());
    void                vDisconnect();
    tdConnectionState   eState() const { return m_eConnectionState; }
    QString             szDeviceName();

    // Unlock: data sent to the MSX until it answers (MSX stuck waiting for data)
    void                vSetUnlock(bool _bUnlock);

    quint64             uiBytesReceived() const { return m_uiBytesReceived; }
    quint64             uiBytesTransmitted() const { return m_uiBytesTransmitted; }
    quint64             uiReceiveErrors() const { return m_uiReceiveErrors; }
    quint64             uiTransmitErrors() const { return m_uiTransmitErrors; }
    void                vResetStatistics();

signals:
    // Text of the log, lines may be sent in several parts. _bModify: the BDOS function being logged modifies the
    // served directories (colour of its details).
    void                log(tdLogType _eLogType, const QString &_szMessage, bool _bModify);
    void                stateChanged(tdConnectionState _eState);
    void                deviceDiscovered(const QString &_szName, const QString &_szID);
    void                dataReceived(int _iSize);
    void                dataTransmitted(int _iSize);
    void                statisticsChanged();

private slots:
    void                onDeviceConnected();
    void                onDeviceReadyRead();
    void                onDeviceDisconnected();
    void                onInterfaceLog(tdLogType _eLogType, const QString &_szMessage);
    void                onRetryTimer();
    void                onUnlockTimer();

private:
    void                vSetState(tdConnectionState _eState);
    void                vTransmitData(const QByteArray &_roData, int _iDelay);
    quint16             uiTransmit(const void *_pvAddress, unsigned int	_uiLength, unsigned char _ucFlags, quint16 _uiCRC, bool _bLast, int _iDelay);
    quint16             uiXModemCRC16(const void * _pucData, size_t _uiSize, quint16 _uiCRC);

    void                vLog(tdLogType _eLogType, QString fmt, ...);
    void                vLogBDOSFunction(unsigned char _ucFunction, bool _bModify);

    // BDOS functions served to the JIO kernel (Server_BDOS.cpp)
    QString             szGetFIBDescription(tdFileInfoBlock &_roFIB);
    QString             szGetFileHandleDescription(unsigned char _ucFileHandle);

    void                vResetNFS();
    unsigned char       ucAddFile(QFile * _poFile);
    bool                bIsDriveServed(unsigned char _ucDrive);
    bool                bIsWriteProtected(const QString &_szHostPath);
    bool                bIsRamDrive(unsigned char _ucDrive);
    void                vDestroyRamDisk();
    qint64              iRamDiskFree();
    QString             szRootDir(unsigned char _ucDrive);
    unsigned char       ucResolvePath(QString _szMSXPath, QString &_rszHostPath, unsigned char _ucDefaultDrive = 0xFF);
    QString             szRelativePath(unsigned char _ucDrive, const QString &_szHostPath, bool _bLongNames = false);
    unsigned char       ucGetTarget(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char &_rucDrive, QString &_rszHostPath);
    unsigned char       ucGetFindTarget(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char &_rucDrive, QString &_rszDirectory, QString &_rszItem);
    unsigned char       ucGetHandlePath(unsigned char _ucFileHandle, QString &_rszPath);
    unsigned char       ucGetAttributes(const QString &_szHostPath);
    void                vSetArchive(const QString &_szHostPath, bool _bSet);
    void                vRenameArchive(const QString &_szHostPath, const QString &_szNewHostPath);
    void                vFillFIB(tdFileInfoBlock &_roFIB, const QString &_szHostPath, unsigned char _ucDrive);
    void                vSetFindEntry(tdFileInfoBlock &_roFIB, const QString &_szHostPath, unsigned char _ucDrive);
    unsigned char       ucDelete(unsigned char _ucDrive, const QString &_szPath);
    unsigned char       ucNewPath(unsigned char _ucDrive, const QString &_szPath, const QString &_szNew, bool _bMove, QString &_rszNewPath);
    void                vAttributes(unsigned char _ucError, const QString &_szPath, unsigned char _ucSet, unsigned char _ucNewAttributes);
    void                vDateTime(unsigned char _ucError, const QString &_szPath, QFile *_poFile, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate);
    void                vAnswerHandle(unsigned char _ucError, QFile *_poFile);
    void                vBDOSAnswer(const void *_pvData, unsigned int _uiSize);
    void                vBDOSData(const void *_pvData, unsigned int _uiSize);
    void                vBDOSError(unsigned char _ucError);

    void                vDOS_SELECT_DISK(unsigned char _ucDiskToSelect);
    void                vDOS_GET_LOGIN_VECTOR();
    unsigned char       ucLoginVector();
    void                vDOS_CREATE_OR_DESTROY_RAMDISK(unsigned char _ucSegments);
    void                vDOS_GET_ALLOCATION_INFORMATION(unsigned char _ucDrive);
    void                vDOS_FIND_FIRST_ENTRY(tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char _ucSearchAttributes);
    void                vDOS_FIND_NEXT_ENTRY(tdFileInfoBlock &_roFIB);
    void                vDOS_FIND_NEW_ENTRY(tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char _ucAttributes, const char *_acTemplate);
    void                vDOS_OPEN_FILE_HANDLE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucOpenMode);
    void                vDOS_CREATE_FILE_HANDLE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucOpenMode, unsigned char _ucAttributes);
    void                vDOS_CLOSE_FILE_HANDLE(unsigned char _ucFileHandle);
    void                vDOS_READ_FROM_FILE_HANDLE(unsigned char _ucFileHandle, unsigned short int _uiSize);
    void                vDOS_WRITE_TO_FILE_HANDLE(unsigned char _ucFileHandle, const QByteArray &_racData);
    void                vDOS_MOVE_FILE_HANDLE_POINTER(unsigned char _ucFileHandle, unsigned char _ucMethodCode, qint32 _iOffset);
    void                vDOS_DELETE_FILE_OR_SUBDIRECTORY(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath);
    void                vDOS_RENAME_OR_MOVE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szNew, bool _bMove);
    void                vDOS_GET_SET_FILE_ATTRIBUTES(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucSet, unsigned char _ucNewAttributes);
    void                vDOS_GET_SET_FILE_DATE_AND_TIME(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate);
    void                vDOS_DELETE_FILE_HANDLE(unsigned char _ucFileHandle);
    void                vDOS_RENAME_OR_MOVE_FILE_HANDLE(unsigned char _ucFileHandle, const QString &_szNew, bool _bMove);
    void                vDOS_GET_SET_FILE_HANDLE_ATTRIBUTES(unsigned char _ucFileHandle, unsigned char _ucSet, unsigned char _ucNewAttributes);
    void                vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(unsigned char _ucFileHandle, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate);
    void                vDOS_GET_CURRENT_DIRECTORY(unsigned char _ucDriveNumber);
    void                vDOS_CHANGE_CURRENT_DIRECTORY(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath);
    void                vDOS_GET_WHOLE_PATH_STRING();
    void                vJIO_GET_LONG_NAME(unsigned char _ucSubFunction, unsigned short int _uiBufferSize, const tdFileInfoBlock &_roFIB, unsigned char _ucDriveNumber);

    Task                oParser();

    ByteReader          oRead(int size)
    {
        m_poCurrentByteReader = std::make_unique<ByteReader>(m_acBuffer, size);
        return *m_poCurrentByteReader;
    }
    std::unique_ptr<ByteReader>     m_poCurrentByteReader;

    // Request received incompletely (bytes lost on the link): abandoned when data arrives more than REQUEST_TIMEOUT ms
    // after the previous data, the parser looks for the next request (otherwise its bytes, e.g. COMMAND_DRIVE_INFO at
    // the boot of the MSX, would be taken as the missing data)
    bool                            m_bInRequest = false;
    QElapsedTimer                   m_oLastData;            // time of the last data received
    void                            vRestartParser();

    // Requests sent again by the MSX (answer late or lost): BDOS requests numbered by the client (flags of the
    // header, not 0), the last ones are kept with their answers, a request sent again with the same number and the
    // same bytes is not executed again, its answers are sent again
    struct tdRequest
    {
        QByteArray                      m_acRequest;        // bytes after the command (function, parameters, data)
        QList<QPair<QByteArray, int>>   m_aoAnswers;        // answer packets, transmission delay
    };
    QMap<unsigned char, tdRequest>  m_oRequests;
    QList<unsigned char>            m_aucRequestOrder;      // numbers of m_oRequests, oldest first
    bool                            m_bRecordRequest = false;
    tdRequest                       m_oRequest;             // request being executed
    void                            vEndRequest(unsigned char _ucNumber);
    QTimer                          *m_poRetryTimer = nullptr;
    QTimer                          *m_poUnlockTimer = nullptr;
    Interface                       *m_poInterface = nullptr;
    QByteArray                      m_acBuffer;
    Drive                           m_oDrive;

    bool                            m_bLogModify = false;   // BDOS function being logged modifies the served directories

    tdInterface                     m_eInterface = eInterfaceSerial;
    tdConnectionState               m_eConnectionState = eCStateDisconnected;
    QString                         m_szDeviceID;
    std::function<QString()>        m_oDeviceResolver;
    bool                            m_bWantConnected = false;   // connection requested (vConnect)
    bool                            m_bConnectedOnce = false;
    bool                            m_bRetryAlways = false;

    tdServeMode                     m_eServeMode = eServeDiskImage;
    bool                            m_bDiskChanged = false;
    quint64                         m_uiBytesReceived = 0;
    quint64                         m_uiBytesTransmitted = 0;
    quint64                         m_uiReceiveErrors = 0;
    quint64                         m_uiTransmitErrors = 0;
    QFile*                          m_apoOpenedFiles[256] = {0};
    QSet<QString>                   m_oArchiveCleared;      // host files with the archive attribute reset by the MSX
                                                            // (no archive attribute on the host, kept while running)
    QString                         m_szBDOSRootDir[8] = { "", "", "", "", "", "", "", "" };
    QString                         m_szBDOSCurrentDir[8] = { "", "", "", "", "", "", "", "" };
    unsigned char                   m_ucCurrentPhysicalDrive = 0;
    QMap<quint32, QString>          m_oFindEntries;         // FIB find id -> host path of entry found
    quint32                         m_uiNextFindId = 0;
    QString                         m_szWholePath;          // whole path of last entry found (_WPATH)
    QString                         m_szLongWholePath;      // same with the host names (JIO_LONG_WHOLE_PATH)
    QTemporaryDir                   *m_poRamDisk = nullptr;  // RAM disk H: (_RAMD), temporary directory
    unsigned char                   m_ucRamDiskSegments = 0; // RAM disk size (16 KB segments, 0 = no RAM disk)
};

#endif
