@echo off
setlocal

set "PROJECT_ROOT=%~dp0."
set "GAMES_DIR=%PROJECT_ROOT%\Games"
set "GAME_NAME=%~1"
set "CONFIGURATION=Debug"

if not defined GAME_NAME (
    echo Usage: dev.bat ^<game-name^>
    echo Use Start.bat for interactive project selection.
    exit /b 1
)

set "GAME_DIR=%GAMES_DIR%\%GAME_NAME%"
set "BUILD_DIR=%GAME_DIR%\build"
set "STOP_SESSION_SCRIPT=%PROJECT_ROOT%\StopGameSession.ps1"

if not exist "%GAME_DIR%\Source\GameModule.cpp" (
    echo Game project not found: "%GAME_DIR%"
    exit /b 1
)

echo [1/4] Stopping the existing %GAME_NAME% session...
powershell -NoProfile -ExecutionPolicy Bypass -File "%STOP_SESSION_SCRIPT%" -BuildDirectory "%BUILD_DIR%"
if errorlevel 1 goto :failure

echo [2/4] Configuring CMake for %GAME_NAME%...
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" "-DGAME_PROJECT=%GAME_NAME%"
if errorlevel 1 goto :failure

echo [3/4] Building %CONFIGURATION%...
cmake --build "%BUILD_DIR%" --config "%CONFIGURATION%"
if errorlevel 1 goto :failure

set "LAUNCHER_PATH=%BUILD_DIR%\%CONFIGURATION%\Launcher.exe"
if not exist "%LAUNCHER_PATH%" (
    echo Launcher not found: "%LAUNCHER_PATH%"
    goto :failure
)

echo [4/4] Starting Launcher for %GAME_NAME%...
start "" /D "%BUILD_DIR%\%CONFIGURATION%" "%LAUNCHER_PATH%"

echo Development session started for %GAME_NAME%.
exit /b 0

:failure
echo Development session failed for %GAME_NAME%.
exit /b 1
