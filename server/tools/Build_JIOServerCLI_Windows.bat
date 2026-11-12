@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem JIOServerCLI (command line server): zip archive.
rem Built on the Windows VM (0_Build_JIOServerCLI_Windows.sh), in a folder of the VM, from the project folder shared
rem with the host.

set "NAME=JIOServerCLI"
set "TARGET=JIOServerCLI"

rem ---- Tools of the VM (edit once) ----
set "QT_DIR=C:\Qt\6.9.3\msvc2022_64"
set "QT_TOOLS=C:\Qt\Tools"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
rem -------------------------------------

for %%I in ("%~dp0..") do set "SCRIPT_DIR=%%~fI"
set /p VERSION=<"%SCRIPT_DIR%\Version.txt"
if errorlevel 1 goto fail
set "PACKAGE=%NAME%_%VERSION%_Windows"
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
rem a JIOServerCLI folder: the executable, its Qt DLLs and the Visual C++ runtime (no vc_redist installer)
mkdir "%DEPLOY%\%TARGET%"
copy /Y "%BUILD_DIR%\%TARGET%.exe" "%DEPLOY%\%TARGET%\"
if errorlevel 1 goto fail
"%QT_DIR%\bin\windeployqt.exe" "%DEPLOY%\%TARGET%\%TARGET%.exe" --release --no-translations --no-compiler-runtime
if errorlevel 1 goto fail
for /d %%D in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do copy /Y "%%D\*.dll" "%DEPLOY%\%TARGET%\"
if not exist "%DEPLOY%\%TARGET%\vcruntime140.dll" goto fail
"%DEPLOY%\%TARGET%\%TARGET%.exe" --version
if errorlevel 1 goto fail
del "%SCRIPT_DIR%\0_Builds\%PACKAGE%.zip" >nul 2>&1
powershell -NoProfile -Command "Compress-Archive -Path '%DEPLOY%\%TARGET%' -DestinationPath '%SCRIPT_DIR%\0_Builds\%PACKAGE%.zip'"
if errorlevel 1 goto fail

echo Package: %SCRIPT_DIR%\0_Builds\%PACKAGE%.zip
exit /b 0

:fail
set "el=%errorlevel%"
if "%el%"=="0" set "el=1"
exit /b %el%
