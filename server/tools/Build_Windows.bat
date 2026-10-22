@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem JIOServer: Windows release build + installer (graphical server + command-line server), via CMake + Ninja.
rem Runs on the Windows build VM (see 0_Build_Windows.sh): builds in a local directory, reads the sources
rem from the project directory (shared), writes the installer to 0_Builds.

rem =========================
rem Tools of the VM (edit once)
rem =========================
set "QT_DIR=C:\Qt\6.9.0\msvc2022_64"
set "QT_TOOLS=C:\Qt\Tools"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
rem =========================

set "PROJECT_NAME=JIOServer"
for %%I in ("%~dp0..") do set "SCRIPT_DIR=%%~fI"

set /p BUILD_VERSION=<"%SCRIPT_DIR%\Version.txt"
if errorlevel 1 goto fail
set "OUT_EXE=%SCRIPT_DIR%\0_Builds\%PROJECT_NAME%_Windows_%BUILD_VERSION%.exe"
set "BUILD_DIR=%USERPROFILE%\Build"
set "DEPLOY=%BUILD_DIR%\deploy"

call "%VCVARS%"
if errorlevel 1 goto fail
set "PATH=%QT_TOOLS%\CMake_64\bin;%QT_TOOLS%\Ninja;%PATH%"

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
if errorlevel 1 goto fail

echo ==^> [%TIME%] cmake configure
cmake -G Ninja -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 goto fail

echo ==^> [%TIME%] cmake build
cmake --build "%BUILD_DIR%"
if errorlevel 1 goto fail

echo ==^> [%TIME%] windeployqt
mkdir "%DEPLOY%"
copy /Y "%BUILD_DIR%\JIOServer.exe" "%DEPLOY%\JIOServer.exe"
if errorlevel 1 goto fail
copy /Y "%BUILD_DIR%\JIOServerCLI.exe" "%DEPLOY%\JIOServerCLI.exe"
if errorlevel 1 goto fail
"%QT_DIR%\bin\windeployqt.exe" "%DEPLOY%\JIOServer.exe" --release
if errorlevel 1 goto fail
"%QT_DIR%\bin\windeployqt.exe" "%DEPLOY%\JIOServerCLI.exe" --release
if errorlevel 1 goto fail

echo ==^> [%TIME%] installer
"%SCRIPT_DIR%\tools\MakeWinInst.py" "%PROJECT_NAME% %BUILD_VERSION%" "%PROJECT_NAME%" "%BUILD_VERSION%" "%DEPLOY%" "%OUT_EXE%"
if errorlevel 1 goto fail

echo ==^> [%TIME%] done
exit /b 0

:fail
set "el=%errorlevel%"
if "%el%"=="0" set "el=1"
exit /b %el%
