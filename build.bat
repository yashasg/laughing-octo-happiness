@echo off
:: CI build script — Windows (MSVC)
:: Usage: build.bat [-test]
::   -test  also build and run unit tests, writing results to test-results.xml
setlocal enabledelayedexpansion

set REPO_ROOT=%~dp0
set BUILD_DIR=%REPO_ROOT%app\build
set BUILD_TESTS=OFF

for %%A in (%*) do (
    if "%%A"=="-test" set BUILD_TESTS=ON
)

cmake -S "%REPO_ROOT%app" -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=%BUILD_TESTS%
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

echo Build succeeded: %BUILD_DIR%\Release\copilot-buddy.exe

if "%BUILD_TESTS%"=="ON" (
    cmake --build "%BUILD_DIR%" --target copilot-buddy-tests --config Release --parallel
    if errorlevel 1 exit /b 1

    "%BUILD_DIR%\tests\Release\copilot-buddy-tests.exe" --gtest_output=xml:"%REPO_ROOT%test-results.xml"
    if errorlevel 1 exit /b 1

    echo Tests passed.
)
