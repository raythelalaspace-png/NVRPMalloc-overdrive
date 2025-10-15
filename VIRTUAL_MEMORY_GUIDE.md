# Virtual Memory Issues and Solutions

## The Problem

Fallout New Vegas is a 32-bit application with a default **2GB virtual memory limit**. When running memory-intensive mods or advanced memory allocators like rpmalloc, you may encounter:

- **"Not enough space for thread data" crashes**
- **Plugin initialization failures**
- **Memory allocation errors**
- **Game instability**

## Root Cause

32-bit applications have a 4GB virtual address space, but Windows reserves 2GB for the kernel by default, leaving only 2GB for the application. Modern allocators like rpmalloc require additional virtual memory for:
- Thread-local heaps
- Memory management structures
- Per-thread caches
- Allocation tracking

## Solutions

### Solution 1: Enable Large Address Aware (Recommended)

This increases Fallout New Vegas's virtual memory limit from 2GB to 4GB.

#### Automatic Method:
1. Run the provided PowerShell script:
   ```powershell
   .\enable_large_address_aware.ps1
   ```
2. Follow the prompts to patch FalloutNV.exe
3. The script will create a backup automatically

#### Manual Method:
1. Download a LAA patcher (like 4GB Patch or CFF Explorer)
2. Apply it to `FalloutNV.exe`
3. Verify the IMAGE_FILE_LARGE_ADDRESS_AWARE flag is set

### Solution 2: Use Conservative Plugin Settings

The updated MemoryPoolNVSE plugin now includes:
- **Conservative memory configuration** for 32-bit environments
- **Automatic fallback** when initialization fails
- **Graceful error handling** without crashing
- **Detailed logging** for troubleshooting

### Solution 3: Reduce Memory Usage

If LAA isn't an option:
- Use fewer memory-intensive mods
- Reduce texture quality/resolution
- Limit script-heavy mods
- Consider ENB alternatives with lower memory usage

## Updated Plugin Behavior

### Before Fix:
```
[ERROR] rpmalloc_initialize() failed
[CRASH] Insufficient virtual memory
```

### After Fix:
```
[05:09:21] Initializing rpmalloc with 32-bit conservative settings...
[05:09:21] rpmalloc_initialize_config() succeeded, initializing thread...
[05:09:21] RPmalloc initialized successfully with conservative settings
```

### Fallback Mode:
```
[05:09:21] RPmalloc initialization failed - plugin running in compatibility mode
[05:09:21] NVSE commands will still work, but no memory allocation replacement
[05:09:21] This is usually due to virtual memory constraints in 32-bit processes
```

## Configuration Details

### Conservative RPmalloc Settings:
- **Page Size**: System default (4KB) instead of large pages
- **Huge Pages**: Disabled to save virtual memory
- **Decommit**: Enabled to release unused virtual memory
- **Unmap on Finalize**: Enabled for clean shutdown
- **Named Pages**: For easier debugging

### Memory Interface:
- **Interface**: nullptr (use default system functions)
- **Config**: Custom conservative configuration
- **Fallback**: Graceful degradation if initialization fails

## Verification

### Check LAA Status:
Run the LAA patcher script to verify current status:
```powershell
.\enable_large_address_aware.ps1
```

### Monitor Plugin Status:
Check the log file for initialization status:
```
Data\NVSE\Plugins\MemoryPoolNVSE_RPmalloc.log
```

### Use NVSE Commands:
In-game console commands:
```
IsMemoryPoolActive        ; Should return 1 if working
IsMemoryPoolHooked        ; Shows if hooks are installed
GetMemoryPoolUsage        ; Current memory usage
```

## Technical Background

### Virtual Memory Layout (32-bit):
```
4GB ┌─────────────────────┐
    │ Kernel Space (2GB)  │ ← Reserved for Windows
2GB ├─────────────────────┤
    │ User Space (2GB)    │ ← Available for application
    │ - Executable code   │
    │ - Static data       │
    │ - Heap/Stack        │
    │ - Loaded DLLs       │
    │ - rpmalloc structs  │ ← Additional memory needed
0GB └─────────────────────┘
```

### With Large Address Aware:
```
4GB ┌─────────────────────┐
    │ User Space (4GB)    │ ← Full 4GB available!
    │ - More room for     │
    │   memory allocators │
    │ - Better mod support│
    │ - Improved stability│
0GB └─────────────────────┘
```

## Troubleshooting

### Still Getting Crashes?
1. **Verify LAA is enabled** - Check with the patcher script
2. **Check other mods** - Some mods consume excessive virtual memory
3. **Monitor memory usage** - Use Process Explorer to track VirtualBytes
4. **Check plugin log** - Look for specific error messages

### Plugin Not Loading?
1. **NVSE version** - Ensure you have NVSE 6.1.0+
2. **File placement** - DLL must be in Data\NVSE\Plugins\
3. **Dependencies** - Install VC++ Redistributable if missing

### Memory Hooks Not Working?
The plugin will still work in "compatibility mode":
- NVSE commands remain functional
- No memory allocation replacement
- Game uses standard allocator
- Full logging and statistics available

## Benefits of LAA + RPmalloc

### Performance Improvements:
- **Reduced memory fragmentation**
- **Faster allocation/deallocation**
- **Better thread scaling**
- **Lower allocation overhead**

### Stability Improvements:
- **Fewer out-of-memory crashes**
- **Better handling of memory-intensive mods**
- **More predictable memory usage patterns**
- **Reduced virtual memory pressure**

### Compatibility:
- **Works with all existing mods**
- **No impact on save games**
- **Reversible changes (backup created)**
- **Community-standard modification**

## Conclusion

Enabling Large Address Aware support is the **recommended solution** for virtual memory issues with MemoryPoolNVSE. The combination of:
1. **LAA-enabled FalloutNV.exe** (4GB virtual memory)
2. **Conservative rpmalloc settings** (optimized for 32-bit)
3. **Graceful fallback handling** (no crashes on failure)

Provides the best experience for modded Fallout New Vegas gameplay.