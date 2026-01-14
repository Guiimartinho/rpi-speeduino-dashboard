@echo off
REM ============================================================================
REM run-preview.bat
REM Runs the Speeduino UI preview on Windows
REM ============================================================================

setlocal EnableDelayedExpansion

echo ============================================
echo   Speeduino UI Preview
echo ============================================
echo.

REM Try to find Qt installation
set QML_EXE=

REM Check if qml is already in PATH
where qml.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set QML_EXE=qml.exe
    goto :found
)

where qml6.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set QML_EXE=qml6.exe
    goto :found
)

REM Search common Qt installation paths
set QT_PATHS=^
    "C:\Qt\6.8.0\mingw_64\bin" ^
    "C:\Qt\6.7.0\mingw_64\bin" ^
    "C:\Qt\6.6.0\mingw_64\bin" ^
    "C:\Qt\6.5.0\mingw_64\bin" ^
    "C:\Qt\6.4.0\mingw_64\bin" ^
    "%USERPROFILE%\Qt\6.8.0\mingw_64\bin" ^
    "%USERPROFILE%\Qt\6.7.0\mingw_64\bin" ^
    "%USERPROFILE%\Qt\6.6.0\mingw_64\bin" ^
    "C:\Program Files\Qt\6.8.0\mingw_64\bin" ^
    "C:\Program Files\Qt\6.7.0\mingw_64\bin"

for %%p in (%QT_PATHS%) do (
    if exist "%%~p\qml.exe" (
        set QML_EXE=%%~p\qml.exe
        echo Found Qt at: %%~p
        goto :found
    )
)

REM Qt not found
echo.
echo ERROR: Qt/QML not found!
echo.
echo Please install Qt 6.x from:
echo   https://www.qt.io/download-qt-installer
echo.
echo Or install via command line:
echo   pip install aqtinstall
echo   aqt install-qt windows desktop 6.6.0 win64_mingw
echo.
echo After installation, either:
echo   1. Add Qt bin folder to PATH
echo   2. Edit this script to add your Qt path
echo.
pause
exit /b 1

:found
echo Using: %QML_EXE%
echo.

REM Get script directory
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

echo Starting preview...
echo Press Ctrl+C to exit
echo.
echo Keyboard shortcuts:
echo   R     - Toggle Reverse
echo   C     - Toggle CEL
echo   Space - Rev engine
echo   Esc   - Return to Idle
echo   F1-F6 - Navigate screens
echo.

REM Run the preview
"%QML_EXE%" PreviewMain.qml

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Preview exited with error code: %ERRORLEVEL%
    echo Check for QML errors above.
    pause
)

endlocal
