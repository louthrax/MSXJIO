@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem JIOServerCLI: command-line server, Windows release, packaged as a zip (no installer), via CMake + Ninja.
rem Runs on the Windows build VM (see 0_Build_CLI_Windows.sh).

rem =========================
rem Tools of the VM (edit once)
rem =========================
set "QT_DIR=C:\Qt\6.9.0\msvc2022_64"
set "QT_TOOLS=C:\Qt\Tools"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
rem =========================

set "PROJECT_NAME=JIOServerCLI"
for %%I in ("%~dp0..") do set "SCRIPT_DIR=%%~fI"

set /p BUILD_VERSION=<"%SCRIPT_DIR%\Version.txt"
if errorlevel 1 goto fail
set "OUT_ZIP=%SCRIPT_DIR%\0_Builds\%PROJECT_NAME%_Windows_%BUILD_VERSION%.zip"
set "BUILD_DIR=%USERPROFILE%\BuildCLI"
set "DEPLOY=%BUILD_DIR%\deploy\%PROJECT_NAME%"

call "%VCVARS%"
if errorlevel 1 goto fail
set "PATH=%QT_TOOLS%\CMake_64\bin;%QT_TOOLS%\Ninja;%PATH%"

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"
if errorlevel 1 goto fail

echo ==^> [%TIME%] cmake configure
cmake -G Ninja -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%"
if errorlevel 1 goto fail

echo ==^> [%TIME%] cmake build (CLI only)
cmake --build "%BUILD_DIR%" --target JIOServerCLI
if errorlevel 1 goto fail

echo ==^> [%TIME%] deploy
mkdir "%DEPLOY%"
copy /Y "%BUILD_DIR%\JIOServerCLI.exe" "%DEPLOY%\JIOServerCLI.exe"
if errorlevel 1 goto fail
"%QT_DIR%\bin\windeployqt.exe" "%DEPLOY%\JIOServerCLI.exe" --release --no-translations --no-compiler-runtime
if errorlevel 1 goto fail
rem Visual C++ runtime DLLs next to the exe (no vc_redist installer in the archive)
for /d %%D in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do copy /Y "%%D\*.dll" "%DEPLOY%\"
if not exist "%DEPLOY%\vcruntime140.dll" goto fail

"%DEPLOY%\JIOServerCLI.exe" --version
if errorlevel 1 goto fail

echo ==^> [%TIME%] zip
if not exist "%SCRIPT_DIR%\0_Builds" mkdir "%SCRIPT_DIR%\0_Builds"
del "%OUT_ZIP%" >nul 2>&1
powershell -NoProfile -Command "Compress-Archive -Path '%BUILD_DIR%\deploy\%PROJECT_NAME%' -DestinationPath '%OUT_ZIP%'"
if errorlevel 1 goto fail

echo ==^> [%TIME%] done
exit /b 0

:fail
set "el=%errorlevel%"
if "%el%"=="0" set "el=1"
exit /b %el%
