QT += core gui widgets bluetooth serialport

CONFIG += c++20

FORMS += MainWindow.ui

unix:!android:CONFIG(release, debug|release) {
    APPNAME = JIOServer
    OUTDIR = $$OUT_PWD
    DEPLOYDIR = $$OUT_PWD/deploy
    SCRIPT = $$PWD/deploy_linux.sh

    QMAKE_POST_LINK += \
        echo "==[ Linux Deployment ]==" && \
        $$SCRIPT $$OUTDIR/$$APPNAME $$DEPLOYDIR
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

win32:CONFIG(release, debug|release) {
    DESTDIR = release
    TARGET_EXE = $$OUT_PWD/$$DESTDIR/$${TARGET}.exe
    DEPLOYDIR = $$OUT_PWD/deploy
    WINDEPLOYQT = $$[QT_INSTALL_BINS]/windeployqt.exe

    OUT_EXE_WIN = $$shell_path($$TARGET_EXE)
    DEPLOYDIR_WIN = $$shell_path($$DEPLOYDIR)
    WINDEPLOYQT_WIN = $$shell_path($$WINDEPLOYQT)
    ZIPFILE = $$shell_path($$OUT_PWD/$$TARGET-win.zip)

    QMAKE_POST_LINK += \
        "$$WINDEPLOYQT_WIN" "$$OUT_EXE_WIN" --dir "$$DEPLOYDIR_WIN" && \
        copy /Y "$${OUT_EXE_WIN}" "$${DEPLOYDIR_WIN}\\$${TARGET}.exe" && \
        powershell -Command "Compress-Archive -Path '$$DEPLOYDIR_WIN\\*' -DestinationPath '$$ZIPFILE' -Force" && \
        echo ZIP created at $$ZIPFILE
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
    Icons.qrc

DISTFILES += \
    MSXClient/0_Make.sh \
    MSXClient/bootcode.inc \
    MSXClient/crt.asm \
    MSXClient/disk.inc \
    MSXClient/dos1x.asm \
    MSXClient/driver.asm \
    MSXClient/drv_jio.asm \
    MSXClient/drv_jio.c \
    MSXClient/drv_jio_c.asm \
    MSXClient/flags.inc \
    MSXClient/msx.inc \
    MSXClient/p0_kernel.asm \
    MSXClient/p1_main.asm \
    MSXClient/p3_paging.asm \
    android/AndroidManifest.xml \
    docs/Notes.odt \
    readme.md
