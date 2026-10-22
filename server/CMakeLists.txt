# JIOServer — CMake build (pilot: Windows; macOS/Linux/Android still use the .pro files).
#
# Two executables share a common core (the protocol/disk/connection code, Qt Core/Bluetooth/
# SerialPort): the graphical server JIOServer and the command-line server JIOServerCLI. The core
# sources are compiled into each executable (as the .pro did), so each links exactly the Qt modules
# it needs and resource initialisation is never stripped.
#
# The Windows application icon is generated from the SVG by a custom command, and the resource that
# embeds it declares an explicit dependency on it (OBJECT_DEPENDS): the .ico is always produced
# before rc runs. Parallel-correct by construction, no qmake-time side effect.

cmake_minimum_required(VERSION 3.21)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/Version.txt JIO_VERSION LIMIT_COUNT 1)
string(STRIP "${JIO_VERSION}" JIO_VERSION)

project(JIOServer VERSION ${JIO_VERSION} LANGUAGES CXX)

if(WIN32 AND NOT MSVC)
  message(FATAL_ERROR "On Windows, build with MSVC in a Visual Studio developer environment (vcvars64.bat).")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)          # the core uses C++20 coroutines
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Bluetooth SerialPort Multimedia)

# Build metadata (MainCLI.cpp stringizes BUILD_VERSION; all three mirror the .pro defines)
execute_process(COMMAND git -C ${CMAKE_CURRENT_SOURCE_DIR} rev-parse HEAD
                OUTPUT_VARIABLE JIO_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
string(TIMESTAMP JIO_DATE "%Y-%m-%d_%H:%M:%S")

# ---- Shared core: protocol, disk image, served directories, connection (Core/Bluetooth/SerialPort)
set(CORE_SOURCES
  Drive.cpp Find.cpp InterfaceBluetoothSocket.cpp InterfaceSerialPort.cpp
  PartitionExtractor.cpp Server.cpp Server_BDOS.cpp
  ByteReader.h Common.h Drive.h Find.h Interface.h InterfaceBluetoothSocket.h
  InterfaceSerialPort.h Pack.h PartitionExtractor.h Server.h
  Bin.qrc)

# ---- Graphical server (built on Android too; androiddeployqt needs qt_add_executable) ----
set(JIOSERVER_SOURCES
  ${CORE_SOURCES}
  CasPlayer.cpp Main.cpp MainWindow.cpp
  CasPlayer.h MainWindow.h
  MainWindow.ui
  Icons.qrc Fonts.qrc Cas.qrc)
if(ANDROID)
  qt_add_executable(JIOServer ${JIOSERVER_SOURCES})
else()
  add_executable(JIOServer ${JIOSERVER_SOURCES})
endif()
target_include_directories(JIOServer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(JIOServer PRIVATE
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Bluetooth Qt6::SerialPort Qt6::Multimedia)
target_compile_definitions(JIOServer PRIVATE
  BUILD_DATE=${JIO_DATE} BUILD_HASH=${JIO_HASH} BUILD_VERSION=${JIO_VERSION})
set_target_properties(JIOServer PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# ---- Command-line server (no GUI, console subsystem) ----
add_executable(JIOServerCLI
  ${CORE_SOURCES}
  MainCLI.cpp)
target_include_directories(JIOServerCLI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(JIOServerCLI PRIVATE Qt6::Core Qt6::Bluetooth Qt6::SerialPort)
target_compile_definitions(JIOServerCLI PRIVATE
  BUILD_DATE=${JIO_DATE} BUILD_HASH=${JIO_HASH} BUILD_VERSION=${JIO_VERSION})
set_target_properties(JIOServerCLI PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# GCC needs -fcoroutines for the C++20 coroutines the core uses (clang and MSVC enable them with -std)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(JIOServer PRIVATE -fcoroutines)
  target_compile_options(JIOServerCLI PRIVATE -fcoroutines)
endif()

# ---- Windows: GUI subsystem and generated application icon (JIOServer only) ----
if(WIN32)
  enable_language(RC)
  set_target_properties(JIOServer PROPERTIES WIN32_EXECUTABLE ON)

  set(APP_ICON ${CMAKE_CURRENT_BINARY_DIR}/JIOServer.ico)
  add_custom_command(
    OUTPUT ${APP_ICON}
    COMMAND magick -background none ${CMAKE_CURRENT_SOURCE_DIR}/JIOServer.svg
            -define icon:auto-resize=16,24,32,48,64,128,256 ${APP_ICON}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/JIOServer.svg
    COMMENT "Generating JIOServer.ico from the SVG"
    VERBATIM)

  set(APP_ICON_PATH "${APP_ICON}")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/windows/app.rc.in
                 ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
  target_sources(JIOServer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
  # The explicit dependency the .pro could not express: rc waits for the generated .ico.
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/app.rc
                              PROPERTIES OBJECT_DEPENDS ${APP_ICON})
endif()

# ---- macOS: application bundles (icon from the SVG for the GUI; the CLI is bundled so
#      macdeployqt can ship its Qt frameworks) ----
if(APPLE)
  set(APP_ICNS ${CMAKE_CURRENT_BINARY_DIR}/JIOServer.icns)
  add_custom_command(OUTPUT ${APP_ICNS}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/Svg2icns.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/JIOServer.svg ${APP_ICNS}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/JIOServer.svg
    COMMENT "Generating JIOServer.icns from the SVG" VERBATIM)
  set_source_files_properties(${APP_ICNS} PROPERTIES
    GENERATED TRUE MACOSX_PACKAGE_LOCATION Resources)
  target_sources(JIOServer PRIVATE ${APP_ICNS})
  set_target_properties(JIOServer PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE JIOServer.icns
    MACOSX_BUNDLE_BUNDLE_NAME JIOServer
    MACOSX_BUNDLE_GUI_IDENTIFIER net.louthrax.jioserver
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${JIO_VERSION})
  set_target_properties(JIOServerCLI PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME JIOServerCLI
    MACOSX_BUNDLE_GUI_IDENTIFIER net.louthrax.jioservercli
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${JIO_VERSION})
endif()

# ---- Android: package template (manifest, icons) for the graphical server ----
if(ANDROID)
  # Launcher icons rendered from the SVG at each density into android/res (generated, git-ignored,
  # removed by 0_Clean.sh). SELF_CALL: parallel inkscape exports race on DBus otherwise (inkscape#4716).
  set(_icons)
  foreach(_entry IN ITEMS ldpi:36 mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192)
    string(REPLACE ":" ";" _entry "${_entry}")
    list(GET _entry 0 _dpi)
    list(GET _entry 1 _size)
    set(_dir ${CMAKE_CURRENT_SOURCE_DIR}/android/res/drawable-${_dpi})
    add_custom_command(OUTPUT ${_dir}/icon.png
      COMMAND ${CMAKE_COMMAND} -E make_directory ${_dir}
      COMMAND ${CMAKE_COMMAND} -E env SELF_CALL=1
              inkscape ${CMAKE_CURRENT_SOURCE_DIR}/JIOServer.svg -w ${_size} -h ${_size}
              --export-type=png --export-filename=${_dir}/icon.png
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/JIOServer.svg
      COMMENT "Rendering Android icon (${_dpi}, ${_size}px)" VERBATIM)
    list(APPEND _icons ${_dir}/icon.png)
  endforeach()
  add_custom_target(jioserver_android_icons DEPENDS ${_icons})
  add_dependencies(JIOServer jioserver_android_icons)

  set_target_properties(JIOServer PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android)
endif()
