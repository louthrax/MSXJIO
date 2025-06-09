#ifndef Port_h
#define Port_h

#include <QObject>

#include "Common.h"

class Interface : public QObject
{
    Q_OBJECT

public:
    explicit Interface(QObject *parent = nullptr) : QObject(parent) {}
    virtual void        vScanDevices() = 0;
    virtual void        vConnectDevice(const QString &_roID) = 0;
    virtual void        vWrite(QByteArray & racData) = 0;
    virtual QByteArray  acReadAll() = 0;
    virtual QString     oGetName() = 0;
    virtual void        vDisconnectDevice() = 0;
    virtual qint64      uiBytesAvailable() = 0;

signals:
    void deviceDiscovered(const QString & _roName, const QString & _roID);
    void deviceConnected();
    void deviceReadyRead();
    void deviceDisconnected();
    void log(tdLogType _eLogType, const QString & _roMessage);
};

#endif
