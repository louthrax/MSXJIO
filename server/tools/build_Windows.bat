@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem =========================
rem Project settings (edit once)
rem =========================
set "QMAKE=C:\Qt\6.9.0\msvc2022_64\bin\qmake.exe"
set "WINDEPLOYQT=C:\Qt\6.9.0\msvc2022_64\bin\windeployqt.exe"
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
rem =========================

set "SCRIPT_DIR=%~dp0\.."
if errorlevel 1 goto fail

for %%F in ("%SCRIPT_DIR%\*.pro") do (
    set "PROJECT_NAME=%%~nF"
    goto :pro_found
)
echo No .pro file found in %SCRIPT_DIR%
goto fail

:pro_found
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
if errorlevel 1 goto fail

git config --global --add safe.directory "%SCRIPT_DIR%"
if errorlevel 1 goto fail

for /f "delims=" %%A in ('git -C "." rev-parse HEAD') do set "BUILD_HASH=%%A"
if errorlevel 1 goto fail

set /p BUILD_VERSION=<"./Version.txt"
if errorlevel 1 goto fail

set "PACKAGE_7Z=%PROJECT_NAME%_Windows_%BUILD_VERSION%.7z"
set "OUT_EXE=%SCRIPT_DIR%\0_Builds\%PROJECT_NAME%_Windows_%BUILD_VERSION%.exe"

del "%PACKAGE_7Z%" >nul 2>&1
rmdir /S /Q "%USERPROFILE%\Build" >nul 2>&1

mkdir "%USERPROFILE%\Build"
if errorlevel 1 goto fail

cd /D "%USERPROFILE%\Build"
if errorlevel 1 goto fail

call "%VCVARS%"
if errorlevel 1 goto fail

"%QMAKE%" "%SCRIPT_DIR%\%PROJECT_NAME%.pro" CONFIG+=release
if errorlevel 1 goto fail

set CL=/MP
nmake -f Makefile
if errorlevel 1 goto fail

"%WINDEPLOYQT%" "release\%PROJECT_NAME%.exe" --dir deploy --release
if errorlevel 1 goto fail

copy /Y "release\%PROJECT_NAME%.exe" "deploy\%PROJECT_NAME%.exe"
if errorlevel 1 goto fail

"%SCRIPT_DIR%\tools\MakeWinInst.py" "%PROJECT_NAME% %BUILD_VERSION%" "%PROJECT_NAME%" "%BUILD_VERSION%" "deploy" "%OUT_EXE%"
if errorlevel 1 goto fail

exit /b 0

:fail
set "el=%errorlevel%"
shutdown /s /t 1
exit /b %el%