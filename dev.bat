@echo off
setlocal

set "PROJECT_ROOT=%~dp0."
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "CONFIGURATION=Debug"

echo [1/4] Stopping development processes...
taskkill /F /IM Launcher.exe /T >nul 2>&1
taskkill /F /IM GameHost.exe /T >nul 2>&1
taskkill /F /IM AssistantHost.exe /T >nul 2>&1

echo [2/4] Configuring CMake...
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%"
if errorlevel 1 goto :failure

echo [3/4] Building %CONFIGURATION%...
cmake --build "%BUILD_DIR%" --config "%CONFIGURATION%"
if errorlevel 1 goto :failure

set "LAUNCHER_PATH=%BUILD_DIR%\%CONFIGURATION%\Launcher.exe"
if not exist "%LAUNCHER_PATH%" (
    echo Launcher not found: "%LAUNCHER_PATH%"
    goto :failure
)

echo [4/4] Starting Launcher...
start "" /D "%BUILD_DIR%\%CONFIGURATION%" "%LAUNCHER_PATH%"

echo Development session started successfully.
exit /b 0

:failure
echo Development session failed. Launcher was not started.
exit /b 1
