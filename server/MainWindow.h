#ifndef MainWindow_h
#define MainWindow_h

#include <QMainWindow>
#include <QTextEdit>
#include <QFile>
#include <QListWidgetItem>
#include <QSettings>
#include <QDirIterator>
#include <QMap>
#include <QTemporaryDir>

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

namespace Ui
{
class	MainWindow;
}

class MainWindow :
                   public QMainWindow
{
    Q_OBJECT

/*
 -----------------------------------------------------------------------------------------------------------------------
 -----------------------------------------------------------------------------------------------------------------------
 */
public:
    MainWindow();
    ~MainWindow();

public slots:
    void        onDeviceDiscovered (const QString & _roName, const QString & _roID);
    void        onDeviceConnected();
    void        onDeviceReadyRead();
    void        onLog(tdLogType _eLogType, const QString & _roMessage);
    void        onDeviceDisconnected();

    void        onButtonClicked();
    void        onDirectoryPathChanged();
    void        onItemActivated(QListWidgetItem *_poItem);
    void        onImagePathValidated();
    void        onAddressLineValidated();

    void        onRedLightTimer();
    void        onGreenLightTimer();
    void        onUnlockTimer();

private:
    void		vTransmitData(const QByteArray &_roData, int _iDelay);
    quint16     uiTransmit(const void *_pvAddress, unsigned int	_uiLength, unsigned char _ucFlags, quint16 _uiCRC, bool _bLast, int _iDelay);
    quint16     uiXModemCRC16(const void * _pucData, size_t _uiSize, quint16 _uiCRC);
    QString     szGetServerInfo();

    void		vSetInterface(tdInterface _eInterface);
    void		vSetState(tdConnectionState _eCState);
    void		vSetServeMode(tdServeMode _eServeMode);

    void		vLog(tdLogType _eLogType, QString fmt, ...);
    void		vLogBDOSFunction(unsigned char _ucFunction, bool _bModify);
    void		vSetFrameColor(QFrame *_poFrame, int _iR, int _iG, int _iB);
    void		vSaveSettings();
    void        vAdjustScrollBars(QAbstractScrollArea *_poWidget);
    void		vUpdateLights();
    void        vUpdateDrivePathsTexts();

    // BDOS functions served to the JIO kernel (MainWindow_BDOS.cpp)
    QString         szGetFIBDescription(tdFileInfoBlock &_roFIB);
    QString         szGetFileHandleDescription(unsigned char _ucFileHandle);

    void            vResetNFS();
    unsigned char   ucAddFile(QFile * _poFile);
    bool            bIsDriveServed(unsigned char _ucDrive);
    bool            bIsWriteProtected(const QString &_szHostPath);
    bool            bIsRamDrive(unsigned char _ucDrive);
    void            vDestroyRamDisk();
    qint64          iRamDiskFree();
    QString         szRootDir(unsigned char _ucDrive);
    unsigned char   ucResolvePath(QString _szMSXPath, QString &_rszHostPath, unsigned char _ucDefaultDrive = 0xFF);
    QString         szRelativePath(unsigned char _ucDrive, const QString &_szHostPath);
    unsigned char   ucGetTarget(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char &_rucDrive, QString &_rszHostPath);
    unsigned char   ucGetFindTarget(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char &_rucDrive, QString &_rszDirectory, QString &_rszItem);
    unsigned char   ucGetHandlePath(unsigned char _ucFileHandle, QString &_rszPath);
    unsigned char   ucGetAttributes(const QString &_szHostPath);
    void            vFillFIB(tdFileInfoBlock &_roFIB, const QString &_szHostPath, unsigned char _ucDrive);
    void            vSetFindEntry(tdFileInfoBlock &_roFIB, const QString &_szHostPath, unsigned char _ucDrive);
    unsigned char   ucDelete(unsigned char _ucDrive, const QString &_szPath);
    unsigned char   ucNewPath(unsigned char _ucDrive, const QString &_szPath, const QString &_szNew, bool _bMove, QString &_rszNewPath);
    void            vAttributes(unsigned char _ucError, const QString &_szPath, unsigned char _ucSet, unsigned char _ucNewAttributes);
    void            vDateTime(unsigned char _ucError, const QString &_szPath, QFile *_poFile, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate);
    void            vAnswerHandle(unsigned char _ucError, QFile *_poFile);
    void            vBDOSAnswer(const void *_pvData, unsigned int _uiSize);
    void            vBDOSData(const void *_pvData, unsigned int _uiSize);
    void            vBDOSError(unsigned char _ucError);

    void            vDOS_SELECT_DISK(unsigned char _ucDiskToSelect);
    void            vDOS_GET_LOGIN_VECTOR();
    unsigned char   ucLoginVector();
    void            vDOS_CREATE_OR_DESTROY_RAMDISK(unsigned char _ucSegments);
    void            vDOS_GET_ALLOCATION_INFORMATION(unsigned char _ucDrive);
    void            vDOS_FIND_FIRST_ENTRY(tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char _ucSearchAttributes);
    void            vDOS_FIND_NEXT_ENTRY(tdFileInfoBlock &_roFIB);
    void            vDOS_FIND_NEW_ENTRY(tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char _ucAttributes, const char *_acTemplate);
    void            vDOS_OPEN_FILE_HANDLE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucOpenMode);
    void            vDOS_CREATE_FILE_HANDLE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucOpenMode, unsigned char _ucAttributes);
    void            vDOS_CLOSE_FILE_HANDLE(unsigned char _ucFileHandle);
    void            vDOS_READ_FROM_FILE_HANDLE(unsigned char _ucFileHandle, unsigned short int _uiSize);
    void            vDOS_WRITE_TO_FILE_HANDLE(unsigned char _ucFileHandle, const QByteArray &_racData);
    void            vDOS_MOVE_FILE_HANDLE_POINTER(unsigned char _ucFileHandle, unsigned char _ucMethodCode, qint32 _iOffset);
    void            vDOS_DELETE_FILE_OR_SUBDIRECTORY(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath);
    void            vDOS_RENAME_OR_MOVE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szNew, bool _bMove);
    void            vDOS_GET_SET_FILE_ATTRIBUTES(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucSet, unsigned char _ucNewAttributes);
    void            vDOS_GET_SET_FILE_DATE_AND_TIME(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate);
    void            vDOS_DELETE_FILE_HANDLE(unsigned char _ucFileHandle);
    void            vDOS_RENAME_OR_MOVE_FILE_HANDLE(unsigned char _ucFileHandle, const QString &_szNew, bool _bMove);
    void            vDOS_GET_SET_FILE_HANDLE_ATTRIBUTES(unsigned char _ucFileHandle, unsigned char _ucSet, unsigned char _ucNewAttributes);
    void            vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(unsigned char _ucFileHandle, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate);
    void            vDOS_GET_CURRENT_DIRECTORY(unsigned char _ucDriveNumber);
    void            vDOS_CHANGE_CURRENT_DIRECTORY(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath);
    void            vDOS_GET_WHOLE_PATH_STRING();

#ifdef Q_OS_ANDROID
    void        vRequestAndroidPermissionsAndSetInterface(QObject *parent);
#endif

    Task        oParser();

    ByteReader  oRead(int size)
    {
        m_poCurrentByteReader = std::make_unique<ByteReader>(m_acBuffer, size);
        return *m_poCurrentByteReader;
    }
    std::unique_ptr<ByteReader>     m_poCurrentByteReader;
    Ui::MainWindow                  *m_poUI = nullptr;
    QTimer							*m_poRedLightOffTimer = nullptr;
    QTimer							*m_poGreenLightOffTimer = nullptr;
    QTimer							*m_poUnlockTimer = nullptr;
    Interface                       *m_poInterface = nullptr;
    QByteArray						m_acBuffer;
    Drive                           m_oDrive;

    bool                            m_bRxCRC;
    bool                            m_bTxCRC;
    bool                            m_bTimeout;
    bool                            m_bAutoRetry;
    bool                            m_bReadOnly;
    bool                            m_bSlowTx;
    bool                            m_bLogModify = false;   // BDOS function being logged modifies the served directories

    bool                            m_bLastButtonClickedIsConnect = false;
    bool                            m_bConnectedOnce = false;
    bool                            m_bDiskChanged = false;
    quint64                         m_uiBytesReceived = 0;
    quint64                         m_uiBytesTransmitted = 0;
    quint64                         m_uiReceiveErrors = 0;
    quint64                         m_uiTransmitErrors = 0;
    tdInterface                     m_eSelectedInterface = eInterfaceSerial;
    tdServeMode                     m_eServeMode = eServeDiskImage;
    QString                         m_oSelectedSerialID;
    QString                         m_oSelectedBlueToothID;
    QString                         &roSelectedID();
    QSettings                       *m_poSettings;
    tdConnectionState               m_eConnectionState = eCStateDisconnected;
    QFile*                          m_apoOpenedFiles[256] = {0};
    QString                         m_szBDOSRootDir[8] = { "", "", "", "", "", "", "", "" };
    QString                         m_szBDOSCurrentDir[8] = { "", "", "", "", "", "", "", "" };
    unsigned char                   m_ucCurrentPhysicalDrive = 0;
    QMap<quint32, QString>          m_oFindEntries;         // FIB find id -> host path of entry found
    quint32                         m_uiNextFindId = 0;
    QString                         m_szWholePath;          // whole path of last entry found (_WPATH)
    QTemporaryDir                   *m_poRamDisk = nullptr;  // RAM disk H: (_RAMD), temporary directory
    unsigned char                   m_ucRamDiskSegments = 0; // RAM disk size (16 KB segments, 0 = no RAM disk)
};

#endif
