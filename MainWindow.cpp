#include <QTimer>
#include <QStandardPaths>
#include <QFileDialog>
#include <QScrollBar>
#include <QButtonGroup>
#include <QThread>
#include <QPixmap>

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "InterfaceSerialPort.h"
#include "InterfaceBluetoothSocket.h"

#include "MSXClient/flags.inc"

#pragma pack(push, 1)
typedef struct
{
	quint32 m_uiSector;
	quint8	m_ucLength;
	quint16 m_uiAddress;
} tdReadWriteHeader;
#pragma pack(pop)
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
    Can't be a method because of co_await...
 =======================================================================================================================
 */
#define vReceive(_pvAddress, _uiSize, _ucFlags, _uiCRC) \
	{ \
		memcpy(_pvAddress, (((QByteArray) co_await oRead(_uiSize)).constData()), _uiSize); \
		if(_ucFlags & FLAG_TX_CRC) \
		{ \
			_uiCRC = uiXModemCRC16(_pvAddress, _uiSize, _uiCRC); \
		} \
	}

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
	if(m_bReadOnly | m_oDrive.bIsMediaWriteProtected()) oFlags += "ReadOnly";

	oText = QString::asprintf
		(
			"\r\nDrive :\r\n\%s\r\nDate  : %s\r\nFlags : %s\r\nFile  : %s\r\n",
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
Task MainWindow::oParser()
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	const char		*szSignature = "JIO";
	const size_t	uiSignaturegLength = strlen(szSignature);
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

		while(iSigPos < uiSignaturegLength)
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
		case COMMAND_DRIVE_INFO:
			{
				bCRCOK = true;
				if(ucFlags & FLAG_TX_CRC)
				{
					vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
					bCRCOK = uiReceivedCRC == uiCRC;
				}

				if(bCRCOK)
				{
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
					QByteArray	oInfoData;
					quint8		W_FLAGS = (m_bRxCRC ? FLAG_RX_CRC : 0) | (m_bTxCRC ? FLAG_TX_CRC : 0) |
						(m_bTimeout ? FLAG_TIMEOUT : 0) | (m_bAutoRetry ? FLAG_AUTO_RETRY : 0);
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
					vLog(eLogError, "Transmission error !");
					m_uiTransmitErrors++;
					vUpdateLights();
				}
			}
			break;

		case COMMAND_DRIVE_READ:
			{
				vReceive(&oHeader, sizeof(oHeader), ucFlags, uiCRC);

				bCRCOK = true;
				if(ucFlags & FLAG_TX_CRC)
				{
					vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
					bCRCOK = uiReceivedCRC == uiCRC;
				}

				if(bCRCOK)
				{
					/*~~~~~~~~~~~~~~~~~~~~~~~~*/
					QByteArray		oFileData;
					unsigned char	ucPartition;
					unsigned int	uiSector;
					/*~~~~~~~~~~~~~~~~~~~~~~~~*/

					ucPartition = oHeader.m_uiSector >> 24;

					uiSector = oHeader.m_uiSector & 0xFFFFFF;

					if(uiSector & 0x800000) uiSector &= 0xFFFF;

					vLog
					(
						eLogRead,
						"Read%s  %2d sec. at P%c: %10d to   0x%04X",
						ucFlags & FLAG_RX_CRC ? "✓" : " ",
						oHeader.m_ucLength,
						ucPartition + '0',
						uiSector,
						oHeader.m_uiAddress
					);

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
						vLog(eLogError, "Error reading from file !");
					}
				}
				else
				{
					vLog(eLogError, "Transmission error !");
					m_uiTransmitErrors++;
					vUpdateLights();
				}
			}
			break;

		case COMMAND_DRIVE_WRITE:
			{
				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
				char	*acData;
				quint16 uiAcknowledge = DRIVE_ACKNOWLEDGE_WRITE_OK;
				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

				vReceive(&oHeader, sizeof(oHeader), ucFlags, uiCRC);

				acData = (char *) malloc(oHeader.m_ucLength * 512);

				vReceive(acData, oHeader.m_ucLength * 512, ucFlags, uiCRC);

				bCRCOK = true;
				if(ucFlags & FLAG_TX_CRC)
				{
					vReceive(&uiReceivedCRC, sizeof(uiReceivedCRC), 0, uiCRC);
					bCRCOK = uiReceivedCRC == uiCRC;
				}

				if(bCRCOK)
				{
					/*~~~~~~~~~~~~~~~~~~~~~~~~*/
					unsigned char	ucPartition;
					unsigned int	uiSector;
					/*~~~~~~~~~~~~~~~~~~~~~~~~*/

					ucPartition = oHeader.m_uiSector >> 24;

					uiSector = oHeader.m_uiSector & 0xFFFFFF;

					if(uiSector & 0x800000) uiSector &= 0xFFFF;

					vLog
					(
						eLogWrite,
						"Write%s %2d sec. at P%c: %10d from 0x%04X",
						ucFlags & FLAG_RX_CRC ? "✓" : " ",
						oHeader.m_ucLength,
						ucPartition + '0',
						uiSector,
						oHeader.m_uiAddress
					);

					if(m_bReadOnly)
					{
						uiAcknowledge = DRIVE_ACKNOWLEDGE_WRITE_PROTECTED;
					}
					else
					{
						switch(m_oDrive.eWriteSectors(ucPartition, uiSector, oHeader.m_ucLength, acData))
						{
						case eDriveErrorOK:
							break;

						case eDriveErrorNoMedia:
							vLog(eLogError, "No media !");
							uiAcknowledge = DRIVE_ACKNOWLEDGE_WRITE_FAILED;
							break;

						case eDriveErrorReadError:
						case eDriveErrorWriteError:
							vLog(eLogError, "Error writing to file !");
							uiAcknowledge = DRIVE_ACKNOWLEDGE_WRITE_FAILED;
							break;

						case eDriveErrorWriteProtected:
							vLog(eLogError, "Media write-protected !");
							uiAcknowledge = DRIVE_ACKNOWLEDGE_WRITE_PROTECTED;
							break;
						}
					}
				}
				else
				{
					uiAcknowledge = DRIVE_ACKNOWLEDGE_WRITE_FAILED;
					vLog(eLogError, "Transmission error !");
					m_uiTransmitErrors++;
					vUpdateLights();
				}

				uiTransmit(&uiAcknowledge, sizeof(uiAcknowledge), 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
				free(acData);
			}
			break;

		case COMMAND_DRIVE_REPORT_CRC_ERROR:
			vLog(eLogError, "CRC error !");
			m_uiReceiveErrors++;
			vUpdateLights();
			break;

		case COMMAND_DRIVE_REPORT_WRITE_FAULT:
			vLog(eLogError, "Write fault error !");
			m_uiTransmitErrors++;
			vUpdateLights();
			break;

		case COMMAND_DRIVE_REPORT_DRIVE_NOT_READY:
			vLog(eLogError, "Timeout error !");
			m_uiReceiveErrors++;
			vUpdateLights();
			break;

		case COMMAND_DRIVE_REPORT_WRITE_PROTECTED:
			vLog(eLogError, "Write protected error !");
			break;

		default:
			vLog(eLogError, "Unknown command: %d", ucCommand);
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

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QButtonGroup	*poGroup = new QButtonGroup(this);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	m_bRxCRC = m_poSettings->value("RxCRC", true).toBool();
	m_bTxCRC = m_poSettings->value("TxCRC", true).toBool();
	m_bAutoRetry = m_poSettings->value("AutoRetry", true).toBool();
	m_bTimeout = m_poSettings->value("Timeout", false).toBool();
	m_bReadOnly = m_poSettings->value("ReadOnly", false).toBool();

	m_oSelectedSerialID = m_poSettings->value("SelectedSerialID").toString();
	m_oSelectedBlueToothID = m_poSettings->value("SelectedBlueToothID").toString();
	m_poUI->imagePathLineEdit->setText(m_poSettings->value("SelectedImagePath").toString());
	m_oDrive.bInsertMedia(m_poSettings->value("SelectedImagePath").toString());

#ifdef Q_OS_ANDROID
	oFont.setPointSizeF(oFont.pointSizeF() * 0.6);
	m_eSelectedInterface = eInterfaceBluetooth;
#else
	oFont.setPointSizeF(oFont.pointSizeF() * 0.8);
	m_eSelectedInterface = (tdInterface) m_poSettings->value("SelectedInterface").toInt();

	poGroup->setExclusive(true);
	poGroup->addButton(m_poUI->USBButton);
	poGroup->addButton(m_poUI->bluetoothButton);

	m_poUI->bluetoothButton->setChecked(m_eSelectedInterface == eInterfaceBluetooth);
	m_poUI->USBButton->setChecked(m_eSelectedInterface == eInterfaceSerial);
#endif
	m_poUI->logWidget->setFont(oFont);

	m_poUI->RxCRC->setChecked(m_bRxCRC);
	m_poUI->TxCRC->setChecked(m_bTxCRC);
	m_poUI->autoRetry->setChecked(m_bAutoRetry);
	m_poUI->timeout->setChecked(m_bTimeout);
	m_poUI->readOnly->setChecked(m_bReadOnly);

	vSetInterface(m_eSelectedInterface);

	m_poUI->addressLineEdit->setText(roSelectedID());

	vSetState(m_eConnectionState);

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
void MainWindow::vTransmitData(const QByteArray &_roData, int _iDelay)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QByteArray	acDataToTransmit = QByteArray(_iDelay, 0xFF) + QByteArray(1, 0xF0) + _roData;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
	m_poSettings->setValue("SelectedImagePath", m_oDrive.oMediaPath());
	m_poSettings->setValue("SelectedSerialID", m_oSelectedSerialID);
	m_poSettings->setValue("SelectedBlueToothID", m_oSelectedBlueToothID);
	m_poSettings->setValue("RxCRC", m_bRxCRC);
	m_poSettings->setValue("TxCRC", m_bTxCRC);
	m_poSettings->setValue("AutoRetry", m_bAutoRetry);
	m_poSettings->setValue("Timeout", m_bTimeout);
	m_poSettings->setValue("ReadOnly", m_bReadOnly);
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
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		QString lastFilePath = m_poUI->imagePathLineEdit->text();
		QString initialDir;
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		if(!lastFilePath.isEmpty() && QFileInfo::exists(lastFilePath))
		{
			initialDir = QFileInfo(lastFilePath).absolutePath();
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
		}
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
		m_oDrive.vEjectMedia();
		m_poUI->imagePathLineEdit->setText("");
		m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/empty.svg"));
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
		if(m_oDrive.bInsertMedia(m_poUI->imagePathLineEdit->text()))
		{
			vLog(eLogInfo, "Media opened successfully.");
			vLog(eLogInfo, szGetServerInfo());
		}

		switch(m_oDrive.eMediaType())
		{
		case eMediaEmpty:		m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/empty.svg")); break;
		case eMediaFloppy:		m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/floppy.svg")); break;
		case eMediaHardDisk:	m_poUI->iconMediaType->setPixmap(QPixmap(":/icons/hardDisk.svg")); break;
		}
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
