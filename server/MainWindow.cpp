#include <QTimer>
#include <QStandardPaths>
#include <QFileDialog>
#include <QScrollBar>
#include <QButtonGroup>
#include <QPixmap>
#include <QBluetoothPermission>
#include <QClipboard>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QDir>
#include <QUrl>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

#include "MainWindow.h"
#include "ui_MainWindow.h"

/*
 =======================================================================================================================
    Android: the file dialogs give content:// URIs, that the file functions of the server (QDir, rename, mkdir,
    file times...) cannot use, and whose access is lost when the application restarts. The URIs of the shared storage
    are converted to paths, accessed with the "All files access" permission (MANAGE_EXTERNAL_STORAGE):
    content://com.android.externalstorage.documents/tree/primary%3ADownload%2Fsdcard -> /storage/emulated/0/Download/sdcard
    Other platforms: unchanged
 =======================================================================================================================
 */
static QString szLocalPath(const QString &_roPath)
{
#ifdef Q_OS_ANDROID
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QUrl	oUrl(_roPath);
    // last segment of the URI path (tree/<id> or document/<id>), "primary:Download/sdcard"
    QString oDocumentID = QUrl::fromPercentEncoding(oUrl.path(QUrl::FullyEncoded).section('/', -1).toUtf8());
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if(oUrl.scheme() != "content")
        return _roPath;

    if(oUrl.host() == "com.android.externalstorage.documents")
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        QString oVolume = oDocumentID.section(':', 0, 0);           // "primary" or the ID of an SD card ("1234-ABCD")
        QString oPath = oDocumentID.section(':', 1);
        QString oRoot = (oVolume == "primary") ? "/storage/emulated/0" : "/storage/" + oVolume;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        return oPath.isEmpty() ? oRoot : oRoot + "/" + oPath;
    }

    if((oUrl.host() == "com.android.providers.downloads.documents") && oDocumentID.startsWith("raw:"))
        return oDocumentID.mid(4);                                  // "raw:/storage/emulated/0/Download/Game.dsk"

    return _roPath;                                                 // other providers: the URI is kept
#else
    return _roPath;
#endif
}

#ifdef Q_OS_ANDROID

/*
 =======================================================================================================================
    "All files access" (Android 11+), needed by the paths of szLocalPath(): opens its page of the system settings when
    it is not granted
 =======================================================================================================================
 */
static bool bRequestAllFilesAccess()
{
    if(QNativeInterface::QAndroidApplication::sdkVersion() < 30)
        return true;                                                // WRITE_EXTERNAL_STORAGE is enough

    if(QJniObject::callStaticMethod<jboolean>("android/os/Environment", "isExternalStorageManager"))
        return true;

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QJniObject	oContext = QNativeInterface::QAndroidApplication::context();
    QJniObject	oPackage = oContext.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    QJniObject	oUri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString("package:" + oPackage.toString()).object<jstring>());
    QJniObject	oIntent(
        "android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
        QJniObject::fromString("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION").object<jstring>(),
        oUri.object());
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    oContext.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", oIntent.object());
    return false;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vRequestAndroidPermissionsAndSetInterface(QObject *parent)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QBluetoothPermission	oBluetoothPermission;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if(!bRequestAllFilesAccess())
    {
        vLog(eLogWarning, "\"All files access\" is needed to serve the disk images and the directories: allow it, then come back\n");
    }

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
    m_poServer(new Server(this)),
    m_poRedLightOffTimer(new QTimer(this)),
    m_poGreenLightOffTimer(new QTimer(this))
{
    m_poSettings = new QSettings();

    m_poUI->setupUi(this);

    connect(m_poServer, &Server::log, this, &MainWindow::onLog);
    connect(m_poServer, &Server::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_poServer, &Server::deviceDiscovered, this, &MainWindow::onDeviceDiscovered);
    connect(m_poServer, &Server::dataReceived, this, &MainWindow::onDataReceived);
    connect(m_poServer, &Server::dataTransmitted, this, &MainWindow::onDataTransmitted);
    connect(m_poServer, &Server::statisticsChanged, this, &MainWindow::vUpdateLights);
    m_poServer->vSetDeviceResolver([this]() { return roSelectedID(); });

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
    connect(m_poUI->commandLinePushButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->bluetoothButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->autoRetry, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->USBButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->TxCRC, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->RxCRC, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->serveImageButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
    connect(m_poUI->serveDirectoriesButton, &QPushButton::clicked, this, &MainWindow::onButtonClicked);

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

    // Layouts (MainWindow.ui): the window can be resized, the log and the list of the devices take the space
    // (Android: full screen, any size and orientation)
    vUpdateDriveRowsHeight();
    setFocusPolicy(Qt::StrongFocus);

    onRedLightTimer();
    m_poRedLightOffTimer->setSingleShot(true);
    connect(m_poRedLightOffTimer, &QTimer::timeout, this, &MainWindow::onRedLightTimer);

    onGreenLightTimer();
    m_poGreenLightOffTimer->setSingleShot(true);
    connect(m_poGreenLightOffTimer, &QTimer::timeout, this, &MainWindow::onGreenLightTimer);

    connect(m_poUI->imagePathLineEdit, &QLineEdit::editingFinished, this, &MainWindow::onImagePathValidated);
    connect(m_poUI->addressLineEdit, &QLineEdit::editingFinished, this, &MainWindow::onAddressLineValidated);

    m_poUI->logWidget->setFont(QFont("Ubuntu Mono", LOG_WIDGET_FONT_SIZE));
    m_poUI->namesListWidget->setFont(QFont("Ubuntu", NAMES_LIST_WIDGET_FONT_SIZE));

    vAdjustScrollBars(m_poUI->logWidget);
    vAdjustScrollBars(m_poUI->namesListWidget);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QButtonGroup	*poGroup = new QButtonGroup(this);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    m_poServer->m_bRxCRC = m_poSettings->value("RxCRC", true).toBool();
    m_poServer->m_bTxCRC = m_poSettings->value("TxCRC", true).toBool();
    m_poServer->m_bAutoRetry = m_poSettings->value("AutoRetry", true).toBool();
    m_poServer->m_bTimeout = m_poSettings->value("Timeout", false).toBool();
    m_poServer->m_bReadOnly = m_poSettings->value("ReadOnly", false).toBool();
    m_poServer->m_bSlowTx = m_poSettings->value("SlowTx", false).toBool();
    tdServeMode eServeMode = (tdServeMode) m_poSettings->value("ServeMode", eServeDiskImage).toInt();

    m_oSelectedSerialID = m_poSettings->value("SelectedSerialID").toString();
    m_oSelectedBlueToothID = m_poSettings->value("SelectedBlueToothID").toString();
    m_poUI->imagePathLineEdit->setText(szLocalPath(m_poSettings->value("LastMediaInserted").toString()));
    onImagePathValidated();
    m_poServer->roDrive().m_oLastPathBrowsed = m_poSettings->value("LastPathBrowsed").toString();

    for(int iDrive = 0; iDrive < 8; iDrive++)
        m_poServer->vSetDrivePath(iDrive, szLocalPath(m_poSettings->value(QString("DrivePath%1").arg(QChar('A' + iDrive))).toString()));


#ifdef Q_OS_ANDROID
    m_eSelectedInterface = eInterfaceBluetooth;
    m_poUI->bluetoothButton->hide();
    m_poUI->USBButton->hide();
    m_poUI->commandLinePushButton->hide();      // no command line server on Android
#else
    m_eSelectedInterface = (tdInterface) m_poSettings->value("SelectedInterface").toInt();
#endif
    poGroup->setExclusive(true);
    poGroup->addButton(m_poUI->USBButton);
    poGroup->addButton(m_poUI->bluetoothButton);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QButtonGroup	*poServeModeGroup = new QButtonGroup(this);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    poServeModeGroup->setExclusive(true);
    poServeModeGroup->addButton(m_poUI->serveImageButton);
    poServeModeGroup->addButton(m_poUI->serveDirectoriesButton);

    m_poUI->bluetoothButton->setChecked(m_eSelectedInterface == eInterfaceBluetooth);
    m_poUI->USBButton->setChecked(m_eSelectedInterface == eInterfaceSerial);

    m_poUI->RxCRC->setChecked(m_poServer->m_bRxCRC);
    m_poUI->TxCRC->setChecked(m_poServer->m_bTxCRC);
    m_poUI->autoRetry->setChecked(m_poServer->m_bAutoRetry);
    m_poUI->timeout->setChecked(m_poServer->m_bTimeout);
    m_poUI->readOnly->setChecked(m_poServer->m_bReadOnly);
    m_poUI->slowTx->setChecked(m_poServer->m_bSlowTx);

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


    m_poUI->serveImageButton->setToolTip("Serve a disk image (floppy, or hard disk with partitions).\nUsed by the JIO MSX-DOS 1 and MSX-DOS 2 ROMs reading sectors.\nApplied at MSX startup.");
    m_poUI->serveDirectoriesButton->setToolTip("Serve host directories as drives A: to H:.\nUsed by the JIO MSX-DOS 2 ROMs with remote file system.\nApplied at MSX startup.");
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
    m_poUI->commandLinePushButton->setToolTip("Copy to the clipboard the command line of the command line server (JIOServerCLI)\nwith the current configuration, also shown in the log.");
    m_poUI->unlockPushButton->setToolTip("Send repeated data to the MSX until it responds.\nUseful when the MSX is stuck waiting for data.");
    m_poUI->RxCRC->setToolTip("Enable CRC checking for incoming data on the MSX.\nApplied at MSX startup.");
    m_poUI->TxCRC->setToolTip("Enable CRC for outgoing data to the MSX.\nApplied at MSX startup.");
    m_poUI->autoRetry->setToolTip("Automatically retry all MSX commands indefinitely.\nDirectories: used by JIO.COM (otherwise \"Not ready\" after 1 s without answer).\nApplied at MSX startup (JIO.COM: at install).");
    m_poUI->timeout->setToolTip("If enabled, abort the command after a timeout.\nIf disabled, wait indefinitely for a response.");
    m_poUI->readOnly->setToolTip("Prevent writes to the disk image, or to the served directories\n(the RAM disk H: stays writable).");
    m_poUI->fileEjectPushButton->setToolTip("Eject disk image.");
    m_poUI->logWidget->setToolTip("Server log.");

    vSetState(m_poServer->eState());

    vUpdateLights();        // counters shown from the start (0 / 0)
    vUpdateDrivePathsTexts();
    vSetServeMode(eServeMode);

#ifdef Q_OS_ANDROID
    vRequestAndroidPermissionsAndSetInterface(this);
#else
    vSetInterface(m_eSelectedInterface);
    restoreGeometry(m_poSettings->value("WindowGeometry").toByteArray());     // size and position of the last run
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
MainWindow::~MainWindow()
{
    vSaveSettings();
    delete m_poServer;          // before the user interface (no signal of the server after it)
    m_poServer = nullptr;
    delete m_poSettings;
    delete m_poUI;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSetInterface(tdInterface _eInterface)
{
    m_poUI->namesListWidget->clear();

    m_eSelectedInterface = _eInterface;

    m_poUI->addressLineEdit->setText(roSelectedID());

    m_poServer->vSetInterface(_eInterface);
    m_poServer->vScanDevices();
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
void MainWindow::onStateChanged(tdConnectionState _eState)
{
    if(_eState == eCStateConnected)
    {
        m_poUI->unlockPushButton->setChecked(false);
        vSaveSettings();
    }

    vSetState(_eState);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDataReceived(int _iSize)
{
    m_poUI->unlockPushButton->setChecked(false);
    vSetFrameColor(m_poUI->redLightLabel, 255, 0, 0);
    m_poRedLightOffTimer->start(m_poRedLightOffTimer->remainingTime() + qMax(16, _iSize / 9));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDataTransmitted(int _iSize)
{
    vSetFrameColor(m_poUI->greenLightLabel, 0, 255, 0);
    m_poGreenLightOffTimer->start(m_poGreenLightOffTimer->remainingTime() + qMax(16, _iSize / 9));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::changeEvent(QEvent *_poEvent)
{
    if(_poEvent->type() == QEvent::FontChange)
        vUpdateDriveRowsHeight();           // application font set after the creation of the window (Main.cpp)
    QMainWindow::changeEvent(_poEvent);
}

/*
 =======================================================================================================================
    Height of the 8 rows of the directories: compact (21 pixels with the desktop font), higher with a larger font
    (Android), so that the text is not cut
 =======================================================================================================================
 */
void MainWindow::vUpdateDriveRowsHeight()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    int			iHeight = qMax(21, QFontMetrics(font()).height() + 4);
    QLineEdit	*apoLineEdits[] =
    {
        m_poUI->directoryPathLineEdit_DriveA, m_poUI->directoryPathLineEdit_DriveB, m_poUI->directoryPathLineEdit_DriveC,
        m_poUI->directoryPathLineEdit_DriveD, m_poUI->directoryPathLineEdit_DriveE, m_poUI->directoryPathLineEdit_DriveF,
        m_poUI->directoryPathLineEdit_DriveG, m_poUI->directoryPathLineEdit_DriveH
    };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    for(QLineEdit *poLineEdit : apoLineEdits)
        poLineEdit->setFixedHeight(iHeight);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSaveSettings()
{
    m_poSettings->setValue("LastMediaInserted", m_poServer->roDrive().m_oLastMediaInserted);
    m_poSettings->setValue("LastPathBrowsed", m_poServer->roDrive().m_oLastPathBrowsed);
    m_poSettings->setValue("SelectedSerialID", m_oSelectedSerialID);
    m_poSettings->setValue("SelectedBlueToothID", m_oSelectedBlueToothID);
    m_poSettings->setValue("RxCRC", m_poServer->m_bRxCRC);
    m_poSettings->setValue("TxCRC", m_poServer->m_bTxCRC);
    m_poSettings->setValue("AutoRetry", m_poServer->m_bAutoRetry);
    m_poSettings->setValue("Timeout", m_poServer->m_bTimeout);
    m_poSettings->setValue("ReadOnly", m_poServer->m_bReadOnly);
    m_poSettings->setValue("SlowTx", m_poServer->m_bSlowTx);
    m_poSettings->setValue("ServeMode", m_poServer->eServeMode());

    for(int iDrive = 0; iDrive < 8; iDrive++)
        m_poSettings->setValue(QString("DrivePath%1").arg(QChar('A' + iDrive)), m_poServer->szDrivePath(iDrive));

#ifndef Q_OS_ANDROID
    m_poSettings->setValue("SelectedInterface", m_eSelectedInterface);
    m_poSettings->setValue("WindowGeometry", saveGeometry());
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
void MainWindow::onItemActivated(QListWidgetItem *_poItem)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QString oAddress = _poItem->data(Qt::UserRole).toString();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    m_poUI->addressLineEdit->setText(oAddress);

    roSelectedID() = oAddress;
    vSetState(m_poServer->eState());
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vUpdateLights()
{
    m_poUI->redLightLabel->setText(QLocale().toString(m_poServer->uiBytesReceived()) + "\n" + QLocale().toString(m_poServer->uiTransmitErrors()));
    m_poUI->greenLightLabel->setText(QLocale().toString(m_poServer->uiBytesTransmitted()) + "\n" + QLocale().toString(m_poServer->uiReceiveErrors()));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vUpdateDrivePathsTexts()
{
    m_poUI->directoryPathLineEdit_DriveA->setText(m_poServer->szDrivePath(0));
    m_poUI->directoryPathLineEdit_DriveB->setText(m_poServer->szDrivePath(1));
    m_poUI->directoryPathLineEdit_DriveC->setText(m_poServer->szDrivePath(2));
    m_poUI->directoryPathLineEdit_DriveD->setText(m_poServer->szDrivePath(3));
    m_poUI->directoryPathLineEdit_DriveE->setText(m_poServer->szDrivePath(4));
    m_poUI->directoryPathLineEdit_DriveF->setText(m_poServer->szDrivePath(5));
    m_poUI->directoryPathLineEdit_DriveG->setText(m_poServer->szDrivePath(6));
    m_poUI->directoryPathLineEdit_DriveH->setText(m_poServer->szDrivePath(7));
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

    if (poSender == m_poUI->directoryPathLineEdit_DriveA) m_poServer->vSetDrivePath(0, poSender->text()); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveB) m_poServer->vSetDrivePath(1, poSender->text()); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveC) m_poServer->vSetDrivePath(2, poSender->text()); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveD) m_poServer->vSetDrivePath(3, poSender->text()); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveE) m_poServer->vSetDrivePath(4, poSender->text()); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveF) m_poServer->vSetDrivePath(5, poSender->text()); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveG) m_poServer->vSetDrivePath(6, poSender->text()); else
    if (poSender == m_poUI->directoryPathLineEdit_DriveH) m_poServer->vSetDrivePath(7, poSender->text());
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
        m_poServer->m_bRxCRC = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->TxCRC)
        m_poServer->m_bTxCRC = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->autoRetry)
        m_poServer->m_bAutoRetry = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->timeout)
        m_poServer->m_bTimeout = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->readOnly)
        m_poServer->m_bReadOnly = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->slowTx)
        m_poServer->m_bSlowTx = ((QPushButton *) poSender)->isChecked();
    else if(poSender == m_poUI->serveImageButton)
    {
        if(m_poServer->eServeMode() != eServeDiskImage)
        {
            vSetServeMode(eServeDiskImage);
            vLog(eLogInfo, "Serving disk image\n");
        }
    }
    else if(poSender == m_poUI->serveDirectoriesButton)
    {
        if(m_poServer->eServeMode() != eServeDirectories)
        {
            vSetServeMode(eServeDirectories);
            vLog(eLogInfo, "Serving directories\n");
        }
    }
    else if(poSender == m_poUI->unlockPushButton)
    {
        m_poServer->vSetUnlock(m_poUI->unlockPushButton->isChecked());
    }
    else if(poSender == m_poUI->refreshPushButton)
    {
        m_poUI->namesListWidget->clear();
        m_poServer->vScanDevices();
    }
    else if(poSender == m_poUI->commandLinePushButton)
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        QString szCommand = szCommandLine();
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        if(szCommand.isEmpty())
        {
            vLog(eLogWarning, m_poServer->eServeMode() == eServeDiskImage ?
                     "No command line: select the disk image to serve first.\n" :
                     "No command line: select at least one directory to serve first.\n");
            return;
        }

        QGuiApplication::clipboard()->setText(szCommand);
        vLog(eLogInfo, "Command line copied to the clipboard:\n");
        vLog(eLogClient, szCommand + "\n");
        vLog(eLogInfo, "It starts the command line server (JIOServerCLI) with the current configuration, without user "
                       "interface: connection to the device, attempted again until it is present. Add -l <file> to "
                       "write the log to a file, --help for all the options. Stop it with Ctrl+C.\n");
    }
    else if(poSender == m_poUI->clearPushButton)
    {
        m_poServer->vResetStatistics();
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

        if(!m_poServer->roDrive().m_oLastPathBrowsed.isEmpty() && QFileInfo::exists(m_poServer->roDrive().m_oLastPathBrowsed))
        {
            initialDir = QFileInfo(m_poServer->roDrive().m_oLastPathBrowsed).absolutePath();
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
            m_poUI->imagePathLineEdit->setText(szLocalPath(oImagePath));
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

        if(!m_poServer->roDrive().m_oLastPathBrowsed.isEmpty() && QFileInfo::exists(m_poServer->roDrive().m_oLastPathBrowsed))
        {
            initialDir = QFileInfo(m_poServer->roDrive().m_oLastPathBrowsed).absolutePath();
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

        oDrivePath = szLocalPath(oDrivePath);
        if(!oDrivePath.isEmpty())
        {
            if (poSender == m_poUI->fileSelectDriveA_PushButton) m_poServer->vSetDrivePath(0, oDrivePath); else
            if (poSender == m_poUI->fileSelectDriveB_PushButton) m_poServer->vSetDrivePath(1, oDrivePath); else
            if (poSender == m_poUI->fileSelectDriveC_PushButton) m_poServer->vSetDrivePath(2, oDrivePath); else
            if (poSender == m_poUI->fileSelectDriveD_PushButton) m_poServer->vSetDrivePath(3, oDrivePath); else
            if (poSender == m_poUI->fileSelectDriveE_PushButton) m_poServer->vSetDrivePath(4, oDrivePath); else
            if (poSender == m_poUI->fileSelectDriveF_PushButton) m_poServer->vSetDrivePath(5, oDrivePath); else
            if (poSender == m_poUI->fileSelectDriveG_PushButton) m_poServer->vSetDrivePath(6, oDrivePath); else
            if (poSender == m_poUI->fileSelectDriveH_PushButton) m_poServer->vSetDrivePath(7, oDrivePath);

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
        if (poSender == m_poUI->fileEjectDriveA_PushButton) m_poServer->vSetDrivePath(0, ""); else
        if (poSender == m_poUI->fileEjectDriveB_PushButton) m_poServer->vSetDrivePath(1, ""); else
        if (poSender == m_poUI->fileEjectDriveC_PushButton) m_poServer->vSetDrivePath(2, ""); else
        if (poSender == m_poUI->fileEjectDriveD_PushButton) m_poServer->vSetDrivePath(3, ""); else
        if (poSender == m_poUI->fileEjectDriveE_PushButton) m_poServer->vSetDrivePath(4, ""); else
        if (poSender == m_poUI->fileEjectDriveF_PushButton) m_poServer->vSetDrivePath(5, ""); else
        if (poSender == m_poUI->fileEjectDriveG_PushButton) m_poServer->vSetDrivePath(6, ""); else
        if (poSender == m_poUI->fileEjectDriveH_PushButton) m_poServer->vSetDrivePath(7, "");

        vUpdateDrivePathsTexts();
    }
    else if(poSender == m_poUI->connectPushButton)
    {
        if(m_poServer->eState() == eCStateDisconnected)
        {
            m_poServer->vConnect(roSelectedID());
        }
        else
        {
            m_poServer->vDisconnect();
        }
    }
    else if(poSender == m_poUI->fileEjectPushButton)
    {
        if(!m_poServer->roDrive().oMediaPath().isEmpty())
        {
            m_poServer->vEjectMedia();
            m_poUI->imagePathLineEdit->setText("");
            vUpdateMediaIcon();
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
    switch(_eCState)
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
    Disk image or directories: only the controls of the selected mode are shown, the other mode is not served.
 =======================================================================================================================
 */
void MainWindow::vSetServeMode(tdServeMode _eServeMode)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    bool			bImage = _eServeMode == eServeDiskImage;
    QList<QWidget*> oImageWidgets =
    {
        m_poUI->iconMediaType, m_poUI->imagePathLineEdit, m_poUI->fileSelectPushButton, m_poUI->fileEjectPushButton
    };
    QList<QWidget*> oDirectoriesWidgets =
    {
        m_poUI->label_DriveA, m_poUI->directoryPathLineEdit_DriveA, m_poUI->fileSelectDriveA_PushButton, m_poUI->fileEjectDriveA_PushButton,
        m_poUI->label_DriveB, m_poUI->directoryPathLineEdit_DriveB, m_poUI->fileSelectDriveB_PushButton, m_poUI->fileEjectDriveB_PushButton,
        m_poUI->label_DriveC, m_poUI->directoryPathLineEdit_DriveC, m_poUI->fileSelectDriveC_PushButton, m_poUI->fileEjectDriveC_PushButton,
        m_poUI->label_DriveD, m_poUI->directoryPathLineEdit_DriveD, m_poUI->fileSelectDriveD_PushButton, m_poUI->fileEjectDriveD_PushButton,
        m_poUI->label_DriveE, m_poUI->directoryPathLineEdit_DriveE, m_poUI->fileSelectDriveE_PushButton, m_poUI->fileEjectDriveE_PushButton,
        m_poUI->label_DriveF, m_poUI->directoryPathLineEdit_DriveF, m_poUI->fileSelectDriveF_PushButton, m_poUI->fileEjectDriveF_PushButton,
        m_poUI->label_DriveG, m_poUI->directoryPathLineEdit_DriveG, m_poUI->fileSelectDriveG_PushButton, m_poUI->fileEjectDriveG_PushButton,
        m_poUI->label_DriveH, m_poUI->directoryPathLineEdit_DriveH, m_poUI->fileSelectDriveH_PushButton, m_poUI->fileEjectDriveH_PushButton
    };
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    for(QWidget *poWidget : oImageWidgets) poWidget->setVisible(bImage);
    for(QWidget *poWidget : oDirectoriesWidgets) poWidget->setVisible(!bImage);

    m_poUI->serveImageButton->setChecked(bImage);
    m_poUI->serveDirectoriesButton->setChecked(!bImage);

    // CRC, timeout and slow transmission are only used for the disk image (COMMAND_DRIVE_*), the settings are kept.
    // "Read only" and "Auto retry" (JIO.COM for the directories) are used in both modes.
    m_poUI->RxCRC->setEnabled(bImage);
    m_poUI->TxCRC->setEnabled(bImage);
    m_poUI->autoRetry->setEnabled(true);          // also used by JIO.COM (directories)
    m_poUI->timeout->setEnabled(bImage);
    m_poUI->slowTx->setEnabled(bImage);

    m_poServer->vSetServeMode(_eServeMode);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onLog(tdLogType _eLogType, const QString &_roMessage, bool _bModify)
{
    /*~~~~~~~~~~~~~~~~~~~~*/
    QTextCharFormat oFormat;
    /*~~~~~~~~~~~~~~~~~~~~*/

    switch(_eLogType)
    {
    case eLogInfo:		  oFormat.setForeground(QColor(  0,   0,   0)); break;
    case eLogWarning:	  oFormat.setForeground(QColor(192,  64,  64)); break;
    case eLogError:		  oFormat.setBackground(QColor(255,   0,   0)); break;
    case eLogRead:		  oFormat.setForeground(QColor(  0, 192,   0)); break;
    // orange: modifications of the disk image (sectors written) or of the served directories
    case eLogWrite:		  oFormat.setForeground(QColor(230, 110,   0)); break;
    case eLogBDOS:		  oFormat.setForeground(QColor(  0,   0, 192)); break;
    case eLogBDOSModify:  oFormat.setForeground(QColor(230, 110,   0)); break;
    case eLogBDOSDetails: oFormat.setForeground(_bModify ? QColor(225, 150,  70) : QColor( 90,  90, 192)); break;
    case eLogConnected:   oFormat.setForeground(QColor(128, 128, 255)); break;
    case eLogClient:      oFormat.setForeground(QColor(  0, 140, 140)); break;
    }

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QScrollBar	*scrollBar = m_poUI->logWidget->verticalScrollBar();
    bool		atBottom = (scrollBar->value() == scrollBar->maximum());
    QTextCursor oCursor(m_poUI->logWidget->document());
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    oCursor.movePosition(QTextCursor::End);
    oCursor.insertText(_roMessage, oFormat);

    if(atBottom)
    {
        scrollBar->setValue(scrollBar->maximum());
    }
}

// Log of the user interface
void MainWindow::vLog(tdLogType _eLogType, const QString &_szMessage)
{
    onLog(_eLogType, _szMessage, false);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onImagePathValidated()
{
    m_poServer->bInsertMedia(m_poUI->imagePathLineEdit->text());
    vUpdateMediaIcon();
}

void MainWindow::vUpdateMediaIcon()
{
    switch(m_poServer->roDrive().eMediaType())
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
    vSetState(m_poServer->eState());
}

/*
 =======================================================================================================================
    Command line of JIOServerCLI (JIOServerCLI.pro) with the configuration of the user interface
 =======================================================================================================================
 */
static QString szQuoteArgument(const QString &_szArgument)
{
    static const QRegularExpression soSafe("^[A-Za-z0-9_@%+=:,./\\\\-]+$");

    if(soSafe.match(_szArgument).hasMatch()) return _szArgument;

#ifdef Q_OS_WIN
    return "\"" + _szArgument + "\"";
#else
    QString szQuoted = _szArgument;
    return "'" + szQuoted.replace("'", "'\\''") + "'";
#endif
}

QString MainWindow::szCommandLine()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QStringList aszArguments;
#ifdef Q_OS_WIN
    QString     szProgram = QCoreApplication::applicationDirPath() + "/JIOServerCLI.exe";
#else
    QString     szProgram = QCoreApplication::applicationDirPath() + "/JIOServerCLI";
#endif
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    // next to the graphical server (Windows installer, macOS bundle), else from the PATH
    aszArguments << (QFileInfo::exists(szProgram) ? QDir::toNativeSeparators(szProgram) : QString("JIOServerCLI"));

    // empty if nothing is served
    if(m_poServer->eServeMode() == eServeDiskImage)
    {
        if(m_poServer->roDrive().oMediaPath().isEmpty()) return QString();
        aszArguments << "-i" << QDir::toNativeSeparators(m_poServer->roDrive().oMediaPath());
        if(!m_poServer->m_bRxCRC) aszArguments << "--no-rx-crc";
        if(!m_poServer->m_bTxCRC) aszArguments << "--no-tx-crc";
        if(m_poServer->m_bTimeout) aszArguments << "--timeout";
        if(m_poServer->m_bSlowTx) aszArguments << "--slow-tx";
    }
    else
    {
        for(int iDrive = 0; iDrive < 8; iDrive++)
        {
            if(!m_poServer->szDrivePath(iDrive).isEmpty())
                aszArguments << "-d" << QString("%1=%2").arg(QChar('A' + iDrive)).arg(QDir::toNativeSeparators(m_poServer->szDrivePath(iDrive)));
        }
        if(!aszArguments.contains("-d")) return QString();
    }

    if(m_poServer->m_bReadOnly) aszArguments << "-r";
    if(!m_poServer->m_bAutoRetry) aszArguments << "--no-auto-retry";

    if(!roSelectedID().isEmpty())
        aszArguments << (m_eSelectedInterface == eInterfaceBluetooth ? "-b" : "-p") << roSelectedID();

    for(QString &rszArgument : aszArguments)
        rszArgument = szQuoteArgument(rszArgument);

    return aszArguments.join(' ');
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
