# Changelog

All notable changes to MemoryPoolNVSE will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-10-15

### Added
- Initial release of MemoryPoolNVSE
- jemalloc integration for high-performance memory allocation
- NVSE plugin architecture with proper lifecycle management
- Comprehensive logging system with timestamped entries
- Fallback support to system allocator when jemalloc is unavailable
- Export functions for other mods: `MemoryPoolMalloc`, `MemoryPoolFree`, `GetMemoryPoolStatus`
- Version compatibility checking for NVSE and game runtime
- Thread-safe memory allocation (relies on underlying allocator)
- Minimal NVSE interface to reduce dependencies
- Proper Visual Studio project configuration
- Documentation and build instructions

### Technical Details
- Built with Visual Studio 2022
- Compatible with NVSE 6.1.0+ and Fallout New Vegas 1.4.0.525+
- 32-bit (Win32) architecture support
- MIT License

### Files Included
- `MemoryPoolNVSE.cpp` - Main plugin source code
- `nvse_minimal.h` - Minimal NVSE plugin interface
- `MemoryPoolNVSE.def` - Export definitions
- `MemoryPoolNVSE.vcxproj` - Visual Studio project file
- `MemoryPoolNVSE.dll` - Pre-built plugin binary
- Documentation: README.md, LICENSE, CHANGELOG.md