@echo off
setlocal
set SCRIPT_DIR=%~dp0
cmake -S "%SCRIPT_DIR%app" -B "%SCRIPT_DIR%app\build" -DCMAKE_BUILD_TYPE=Release
cmake --build "%SCRIPT_DIR%app\build" --parallel
"%SCRIPT_DIR%app\build\copilot-buddy.exe"
