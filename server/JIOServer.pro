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

    # Define PNG output files for each resolution
    HDPI_PNG    = $$ANDROID_RES_DIR/drawable-hdpi/icon.png
    LDPI_PNG    = $$ANDROID_RES_DIR/drawable-ldpi/icon.png
    MDPI_PNG    = $$ANDROID_RES_DIR/drawable-mdpi/icon.png
    XHDPI_PNG   = $$ANDROID_RES_DIR/drawable-xhdpi/icon.png
    XXHDPI_PNG  = $$ANDROID_RES_DIR/drawable-xxhdpi/icon.png
    XXXHDPI_PNG = $$ANDROID_RES_DIR/drawable-xxxhdpi/icon.png

    # Generate PNGs for each resolution explicitly
    $${LDPI_PNG}.commands    = mkdir -p $$ANDROID_RES_DIR/drawable-ldpi    && inkscape $$SVG_ICON --export-type=png --export-width=36  --export-height=36  --export-filename=$$LDPI_PNG
    $${MDPI_PNG}.commands    = mkdir -p $$ANDROID_RES_DIR/drawable-mdpi    && inkscape $$SVG_ICON --export-type=png --export-width=48  --export-height=48  --export-filename=$$MDPI_PNG
    $${HDPI_PNG}.commands    = mkdir -p $$ANDROID_RES_DIR/drawable-hdpi    && inkscape $$SVG_ICON --export-type=png --export-width=72  --export-height=72  --export-filename=$$HDPI_PNG
    $${XHDPI_PNG}.commands   = mkdir -p $$ANDROID_RES_DIR/drawable-xhdpi   && inkscape $$SVG_ICON --export-type=png --export-width=96  --export-height=96  --export-filename=$$XHDPI_PNG
    $${XXHDPI_PNG}.commands  = mkdir -p $$ANDROID_RES_DIR/drawable-xxhdpi  && inkscape $$SVG_ICON --export-type=png --export-width=144 --export-height=144 --export-filename=$$XXHDPI_PNG
    $${XXXHDPI_PNG}.commands = mkdir -p $$ANDROID_RES_DIR/drawable-xxxhdpi && inkscape $$SVG_ICON --export-type=png --export-width=192 --export-height=192 --export-filename=$$XXXHDPI_PNG

    $${LDPI_PNG}.depends     = $$SVG_ICON
    $${MDPI_PNG}.depends     = $$SVG_ICON
    $${HDPI_PNG}.depends     = $$SVG_ICON
    $${XHDPI_PNG}.depends    = $$SVG_ICON
    $${XXHDPI_PNG}.depends   = $$SVG_ICON
    $${XXXHDPI_PNG}.depends  = $$SVG_ICON

    DEPS = $$HDPI_PNG $$LDPI_PNG $$MDPI_PNG $$XHDPI_PNG $$XXHDPI_PNG $$XXXHDPI_PNG
    QMAKE_EXTRA_TARGETS += $$DEPS
    PRE_TARGETDEPS += $$DEPS
    CLEAN_FILES += $$HDPI_PNG $$LDPI_PNG $$MDPI_PNG $$XHDPI_PNG $$XXHDPI_PNG $$XXXHDPI_PNG
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
    InterfaceBluetoothSocket.cpp \
    InterfaceSerialPort.cpp \
    Main.cpp \
    MainWindow.cpp \
    PartitionExtractor.cpp

HEADERS += \
    ByteReader.h \
    Common.h \
    Drive.h \
    Interface.h \
    InterfaceBluetoothSocket.h \
    InterfaceSerialPort.h \
    MainWindow.h \
    PartitionExtractor.h

RESOURCES += \
    Icons.qrc \
    Fonts.qrc

DISTFILES += \
    ../clients/JIO_MSX-DOS/crt.asm \
    ../clients/JIO_MSX-DOS/disk.inc \
    ../clients/JIO_MSX-DOS/dos1x.asm \
    ../clients/JIO_MSX-DOS/drv_jio.c \
    ../clients/JIO_MSX-DOS/drv_jio.asm \
    ../clients/JIO_MSX-DOS/drv_jio.inc \
    ../clients/JIO_MSX-DOS/msx.inc \
    ../clients/JIO_MSX-DOS/p0_kernel.asm \
    ../clients/JIO_MSX-DOS/p1_main.asm \
    ../clients/JIO_MSX-DOS/p3_paging.asm \
    android/AndroidManifest.xml \
    readme.md
