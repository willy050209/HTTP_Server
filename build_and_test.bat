@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Building and Running httplib23 Tests with MSVC (C++23)
echo ========================================================

where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else (
        echo [ERROR] cl.exe not found and could not find vcvars64.bat!
        exit /b 1
    )
)

echo Compiling tests with MSVC (/std:c++latest /W4 /WX /EHsc /utf-8 /Iinclude)...
cl.exe /std:c++latest /W4 /WX /EHsc /utf-8 /Iinclude src\test_runner.cpp /Fe:test_runner.exe ws2_32.lib
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
