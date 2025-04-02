#ifndef MainWindow_h
#define MainWindow_h

#include <QMainWindow>
#include <QTextEdit>
#include <QFile>
#include <QListWidgetItem>
#include <QSettings>

#include "ByteReader.h"
#include "Interface.h"
#include "Common.h"

typedef enum {
    eInterfaceBluetooth,
    eInterfaceSerial
} tdInterface;

typedef enum {
    eCStateConnecting,
    eCStateConnected,
    eCStateDisconnected
} tdConnectionState;

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
    void        onCheckBoxStateChanged(int _iState);
    void        onTextChanged(QString _oText);

    void        onRedLightTimer();
    void        onGreenLightTimer();
    void        onUnlockTimer();

private:
    void        vAdjustScrollBars(QAbstractScrollArea *_poWidget);
    void		vSetInterface(tdInterface _eInterface);
    void		vSendChecksum(const QByteArray &_roData);
    void		vSendData(const QByteArray &_roData);
    quint8		ucCalculateChecksum(const QByteArray &_roData);
    QByteArray	acGetServerInfo();
    void		vLog(tdLogType _eLogType, QString fmt, ...);
    QString		oFormatSize(qint64 _uiSizeBytes);
    void		vSetFrameColor(QFrame *_poFrame, int _iR, int _iG, int _iB);
    void		vSaveSettings();
    void		vSetState(tdConnectionState _eCState);
    void		vUpdateLights();
#ifdef Q_OS_ANDROID
    void		vRequestAndroidPermissions();
#endif

    QByteArray crc16_xmodem(const QByteArray &data);

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
    QFile							*m_poImageFile = nullptr;
    QByteArray						m_acBuffer;
    bool                            m_bReadCRC = true;
    bool                            m_bWriteCRC = false;
    bool                            m_bRetryTimeout = false;
    bool                            m_bRetryCRC = false;
    bool                            m_bLastButtonClickedIsConnect = false;
    bool                            m_bConnectedOnce = false;
    quint64                         m_uiBytesReceived = 0;
    quint64                         m_uiBytesSent = 0;
    quint64                         m_uiReceiveErrors = 0;
    quint64                         m_uiSendErrors = 0;
    tdInterface                     m_eSelectedInterface = eInterfaceSerial;
    QString                         m_oSelectedImagePath;
    QString                         m_oSelectedSerialID;
    QString                         m_oSelectedBlueToothID;
    QString                         &roSelectedID();
    QSettings                       *m_poSettings;
    tdConnectionState               m_eConnectionState = eCStateDisconnected;
};

#endif
