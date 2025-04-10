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
	quint32 m_uiSector : 24;
	quint8	m_ucLength;
	quint16 m_uiAddress;
} tdReadWriteHeader;
#pragma pack(pop)
static_assert(sizeof(tdReadWriteHeader) == 6, "tdReadWriteHeader must be 7 bytes");
std::coroutine_handle<> ByteReader::	m_soHandle = nullptr;

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
	bool			_bLast
)
{
    if(_ucFlags & FLAGBITS_RX_CRC)
	{
		_uiCRC = uiXModemCRC16(_pvAddress, _uiLength, _uiCRC);
	}

    vTransmitData(QByteArray((const char *) _pvAddress, _uiLength));

    if(_bLast && (_ucFlags & FLAGBITS_RX_CRC))
	{
        vTransmitData(QByteArray((const char *) &_uiCRC, sizeof(_uiCRC)));
	}

	return _uiCRC;
}

#define vReceive(_pvAddress, _uiSize, _ucFlags, _uiCRC) \
	{ \
		*(_pvAddress) = *(typeof(_pvAddress)) (((QByteArray) co_await oRead(_uiSize)).constData()); \
        if(_ucFlags & FLAGBITS_TX_CRC) \
		{ \
			_uiCRC = uiXModemCRC16(_pvAddress, _uiSize, _uiCRC); \
		} \
	}

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
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        quint8				ucCommandAndFlags;
		tdReadWriteHeader	oHeader;
		size_t				iSigPos;
		quint16				uiCRC;
		bool				bCRCOK;
		quint16				uiReceivedCRC;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		iSigPos = 0;
		uiCRC = 0;

		while(iSigPos < sigLength)
		{
            vReceive(&ucCommandAndFlags, sizeof(ucCommandAndFlags), FLAGBITS_TX_CRC, uiCRC);

            if(ucCommandAndFlags == signature[iSigPos])
			{
				iSigPos++;
			}
            else if(ucCommandAndFlags == signature[0])
			{
				iSigPos = 1;
			}
			else
			{
				iSigPos = 0;
				uiCRC = 0;
			}
		}

        vReceive(&ucCommandAndFlags, sizeof(ucCommandAndFlags), FLAGBITS_TX_CRC, uiCRC);

        switch(ucCommandAndFlags & 0x0F)
		{
		case COMMAND_INFO:
			{
				bCRCOK = true;
                if(ucCommandAndFlags & FLAGBITS_TX_CRC)
				{
					vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
                    bCRCOK = uiReceivedCRC == uiCRC;
				}

				if(bCRCOK)
				{
					/*~~~~~~~~~~~~~~~~~~*/
					QByteArray	oInfoData;
					/*~~~~~~~~~~~~~~~~~~*/

					oInfoData = acGetServerInfo();
                    uiTransmit(oInfoData.constData(), oInfoData.size(), ucCommandAndFlags, 0, true);
				}
                else
                {
                    vLog(eLogError, "Transmission error !");
                    m_uiTransmitErrors++;
                    vUpdateLights();
                }
			}
			break;

		case COMMAND_READ:
			{
                vReceive(&oHeader, sizeof(oHeader), ucCommandAndFlags, uiCRC);

				bCRCOK = true;
                if(ucCommandAndFlags & FLAGBITS_TX_CRC)
				{
					vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
                    bCRCOK = uiReceivedCRC == uiCRC;
				}

				if(bCRCOK)
				{
					/*~~~~~~~~~~~~~~~~~~*/
					QByteArray	oFileData;
					/*~~~~~~~~~~~~~~~~~~*/

					vLog
					(
						eLogRead,
						"Read%c  %2d sector(s) at %10d to   0x%04X",
                        ucCommandAndFlags & FLAGBITS_RX_CRC ? 'C' : ' ',
						oHeader.m_ucLength,
						oHeader.m_uiSector,
						oHeader.m_uiAddress
					);
					m_poImageFile->seek (static_cast<quint64>(oHeader.m_uiSector) *512);
					oFileData = m_poImageFile->read(oHeader.m_ucLength * 512);

                    uiTransmit(oFileData.constData(), oFileData.size(), ucCommandAndFlags, 0, true);
				}
                else
                {
                    vLog(eLogError, "Transmission error !");
                    m_uiTransmitErrors++;
                    vUpdateLights();
                }
			}
			break;

		case COMMAND_WRITE:
			{
				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
				quint16 uiAcknowledgeOK = 0x1111;
				quint16 uiAcknowledgeBAD = 0x2222;
				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                vReceive(&oHeader, sizeof(oHeader), ucCommandAndFlags, uiCRC);

				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
				QByteArray	oData(oHeader.m_ucLength * 512, Qt::Uninitialized);
				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

                vReceive(oData.data(), oData.size(), ucCommandAndFlags, uiCRC);

				bCRCOK = true;
                if(ucCommandAndFlags & FLAGBITS_TX_CRC)
				{
                    vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), ucCommandAndFlags, uiCRC);
                    bCRCOK = uiReceivedCRC == uiCRC;
				}

				if(bCRCOK)
				{
                    if(ucCommandAndFlags & FLAGBITS_TX_CRC)
                    {
                        uiTransmit(&uiAcknowledgeOK, sizeof(uiAcknowledgeOK), 0, 0, false);
                    }

					vLog
					(
						eLogWrite,
						"Write%c %2d sector(s) at %10d from 0x%04X",
                        ucCommandAndFlags & FLAGBITS_TX_CRC ? 'C' : ' ',
						oHeader.m_ucLength,
						oHeader.m_uiSector,
						oHeader.m_uiAddress
					);

					m_poImageFile->seek (static_cast<quint64>(oHeader.m_uiSector) *512);
					m_poImageFile->write(oData);
					m_poImageFile->flush();
				}
				else
				{
                    vLog(eLogError, "Transmission error !");
                    m_uiTransmitErrors++;
                    vUpdateLights();
                    uiTransmit(&uiAcknowledgeBAD, sizeof(uiAcknowledgeBAD), 0, 0, false);
				}
			}
			break;

		case COMMAND_REPORT_BAD_RX_CRC:
			vLog(eLogError, "Reception error !");
            m_uiReceiveErrors++;
			vUpdateLights();
			break;

		case COMMAND_REPORT_BAD_TX_CRC:
			vLog(eLogError, "Transmission error !");
            m_uiTransmitErrors++;
			vUpdateLights();
			break;

		case COMMAND_REPORT_BAD_ACKNOWLEDGE:
			vLog(eLogError, "Acknowledge error !");
            m_uiReceiveErrors++;
			vUpdateLights();
			break;

		case COMMAND_REPORT_TIMEOUT:
			vLog(eLogError, "Timeout error !");
            m_uiReceiveErrors++;
			vUpdateLights();
			break;

		default:
            vLog(eLogError, "Unknown command: %c", ucCommandAndFlags);
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

	m_bRxCRC = m_poSettings->value("RxCRC", false).toBool();
	m_bTxCRC = m_poSettings->value("TxCRC", false).toBool();
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

	m_poUI->readCRC->setChecked(m_bRxCRC);
	m_poUI->writeCRC->setChecked(m_bTxCRC);
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
void MainWindow::vTransmitData(const QByteArray &_roData)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QByteArray	acDataToTransmit = QByteArray(3, 0xFF) + QByteArray(1, 0xF0) + _roData;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
			m_bRxCRC ? "Rx" : "",
			m_bTxCRC ? "Tx" : ""
		);

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QByteArray	acPayload = oText.toUtf8().left(510);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	vLog(eLogInfo, oText);

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	quint8	ucFlags = (m_bRxCRC ? FLAGBITS_RX_CRC : 0) | (m_bTxCRC ? FLAGBITS_TX_CRC : 0) |
		(m_bRetryTimeout ? FLAGBITS_RETRY_TIMEOUT : 0) | (m_bRetryCRC ? FLAGBITS_RETRY_CRC : 0);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
	m_poSettings->setValue("RxCRC", m_bRxCRC);
	m_poSettings->setValue("TxCRC", m_bTxCRC);
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

    vTransmitData(ba);
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
void MainWindow::onButtonClicked()
{
	/*~~~~~~~~~~~~~~*/
	QObject *poSender;
	/*~~~~~~~~~~~~~~*/

	poSender = QObject::sender();

	if(poSender == m_poUI->readCRC)
		m_bRxCRC = ((QPushButton *) poSender)->isChecked();
	else if(poSender == m_poUI->writeCRC)
		m_bTxCRC = ((QPushButton *) poSender)->isChecked();
	else if(poSender == m_poUI->retryCRC)
		m_bRetryCRC = ((QPushButton *) poSender)->isChecked();
	else if(poSender == m_poUI->retryTimeout)
		m_bRetryTimeout = ((QPushButton *) poSender)->isChecked();
	else if(poSender == m_poUI->unlockPushButton)
	{
		if(m_poUnlockTimer->isActive())
			m_poUnlockTimer->stop();
		else
			m_poUnlockTimer->start(10);
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
