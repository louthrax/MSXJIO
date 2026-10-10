@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Command line server (JIOServerCLI.pro): JIOServerCLI.exe and its Qt DLLs in a zip archive (no installer)

rem =========================
rem Project settings (edit once)
rem =========================
set "QMAKE=C:\Qt\6.9.0\msvc2022_64\bin\qmake.exe"
set "WINDEPLOYQT=C:\Qt\6.9.0\msvc2022_64\bin\windeployqt.exe"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
rem =========================

set "SCRIPT_DIR=%~dp0\.."
if errorlevel 1 goto fail

set "PROJECT_NAME=JIOServerCLI"
if exist "%SCRIPT_DIR%\%PROJECT_NAME%.pro" goto :pro_found
echo No %PROJECT_NAME%.pro file found in %SCRIPT_DIR%
goto fail

:pro_found
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
if errorlevel 1 goto fail

git config --global --add safe.directory "%SCRIPT_DIR%"
if errorlevel 1 goto fail

set /p BUILD_VERSION=<"./Version.txt"
if errorlevel 1 goto fail

set "OUT_ZIP=%SCRIPT_DIR%\0_Builds\%PROJECT_NAME%_Windows_%BUILD_VERSION%.zip"
set "BUILD_DIR=%USERPROFILE%\BuildCLI"

rmdir /S /Q "%BUILD_DIR%" >nul 2>&1

mkdir "%BUILD_DIR%"
if errorlevel 1 goto fail

cd /D "%BUILD_DIR%"
if errorlevel 1 goto fail

call "%VCVARS%"
if errorlevel 1 goto fail

"%QMAKE%" "%SCRIPT_DIR%\%PROJECT_NAME%.pro" CONFIG+=release
if errorlevel 1 goto fail

set CL=/MP
nmake -f Makefile
if errorlevel 1 goto fail

mkdir "deploy\%PROJECT_NAME%"
if errorlevel 1 goto fail

copy /Y "release\%PROJECT_NAME%.exe" "deploy\%PROJECT_NAME%\%PROJECT_NAME%.exe"
if errorlevel 1 goto fail

rem Qt DLLs; Visual C++ runtime DLLs copied next to the exe (no vc_redist installer of 25 MB in the archive)
"%WINDEPLOYQT%" "deploy\%PROJECT_NAME%\%PROJECT_NAME%.exe" --release --no-translations --no-compiler-runtime
if errorlevel 1 goto fail

for /d %%D in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do copy /Y "%%D\*.dll" "deploy\%PROJECT_NAME%\"
if not exist "deploy\%PROJECT_NAME%\vcruntime140.dll" goto fail

"deploy\%PROJECT_NAME%\%PROJECT_NAME%.exe" --version
if errorlevel 1 goto fail

if not exist "%SCRIPT_DIR%\0_Builds" mkdir "%SCRIPT_DIR%\0_Builds"
del "%OUT_ZIP%" >nul 2>&1
powershell -NoProfile -Command "Compress-Archive -Path 'deploy\%PROJECT_NAME%' -DestinationPath '%OUT_ZIP%'"
if errorlevel 1 goto fail

exit /b 0

:fail
set "el=%errorlevel%"
exit /b %el%
