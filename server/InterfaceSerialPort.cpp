#include <QSerialPortInfo>
#include <QDebug>
#include <QSerialPortInfo>

#include "InterfaceSerialPort.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
InterfaceSerialPort::InterfaceSerialPort(QObject *parent) :
    Interface(parent)
{
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
InterfaceSerialPort::~InterfaceSerialPort()
{
    vDisconnectDevice2();
    delete m_poSerialPort;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceSerialPort::onReadyRead()
{
    emit deviceReadyRead();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceSerialPort::onError(QSerialPort::SerialPortError _eError)
{
    const char * szErrorType;
    QString oError;

    switch (_eError)
    {
        case QSerialPort::SerialPortError::NoError: return;
        case QSerialPort::SerialPortError::DeviceNotFoundError: szErrorType = "DeviceNotFoundError"; break;
        case QSerialPort::SerialPortError::PermissionError: szErrorType = "PermissionError"; break;
        case QSerialPort::SerialPortError::OpenError: szErrorType = "OpenError"; break;
        case QSerialPort::SerialPortError::WriteError: szErrorType = "WriteError"; break;
        case QSerialPort::SerialPortError::ReadError: szErrorType = "ReadError"; break;
        case QSerialPort::SerialPortError::ResourceError: szErrorType = "ResourceError"; break;
        case QSerialPort::SerialPortError::UnsupportedOperationError: szErrorType = "UnsupportedOperationError"; break;
        case QSerialPort::SerialPortError::UnknownError: szErrorType = "UnknownError"; break;
        case QSerialPort::SerialPortError::TimeoutError: szErrorType = "TimeoutError"; break;
        case QSerialPort::SerialPortError::NotOpenError: szErrorType = "NotOpenError"; break;
        default: szErrorType = ""; break;
    }

    oError = "Serial port error (" + QString(szErrorType) + ") ";

    if (m_poSerialPort)
        oError += m_poSerialPort->errorString();

    emit log(eLogError, oError);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceSerialPort::vScanDevices()
{
    const auto infos = QSerialPortInfo::availablePorts();

    emit log(eLogInfo, "USB devices list updated");

    for(const QSerialPortInfo &info : infos)
    {
        emit deviceDiscovered(info.portName(), info.portName());
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceSerialPort::vWrite(QByteArray & _racData)
{
    if (m_poSerialPort && m_poSerialPort->isOpen())
    {
        m_poSerialPort->write(_racData);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceSerialPort::vConnectDevice(const QString &_roID)
{
    vDisconnectDevice2();

    m_poSerialPort = new QSerialPort(this);
    connect(m_poSerialPort, &QSerialPort::readyRead, this, &InterfaceSerialPort::onReadyRead);
    connect(m_poSerialPort, &QSerialPort::errorOccurred, this, &InterfaceSerialPort::onError);

    m_poSerialPort->setPortName(_roID);
    m_poSerialPort->setBaudRate(QSerialPort::Baud115200);
    m_poSerialPort->setDataBits(QSerialPort::Data8);
    m_poSerialPort->setParity(QSerialPort::NoParity);
    m_poSerialPort->setStopBits(QSerialPort::OneStop);
    m_poSerialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_poSerialPort->open(QIODevice::ReadWrite))
        emit deviceConnected();
    else
    {
        delete m_poSerialPort;
        m_poSerialPort = nullptr;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceSerialPort::vDisconnectDevice()
{
    vDisconnectDevice2();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void InterfaceSerialPort::vDisconnectDevice2()
{
    if (m_poSerialPort)
    {
        if (m_poSerialPort->isOpen())
        {
            m_poSerialPort->flush();
            m_poSerialPort->close();
        }

        delete m_poSerialPort;
        m_poSerialPort = nullptr;

    }

    emit deviceDisconnected();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QByteArray InterfaceSerialPort::acReadAll()
{
    return (m_poSerialPort &&m_poSerialPort->isOpen()) ? m_poSerialPort->readAll() : QByteArray();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString InterfaceSerialPort::oGetName()
{
    if (!m_poSerialPort)
        return "";

    QSerialPortInfo info(*m_poSerialPort);

    return QString::asprintf(
        "%s\n"
        "  Description: %s\n"
        "  Manufacturer: %s\n"
        "  Serial Number: %s\n"
        "  Vendor ID: 0x%04X\n"
        "  Product ID: 0x%04X\n"
        "  System Location: %s",
        qPrintable(info.portName()),
        qPrintable(info.description()),
        qPrintable(info.manufacturer()),
        qPrintable(info.serialNumber()),
        info.hasVendorIdentifier() ? info.vendorIdentifier() : 0,
        info.hasProductIdentifier() ? info.productIdentifier() : 0,
        qPrintable(info.systemLocation())
        );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
qint64 InterfaceSerialPort::uiBytesAvailable()
{
    return m_poSerialPort->bytesAvailable();
}

