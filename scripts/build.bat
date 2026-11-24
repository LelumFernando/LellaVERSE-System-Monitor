@echo off
setlocal

echo ============================================================
echo  LellaVERSE System Monitor - Native MSVC x64 Build
echo ============================================================
echo.

cd /d "%~dp0\.."

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" (
    set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist "%VCVARS%" (
    echo [ERROR] vcvarsall.bat not found!
    exit /b 1
)

echo [OK] Using VC Environment: %VCVARS%
echo.

call "%VCVARS%" x64
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to initialize x64 build environment!
    exit /b %ERRORLEVEL%
)

if not exist "build" mkdir "build"
if not exist "release" mkdir "release"

echo [*] Compiling Windows Resources (app.rc + app.manifest + icon)...
rc.exe /nologo /fo build\app.res resources\app.rc
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Resource compilation failed!
    exit /b %ERRORLEVEL%
)

echo [*] Compiling C++17 native source files with /utf-8...
set CFLAGS=/nologo /std:c++17 /utf-8 /O2 /Oi /Ot /GL /GS- /Gy /DNDEBUG /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /EHsc /MT /Iinclude /W3 /c

set SOURCES=src\main.cpp src\overlay.cpp src\renderer.cpp src\monitor_manager.cpp src\telemetry_manager.cpp src\network_monitor.cpp src\cpu_monitor.cpp src\gpu_monitor.cpp src\memory_monitor.cpp src\power_monitor.cpp src\fps_monitor.cpp src\hardware_sensor_driver.cpp src\settings.cpp

cl %CFLAGS% /Fobuild\ %SOURCES%
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed!
    exit /b %ERRORLEVEL%
)

echo [*] Linking release executable (release\LellaVERSE_System_Monitor.exe)...
set LFLAGS=/NOLOGO /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /MACHINE:X64 /OUT:release\LellaVERSE_System_Monitor.exe

set LIBS=gdiplus.lib iphlpapi.lib ws2_32.lib pdh.lib powrprof.lib dxgi.lib d3d11.lib dwmapi.lib shell32.lib ole32.lib advapi32.lib user32.lib gdi32.lib comctl32.lib kernel32.lib setupapi.lib

link %LFLAGS% build\*.obj build\app.res %LIBS%
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Linker failed!
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================================
echo  BUILD SUCCESSFUL!
echo  Executable: release\LellaVERSE_System_Monitor.exe
echo ============================================================
echo.

for %%F in (release\LellaVERSE_System_Monitor.exe) do (
    echo [INFO] Final Binary Size: %%~zF bytes
)

echo.
exit /b 0
