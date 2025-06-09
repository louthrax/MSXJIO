#ifndef SerialPort_h
#define SerialPort_h

#include <QSerialPort>



#include "Interface.h"

class InterfaceSerialPort : public Interface
{
    Q_OBJECT

public:
    InterfaceSerialPort(QObject *parent = nullptr);
    ~InterfaceSerialPort();
    void        vScanDevices();
    void        vConnectDevice(const QString &_roID);
    void        vWrite(QByteArray & racData);
    QByteArray  acReadAll();
    QString     oGetName();
    void        vDisconnectDevice();
    void        vDisconnectDevice2();
    qint64      uiBytesAvailable();

public slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_poSerialPort = nullptr;
};

#endif
