#include <QSerialPortInfo>
#include <QBluetoothPermission>
#include <QCoreApplication>

#include "InterfaceBluetoothSocket.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
InterfaceBluetoothSocket::InterfaceBluetoothSocket(QObject *parent) :
    Interface(parent)
{
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
InterfaceBluetoothSocket::~InterfaceBluetoothSocket()
{
    vDisconnectDevice2();
    delete m_poDiscoveryAgent;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::onDeviceDiscovered(const QBluetoothDeviceInfo &_roInfo)
{
    emit deviceDiscovered(_roInfo.name(), _roInfo.address().toString());
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::vWrite(QByteArray &_racData)
{
    if(m_poBluetoothSocket && m_poBluetoothSocket->isOpen())
	{
		m_poBluetoothSocket->write(_racData);
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::vConnectDevice(const QString &_roID)
{
    vDisconnectDevice();
    delete m_poBluetoothSocket;

    m_poBluetoothSocket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
    connect(m_poBluetoothSocket, &QBluetoothSocket::connected, this, &InterfaceBluetoothSocket::onConnected);
    connect(m_poBluetoothSocket, &QBluetoothSocket::readyRead, this, &InterfaceBluetoothSocket::onReadyRead);
    connect(m_poBluetoothSocket, &QBluetoothSocket::disconnected, this, &InterfaceBluetoothSocket::onDisconnected);
    connect(m_poBluetoothSocket, &QBluetoothSocket::errorOccurred, this, &InterfaceBluetoothSocket::onError);
    m_poBluetoothSocket->connectToService
        (
            QBluetoothAddress(_roID),
            QBluetoothUuid(QStringLiteral("00001101-0000-1000-8000-00805F9B34FB"))
        );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::vDisconnectDevice()
{
    vDisconnectDevice2();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::vDisconnectDevice2()
{
    if (m_poBluetoothSocket)
    {
        if (m_poBluetoothSocket->isOpen())
        {
            m_poBluetoothSocket->close();
            m_poBluetoothSocket->disconnectFromService();
            delete m_poBluetoothSocket;
            m_poBluetoothSocket = nullptr;
        }
    }
#ifdef ANDROID
    if (m_bConnected)
    {
        m_bConnected = false;
        emit deviceDisconnected();
    }
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QByteArray InterfaceBluetoothSocket::acReadAll()
{
    return (m_poBluetoothSocket && m_poBluetoothSocket->isOpen()) ? m_poBluetoothSocket->readAll() : QByteArray();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::onReadyRead()
{
    emit deviceReadyRead();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::onConnected()
{
#ifdef Q_OS_ANDROID
    m_bConnected = true;
#endif
    emit deviceConnected();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::onDisconnected()
{
    emit deviceDisconnected();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::onError(QBluetoothSocket::SocketError _eError)
{
    const char * szErrorType;
    QString oError;

    switch (_eError)
    {
    case QBluetoothSocket::SocketError::NoSocketError: return;
        case QBluetoothSocket::SocketError::UnknownSocketError: szErrorType = "UnknownSocketError"; break;
        case QBluetoothSocket::SocketError::RemoteHostClosedError: szErrorType = "RemoteHostClosedError"; break;
        case QBluetoothSocket::SocketError::HostNotFoundError: szErrorType = "HostNotFoundError"; break;
        case QBluetoothSocket::SocketError::ServiceNotFoundError: szErrorType = "ServiceNotFoundError"; break;
        case QBluetoothSocket::SocketError::NetworkError: szErrorType = "NetworkError"; break;
        case QBluetoothSocket::SocketError::UnsupportedProtocolError: szErrorType = "UnsupportedProtocolError"; break;
        case QBluetoothSocket::SocketError::OperationError: szErrorType = "OperationError"; break;
        case QBluetoothSocket::SocketError::MissingPermissionsError: szErrorType = "MissingPermissionsError"; break;
        default: szErrorType = ""; break;
    }

    oError = "Bluetooth socket error (" + QString(szErrorType) + ") ";

    if (m_poBluetoothSocket)
        oError += m_poBluetoothSocket->errorString();

    emit log(eLogError, oError);

#ifdef Q_OS_ANDROID
    if (_eError == QBluetoothSocket::SocketError::ServiceNotFoundError)
        emit deviceDisconnected();
#endif
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::vScanDevices()
{
    if (m_poDiscoveryAgent)
    {
        delete m_poDiscoveryAgent;
        m_poDiscoveryAgent = nullptr;
        emit log(eLogInfo, "Bluetooth discovery canceled");
    }

    m_poDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect
        (
            m_poDiscoveryAgent,
            &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this,
            &InterfaceBluetoothSocket::onDeviceDiscovered
            );
    connect(m_poDiscoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished, this, &InterfaceBluetoothSocket::onDiscoveryFinished);

#ifdef Q_OS_ANDROID
    vRequestAndroidPermissionsAndStartDiscovery();
#else
    m_poDiscoveryAgent->start();
#endif

    emit log(eLogInfo, "Bluetooth discovery started...");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceBluetoothSocket::onDiscoveryFinished()
{
    if (m_poDiscoveryAgent)
    {
        delete m_poDiscoveryAgent;
        m_poDiscoveryAgent = nullptr;
        emit log(eLogInfo, "Bluetooth discovery finished");
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString InterfaceBluetoothSocket::oGetName()
{
    if (!m_poBluetoothSocket)
        return "";

    QString name = m_poBluetoothSocket->peerName();
    QString address = m_poBluetoothSocket->peerAddress().toString();
    QString type = m_poBluetoothSocket->socketType() == QBluetoothServiceInfo::RfcommProtocol
                       ? "RFCOMM"
                       : "Unknown";

    return QString::asprintf(
        "%s\n"
        "  Device Address: %s\n"
        "  Socket Type: %s\n"
        "  Local Address: %s\n"
        "  Local Port: %d\n"
        "  Remote Port: %d",
        qPrintable(name.isEmpty() ? "<unknown>" : name),
        qPrintable(address),
        qPrintable(type),
        qPrintable(m_poBluetoothSocket->localAddress().toString()),
        m_poBluetoothSocket->localPort(),
        m_poBluetoothSocket->peerPort()
        );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
qint64 InterfaceBluetoothSocket::uiBytesAvailable()
{
    return m_poBluetoothSocket->bytesAvailable();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
#ifdef Q_OS_ANDROID

/*$off*/
void InterfaceBluetoothSocket::vRequestAndroidPermissionsAndStartDiscovery()
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QBluetoothPermission	oBluetoohPermission;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    qApp->requestPermission(
        oBluetoohPermission,
        this,
        [this] (const QPermission &perm)
        {
            if(perm.status() != Qt::PermissionStatus::Granted)
            {
                qCritical() << "❌ Bluetooth permission denied!"; QCoreApplication::exit(1); return;
            }

            QLocationPermission locPerm; qApp->requestPermission
                (
                    locPerm,
                    this,
                    [this] (const QPermission &locPermResult)
                    {
                        if(locPermResult.status() != Qt::PermissionStatus::Granted)
                        {
                            qCritical() << "❌ Location permission denied!"; QCoreApplication::exit(1); return;
                        }
                        m_poDiscoveryAgent->start();
                    }
                    );
        }
        );
}
/*$on*/

#endif
