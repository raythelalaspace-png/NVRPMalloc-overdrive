// virtualfree_hook.h - VirtualFree Hook System
// Intercepts VirtualFree calls to delay/prevent memory decommitment
#pragma once

#include <windows.h>
#include <stdint.h>

// Hook configuration
struct VirtualFreeHookConfig {
    bool delay_decommit;        // Delay MEM_DECOMMIT operations
    bool prevent_release;       // Prevent MEM_RELEASE for certain regions
    uint32_t delay_ms;          // Delay time for decommit operations
    size_t min_keep_size;       // Minimum size to keep committed
    bool log_operations;        // Log VirtualFree calls
    // Quotas & safety
    size_t max_kept_committed_bytes; // Quota for delayed kept memory (0 = unlimited)
    uint32_t low_va_trigger_mb; // If top-of-VA free < threshold, flush and stop delaying
};

// Statistics
struct VirtualFreeStats {
    volatile long total_calls;
    volatile long decommit_blocked;
    volatile long release_blocked;
    volatile long decommit_delayed;
    size_t bytes_kept_committed;      // total since init (cumulative)
    size_t kept_committed_current;    // current queued/kept bytes (for quotas)
};

// Initialize VirtualFree hook system
bool InitVirtualFreeHook(const VirtualFreeHookConfig* config);

// Shutdown hook system
void ShutdownVirtualFreeHook();

// Get statistics
void GetVirtualFreeStats(VirtualFreeStats* stats);

// Flush delayed operations (call before exit)
void FlushDelayedFrees();
