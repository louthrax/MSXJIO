#include <QTimer>
#include <QStandardPaths>
#include <QFileDialog>
#include <QScrollBar>
#include <QButtonGroup>
#include <QThread>

#include "MainWindow.h"
#ifdef Q_OS_ANDROID
#include "ui_MainWindowAndroid.h"
#else
#include "ui_MainWindow.h"
#endif
#include "InterfaceSerialPort.h"
#include "InterfaceBluetoothSocket.h"

#include "MSXClient/flags.inc"

#pragma pack(push, 1)
typedef struct
{
    quint8	m_ucCommand;
    quint32 m_uiSector : 24;
    quint8	m_ucLength;
    quint16 m_uiAddress;
} tdReadWriteHeader;
#pragma pack(pop)

static_assert(sizeof(tdReadWriteHeader) == 7, "tdReadWriteHeader must be 7 bytes");
std::coroutine_handle<> ByteReader::	m_soHandle = nullptr;
#define vRead(a)	*a = *(typeof(a)) (((QByteArray) co_await oRead(sizeof(typeof(*a)))).constData());

/*
 =======================================================================================================================
 =======================================================================================================================
 */
Task MainWindow::oParser()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char		*signature = "JIO";
    const size_t	sigLength = strlen(signature);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    while(true)
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        char				cCommand;
        tdReadWriteHeader	oHeader;
        size_t				iSigPos = 0;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        while(iSigPos < sigLength)
        {
            vRead(&cCommand);

            if(cCommand == signature[iSigPos])
            {
                iSigPos++;
            }
            else if(cCommand == signature[0])
            {
                iSigPos = 1;
            }
            else
            {
                iSigPos = 0;
            }
        }

        vRead(&oHeader);

        switch(oHeader.m_ucCommand & 0x0F)
        {
        case COMMAND_INFO:
        {
            /*~~~~~~~~~~~*/
            QByteArray	ba;
            /*~~~~~~~~~~~*/

            ba = acGetServerInfo();
            vSendData(ba);
            vSendChecksum(ba);
        }
        break;

        case COMMAND_READ:
        {
            /*~~~~~~~~~~~~~~~~~~*/
            QByteArray	oFileData;
            /*~~~~~~~~~~~~~~~~~~*/

            vLog
                (
                    eLogRead,
                    "Read%c  %2d sector(s) at %10d to   0x%04X",
                    oHeader.m_ucCommand & FLAGBITS_READ_CRC ? 'C' : ' ',
                    oHeader.m_ucLength,
                    oHeader.m_uiSector,
                    oHeader.m_uiAddress
                    );
            m_poImageFile->seek (static_cast<quint64>(oHeader.m_uiSector) *512);
            oFileData = m_poImageFile->read(oHeader.m_ucLength * 512);
            vSendData(oFileData);
            if(oHeader.m_ucCommand & FLAGBITS_READ_CRC)
            {
                vSendChecksum(oFileData);
            }
        }
        break;

        case COMMAND_WRITE:
        {
            vLog
                (
                    eLogWrite,
                    "Write%c %2d sector(s) at %10d from 0x%04X",
                    oHeader.m_ucCommand & FLAGBITS_WRITE_CRC ? 'C' : ' ',
                    oHeader.m_ucLength,
                    oHeader.m_uiSector,
                    oHeader.m_uiAddress
                    );

            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
            QByteArray	data = co_await oRead(oHeader.m_ucLength * 512);
            /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

            if(oHeader.m_ucCommand & FLAGBITS_WRITE_CRC)
            {
                vSendChecksum(data);
            }

            m_poImageFile->seek (static_cast<quint64>(oHeader.m_uiSector) *512);
            m_poImageFile->write(data);
            m_poImageFile->flush();
        }
        break;

        case COMMAND_READ_ERROR:
            vLog(eLogError, "Read error !");
            m_uiSendErrors++;
            vUpdateLights();
            break;

        case COMMAND_INFO_ERROR:
            vLog(eLogError, "Info error !");
            m_uiSendErrors++;
            vUpdateLights();
            break;

        case COMMAND_WRITE_ERROR:
            vLog(eLogError, "Write error !");
            m_uiReceiveErrors++;
            vUpdateLights();
            break;

        default:
            vLog(eLogError, "Unknown command: " + QString(cCommand));
            break;
        }
    }
}

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
#ifdef ANDROID
    m_poSettings = new QSettings();
#else
    m_poSettings = new QSettings
        (
            QCoreApplication::applicationDirPath() + "/" + windowTitle() + ".ini",
            QSettings::IniFormat
            );
#endif
    m_poUI->setupUi(this);
    setFixedSize(size());
    setFocusPolicy(Qt::StrongFocus);

    onRedLightTimer();
    m_poRedLightOffTimer->setSingleShot(true);
    connect(m_poRedLightOffTimer, &QTimer::timeout, this, &MainWindow::onRedLightTimer);

    onGreenLightTimer();
    m_poGreenLightOffTimer->setSingleShot(true);
    connect(m_poGreenLightOffTimer, &QTimer::timeout, this, &MainWindow::onGreenLightTimer);

    connect(m_poUnlockTimer, &QTimer::timeout, this, &MainWindow::onUnlockTimer);

    m_poUI->logWidget->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QFont	oFont = m_poUI->logWidget->font();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    vAdjustScrollBars(m_poUI->logWidget);
    vAdjustScrollBars(m_poUI->namesListWidget);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QButtonGroup	*group = new QButtonGroup(this);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    m_bReadCRC = m_poSettings->value("ReadCRC", false).toBool();
    m_bWriteCRC = m_poSettings->value("WriteCRC", false).toBool();
    m_bRetryCRC = m_poSettings->value("RetryCRC", false).toBool();
    m_bRetryTimeout = m_poSettings->value("RetryTimeout", false).toBool();
    m_oSelectedImagePath = m_poSettings->value("SelectedImagePath").toString();
    m_oSelectedSerialID = m_poSettings->value("SelectedSerialID").toString();
    m_oSelectedBlueToothID = m_poSettings->value("SelectedBlueToothID").toString();

#ifdef Q_OS_ANDROID
    oFont.setPointSizeF(oFont.pointSizeF() * 0.6);
    m_eSelectedInterface = eInterfaceBluetooth;
#else
    oFont.setPointSizeF(oFont.pointSizeF() * 0.8);
    m_eSelectedInterface = (tdInterface) m_poSettings->value("SelectedInterface").toInt();

    group->setExclusive(true);
    group->addButton(m_poUI->USBButton);
    group->addButton(m_poUI->bluetoothButton);

    m_poUI->bluetoothButton->setChecked(m_eSelectedInterface == eInterfaceBluetooth);
    m_poUI->USBButton->setChecked(m_eSelectedInterface == eInterfaceSerial);
#endif
    m_poUI->logWidget->setFont(oFont);

    m_poUI->readCRC->setChecked(m_bReadCRC);
    m_poUI->writeCRC->setChecked(m_bWriteCRC);
    m_poUI->retryCRC->setChecked(m_bRetryCRC);
    m_poUI->retryTimeout->setChecked(m_bRetryTimeout);

    vSetInterface(m_eSelectedInterface);

    m_poUI->imagePathLineEdit->setText(m_oSelectedImagePath);
    m_poUI->addressLineEdit->setText(roSelectedID());

    vSetState(m_eConnectionState);

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
        vLog(eLogInfo, "Switched to USB mode");
        m_poInterface = new InterfaceSerialPort(this);
        break;

    case eInterfaceBluetooth:
        vLog(eLogInfo, "Switched to Bluetooth mode");
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

    vLog(eLogConnected, "Connected to " + m_poInterface->oGetName());
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
    vLog(_eLogType, _roMessage);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onDeviceDisconnected()
{
    m_poUnlockTimer->stop();

    vLog(eLogError, "Device disconnected");

    if(m_bLastButtonClickedIsConnect && m_bConnectedOnce)
    {
        vLog(eLogInfo, "Attempting reconnection...");

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
void MainWindow::vSendChecksum(const QByteArray &_roData)
{
    vSendData(crc16_xmodem(_roData));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSendData(const QByteArray &_roData)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	acDataToSend = QByteArray(3, 0xFF) + QByteArray(1, 0xF0) + _roData;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    vSetFrameColor(m_poUI->greenLightLabel, 0, 255, 0);
    m_poGreenLightOffTimer->start(m_poGreenLightOffTimer->remainingTime() + qMax(16, _roData.size() / 9));
    m_uiBytesSent += _roData.size();

    vUpdateLights();

    m_poInterface->vWrite(acDataToSend);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString MainWindow::oFormatSize(qint64 _uiSizeBytes)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QStringList aoUnits = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    qint64		ulSize = _uiSizeBytes;
    int			unitIndex = 0;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    while(ulSize >= 1024 && unitIndex < aoUnits.size() - 1)
    {
        ulSize = (ulSize + 512) / 1024;
        unitIndex++;
    }

    return QString::number(ulSize) + aoUnits[unitIndex];
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QByteArray MainWindow::acGetServerInfo()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QFileInfo	oInfo(*m_poImageFile);
    QString		oText;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    oText = QString::asprintf
        (
            "\r\nFile  : %s\r\nSize  : %s\r\nDate  : %s\r\nCRCs  : %s%s\r\n",
            qPrintable(oInfo.absoluteFilePath()),
            qPrintable(oFormatSize(oInfo.size())),
            qPrintable(oInfo.lastModified().toString(Qt::ISODate)),
            m_bReadCRC ? "R" : "",
            m_bWriteCRC ? "W" : ""
            );

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	acPayload = oText.toUtf8().left(510);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    vLog(eLogInfo, oText);

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    quint8	ucFlags = (m_bReadCRC ? FLAGBITS_READ_CRC : 0) | (m_bWriteCRC ? FLAGBITS_WRITE_CRC : 0) |
                     (m_bRetryTimeout ? FLAGBITS_RETRY_TIMEOUT : 0) | (m_bRetryCRC ? FLAGBITS_RETRY_CRC : 0);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    return QByteArray(1, ucFlags) + acPayload.leftJustified(511, '\0');
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vSaveSettings()
{
    m_poSettings->setValue("SelectedImagePath", m_oSelectedImagePath);
    m_poSettings->setValue("SelectedSerialID", m_oSelectedSerialID);
    m_poSettings->setValue("SelectedBlueToothID", m_oSelectedBlueToothID);
    m_poSettings->setValue("ReadCRC", m_bReadCRC);
    m_poSettings->setValue("WriteCRC", m_bWriteCRC);
    m_poSettings->setValue("RetryCRC", m_bRetryCRC);
    m_poSettings->setValue("RetryTimeout", m_bRetryTimeout);
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
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	ba = QByteArray(10, 0xAA);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    vSendData(ba);
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
void MainWindow::onCheckBoxStateChanged(int _iState)
{
    /*~~~~~~~~~~~~~~*/
    QObject *poSender;
    /*~~~~~~~~~~~~~~*/

    poSender = QObject::sender();

    if(poSender == m_poUI->readCRC)
        m_bReadCRC = _iState;
    else if(poSender == m_poUI->writeCRC)
        m_bWriteCRC = _iState;
    else if(poSender == m_poUI->retryCRC)
        m_bRetryCRC = _iState;
    else if(poSender == m_poUI->retryTimeout)
        m_bRetryTimeout = _iState;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vUpdateLights()
{
    m_poUI->redLightLabel->setText(QLocale().toString(m_uiBytesReceived) + "\n" + QLocale().toString(m_uiReceiveErrors));
    m_poUI->greenLightLabel->setText(QLocale().toString(m_uiBytesSent) + "\n" + QLocale().toString(m_uiSendErrors));
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

    if(poSender == m_poUI->unlockPushButton)
    {
        if (m_poUnlockTimer->isActive())
            m_poUnlockTimer->stop();
        else
            m_poUnlockTimer->start(10);
    }
    if(poSender == m_poUI->refreshPushButton)
    {
        m_poUI->namesListWidget->clear();
        m_poInterface->vScanDevices();
    }
    else if(poSender == m_poUI->clearPushButton)
    {
        m_uiBytesReceived = 0;
        m_uiBytesSent = 0;
        m_uiReceiveErrors = 0;
        m_uiSendErrors = 0;
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
        QString oImagePath = QFileDialog::getOpenFileName
            (
                nullptr,
                "Select Disk Image",
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                "Disk Images (*.dsk);;All Files (*)"
                );
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        m_poUI->imagePathLineEdit->setText(oImagePath);
    }
    else if(poSender == m_poUI->connectPushButton)
    {
        if(m_eConnectionState == eCStateDisconnected)
        {
            m_poImageFile = new QFile(m_oSelectedImagePath);
            if(m_poImageFile)
            {
                if(!QFile::exists(m_poImageFile->fileName()))
                {
                    m_poImageFile = nullptr;
                }
                else if(!m_poImageFile->open(QIODevice::ReadWrite))
                {
                    m_poImageFile = nullptr;
                }
            }

            if(m_poImageFile)
            {
                m_poUI->connectPushButton->setIcon(QIcon(":/icons/connecting.svg"));
                m_bLastButtonClickedIsConnect = true;
                vSetState(eCStateConnecting);
                m_poInterface->vConnectDevice(roSelectedID());
            }
            else
            {
                vLog(eLogError, "Failed to open image file " + m_oSelectedImagePath);
            }
        }
        else if((m_eConnectionState == eCStateConnected) || (m_eConnectionState == eCStateConnecting))
        {
            m_bLastButtonClickedIsConnect = false;
            m_bConnectedOnce = false;
            m_poInterface->vDisconnectDevice();
            vSetState(eCStateDisconnected);
        }
    }
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
        break;

    case eCStateConnected:
        m_poUI->connectPushButton->setIcon(QIcon(":/icons/connected.svg"));
        m_poUI->connectPushButton->setEnabled(true);
        break;

    case eCStateDisconnected:
        m_poUI->connectPushButton->setIcon(QIcon(":/icons/disconnected.svg"));
        m_poUI->connectPushButton->setEnabled(!m_oSelectedImagePath.isEmpty() && !roSelectedID().isEmpty());
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
    case eLogInfo:		oFormat.setForeground(QColor(0, 0, 0)); break;
    case eLogWarning:	oFormat.setForeground(QColor(192, 64, 64)); break;
    case eLogError:		oFormat.setBackground(QColor(255, 0, 0)); break;
    case eLogRead:		oFormat.setForeground(QColor(0, 192, 0)); break;
    case eLogWrite:		oFormat.setForeground(QColor(255, 128, 128)); break;
    case eLogConnected: oFormat.setForeground(QColor(128, 128, 255)); break;
    }

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QScrollBar	*scrollBar = m_poUI->logWidget->verticalScrollBar();
    bool		atBottom = (scrollBar->value() == scrollBar->maximum());
    QTextCursor oCursor(m_poUI->logWidget->document());
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    oCursor.movePosition(QTextCursor::End);
    oCursor.insertText(message + '\n', oFormat);

    if(atBottom)
    {
        scrollBar->setValue(scrollBar->maximum());
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::onTextChanged(QString _oText)
{
    /*~~~~~~~~~~~~~~*/
    QObject *poSender;
    /*~~~~~~~~~~~~~~*/

    poSender = QObject::sender();

    if(poSender == m_poUI->imagePathLineEdit)
    {
        m_oSelectedImagePath = _oText;
        vSetState(m_eConnectionState);
    }
    else if(poSender == m_poUI->addressLineEdit)
    {
        roSelectedID() = _oText;
        vSetState(m_eConnectionState);
    }
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

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QByteArray MainWindow::crc16_xmodem(const QByteArray &data)
{
    /*~~~~~~~~~~~~~~~~~*/
    quint16 crc = 0x0000;
    /*~~~~~~~~~~~~~~~~~*/

    for(unsigned char byte : data)
    {
        crc ^= static_cast<quint16>(byte) << 8;

        for(int i = 0; i < 8; ++i)
        {
            if(crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return QByteArray(1, (crc >> 0) & 0xFF) + QByteArray(1, (crc >> 8) & 0xFF);
}
