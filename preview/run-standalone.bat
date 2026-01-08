@echo off
REM Run standalone preview (no external dependencies)
REM This version works even if the main project has import errors

setlocal

REM Find Qt
for %%p in (
    "C:\Qt\6.8.0\mingw_64\bin\qml.exe"
    "C:\Qt\6.7.0\mingw_64\bin\qml.exe"
    "C:\Qt\6.6.0\mingw_64\bin\qml.exe"
    "C:\Qt\6.5.0\mingw_64\bin\qml.exe"
) do (
    if exist %%p (
        set QML_EXE=%%p
        goto :run
    )
)

where qml.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set QML_EXE=qml.exe
    goto :run
)

echo Qt not found. Install from: https://www.qt.io/download-qt-installer
pause
exit /b 1

:run
cd /d "%~dp0"
echo Running standalone preview...
echo Keys: 1-4 = screens, C = CEL, T = overheat
"%QML_EXE%" StandalonePreview.qml
pause
