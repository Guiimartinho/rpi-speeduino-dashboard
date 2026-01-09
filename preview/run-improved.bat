@echo off
REM ============================================================================
REM run-improved.bat
REM Runs the Improved Speeduino Dashboard Preview
REM ============================================================================

setlocal EnableDelayedExpansion

echo.
echo ========================================================
echo   Speeduino Dashboard - Improved Preview
echo ========================================================
echo.

REM Try to find Qt installation
set QML_EXE=

REM Check common paths
for %%p in (
    "E:\Softwares\Qt\6.7.3\mingw_64\bin\qml.exe"
    "C:\Qt\6.8.0\mingw_64\bin\qml.exe"
    "C:\Qt\6.7.3\mingw_64\bin\qml.exe"
    "C:\Qt\6.7.0\mingw_64\bin\qml.exe"
    "C:\Qt\6.6.0\mingw_64\bin\qml.exe"
) do (
    if exist %%p (
        set QML_EXE=%%p
        goto :found
    )
)

REM Check if qml is in PATH
where qml.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set QML_EXE=qml.exe
    goto :found
)

echo ERROR: Qt/QML not found!
echo Please install Qt 6.x or add it to your PATH
pause
exit /b 1

:found
echo Using: %QML_EXE%
echo.
echo Keyboard Shortcuts:
echo   1-4      : Switch screens (Home/Dash/Settings/Auto)
echo   M        : Cycle display modes (Sport/Eco/Minimal/Full)
echo   C        : Toggle CEL warning
echo   T        : Toggle overheat warning
echo   F        : Toggle low fuel warning
echo   R        : Toggle reverse gear
echo   Space    : Rev engine
echo   Up/Down  : Shift gear
echo   Escape   : Return to idle
echo.
echo Starting preview...
echo.

cd /d "%~dp0"
%QML_EXE% ImprovedPreview.qml

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Preview exited with error code: %ERRORLEVEL%
    pause
)

endlocal
