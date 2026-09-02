@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo [1/3] Setting up MSVC Environment
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
echo [2/3] Compiling Multi-Standard C++ Servers and Runner (/O2 Release)
echo ========================================================
echo Compiling C++23 Server (/std:c++latest)...
cl.exe /nologo /std:c++latest /O2 /EHsc /utf-8 /Iinclude benchmark\servers\cpp_httplib23\server.cpp /Fe:benchmark\servers\cpp_httplib23\server_cpp23.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile C++23 server!
    exit /b %errorlevel%
)
copy /y benchmark\servers\cpp_httplib23\server_cpp23.exe benchmark\servers\cpp_httplib23\server_cpp.exe >nul

echo Compiling C++20 Server (/std:c++20)...
cl.exe /nologo /std:c++20 /O2 /EHsc /utf-8 /Iinclude benchmark\servers\cpp_httplib23\server.cpp /Fe:benchmark\servers\cpp_httplib23\server_cpp20.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile C++20 server!
    exit /b %errorlevel%
)

echo Compiling C++17 Server (/std:c++17)...
cl.exe /nologo /std:c++17 /O2 /EHsc /utf-8 /Iinclude benchmark\servers\cpp_httplib23\server.cpp /Fe:benchmark\servers\cpp_httplib23\server_cpp17.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile C++17 server!
    exit /b %errorlevel%
)

echo Compiling C++14/11 Server (/std:c++14)...
cl.exe /nologo /std:c++14 /O2 /EHsc /utf-8 /Iinclude benchmark\servers\cpp_httplib23\server.cpp /Fe:benchmark\servers\cpp_httplib23\server_cpp14.exe ws2_32.lib
if %errorlevel% neq 0 (
    echo [ERROR] Failed to compile C++14 server!
    exit /b %errorlevel%
)

echo Compiling Benchmark Client Runner (/std:c++latest)...
cl.exe /nologo /std:c++latest /O2 /EHsc /utf-8 /Iinclude benchmark\runner\bench_runner.cpp /Fe:benchmark\runner\bench_runner_v2.exe ws2_32.lib
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
echo [SUCCESS] All C++23/20/17/14 and .NET benchmark binaries compiled!
echo ========================================================
