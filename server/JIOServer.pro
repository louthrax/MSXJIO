QT += core gui widgets bluetooth serialport

CONFIG += c++20

MAKEFILE = Makefile

FORMS += MainWindow.ui

linux:!android:!macx:static {

    QMAKE_CXXFLAGS_RELEASE += -O3
    QMAKE_LFLAGS_RELEASE   += -O3

    QMAKE_LIBS += \
        -lbrotlicommon \
        -lXau \
        -lXdmcp \
        -lxcb-util \
        -ldbus-1

    QMAKE_LFLAGS += -static

    QMAKE_POST_LINK += strip $$OUT_PWD/$$TARGET && upx $$OUT_PWD/$$TARGET
}

android {
    QT += svg
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android

    # Define paths
    SVG_ICON = $$PWD/icons/JIOServer.svg
    ANDROID_RES_DIR = $$PWD/android/res

    RESOLUTIONS_W = \
        ldpi:36 \
        mdpi:48 \
        hdpi:72 \
        xhdpi:96 \
        xxhdpi:144 \
        xxxhdpi:192

    for (ENTRY, RESOLUTIONS_W) {
        RES = $$section(ENTRY, :, 0, 0)
        WIDTH = $$section(ENTRY, :, 1, 1)

        DIR = $$ANDROID_RES_DIR/drawable-$$RES
        PNG_FILE = $$DIR/icon.png
        MYTARGET = icon_$${RES}_png

        $${MYTARGET}.commands = \
            mkdir -p $$DIR && \
            rsvg-convert -w $${WIDTH} -h $${WIDTH} $$SVG_ICON -o $$PNG_FILE


        $${MYTARGET}.depends = $$SVG_ICON

        QMAKE_EXTRA_TARGETS += $$MYTARGET
        PRE_TARGETDEPS += $$MYTARGET
        QMAKE_CLEAN += $$PNG_FILE
    }
}

ICON_NAME = JIOServer
ICON_SRC = $$PWD/icons/$${ICON_NAME}.svg

win32:CONFIG(release, debug|release) {
    DESTDIR = release
    TARGET_EXE = $$OUT_PWD/$$DESTDIR/$${TARGET}.exe
    DEPLOYDIR = $$OUT_PWD/deploy
    WINDEPLOYQT = $$[QT_INSTALL_BINS]/windeployqt.exe

    OUT_EXE_WIN = $$shell_path($$TARGET_EXE)
    DEPLOYDIR_WIN = $$shell_path($$DEPLOYDIR)
    WINDEPLOYQT_WIN = $$shell_path($$WINDEPLOYQT)
    ZIPFILE = $$shell_path($$OUT_PWD/../$$TARGET-win.zip)

    RC_ICONS = $$ICON_OUT
    RC_FILE = JIOServer.rc

    ICON_OUT = $$shell_quote($$shell_path($${ICON_NAME}.ico))

    QMAKE_EXTRA_TARGETS += make_icon
    PRE_TARGETDEPS += $$ICON_OUT

    make_icon.target = $$ICON_OUT
    make_icon.depends = $$ICON_SRC
    make_icon.commands = \
        echo Generating icon... && \
        magick convert -background none -resize 256x256 $$ICON_SRC ico-256.png &&\
        magick convert -background none -resize 128x128 $$ICON_SRC ico-128.png &&\
        magick convert -background none -resize   64x64 $$ICON_SRC ico-64.png &&\
        magick convert -background none -resize   32x32 $$ICON_SRC ico-32.png &&\
        magick convert -background none -resize   16x16 $$ICON_SRC ico-16.png &&\
        magick convert ico-256.png ico-128.png ico-64.png ico-32.png ico-16.png $$ICON_OUT && \
        echo $$ICON_OUT generated.

    QMAKE_POST_LINK += \
        "$$WINDEPLOYQT_WIN" "$$OUT_EXE_WIN" --dir "$$DEPLOYDIR_WIN" && \
        copy /Y "$${OUT_EXE_WIN}" "$${DEPLOYDIR_WIN}\\$${TARGET}.exe" && \
        del "$$ZIPFILE" && \
        7z a -tzip "$$ZIPFILE" "$$DEPLOYDIR_WIN\*" && \
        echo ZIP created at $$ZIPFILE;
}

APP_BUNDLE = $$OUT_PWD/$$DESTDIR/$${TARGET}.app

macx {
    INFO_PLIST = $$APP_BUNDLE/Contents/Info.plist
    QMAKE_POST_LINK += /usr/libexec/PlistBuddy -c $$quote('"Set :NSBluetoothAlwaysUsageDescription string Enable Bluetooth communication with MSX"') "$$INFO_PLIST";
}


macx:CONFIG(release, debug|release) {
    DEPLOYDIR = $$OUT_PWD/deploy
    MACDEPLOYQT = $$[QT_INSTALL_BINS]/macdeployqt
    DMGFILE = $$OUT_PWD/$${TARGET}-mac.dmg

    QMAKE_EXTRA_TARGETS += make_icon

    QMAKE_BUNDLE_DATA += app_icon
    app_icon.files = $${ICON_NAME}.icns
    app_icon.path = Contents/Resources

    make_icon.target = $${ICON_NAME}.icns
    make_icon.depends = $$ICON_SRC
    make_icon.commands = \
        echo "==[ Generating $${ICON_NAME}.icns from $$ICON_SRC ]==" && \
        rm -rf $${ICON_NAME}.iconset $${ICON_NAME}.icns && \
        mkdir -p $${ICON_NAME}.iconset && \
        /usr/local/bin/convert -background none -resize 16x16     $$ICON_SRC $${ICON_NAME}.iconset/icon_16x16.png && \
        /usr/local/bin/convert -background none -resize 32x32     $$ICON_SRC $${ICON_NAME}.iconset/icon_16x16@2x.png && \
        /usr/local/bin/convert -background none -resize 32x32     $$ICON_SRC $${ICON_NAME}.iconset/icon_32x32.png && \
        /usr/local/bin/convert -background none -resize 64x64     $$ICON_SRC $${ICON_NAME}.iconset/icon_32x32@2x.png && \
        /usr/local/bin/convert -background none -resize 128x128   $$ICON_SRC $${ICON_NAME}.iconset/icon_128x128.png && \
        /usr/local/bin/convert -background none -resize 256x256   $$ICON_SRC $${ICON_NAME}.iconset/icon_256x256.png && \
        /usr/local/bin/convert -background none -resize 512x512   $$ICON_SRC $${ICON_NAME}.iconset/icon_512x512.png && \
        /usr/local/bin/convert -background none -resize 1024x1024 $$ICON_SRC $${ICON_NAME}.iconset/icon_512x512@2x.png && \
        iconutil -c icns $${ICON_NAME}.iconset && \
        echo "✅ $${ICON_NAME}.icns generated."

    QMAKE_POST_LINK += \
        echo "==[ macOS Deployment ]==" && \
        "$$MACDEPLOYQT" "$$APP_BUNDLE" -verbose=1 && \
        rm -rf "$$APP_BUNDLE/Contents/Resources/qt.conf" \
        rm -rf "$$DEPLOYDIR" && \
        mkdir -p "$$DEPLOYDIR" && \
        cp -R "$$APP_BUNDLE" "$$DEPLOYDIR/" && \
        hdiutil create -volname "$${TARGET}" -srcfolder "$$DEPLOYDIR" -ov -format UDZO ~/Tmp/$${TARGET}-mac.dmg && \
        mv ~/Tmp/$${TARGET}-mac.dmg $$OUT_PWD/../$${TARGET}-mac.dmg && \
        echo "DMG created at $$DMGFILE";
}

SOURCES += \
    Drive.cpp \
    Find.cpp \
    InterfaceBluetoothSocket.cpp \
    InterfaceSerialPort.cpp \
    Main.cpp \
    MainWindow.cpp \
    MainWindow_BDOS.cpp \
    PartitionExtractor.cpp

HEADERS += \
    ByteReader.h \
    Common.h \
    Drive.h \
    Find.h \
    Interface.h \
    InterfaceBluetoothSocket.h \
    InterfaceSerialPort.h \
    MainWindow.h \
    PartitionExtractor.h

RESOURCES += \
    Icons.qrc \
    Fonts.qrc

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
