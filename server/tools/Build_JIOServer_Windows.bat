@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem JIOServer (graphical server): Windows installer, with the command line server.
rem Built on the Windows VM (0_Build_JIOServer_Windows.sh), in a folder of the VM, from the project folder shared
rem with the host.

set "NAME=JIOServer"
set "TARGET=JIOServer"

rem ---- Tools of the VM (edit once) ----
set "QT_DIR=C:\Qt\6.9.3\msvc2022_64"
set "QT_TOOLS=C:\Qt\Tools"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
rem -------------------------------------

for %%I in ("%~dp0..") do set "SCRIPT_DIR=%%~fI"
set /p VERSION=<"%SCRIPT_DIR%\Version.txt"
if errorlevel 1 goto fail
set "PACKAGE=%NAME%_Windows_%VERSION%"
set "BUILD_DIR=%USERPROFILE%\Build"
set "DEPLOY=%BUILD_DIR%\deploy"

rem Visual Studio environment (cl, link, rc, Visual C++ runtime), then CMake and Ninja of the Qt Tools
call "%VCVARS%"
if errorlevel 1 goto fail
set "PATH=%QT_TOOLS%\CMake_64\bin;%QT_TOOLS%\Ninja;%PATH%"

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
mkdir "%BUILD_DIR%" "%DEPLOY%"
if not exist "%SCRIPT_DIR%\0_Builds" mkdir "%SCRIPT_DIR%\0_Builds"

echo ==^> [%TIME%] cmake
cmake -G Ninja -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 goto fail
cmake --build "%BUILD_DIR%" --target %TARGET%
if errorlevel 1 goto fail

echo ==^> [%TIME%] package
cmake --build "%BUILD_DIR%" --target JIOServerCLI
if errorlevel 1 goto fail
copy /Y "%BUILD_DIR%\%TARGET%.exe" "%DEPLOY%\"
if errorlevel 1 goto fail
copy /Y "%BUILD_DIR%\JIOServerCLI.exe" "%DEPLOY%\"
if errorlevel 1 goto fail
"%QT_DIR%\bin\windeployqt.exe" "%DEPLOY%\%TARGET%.exe" --release
if errorlevel 1 goto fail
"%QT_DIR%\bin\windeployqt.exe" "%DEPLOY%\JIOServerCLI.exe" --release
if errorlevel 1 goto fail
"%SCRIPT_DIR%\tools\MakeWinInst.py" "%NAME% %VERSION%" "%NAME%" "%VERSION%" "%DEPLOY%" "%SCRIPT_DIR%\0_Builds\%PACKAGE%.exe"
if errorlevel 1 goto fail

echo Package: %SCRIPT_DIR%\0_Builds\%PACKAGE%.exe
exit /b 0

:fail
set "el=%errorlevel%"
if "%el%"=="0" set "el=1"
exit /b %el%
