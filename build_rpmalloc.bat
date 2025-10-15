@echo off
echo Building MemoryPoolNVSE with RPmalloc integration...
echo.

REM Find MSBuild
set "msbuild_path="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
    set "msbuild_path=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
    set "msbuild_path=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
) else (
    echo ERROR: MSBuild not found! Please install Visual Studio 2022 Community or Build Tools.
    pause
    exit /b 1
)

echo Using MSBuild: %msbuild_path%
echo.

REM Clean previous builds
if exist "Release" rmdir /s /q "Release"
if exist "Debug" rmdir /s /q "Debug"

REM Build Release version (32-bit for Fallout New Vegas)
echo Building Release version (Win32)...
"%msbuild_path%" MemoryPoolNVSE_RPmalloc.vcxproj /p:Configuration=Release /p:Platform=Win32 /v:minimal

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo.
echo SUCCESS: Build completed successfully!

if exist "Release\MemoryPoolNVSE_RPmalloc.dll" (
    echo.
    echo Output: Release\MemoryPoolNVSE_RPmalloc.dll
    for %%I in (Release\MemoryPoolNVSE_RPmalloc.dll) do echo Size: %%~zI bytes
    echo.
    echo To install:
    echo 1. Copy Release\MemoryPoolNVSE_RPmalloc.dll to your Fallout New Vegas Data\NVSE\Plugins\ folder
    echo 2. Launch the game via NVSE
    echo 3. Check Data\NVSE\Plugins\MemoryPoolNVSE_RPmalloc.log for status
) else (
    echo ERROR: DLL not found after build!
)

echo.
pause