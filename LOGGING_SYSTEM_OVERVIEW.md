# MemoryPoolNVSE - Comprehensive Logging & Monitoring System

## Overview

The RPmalloc plugin now includes a **professional-grade monitoring system** that provides real-time insights into plugin performance, health, and memory usage across **4 different output files**.

## Output Files Created

### 1. 📄 `MemoryPoolNVSE_rpmalloc.log` - Main Log File
**Enhanced format with microsecond precision:**
```
[2025-10-15 04:18:12.345] [INFO] [TID:1234] === MemoryPoolNVSE RPmalloc Plugin Initialized ===
[2025-10-15 04:18:12.346] [INFO] [TID:1234] Plugin Version: 1.0.0
[2025-10-15 04:18:12.347] [INFO] [TID:1234] Build: Oct 15 2025 04:18:10
[2025-10-15 04:18:12.348] [INFO] [TID:1234] Process ID: 5678
[2025-10-15 04:18:12.349] [PERF] [TID:1234] Large allocation: 2097152 bytes took 1250 microseconds
```

**Log Levels:**
- `DEBUG` - Detailed diagnostic information
- `INFO` - General operational messages  
- `WARN` - Warning conditions that don't affect functionality
- `ERROR` - Error conditions that may impact operation
- `CRITICAL` - Critical failures that compromise plugin health
- `PERF` - Performance-related measurements

### 2. 📊 `MemoryPoolNVSE_stats.json` - Real-time Statistics
**Live JSON dashboard data:**
```json
{
  "plugin": {
    "name": "MemoryPoolNVSE_RPmalloc",
    "version": "1.0.0",
    "build_date": "Oct 15 2025 04:18:10",
    "status": "active",
    "uptime_seconds": 125.4,
    "is_healthy": true
  },
  "memory_stats": {
    "total_allocations": 2847,
    "total_frees": 2203,
    "current_allocations": 644,
    "peak_allocations": 892,
    "total_bytes_allocated": 15728640,
    "total_bytes_freed": 12582912,
    "current_bytes_allocated": 3145728,
    "peak_bytes_allocated": 4194304,
    "allocation_failures": 0,
    "average_allocation_size": 5524.8
  },
  "performance": {
    "allocations_per_second": 22.7,
    "frees_per_second": 17.6,
    "memory_usage_mb": 3.00,
    "peak_memory_usage_mb": 4.00
  },
  "health": {
    "error_count": 0,
    "warning_count": 2,
    "critical_count": 0,
    "last_error": "none",
    "tracked_allocations": 644,
    "max_tracked_allocations": 32768
  },
  "logging": {
    "debug_logs": 0,
    "info_logs": 156,
    "warning_logs": 2,
    "error_logs": 0,
    "critical_logs": 0,
    "performance_logs": 8
  },
  "timestamp": 1697371092845234
}
```

### 3. 📈 `MemoryPoolNVSE_performance.csv` - Time-series Data
**Performance metrics for analysis:**
```csv
timestamp,allocs_per_sec,frees_per_sec,memory_usage,peak_memory
1697371092845234,22.7,17.6,3145728,4194304
1697371097845234,24.1,19.2,3356672,4194304
1697371102845234,21.3,16.8,2985984,4194304
1697371107845234,25.6,21.4,3512320,4194304
```

### 4. 📋 `MemoryPoolNVSE_status.txt` - Quick Status Check
**Human-readable status for external monitoring:**
```
MemoryPoolNVSE RPmalloc Status
==============================
Status: ACTIVE
Health: HEALTHY
Uptime: 125.4 seconds
Memory Usage: 3.00 MB
Peak Memory: 4.00 MB
Allocations: 2847
Errors: 0
Last Update: 1697371092845234
```

## Key Features

### 🔍 Real-time Plugin Health Monitoring
- **Status Tracking**: UNINITIALIZED → INITIALIZING → ACTIVE → DEGRADED → ERROR → SHUTDOWN
- **Health Assessment**: Automatic health evaluation based on errors, warnings, and performance
- **Uptime Calculation**: Precise uptime tracking in seconds
- **Last Activity**: Timestamp of most recent plugin activity

### ⚡ Performance Analytics
- **Allocation Rate**: Allocations per second (calculated every 1000 allocations)
- **Free Rate**: Deallocations per second
- **Memory Usage**: Current and peak memory consumption
- **Large Allocation Timing**: Microsecond timing for allocations > 1MB
- **Automatic Degradation**: Status changes to DEGRADED on allocation failures

### 📊 Comprehensive Statistics
- **Memory Counters**: Total/current/peak allocations and bytes
- **Failure Tracking**: Allocation failure count and investigation
- **Average Metrics**: Average allocation size calculations
- **Thread Safety**: All statistics protected with critical sections
- **Leak Detection**: Tracks all allocations for cleanup verification

### 🔧 Advanced Logging Features
- **Thread-aware**: Shows Thread ID for multi-threaded debugging
- **Microsecond Precision**: High-resolution timestamps for performance analysis
- **Buffered Writing**: Efficient logging with immediate flush for critical messages
- **Log Level Counting**: Tracks count of each log level for analysis
- **Critical Error Storage**: Stores last error message for health reporting

### 📝 Automatic Report Generation
- **Periodic Updates**: Stats files updated every 10,000 allocations
- **JSON Export**: Machine-readable statistics for external tools
- **CSV Time-series**: Performance data suitable for graphing
- **Status File**: Quick status check for monitoring systems

## Plugin Status States

1. **UNINITIALIZED** - Plugin not yet loaded
2. **INITIALIZING** - Plugin loading and setting up systems  
3. **ACTIVE** - Normal operation, all systems healthy
4. **DEGRADED** - Minor issues detected but still functional
5. **ERROR** - Serious problems affecting operation
6. **SHUTDOWN** - Plugin shutting down gracefully

## Usage for Monitoring

### For Developers
```cpp
// Check plugin status programmatically
WriteLog(LOG_INFO, "Current status: %s", 
         (g_health.status == PLUGIN_STATUS_ACTIVE) ? "ACTIVE" : "OTHER");

// Performance monitoring
WriteLog(LOG_PERF, "Allocation took %llu microseconds", duration);

// Health updates
if (g_health.status == PLUGIN_STATUS_ACTIVE) {
    g_health.status = PLUGIN_STATUS_DEGRADED;
}
```

### For External Tools
```bash
# Quick status check
cat "Data/NVSE/Plugins/MemoryPoolNVSE_status.txt"

# JSON parsing for dashboards  
jq '.plugin.status' "Data/NVSE/Plugins/MemoryPoolNVSE_stats.json"

# Performance analysis
import pandas as pd
df = pd.read_csv("Data/NVSE/Plugins/MemoryPoolNVSE_performance.csv")
```

### For End Users
1. **Check if plugin is working**: Look at status.txt
2. **Monitor memory usage**: Check stats.json memory_stats section
3. **View detailed logs**: Read the main .log file
4. **Performance trends**: Import .csv into Excel/Google Sheets

## File Update Frequency

- **Main Log**: Real-time (immediate for errors/critical)
- **JSON Stats**: Every 10,000 allocations (~every few seconds during gameplay)
- **Performance CSV**: Every 1,000 allocations (~every second during active use)
- **Status Text**: Same as JSON stats (every 10,000 allocations)
- **All Files**: Final update on plugin shutdown

## Benefits

### 🎯 **For Plugin Developers**
- Detailed debugging information with thread and timing data
- Performance bottleneck identification
- Memory leak detection and analysis
- Health monitoring for proactive issue detection

### 🔧 **For System Administrators**  
- External monitoring integration via JSON/CSV exports
- Quick status checks without log parsing
- Historical performance data for trend analysis
- Automated alerts based on health status

### 🎮 **For End Users**
- Confirmation that plugin is working correctly
- Performance impact assessment
- Easy troubleshooting with structured logs
- Peace of mind with health monitoring

## Advanced Features

### Health-based Status Changes
```cpp
// Automatic status degradation on failures
if (allocation_failed) {
    if (g_health.status == PLUGIN_STATUS_ACTIVE) {
        g_health.status = PLUGIN_STATUS_DEGRADED;
    }
}
```

### Performance-triggered Reporting
```cpp
// Extra logging for large allocations
if (size > 1MB) {
    WriteLog(LOG_PERF, "Large allocation: %zu bytes took %llu μs", size, duration);
}
```

### Critical Error Handling
```cpp
// Store critical errors for health reporting
if (level >= LOG_ERROR) {
    strncpy_s(g_health.lastError, message, _TRUNCATE);
    g_health.isHealthy = false;
}
```

This comprehensive logging system transforms the MemoryPoolNVSE plugin from a simple memory allocator into a **fully monitored, enterprise-grade system** with real-time health monitoring, performance analytics, and detailed diagnostic capabilities.