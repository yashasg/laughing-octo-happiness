@echo off
:: CI build script — Windows (MSVC)
:: Builds copilot-buddy in Release mode. Does not run the binary.
setlocal enabledelayedexpansion

set REPO_ROOT=%~dp0
set BUILD_DIR=%REPO_ROOT%app\build

cmake -S "%REPO_ROOT%app" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

echo Build succeeded: %BUILD_DIR%\Release\copilot-buddy.exe
