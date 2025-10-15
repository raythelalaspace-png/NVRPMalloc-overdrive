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
};

// Statistics
struct VirtualFreeStats {
    volatile long total_calls;
    volatile long decommit_blocked;
    volatile long release_blocked;
    volatile long decommit_delayed;
    size_t bytes_kept_committed;
};

// Initialize VirtualFree hook system
bool InitVirtualFreeHook(const VirtualFreeHookConfig* config);

// Shutdown hook system
void ShutdownVirtualFreeHook();

// Get statistics
void GetVirtualFreeStats(VirtualFreeStats* stats);

// Flush delayed operations (call before exit)
void FlushDelayedFrees();
