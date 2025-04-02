#ifndef BluetoothPort_h
#define BluetoothPort_h

#include <QBluetoothSocket>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothAddress>

#include "Interface.h"

class InterfaceBluetoothSocket : public Interface
{
    Q_OBJECT

public:
    InterfaceBluetoothSocket(QObject *parent = nullptr);
    ~InterfaceBluetoothSocket();
    void        vScanDevices();
    void        vConnectDevice(const QString &_roID);
    void        vWrite(QByteArray & racData);
    QByteArray  acReadAll();
    QString     oGetName();
    void        vDisconnectDevice();
    void        vDisconnectDevice2();
    qint64      uiBytesAvailable();

public slots:
    void onDeviceDiscovered (const QBluetoothDeviceInfo &_roInfo);
    void onDiscoveryFinished();
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QBluetoothSocket::SocketError _eError);

private:
    QBluetoothSocket                *m_poBluetoothSocket = nullptr;
    QBluetoothDeviceDiscoveryAgent	*m_poDiscoveryAgent = nullptr;

#ifdef Q_OS_ANDROID
    bool                            m_bConnected = false;
    void vRequestAndroidPermissionsAndStartDiscovery();
#endif
};

#endif
