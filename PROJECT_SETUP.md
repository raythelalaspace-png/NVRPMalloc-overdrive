# MemoryPoolNVSE Project Setup

This document describes the complete Visual Studio project setup for MemoryPoolNVSE with RPmalloc integration.

## Project Structure

```
MemoryPoolNVSE/
├── MemoryPoolNVSE_rpmalloc.cpp      # Main RPmalloc plugin source
├── MemoryPoolNVSE_Simple.cpp        # Simplified version
├── MemoryPoolNVSE.cpp               # Legacy version
├── rpmalloc.c                       # RPmalloc implementation
├── rpmalloc.h                       # RPmalloc header
├── nvse_minimal.h                   # NVSE plugin interface
├── MemoryPoolNVSE_RPmalloc.vcxproj  # Main RPmalloc project
├── MemoryPoolNVSE_RPmalloc.def      # Export definitions
├── MemoryPoolNVSE.vcxproj           # Legacy project
├── MemoryPoolNVSE.sln               # Visual Studio solution
├── build_rpmalloc.bat               # Batch build script
├── build_rpmalloc.ps1               # PowerShell build script
└── README.md                        # Documentation
```

## Visual Studio Configuration

### Main Project: MemoryPoolNVSE_RPmalloc.vcxproj

**Project Properties:**
- **Configuration Type**: Dynamic Library (.dll)
- **Platform**: Win32 (32-bit for Fallout New Vegas)
- **Toolset**: v143 (Visual Studio 2022)
- **Character Set**: Multi-Byte
- **Runtime Library**: 
  - Debug: Multi-threaded Debug (/MTd)
  - Release: Multi-threaded (/MT)

**Preprocessor Definitions:**
```
WIN32
_WINDOWS
_USRDLL
MEMORYPOOLNVSE_EXPORTS
RUNTIME=1
_CRT_SECURE_NO_WARNINGS
ENABLE_OVERRIDE=0
NDEBUG (Release only)
```

**Source Files:**
- `MemoryPoolNVSE_rpmalloc.cpp` (C++)
- `rpmalloc.c` (C11 with experimental atomics)

**Headers:**
- `nvse_minimal.h` (NVSE interface)
- `rpmalloc.h` (RPmalloc API)

**Dependencies:**
```
psapi.lib
kernel32.lib
user32.lib
[...standard Windows libraries]
```

**Output:**
- `Release\MemoryPoolNVSE_RPmalloc.dll` (~147 KB)

## Building

### Method 1: PowerShell Script (Recommended)
```powershell
.\build_rpmalloc.ps1
```

### Method 2: Batch Script
```batch
build_rpmalloc.bat
```

### Method 3: Visual Studio
1. Open `MemoryPoolNVSE.sln`
2. Select "MemoryPoolNVSE_RPmalloc" project
3. Set to Release | Win32
4. Build → Build Solution (F7)

### Method 4: MSBuild Command Line
```cmd
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" MemoryPoolNVSE_RPmalloc.vcxproj /p:Configuration=Release /p:Platform=Win32
```

## Key Features

### RPmalloc Integration
- **Direct Integration**: RPmalloc compiled directly into DLL
- **No Dependencies**: No external DLL files needed
- **Thread Safety**: Per-thread heaps with minimal contention
- **High Performance**: Optimized for gaming workloads

### NVSE Integration
- **Plugin Interface**: Proper NVSE plugin with Query/Load functions
- **Script Commands**: 5 runtime commands for status checking
- **Messaging**: Uses NVSE messaging for proper initialization timing
- **Compatibility**: Compatible with NVSE 6+ and Fallout NV 1.4+

### Memory Hooking
- **IAT Hooking**: Replaces malloc/free/calloc/realloc in Import Address Table
- **Multiple Runtimes**: Supports various MSVC runtime versions
- **Fallback Support**: Gracefully handles hooking failures
- **Debug Logging**: Comprehensive logging for troubleshooting

### NVSE Script Commands
```
IsMemoryPoolActive        ; Returns 1 if plugin is active
IsMemoryPoolHooked        ; Returns 1 if hooks are installed
GetMemoryPoolAllocations  ; Returns total allocation count
GetMemoryPoolFrees        ; Returns total free count
GetMemoryPoolUsage        ; Returns current memory usage in MB
```

## Installation

1. **Build the plugin** using one of the methods above
2. **Copy DLL** to `Fallout New Vegas\Data\NVSE\Plugins\MemoryPoolNVSE_RPmalloc.dll`
3. **Launch game** via NVSE (`nvse_loader.exe`)
4. **Check logs** at `Data\NVSE\Plugins\MemoryPoolNVSE_RPmalloc.log`

## Troubleshooting

### Build Issues
- **MSBuild not found**: Install Visual Studio 2022 Community
- **Platform errors**: Ensure building for Win32, not x64
- **Missing headers**: Ensure all source files are present

### Runtime Issues
- **Plugin not loading**: Check NVSE version compatibility
- **Hooks not installing**: Check log for IAT debugging info
- **Memory not replaced**: Game may use different allocation method

### Log Analysis
The plugin provides detailed logging:
```
[04:49:46] === IMPORTED DLLS DEBUG ===
[04:49:46] Installing memory allocation hooks...
[04:49:46] RPmalloc initialized and tested successfully
```

## Development Notes

### Project Configurations
- **Debug**: Full debugging info, runtime checks enabled
- **Release**: Optimized for size and speed, link-time optimization

### Code Organization
- **Initialization**: Proper NVSE plugin lifecycle
- **Memory Management**: RPmalloc wrapper functions
- **IAT Hooking**: PE import table modification
- **Statistics**: Allocation tracking and reporting
- **Logging**: Timestamped diagnostic output

### Compatibility
- **32-bit only**: Fallout New Vegas is a 32-bit application
- **Static linking**: All dependencies compiled in
- **NVSE 6+**: Requires modern NVSE version
- **Windows 7+**: Modern Windows API usage

## Version History

- **Current**: RPmalloc-based version with direct integration
- **Legacy**: jemalloc-based version (deprecated)
- **Simple**: Minimal implementation for testing

This setup provides a complete, production-ready NVSE plugin with high-performance memory allocation using RPmalloc.