#ifndef MainWindow_h
#define MainWindow_h

#include <QMainWindow>
#include <QTextEdit>
#include <QFile>
#include <QListWidgetItem>
#include <QSettings>
#include <QDirIterator>

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

    void		vLog(tdLogType _eLogType, QString fmt, ...);
    void		vSetFrameColor(QFrame *_poFrame, int _iR, int _iG, int _iB);
    void		vSaveSettings();
    void        vAdjustScrollBars(QAbstractScrollArea *_poWidget);
    void		vUpdateLights();

    void vUpdateFIB(tdFileInfoBlock *_poFIB);


    QString         szGetFIBDescription(tdFileInfoBlock &_roFIB);
    QString         szGetFileHandleDescription(unsigned char _ucFileHandle);

    unsigned char   ucAddFile(QFile * _poFile);
    void            vDOS_CLOSE_FILE_HANDLE(unsigned char _ucFileHandle);
    void            vDOS_READ_FROM_FILE_HANDLE(unsigned char _ucFileHandle, unsigned short int _uiSize);
    void            vDOS_WRITE_TO_FILE_HANDLE(unsigned char _ucFileHandle, unsigned short int _uiSize, char * _pcData);
    void            vDOS_FIND_FIRST_ENTRY(unsigned char _ucSearchAttributes, unsigned _ucPhysicalDrive, QString _szDirectory, tdFileInfoBlock &_roFIB);
    void            vDOS_FIND_NEW_ENTRY(unsigned char _ucSearchAttributes, unsigned _ucPhysicalDrive, QString _szDirectory, tdFileInfoBlock &_roFIB);
    void            vDOS_OPEN_FILE_HANDLE(unsigned char _ucOpenMode, QString _szDirectory);
    void            vDOS_CHANGE_CURRENT_DIRECTORY(unsigned _ucPhysicalDrive, QString _szDirectory);
    void            vDOS_FIND_NEXT_ENTRY(tdFileInfoBlock &_roFIB);
    void            vDOS_MOVE_FILE_HANDLE_POINTER(unsigned char _ucFileHandle, unsigned char _ucMethodCode, int _iOffset);
    void            vDOS_GET_CURRENT_DIRECTORY(unsigned char _ucDriveNumber);
    void            vDOS_CREATE_FILE_HANDLE(QString _szPath, unsigned char _ucOpenMode, unsigned char _ucAttributes);
    void            vDOS_GET_WHOLE_PATH_STRING(unsigned char _ucPhysicalDrive, QString szDirectory);
    void            vDOS_DELETE_FILE_OR_SUBDIRECTORY(QString _szPath);
    void            vDOS_GET_SET_FILE_ATTRIBUTES(QString _szPath, unsigned char _ucSetAttributes, unsigned char _ucNewAttributes);
    void            vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(QString _szPath, unsigned char ucGetOrSet, unsigned short int uiNewDate, unsigned short int uiNewTime);
    void            vDOS_SELECT_DISK(unsigned char _ucDiskToSelect);

    void            vResetNFS();

    unsigned char   BDOSToQt(QString & _roString);
    void            QtToBDOS(QString & _roString);

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

    bool                            m_bLastButtonClickedIsConnect = false;
    bool                            m_bConnectedOnce = false;
    bool                            m_bDiskChanged = false;
    quint64                         m_uiBytesReceived = 0;
    quint64                         m_uiBytesTransmitted = 0;
    quint64                         m_uiReceiveErrors = 0;
    quint64                         m_uiTransmitErrors = 0;
    tdInterface                     m_eSelectedInterface = eInterfaceSerial;
    QString                         m_oSelectedSerialID;
    QString                         m_oSelectedBlueToothID;
    QString                         &roSelectedID();
    QSettings                       *m_poSettings;
    tdConnectionState               m_eConnectionState = eCStateDisconnected;
    QFile*                          m_apoOpenedFiles[256] = {0};
    QString                         m_szBDOSRootDir[8] = {
                                        "/mnt/DataLinux/Projects/MSXJIO/clients/JIO_NFS/drivea",
                                        "/mnt/DataLinux/Projects/MSXJIO/clients/JIO_NFS/driveb",
                                        "/mnt/DataLinux/Projects/MSXJIO/clients/JIO_NFS/drivec",
                                        "/mnt/DataLinux/Projects/MSXJIO/clients/JIO_NFS/drived",
                                        "",
                                        "",
                                        "",
                                        "" };
    QString                         m_szBDOSCurrentDir[8] = { "", "", "", "", "", "", "", "" };
    unsigned char                   m_ucCurrentPhysicalDrive = 0;
};

#endif
