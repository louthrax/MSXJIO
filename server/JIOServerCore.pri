# Server without user interface (protocol, disk image, served directories, connection), shared by the graphical
# server (JIOServer.pro) and the command line server (JIOServerCLI.pro)

QT += core bluetooth serialport

INCLUDEPATH += $$PWD

# Generated files of each program in its own folders: the two programs can be built in the same folder
# (JIOServerAll.pro), their objects are compiled with different Qt modules
OBJECTS_DIR = obj_$$TARGET
MOC_DIR     = moc_$$TARGET
RCC_DIR     = rcc_$$TARGET
UI_DIR      = ui_$$TARGET

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
