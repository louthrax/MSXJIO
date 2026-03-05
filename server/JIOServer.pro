QT += core gui widgets bluetooth serialport

CONFIG += c++20

MAKEFILE = Makefile

ICON_SVG = $$PWD/$${TARGET}.svg

unix {
    BUILD_DATE    = $$system(date +'%Y-%m-%d_%H:%M:%S')
    BUILD_HASH    = $$system(git config --global --add safe.directory "$$PWD" && git rev-parse HEAD)
    BUILD_VERSION = $$system(cat Version.txt)

    !macx:!android {
        QMAKE_CXXFLAGS += -fcoroutines
    }
}

macx {
    ICON=$$PWD/$${TARGET}.icns
    ICON_RELATIVE=$$replace(ICON, /Users/laurent, ..)

    icns_from_svg.target   = $$ICON_RELATIVE
    icns_from_svg.depends  = $$ICON_SVG
    icns_from_svg.commands = $$PWD/svg2icns.sh $$ICON_SVG $$ICON_RELATIVE

    QMAKE_EXTRA_TARGETS += icns_from_svg
}

win32 {
    BUILD_DATE    = $$system(powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd_HH:mm:ss'")
    BUILD_HASH    = $$system(powershell -NoProfile -Command "git -C '$$PWD' rev-parse HEAD")
    BUILD_VERSION = $$system(powershell -NoProfile -Command "(Get-Content '$$PWD\\Version.txt' -Raw).Trim()")

    ICON_ICO = $$OUT_PWD/$${TARGET}.ico
    RC_ICONS += $$ICON_ICO

    ico_from_svg.target   = $$ICON_ICO
    ico_from_svg.depends  = $$ICON_SVG
    ico_from_svg.commands = magick -background none $$ICON_SVG \
        -define icon:auto-resize=16,24,32,48,64,128,256 \
        $$ICON_ICO

    QMAKE_EXTRA_TARGETS += ico_from_svg
    PRE_TARGETDEPS      += $$ICON_ICO
}

DEFINES += BUILD_DATE=$$BUILD_DATE BUILD_HASH=$$BUILD_HASH BUILD_VERSION=$$BUILD_VERSION

WARN_CXX = -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-non-prototype
WARN_C   = -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-non-prototype

CONFIG(release, debug|release) {

    unix|android|win32-g++ {
        QMAKE_CFLAGS_RELEASE   += -O3 -flto
        QMAKE_CXXFLAGS_RELEASE += -O3 -flto
        QMAKE_LFLAGS_RELEASE   += -flto
    }

    win32-msvc {
        QMAKE_CFLAGS_RELEASE   += /Ox /GL /Gw
        QMAKE_CXXFLAGS_RELEASE += /Ox /GL /Gw
        QMAKE_LFLAGS_RELEASE   += /LTCG /OPT:REF /OPT:ICF
    }
}

linux|macx {
    QMAKE_CXXFLAGS_WARN_ON = $$WARN_CXX
    QMAKE_CFLAGS_WARN_ON   = $$WARN_C
}

linux:!android:!macx:static {
    QMAKE_LIBS += \
        -lbrotlicommon \
        -lXau \
        -lXdmcp \
        -lxcb-util \
        -ldbus-1

    QMAKE_LFLAGS += -static

    QMAKE_POST_LINK += strip $$OUT_PWD/$${TARGET} && upx $$OUT_PWD/$${TARGET}
}

RESOURCES += \
    Icons.qrc \
    Fonts.qrc

HEADERS += \
    ByteReader.h \
    Common.h \
    Drive.h \
    Find.h \
    Interface.h \
    InterfaceBluetoothSocket.h \
    InterfaceSerialPort.h \
    MainWindow.h \
    Pack.h \
    PartitionExtractor.h

SOURCES += \
    Drive.cpp \
    Find.cpp \
    InterfaceBluetoothSocket.cpp \
    InterfaceSerialPort.cpp \
    Main.cpp \
    MainWindow.cpp \
    MainWindow_BDOS.cpp \
    PartitionExtractor.cpp

FORMS += MainWindow.ui

DISTFILES += \
    ../clients/JIO_MSX-DOS/*.sh \
    ../clients/JIO_MSX-DOS/*.asm \
    ../clients/JIO_MSX-DOS/*.c \
    ../clients/JIO_MSX-DOS/*.h \
    ../clients/JIO_NFS/*.sh \
    ../clients/JIO_NFS/*.asm \
    ../clients/JIO_NFS/*.c \
    ../clients/JIO_NFS/*.h \
    android/AndroidManifest.xml \
    readme.md
