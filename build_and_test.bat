@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Building and Running httplib23 Multi-Standard Test Matrix
echo (C++23 / C++20 / C++17 / C++14-11 Compatibility)
echo ========================================================

where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" > nul
    ) else (
        echo [ERROR] cl.exe not found and could not find vcvars64.bat!
        exit /b 1
    )
)

echo.
echo ========================================================
echo [1/4] Testing with MSVC C++23 (/std:c++latest)
echo ========================================================
cl.exe /nologo /std:c++latest /W4 /WX /EHsc /utf-8 /Iinclude src\test_runner.cpp /Fe:test_runner_cpp23.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] C++23 Compilation failed!
    exit /b %errorlevel%
)
test_runner_cpp23.exe
if %errorlevel% neq 0 (
    echo [ERROR] C++23 Tests failed!
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo [2/4] Testing with MSVC C++20 (/std:c++20)
echo ========================================================
cl.exe /nologo /std:c++20 /W4 /WX /EHsc /utf-8 /Iinclude src\test_runner.cpp /Fe:test_runner_cpp20.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] C++20 Compilation failed!
    exit /b %errorlevel%
)
test_runner_cpp20.exe
if %errorlevel% neq 0 (
    echo [ERROR] C++20 Tests failed!
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo [3/4] Testing with MSVC C++17 (/std:c++17)
echo ========================================================
cl.exe /nologo /std:c++17 /W4 /WX /EHsc /utf-8 /Iinclude src\test_runner.cpp /Fe:test_runner_cpp17.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] C++17 Compilation failed!
    exit /b %errorlevel%
)
test_runner_cpp17.exe
if %errorlevel% neq 0 (
    echo [ERROR] C++17 Tests failed!
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo [4/4] Testing with MSVC C++14 (/std:c++14 - C++11 Mode)
echo ========================================================
cl.exe /nologo /std:c++14 /W4 /WX /EHsc /utf-8 /Iinclude src\test_runner.cpp /Fe:test_runner_cpp14.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] C++14/11 Compilation failed!
    exit /b %errorlevel%
)
test_runner_cpp14.exe
if %errorlevel% neq 0 (
    echo [ERROR] C++14/11 Tests failed!
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo [ALL STANDARDS PASSED] C++23, C++20, C++17, C++14/11 passed 100% cleanly!
echo ========================================================
