@echo off
if /I "%~1"=="-C" if /I "%~3"=="log" (
    echo namespace tracy { static inline const char* GitRef = "unknown"; }
    exit /b 0
)
"C:\Program Files\Git\cmd\git.exe" %*
