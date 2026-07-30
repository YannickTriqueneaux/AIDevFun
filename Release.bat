@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0.") do set "PROJECT_ROOT=%%~fI"
set "GAMES_DIR=%PROJECT_ROOT%\Games"
set "CONFIGURATION=Release"
set "GAME_COUNT=0"
set "REQUESTED_GAME=%~1"

if not exist "%GAMES_DIR%" (
    echo Games directory not found: "%GAMES_DIR%"
    exit /b 1
)

echo.
echo Games available for release:
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
set /p "GAME_SELECTION=Select a game to release [1-%GAME_COUNT%]: "

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
set "GAME_DIR=%GAMES_DIR%\%SELECTED_GAME%"
set "BUILD_DIR=%GAME_DIR%\build"
set "RELEASE_DIR=%GAME_DIR%\Release"
set "RELEASE_EXE=%RELEASE_DIR%\%SELECTED_GAME%.exe"

echo.
echo Releasing game: %SELECTED_GAME%
echo.
echo [1/3] Closing an existing released executable...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_ROOT%\StopGameSession.ps1" -BuildDirectory "%RELEASE_DIR%"
if errorlevel 1 goto :failure

echo [2/3] Configuring CMake...
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" "-DGAME_PROJECT=%SELECTED_GAME%"
if errorlevel 1 goto :failure

echo [3/3] Building the monolithic Release executable...
cmake --build "%BUILD_DIR%" --config "%CONFIGURATION%" --target GameRelease
if errorlevel 1 goto :failure

if not exist "%RELEASE_EXE%" (
    echo Release executable not found: "%RELEASE_EXE%"
    goto :failure
)

echo.
echo Release completed successfully.
echo Executable:
echo %RELEASE_EXE%
exit /b 0

:failure
echo.
echo Release failed for %SELECTED_GAME%.
exit /b 1
