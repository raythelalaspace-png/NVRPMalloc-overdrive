# MemoryPoolNVSE Professional Logging System

## Overview

The MemoryPoolNVSE plugin now features a comprehensive professional logging system that replaces the basic SimpleLog with advanced features including log levels, duplicate message detection, and detailed JEmalloc memory statistics tracking.

## Features Implemented

### 1. Professional Logging System

- **Log Levels**: DEBUG, INFO, WARN, ERROR with appropriate filtering
- **Duplicate Detection**: Prevents spam by counting repeated identical messages
- **Timestamp Formatting**: Professional time stamps in HH:MM:SS format
- **Thread Safety**: Critical section protection for multi-threaded environments
- **Multiple Log Files**: Separate main log and statistics log files

### 2. JEmalloc Memory Statistics Tracking

#### Statistics Collected:
- **Total Allocated**: Cumulative bytes allocated through JEmalloc
- **Total Freed**: Cumulative bytes freed through JEmalloc
- **Current Usage**: Real-time memory usage (allocated - freed)
- **Peak Usage**: Maximum memory usage reached during session
- **Lowest Usage**: Minimum memory usage reached during session
- **Allocation Count**: Total number of allocation calls
- **Free Count**: Total number of free calls
- **Average Allocation Size**: Mean size per allocation
- **Average Usage Rate**: Memory usage per second over time

#### CSV Statistics Log:
- File: `MemoryPoolNVSE_JemallocStats.log`
- Format: Timestamp,TotalAllocated,TotalFreed,CurrentUsed,PeakUsed,LowestUsed,AllocCount,FreeCount,AvgAllocSize,AvgUsage
- Real-time updates on each allocation/deallocation

### 3. Enhanced Logging Output

#### Main Log File Features:
- **Structured Headers**: Clear section separators for different initialization phases
- **Reduced Verbosity**: Debug-level logs for detailed information, INFO level for important events
- **Error Classification**: Appropriate log levels (ERROR, WARN, INFO, DEBUG)
- **No Repeated Messages**: Duplicate detection prevents log spam
- **Professional Formatting**: Consistent message structure and formatting

#### Log Files Generated:
1. `MemoryPoolNVSE.log` - Main plugin log with all system events
2. `MemoryPoolNVSE_JemallocStats.log` - CSV format memory statistics log

### 4. Integration Points

#### Memory Allocation Tracking:
- `MemoryPoolMalloc()`: Tracks allocation size and updates statistics
- `MemoryPoolFree()`: Tracks deallocation events
- `UpdateJemallocStats()`: Core statistics update function
- `WriteJemallocStatsToLog()`: Real-time CSV logging

#### Lifecycle Management:
- **Initialization**: `InitializeLogging()` called during plugin query
- **Statistics**: `LogJemallocStatsSummary()` provides human-readable summaries
- **Cleanup**: `CleanupLogging()` ensures proper file closure on exit

### 5. Log Level Configuration

Current configuration:
- **Minimum Level**: INFO (can be adjusted by changing `g_logState.minLogLevel`)
- **DEBUG**: Detailed module enumeration, detailed pool statistics
- **INFO**: Important system events, initialization phases, success messages
- **WARN**: Non-fatal issues, fallback operations
- **ERROR**: Critical failures, version incompatibilities

## Usage Examples

### Reading Statistics Summary
The plugin logs comprehensive statistics at startup and shutdown:

```
[14:30:15] [INFO ] === JEMALLOC MEMORY STATISTICS SUMMARY ===
[14:30:15] [INFO ] Total allocated: 1048576 bytes (1.00 MB)
[14:30:15] [INFO ] Total freed: 524288 bytes (0.50 MB)
[14:30:15] [INFO ] Current usage: 524288 bytes (0.50 MB)
[14:30:15] [INFO ] Peak usage: 1048576 bytes (1.00 MB)
[14:30:15] [INFO ] Lowest usage: 0 bytes (0.00 MB)
[14:30:15] [INFO ] Total allocations: 100
[14:30:15] [INFO ] Total deallocations: 50
[14:30:15] [INFO ] Average allocation size: 10485.76 bytes
[14:30:15] [INFO ] Average memory usage rate: 1024.00 bytes/second
```

### CSV Data Analysis
The CSV statistics log can be imported into Excel or other analysis tools for:
- Memory usage graphs over time
- Allocation pattern analysis
- Memory leak detection
- Performance optimization insights

## Benefits

1. **Professional Debugging**: Structured, filterable logs for easier troubleshooting
2. **Performance Monitoring**: Real-time memory usage tracking and analysis
3. **Memory Leak Detection**: Comprehensive allocation/deallocation tracking
4. **Reduced Log Spam**: Intelligent duplicate message handling
5. **Data Analysis**: CSV export for external analysis tools
6. **Production Ready**: Thread-safe, efficient logging suitable for release builds

## Future Enhancements

- Configurable log levels via configuration file
- Log rotation based on file size
- Network logging capabilities
- Integration with external monitoring tools
- Memory pool-specific statistics breakdown