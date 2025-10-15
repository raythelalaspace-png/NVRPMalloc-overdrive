# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Build Commands

### Quick Build (Recommended)
```powershell
.\build_rpmalloc.ps1
```

### Alternative Build Methods
```batch
# Batch script
build_rpmalloc.bat

# Visual Studio
start MemoryPoolNVSE.sln
# Then: Build → Build Solution (F7)

# MSBuild Command Line
"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" MemoryPoolNVSE_RPmalloc.vcxproj /p:Configuration=Release /p:Platform=Win32
```

### Build Requirements
- Visual Studio 2022 (Community/Professional/Enterprise)
- Windows 10/11 SDK
- **Platform**: Win32 (32-bit) only - Fallout New Vegas is 32-bit
- **Configuration**: Release for distribution, Debug for development

### Testing Installation
1. Copy `Release\MemoryPoolNVSE_RPmalloc.dll` to `FalloutNV\Data\NVSE\Plugins\`
2. Launch via `nvse_loader.exe`
3. Check log at `Data\NVSE\Plugins\MemoryPoolNVSE_RPmalloc.log`

## High-Level Architecture

### Core Components

#### 1. **NVSE Plugin System**
- Entry points: `NVSEPlugin_Query()`, `NVSEPlugin_Load()`
- Message handling for proper initialization timing (`PostPostLoad`)
- 5 script commands for runtime status checking
- Compatible with NVSE 6.1.0+ and Fallout NV 1.4.0.525+

#### 2. **RPmalloc Integration**
- **Direct source integration**: `rpmalloc.c` compiled into DLL (no external dependencies)
- **Thread-safe design**: Per-thread heaps with minimal lock contention
- **Performance optimized**: 16-byte aligned allocations, automatic huge page detection
- **Memory classes**: Small (0-4KB), Medium (4KB-256KB), Large (256KB+)

#### 3. **IAT Hooking System (Safe Mode)**
- **Standard C runtime targeting**: Only hooks malloc/free/calloc/realloc from legitimate runtime DLLs
- **Multiple runtime support**: Targets msvcrt.dll, msvcr110.dll, msvcr120.dll, ucrtbase.dll, msvcr90.dll, msvcr100.dll
- **Safety-first approach**: Never hooks Windows heap functions (HeapAlloc/HeapFree) that have incompatible signatures
- **Graceful degradation**: Works as script-only plugin when IAT hooking fails

#### 4. **Memory Management Pipeline**
```
Game malloc() call → IAT redirect → hooked_malloc() → rpmalloc() → TouchMemoryPages() → Statistics tracking
```

#### 5. **Statistics & Monitoring**
- **Real-time tracking**: Allocation counts, sizes, peak usage, failures
- **Memory leak detection**: Up to 32,768 tracked allocations
- **Performance metrics**: Average allocation size, fragmentation analysis
- **Comprehensive logging**: Timestamped debug output to dedicated log file

### Project Structure

#### Primary Projects
- **MemoryPoolNVSE_RPmalloc** (Main): Current RPmalloc-based implementation
- **MemoryPoolNVSE_Legacy**: Historical version (deprecated)

#### Key Files
- `MemoryPoolNVSE_rpmalloc.cpp`: Main plugin implementation with IAT hooking
- `rpmalloc.c`/`rpmalloc.h`: High-performance allocator source  
- `nvse_minimal.h`: NVSE plugin interface definitions
- `MemoryPoolNVSE_RPmalloc.def`: DLL export definitions

### Technical Specifications

#### Memory Allocator Features
- **Lock-free fast path**: Atomic operations for common allocations
- **Cross-thread deallocation**: Safe memory return across thread boundaries  
- **Virtual memory management**: Direct VirtualAlloc/VirtualFree usage
- **Windows-specific optimizations**: FLS callbacks, large page support

#### Performance Optimizations
- **Page commitment**: TouchMemoryPages() forces physical memory residency
- **Executable code section**: 64KB dedicated PE section for hook trampolines
- **Memory alignment**: 64-byte alignment for large allocations (≥1MB)
- **Statistics caching**: Efficient tracking with minimal overhead

#### Compiler Configuration
- **Runtime Library**: Multi-threaded static (/MT for Release, /MTd for Debug)
- **C11 Atomics**: `/experimental:c11atomics` flag required
- **Preprocessor**: `RUNTIME=1`, `ENABLE_OVERRIDE=0`, `_CRT_SECURE_NO_WARNINGS`
- **Dependencies**: psapi.lib for process enumeration

## Development Notes

### Code Organization Patterns
- **Initialization phases**: Code section → RPmalloc init → IAT hooking → NVSE integration
- **Error handling**: Comprehensive fallback chains with detailed logging
- **Thread safety**: Critical sections for global state, atomic operations for statistics
- **Memory tracking**: Global allocation map with size tracking for leak detection

### Key Implementation Details
- All builds must target **Win32 platform** (Fallout NV is 32-bit)
- RPmalloc compiled as C11 with experimental atomics support with **minimal TLS configuration**
- IAT hooks installed during NVSE PostPostLoad message for proper timing
- Memory statistics tracked globally with thread-safe atomic updates
- Large Address Aware flag set at runtime for 4GB virtual memory access
- **TLS optimizations**: Statistics disabled, decommit disabled, first-class heaps disabled to prevent "not enough thread data space" errors

### Testing Strategy  
- Plugin provides NVSE script commands: `IsMemoryPoolActive`, `IsMemoryPoolHooked`, `GetMemoryPoolAllocations`, `GetMemoryPoolFrees`, `GetMemoryPoolUsage`
- Log file provides detailed initialization sequence and runtime statistics
- Memory leak detection reports remaining allocations on shutdown
- Performance monitoring tracks allocation rates and memory fragmentation

### Compatibility Requirements
- **NVSE**: Minimum 6.1.0, uses messaging system for initialization
- **Game**: Fallout New Vegas 1.4.0.525+, 32-bit architecture only
- **Windows**: 7+ required, optimized for Windows 10/11
- **Visual C++ Runtime**: Statically linked to avoid DLL dependencies

## Troubleshooting

### "Not Enough Thread Data Space" Error
This error occurs when the Windows process has exhausted its Thread Local Storage (TLS) slots (limited to 128 per process).

**Current Fix Applied:**
- Disabled rpmalloc statistics (`ENABLE_STATISTICS=0`)
- Disabled first-class heaps (`RPMALLOC_FIRST_CLASS_HEAPS=0`) 
- Disabled memory decommit (`ENABLE_DECOMMIT=0`)
- Progressive fallback: Custom interface → Config-only → Absolute minimal

**If Error Persists:**
1. Reduce number of NVSE plugins (each may use TLS slots)
2. Check log file for which initialization level succeeded
3. Consider using system allocator fallback if rpmalloc fails completely

**Log Indicators:**
```
Success: Minimal interface configuration initialized  # Best case
Success: Config-only initialization                   # Good
Success: Absolute minimal default configuration       # Acceptable
ERROR: Even default rpmalloc init failed             # Use fewer plugins
```

### IAT Hooking Not Working
This is normal for many games including Fallout New Vegas and indicates the game doesn't import standard C runtime functions through its IAT.

**Expected Log Messages:**
```
No functions hooked from msvcrt.dll
No functions hooked from msvcr110.dll
...
No C runtime functions found in IAT - this is normal for some games
RPmalloc will still work for NVSE script commands and manual allocations
Registered 5 NVSE script commands
```

**This is NOT an error** - the plugin will work in script-only mode:
- NVSE script commands fully functional
- Manual allocations through other plugins work
- rpmalloc properly initialized and available
- Game stability maintained (no unsafe hooks)

**DO NOT** attempt to hook Windows heap functions - this causes crashes!
