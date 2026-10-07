# JIO server, command line version (no user interface, no settings: all given by arguments)

QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle

TARGET = JIOServerCLI
MAKEFILE = Makefile

include(JIOServerCore.pri)

WARN_CXX = -Wall -Wextra -Wno-unused-parameter

unix {
    BUILD_DATE    = $$system(date +'%Y-%m-%d_%H:%M:%S')
    BUILD_HASH    = $$system(git config --global --add safe.directory "$$PWD" && git -C "$$PWD" rev-parse HEAD)
    BUILD_VERSION = $$system(cat "$$PWD/Version.txt")

    !macx:!android {
        QMAKE_CXXFLAGS += -fcoroutines
    }
}

win32 {
    BUILD_DATE    = $$system(powershell -NoProfile -Command "Get-Date -Format 'yyyy-MM-dd_HH:mm:ss'")
    BUILD_HASH    = $$system(powershell -NoProfile -Command "git -C '$$PWD' rev-parse HEAD")
    BUILD_VERSION = $$system(powershell -NoProfile -Command "(Get-Content '$$PWD\\Version.txt' -Raw).Trim()")
}

DEFINES += BUILD_DATE=$$BUILD_DATE BUILD_HASH=$$BUILD_HASH BUILD_VERSION=$$BUILD_VERSION

CONFIG(release, debug|release) {

    unix|win32-g++ {
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
}

linux:!android:!macx:static {
    QMAKE_LIBS += -lbrotlicommon -ldbus-1
    QMAKE_LFLAGS += -static
    QMAKE_POST_LINK += strip $$OUT_PWD/$${TARGET} && upx $$OUT_PWD/$${TARGET}
}

SOURCES += \
    MainCLI.cpp
