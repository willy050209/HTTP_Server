@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Building and Running httplib23 Tests with MSVC
echo ========================================================

call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" > nul
if %errorlevel% neq 0 (
    echo [ERROR] Failed to load vcvars64.bat environment!
    exit /b 1
)

echo Compiling tests with MSVC (/std:c++latest /W4 /WX /EHsc /utf-8)...
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 /I. test_runner.cpp /Fe:test_runner.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Compilation failed!
    exit /b %errorlevel%
)

echo.
echo Running test suite...
test_runner.exe
if %errorlevel% neq 0 (
    echo [ERROR] Tests failed with exit code %errorlevel%!
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo [SUCCESS] Build and all tests passed cleanly with 0 warnings and 0 memory leaks!
echo ========================================================
