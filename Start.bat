@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PROJECT_ROOT=%~dp0."
set "GAMES_DIR=%PROJECT_ROOT%\Games"
set "CONFIGURATION=Debug"
set "GAME_COUNT=0"
set "REQUESTED_GAME=%~1"
set "OPENAI_CONFIG_SCRIPT=%PROJECT_ROOT%\AssistantHost\Config\ConfigureOpenAI.ps1"
set "OPENAI_SETTINGS=%PROJECT_ROOT%\AssistantHost\Config\settings.json"

if not exist "%OPENAI_CONFIG_SCRIPT%" (
    echo OpenAI configuration helper not found: "%OPENAI_CONFIG_SCRIPT%"
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%OPENAI_CONFIG_SCRIPT%" -SettingsPath "%OPENAI_SETTINGS%"
if errorlevel 1 (
    echo OpenAI configuration was cancelled or failed.
    exit /b 1
)

if not exist "%GAMES_DIR%" (
    echo Games directory not found: "%GAMES_DIR%"
    exit /b 1
)

echo.
echo Available games:
echo.

for /d %%D in ("%GAMES_DIR%\*") do (
    if exist "%%~fD\Source\GameModule.cpp" (
        set /a GAME_COUNT+=1
        set "GAME_!GAME_COUNT!=%%~nxD"
        echo   !GAME_COUNT!. %%~nxD
        if /I "%%~nxD"=="!REQUESTED_GAME!" set "SELECTED_GAME=%%~nxD"
    )
)

if "%GAME_COUNT%"=="0" (
    echo No valid game project was found under Games\.
    echo Each game must contain Source\GameModule.cpp.
    exit /b 1
)

if defined REQUESTED_GAME (
    if not defined SELECTED_GAME (
        echo Requested game not found: %REQUESTED_GAME%
        exit /b 1
    )
    goto :selection_complete
)

echo.
set /p "GAME_SELECTION=Select a game [1-%GAME_COUNT%]: "

for /f "delims=0123456789" %%A in ("!GAME_SELECTION!") do (
    echo Invalid selection.
    exit /b 1
)

if "!GAME_SELECTION!"=="" (
    echo Invalid selection.
    exit /b 1
)
if !GAME_SELECTION! LSS 1 (
    echo Invalid selection.
    exit /b 1
)
if !GAME_SELECTION! GTR !GAME_COUNT! (
    echo Invalid selection.
    exit /b 1
)

for %%N in (!GAME_SELECTION!) do set "SELECTED_GAME=!GAME_%%N!"
if not defined SELECTED_GAME (
    echo Invalid selection.
    exit /b 1
)

:selection_complete
set "BUILD_DIR=%GAMES_DIR%\%SELECTED_GAME%\build"
set "STOP_SESSION_SCRIPT=%PROJECT_ROOT%\StopGameSession.ps1"

echo.
echo Selected game: %SELECTED_GAME%
echo.
echo [1/4] Stopping the existing %SELECTED_GAME% session...
powershell -NoProfile -ExecutionPolicy Bypass -File "%STOP_SESSION_SCRIPT%" -BuildDirectory "%BUILD_DIR%"
if errorlevel 1 goto :failure

echo [2/4] Configuring CMake for %SELECTED_GAME%...
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" "-DGAME_PROJECT=%SELECTED_GAME%"
if errorlevel 1 goto :failure

echo [3/4] Building %CONFIGURATION%...
cmake --build "%BUILD_DIR%" --config "%CONFIGURATION%"
if errorlevel 1 goto :failure

set "LAUNCHER_PATH=%BUILD_DIR%\%CONFIGURATION%\Launcher.exe"
if not exist "%LAUNCHER_PATH%" (
    echo Launcher not found: "%LAUNCHER_PATH%"
    goto :failure
)

echo [4/4] Starting Launcher for %SELECTED_GAME%...
start "" /D "%BUILD_DIR%\%CONFIGURATION%" "%LAUNCHER_PATH%"

echo Development session started for %SELECTED_GAME%.
exit /b 0

:failure
echo Failed to start the development session for %SELECTED_GAME%.
exit /b 1
