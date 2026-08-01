@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PROJECT_ROOT=%~dp0."
set "GAMES_DIR=%PROJECT_ROOT%\Games"
set "BASE_GAME_DIR=%GAMES_DIR%\BaseGame"
set "CONFIGURATION=Debug"
set "GAME_COUNT=0"
set "REQUESTED_GAME="
set "PROFILE_ENABLED=OFF"
set "OPENAI_CONFIG_SCRIPT=%PROJECT_ROOT%\AssistantHost\Config\ConfigureOpenAI.ps1"
set "OPENAI_SETTINGS=%PROJECT_ROOT%\AssistantHost\Config\settings.json"

:parse_arguments
if "%~1"=="" goto :arguments_parsed
if /I "%~1"=="-profile" (
    set "PROFILE_ENABLED=ON"
) else (
    if defined REQUESTED_GAME (
        echo Unexpected argument: %~1
        exit /b 1
    )
    set "REQUESTED_GAME=%~1"
)
shift
goto :parse_arguments

:arguments_parsed

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

set /a CREATE_GAME_OPTION=GAME_COUNT+1
echo   !CREATE_GAME_OPTION!. Create a new game
echo.
set /p "GAME_SELECTION=Select a game [1-!CREATE_GAME_OPTION!]: "

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
if !GAME_SELECTION! GTR !CREATE_GAME_OPTION! (
    echo Invalid selection.
    exit /b 1
)

if "!GAME_SELECTION!"=="!CREATE_GAME_OPTION!" (
    call :create_new_game
    if errorlevel 1 exit /b 1
    goto :selection_complete
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
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" "-DGAME_PROJECT=%SELECTED_GAME%" "-DENGINE_PROFILE_ENABLED=%PROFILE_ENABLED%" "-DTRACY_SOURCE_DIR=%PROJECT_ROOT%\..\tracy-master"
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
if /I "%PROFILE_ENABLED%"=="ON" (
    set "TRACY_PROFILER_SOURCE=%PROJECT_ROOT%\..\tracy-master\profiler"
    set "TRACY_PROFILER_BUILD=%SystemDrive%\AITesterTracyProfiler"
    echo Building Tracy Profiler...
    cmake -S "!TRACY_PROFILER_SOURCE!" -B "!TRACY_PROFILER_BUILD!" "-DGIT_EXECUTABLE=%PROJECT_ROOT%\Tools\TracyGit.cmd" -DCMAKE_DISABLE_FIND_PACKAGE_Git=FALSE
    if errorlevel 1 goto :failure
    cmake --build "!TRACY_PROFILER_BUILD!" --config Release
    if errorlevel 1 goto :failure
    set "TRACY_PROFILER_PATH=!TRACY_PROFILER_BUILD!\Release\tracy-profiler.exe"
    if not exist "!TRACY_PROFILER_PATH!" (
        echo Tracy Profiler not found: "!TRACY_PROFILER_PATH!"
        goto :failure
    )
    start "" /D "!TRACY_PROFILER_BUILD!\Release" "!TRACY_PROFILER_PATH!" -a 127.0.0.1
)
start "" /D "%BUILD_DIR%\%CONFIGURATION%" "%LAUNCHER_PATH%"

echo Development session started for %SELECTED_GAME%.
exit /b 0

:failure
echo Failed to start the development session for %SELECTED_GAME%.
exit /b 1

:create_new_game
if not exist "%BASE_GAME_DIR%\Source\GameModule.cpp" (
    echo Base game template not found: "%BASE_GAME_DIR%"
    exit /b 1
)

echo.
set "NEW_GAME_NAME="
set /p "NEW_GAME_NAME=Name for the new game: "
if not defined NEW_GAME_NAME (
    echo No game name was provided.
    exit /b 1
)

echo Validating game name...
powershell -NoProfile -Command ^
    "$n=$env:NEW_GAME_NAME; $reserved=@('CON','PRN','AUX','NUL','COM1','COM2','COM3','COM4','COM5','COM6','COM7','COM8','COM9','LPT1','LPT2','LPT3','LPT4','LPT5','LPT6','LPT7','LPT8','LPT9'); $plain=$n -match '^[A-Za-z0-9]+$'; $hidden=$n -match '^__[A-Za-z0-9]+$'; $deviceName=$n.TrimStart('_').ToUpperInvariant(); if ([string]::IsNullOrWhiteSpace($n) -or $n.Length -gt 64 -or (-not $plain -and -not $hidden) -or $reserved -contains $deviceName) { exit 1 }"
if errorlevel 1 (
    echo Invalid game name.
    echo Use 1-64 letters and numbers only.
    exit /b 1
)

for /d %%D in ("%GAMES_DIR%\*") do (
    if /I "%%~nxD"=="!NEW_GAME_NAME!" (
        echo A game or folder with that name already exists:
        echo "%%~fD"
        exit /b 1
    )
)

set "NEW_GAME_DIR=%GAMES_DIR%\!NEW_GAME_NAME!"
if exist "!NEW_GAME_DIR!" (
    echo A game or folder with that name already exists:
    echo "!NEW_GAME_DIR!"
    exit /b 1
)

echo Creating Games\!NEW_GAME_NAME! from BaseGame...
robocopy "%BASE_GAME_DIR%" "!NEW_GAME_DIR!" /E /XD build Release /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
    echo Failed to copy BaseGame into Games\!NEW_GAME_NAME!.
    exit /b 1
)

if not exist "!NEW_GAME_DIR!\Source\GameModule.cpp" (
    echo The new game copy is incomplete.
    exit /b 1
)

if "!NEW_GAME_NAME:~0,2!"=="__" (
    set "LOCAL_GIT_EXCLUDE=%PROJECT_ROOT%\.git\info\exclude"
    if exist "%PROJECT_ROOT%\.git\info" (
        powershell -NoProfile -Command ^
            "$file=$env:LOCAL_GIT_EXCLUDE; $line='/Games/'+$env:NEW_GAME_NAME+'/'; $exists=Test-Path -LiteralPath $file; if (-not $exists -or -not (Get-Content -LiteralPath $file | Where-Object { $_ -ieq $line })) { Add-Content -LiteralPath $file -Value $line }"
        if errorlevel 1 (
            echo Warning: the game was created, but its local Git exclusion failed.
        ) else (
            echo Added Games\!NEW_GAME_NAME! to the local Git exclude file.
        )
    ) else (
        echo Warning: no local .git\info directory was found.
        echo The game was created but could not be hidden from Git.
    )
)

set "SELECTED_GAME=!NEW_GAME_NAME!"
echo Created game: !SELECTED_GAME!
exit /b 0
