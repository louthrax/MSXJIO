# Server without user interface (protocol, disk image, served directories, connection), shared by the graphical
# server (JIOServer.pro) and the command line server (cli/JIOServerCLI.pro)

QT += core bluetooth serialport

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/ByteReader.h \
    $$PWD/Common.h \
    $$PWD/Drive.h \
    $$PWD/Find.h \
    $$PWD/Interface.h \
    $$PWD/InterfaceBluetoothSocket.h \
    $$PWD/InterfaceSerialPort.h \
    $$PWD/Pack.h \
    $$PWD/PartitionExtractor.h \
    $$PWD/Server.h

SOURCES += \
    $$PWD/Drive.cpp \
    $$PWD/Find.cpp \
    $$PWD/InterfaceBluetoothSocket.cpp \
    $$PWD/InterfaceSerialPort.cpp \
    $$PWD/PartitionExtractor.cpp \
    $$PWD/Server.cpp \
    $$PWD/Server_BDOS.cpp
