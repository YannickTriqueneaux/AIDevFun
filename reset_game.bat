@echo off
setlocal

set "PROJECT_ROOT=%~dp0."
set "GAMES_DIR=%PROJECT_ROOT%\Games"
set "BASE_GAME_DIR=%GAMES_DIR%\BaseGame"
set "BACKUP_ROOT=%GAMES_DIR%\.Backups"

if not exist "%BASE_GAME_DIR%\Source\GameModule.cpp" (
    echo Base game not found: "%BASE_GAME_DIR%"
    exit /b 1
)

set "GAME_NAME=%~1"
if not defined GAME_NAME (
    set /p "GAME_NAME=Game project to reset: "
)

if not defined GAME_NAME (
    echo No game project was specified.
    exit /b 1
)
if /I "%GAME_NAME%"=="BaseGame" (
    echo BaseGame is the protected template and cannot be reset.
    exit /b 1
)

set "ACTIVE_GAME_DIR=%GAMES_DIR%\%GAME_NAME%"
if not exist "%ACTIVE_GAME_DIR%\Source\GameModule.cpp" (
    echo Game project not found: "%ACTIVE_GAME_DIR%"
    exit /b 1
)

echo This will replace Games\%GAME_NAME% with a clean copy of Games\BaseGame.
echo The current project will be moved to Games\.Backups first.
choice /C YN /N /M "Continue? [Y/N] "
if errorlevel 2 (
    echo Reset cancelled.
    exit /b 0
)

set "BUILD_DIR=%ACTIVE_GAME_DIR%\build"
echo Stopping the %GAME_NAME% session...
powershell -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_ROOT%\StopGameSession.ps1" -BuildDirectory "%BUILD_DIR%"
if errorlevel 1 goto :failure

if not exist "%BACKUP_ROOT%" mkdir "%BACKUP_ROOT%"
if errorlevel 1 goto :failure

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "BACKUP_STAMP=%%I"
set "BACKUP_DIR=%BACKUP_ROOT%\%GAME_NAME%_%BACKUP_STAMP%"

echo Backing up Games\%GAME_NAME%...
move "%ACTIVE_GAME_DIR%" "%BACKUP_DIR%" >nul
if errorlevel 1 goto :failure
echo Backup created at: "%BACKUP_DIR%"

echo Restoring the base game...
xcopy "%BASE_GAME_DIR%\*" "%ACTIVE_GAME_DIR%\" /E /I /H /K /Y >nul
if errorlevel 1 goto :restore_failure

echo Configuring CMake for %GAME_NAME%...
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" "-DGAME_PROJECT=%GAME_NAME%"
if errorlevel 1 goto :failure

echo Game reset successfully. Rebuilding and launching...
call "%PROJECT_ROOT%\dev.bat" "%GAME_NAME%"
exit /b %errorlevel%

:restore_failure
echo Failed to copy BaseGame into Games\%GAME_NAME%.
echo Your previous game remains available at "%BACKUP_DIR%".
exit /b 1

:failure
echo Game reset failed.
exit /b 1
