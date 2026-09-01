@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo [1/3] Setting up MSVC C++23 Environment
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
    )
)

echo.
echo ========================================================
echo [2/3] Compiling C++23 Server and Benchmark Runner (/O2 Release)
echo ========================================================
cl.exe /std:c++latest /O2 /EHsc /utf-8 /Iinclude benchmark\servers\cpp_httplib23\server.cpp /Fe:benchmark\servers\cpp_httplib23\server_cpp.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile C++ server!
    exit /b %errorlevel%
)

cl.exe /std:c++latest /O2 /EHsc /utf-8 /Iinclude benchmark\runner\bench_runner.cpp /Fe:benchmark\runner\bench_runner_v2.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile benchmark runner!
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo [3/3] Building ASP.NET Core Release Artifacts
echo ========================================================
dotnet build benchmark\servers\dotnet_aspnet\AspNetServer.csproj -c Release
if %errorlevel% neq 0 (
    echo [ERROR] Failed to build ASP.NET Core server!
    exit /b %errorlevel%
)

echo.
echo ========================================================
echo [SUCCESS] All C++ and .NET benchmark binaries compiled!
echo ========================================================
