# MemoryPoolNVSE

A high-performance memory allocator NVSE plugin for Fallout New Vegas that integrates jemalloc for improved memory management and reduced fragmentation.

## Features

- 🚀 **jemalloc Integration**: Uses the high-performance jemalloc memory allocator
- 🔧 **NVSE Plugin Architecture**: Proper NVSE plugin with lifecycle management
- 📝 **Comprehensive Logging**: Detailed logging system for debugging and monitoring
- 🔄 **Fallback Support**: Falls back to system allocator if jemalloc is unavailable
- 🎯 **Export Functions**: Provides memory allocation functions for other mods
- ✅ **Version Checking**: Ensures compatibility with NVSE and game versions

## Installation

### Prerequisites

- [NVSE (New Vegas Script Extender)](https://nvse.silverlock.org/) - Required
- Fallout New Vegas (patched to latest version)
- (Optional) `jemalloc.dll` for enhanced memory performance

### Installation Steps

1. **Download the Plugin**:
   - Download `MemoryPoolNVSE.dll` from the releases page

2. **Install NVSE Plugin**:
   ```
   Data/NVSE/Plugins/MemoryPoolNVSE.dll
   ```

3. **Optional - Install jemalloc**:
   - Place `jemalloc.dll` in your Fallout New Vegas game directory (next to `FalloutNV.exe`)
   - If not present, the plugin will use the system's default allocator

4. **Launch Game**:
   - Launch Fallout New Vegas through NVSE

## Configuration

No configuration files are needed. The plugin automatically:
- Detects and loads jemalloc if available
- Creates log files in the game directory
- Provides status information through logging

## Usage

### For End Users

Simply install and run. The plugin works transparently in the background to improve memory allocation performance.

### For Mod Developers

The plugin exports functions that other mods can use:

```cpp
// C/C++ Usage
extern "C" {
    void* MemoryPoolMalloc(size_t size);
    void MemoryPoolFree(void* ptr);
    const char* GetMemoryPoolStatus();
}
```

#### Function Reference

- **`MemoryPoolMalloc(size_t size)`**: Allocates memory using jemalloc or system allocator
- **`MemoryPoolFree(void* ptr)`**: Frees memory allocated by MemoryPoolMalloc
- **`GetMemoryPoolStatus()`**: Returns status string indicating which allocator is in use

### Log Files

The plugin creates `MemoryPoolNVSE.log` in your game directory with detailed information:

```
[2025-01-01 12:00:00] MemoryPoolNVSE: Plugin Query called
[2025-01-01 12:00:00] MemoryPoolNVSE: Plugin Load called
[2025-01-01 12:00:00] MemoryPoolNVSE: Successfully registered for NVSE messaging
[2025-01-01 12:00:01] MemoryPoolNVSE: Received PostPostLoad message - All plugins loaded
[2025-01-01 12:00:01] MemoryPoolNVSE: Starting initialization...
[2025-01-01 12:00:01] MemoryPoolNVSE: Attempting to load jemalloc.dll...
[2025-01-01 12:00:01] MemoryPoolNVSE: SUCCESS - jemalloc.dll loaded successfully
[2025-01-01 12:00:01] MemoryPoolNVSE: Memory pool initialization COMPLETE - All systems operational
```

## Building from Source

### Requirements

- Visual Studio 2022 (or compatible)
- Windows 10/11 SDK
- NVSE headers (included in repository)

### Build Steps

1. **Clone Repository**:
   ```bash
   git clone <repository-url>
   cd MemoryPoolNVSE
   ```

2. **Open in Visual Studio**:
   ```bash
   start MemoryPoolNVSE.sln
   ```

3. **Build Configuration**:
   - **Debug/Win32**: For testing and development
   - **Release/Win32**: For distribution
   - **x64 builds**: Available but not recommended for Fallout New Vegas

4. **Build Project**:
   - Press F7 or Build → Build Solution
   - Output DLL will be in `Debug/` or `Release/` folder

### Project Structure

```
MemoryPoolNVSE/
├── MemoryPoolNVSE.cpp      # Main plugin source code
├── nvse_minimal.h          # Minimal NVSE plugin interface
├── MemoryPoolNVSE.def      # Export definitions
├── MemoryPoolNVSE.vcxproj  # Visual Studio project file
├── README.md               # This file
└── .gitignore              # Git ignore rules
```

## Technical Details

### Memory Allocator Priority

1. **jemalloc** (if available): High-performance, low-fragmentation allocator
2. **System allocator** (fallback): Standard C runtime malloc/free

### NVSE Integration

- Uses NVSE messaging system for proper initialization timing
- Registers for `PostPostLoad` message to ensure all plugins are loaded
- Provides proper plugin query and load functions
- Version compatibility checking for NVSE and game runtime

### Thread Safety

The plugin relies on the underlying allocator's thread safety:
- jemalloc is thread-safe by design
- System allocator is thread-safe in modern C runtimes

### Performance Characteristics

- **jemalloc**: Optimized for multi-threaded applications, reduced memory fragmentation
- **Low overhead**: Minimal performance impact when jemalloc is unavailable
- **Lazy loading**: Only initializes when needed

## Troubleshooting

### Plugin Not Loading

1. **Check NVSE Installation**:
   - Ensure NVSE is properly installed
   - Launch game via `nvse_loader.exe`

2. **Check File Placement**:
   ```
   Fallout New Vegas/Data/NVSE/Plugins/MemoryPoolNVSE.dll
   ```

3. **Check Log File**:
   - Look for `MemoryPoolNVSE.log` in game directory
   - Check for error messages

### jemalloc Not Loading

1. **Check jemalloc.dll Placement**:
   ```
   Fallout New Vegas/jemalloc.dll  (next to FalloutNV.exe)
   ```

2. **Check Log Messages**:
   - Plugin will log success/failure of jemalloc loading
   - Will fall back to system allocator if jemalloc fails

3. **Architecture Mismatch**:
   - Ensure jemalloc.dll is 32-bit (x86) to match Fallout New Vegas

### Common Issues

- **Missing NVSE**: Plugin requires NVSE to function
- **Wrong Architecture**: Use 32-bit (Win32) builds only
- **Permissions**: Ensure game directory is writable for log files

## Compatibility

### NVSE Versions

- **Minimum**: NVSE 6.1.0 (0x06010000)
- **Recommended**: Latest stable NVSE version

### Game Versions

- **Minimum**: Fallout New Vegas 1.4.0.525
- **Recommended**: Latest patched version

### Known Compatible Mods

This plugin is designed to be compatible with all mods that don't interfere with memory allocation.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

### Coding Standards

- Follow existing code style
- Add comprehensive logging for new features
- Maintain backwards compatibility
- Test on both Debug and Release builds

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- [NVSE Team](https://nvse.silverlock.org/) - For the excellent script extender
- [jemalloc](https://jemalloc.net/) - For the high-performance memory allocator
- Fallout New Vegas modding community

## Support

- **Issues**: Report bugs via GitHub Issues
- **Discussions**: Use GitHub Discussions for questions and feedback
- **Documentation**: Check this README and inline code comments

---

**Note**: This plugin is provided as-is. Always backup your saves before installing new mods.