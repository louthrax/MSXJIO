#ifndef MainWindow_h
#define MainWindow_h

#include <QMainWindow>
#include <QTextEdit>
#include <QListWidgetItem>
#include <QSettings>
#include <QTimer>

#include "Common.h"
#include "Server.h"

namespace Ui
{
class	MainWindow;
}

/*
 =======================================================================================================================
    Graphical JIO server: user interface and settings of the server (Server.h)
 =======================================================================================================================
 */
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
    void        onLog(tdLogType _eLogType, const QString & _roMessage, bool _bModify);
    void        onStateChanged(tdConnectionState _eState);
    void        onDataReceived(int _iSize);
    void        onDataTransmitted(int _iSize);

    void        onButtonClicked();
    void        onDirectoryPathChanged();
    void        onItemActivated(QListWidgetItem *_poItem);
    void        onImagePathValidated();
    void        onAddressLineValidated();

    void        onRedLightTimer();
    void        onGreenLightTimer();

private:
    void		vSetInterface(tdInterface _eInterface);
    void		vSetState(tdConnectionState _eCState);
    void		vSetServeMode(tdServeMode _eServeMode);

    void		vLog(tdLogType _eLogType, const QString &_szMessage);
    void		vSetFrameColor(QFrame *_poFrame, int _iR, int _iG, int _iB);
    void		vSaveSettings();
    void        vAdjustScrollBars(QAbstractScrollArea *_poWidget);
    void		vUpdateLights();
    void        vUpdateDrivePathsTexts();
    void        vUpdateMediaIcon();
    QString     szCommandLine();

#ifdef Q_OS_ANDROID
    void        vRequestAndroidPermissionsAndSetInterface(QObject *parent);
#endif

    Ui::MainWindow                  *m_poUI = nullptr;
    Server                          *m_poServer = nullptr;
    QTimer							*m_poRedLightOffTimer = nullptr;
    QTimer							*m_poGreenLightOffTimer = nullptr;

    tdInterface                     m_eSelectedInterface = eInterfaceSerial;
    QString                         m_oSelectedSerialID;
    QString                         m_oSelectedBlueToothID;
    QString                         &roSelectedID();
    QSettings                       *m_poSettings;
};

#endif
