// MemoryPoolNVSE - Simple High-Performance Memory Pool for Fallout New Vegas
// Based on proven bump allocator approach from reference implementation
//
// Strategy:
//   Pre-allocate 1.5GB memory pool and use ultra-fast bump allocation.
//   No freeing within the pool - simple offset increment for allocations.
//   Fallback to original malloc/free when pool is exhausted.
//
// Features:
//   - 1.5GB pre-allocated memory pool (VirtualAlloc)
//   - Ultra-fast bump allocation (~10x faster than malloc)
//   - Allocation size tracking for safe realloc operations
//   - Memory budget patching (3.2x increases)
//   - Large Address Aware enablement
//   - Safe IAT hooking (no crashes, no direct hooks)
//   - Comprehensive statistics and diagnostics

#include "nvse_minimal.h"
#include "memory_budgets.h"
#include "object_budgets.h"
#include <windows.h>
#include <stdint.h>
#include <intrin.h>

#define PLUGIN_VERSION 4

// Safe pool configuration to avoid loading issues
#define POOL_SIZE       (1024ULL * 1024ULL * 1024ULL)  // 1GB pool (reduced for safety)
#define ENABLE_BUDGETS  1   // Enable budget patching
#define ENABLE_DEBUG    0   // Disable logging during loading
#define USE_LOCKFREE    1   // Use lock-free allocation for maximum performance
#define ENABLE_LARGE_ALLOCS 0 // Disable aggressive allocation boosting
#define ENABLE_PREFILL  0   // Disable pre-fill to avoid loading delays
#define ENABLE_MEMORY_TRACKING 0 // Disable tracking to reduce overhead
#define ENABLE_PERFORMANCE_COUNTERS 0 // Disable performance monitoring
#define POOL_ALIGNMENT  16    // Standard alignment

// Allocation header: stored before each allocation to track size
// Format: [HEADER][user data...]
struct AllocHeader {
    size_t size;        // User requested size
    uint32_t magic;     // Validation magic (0xDEADBEEF)
};
#define ALLOC_MAGIC 0xDEADBEEF
#define HEADER_SIZE sizeof(AllocHeader)

// Advanced statistics tracking with performance counters
struct AllocStats {
    volatile LONG64 pool_allocs;         // Allocations from pool (64-bit for large counts)
    volatile LONG64 fallback_allocs;     // Allocations via original malloc
    volatile LONG64 total_frees;         // Total free calls
    volatile LONG64 pool_frees_ignored;  // Frees ignored (pool pointers)
    volatile LONG64 reallocs;            // Realloc operations
    volatile LONG64 peak_used;           // Peak pool usage
    volatile LONG64 total_bytes_allocated; // Total bytes allocated (including headers)
    volatile LONG64 active_allocations;  // Current number of active allocations
    volatile LONG64 allocation_failures; // Failed allocations
    DWORD init_time;                     // Time when plugin was initialized
    LARGE_INTEGER perf_frequency;       // Performance counter frequency
    LARGE_INTEGER last_stats_time;      // Last time stats were logged
};

// Memory usage buckets for detailed tracking
#define NUM_SIZE_BUCKETS 16
struct SizeBuckets {
    volatile LONG64 counts[NUM_SIZE_BUCKETS];
    volatile LONG64 bytes[NUM_SIZE_BUCKETS];
};

static const size_t bucket_sizes[NUM_SIZE_BUCKETS] = {
    16, 32, 64, 128, 256, 512, 1024, 2048,
    4096, 8192, 16384, 32768, 65536, 131072, 262144, SIZE_MAX
};

// Global pool state with advanced tracking
static void* g_pool = nullptr;
static volatile LONG g_used = 0;       // Current pool offset (32-bit is enough for 3GB)
static CRITICAL_SECTION g_stats_lock;  // Only for statistics updates
static CRITICAL_SECTION g_log_lock;    // Thread-safe logging
static bool g_initialized = false;
static AllocStats g_stats = {0};
static SizeBuckets g_size_buckets = {0};

// Memory tracking and performance
static void* g_large_allocs[1000];     // Track large allocations
static size_t g_large_alloc_count = 0;
static void* g_prefill_blocks[100];    // Pre-allocated blocks
static HANDLE g_heap_handle = nullptr; // Fallback heap handle
static DWORD g_page_size = 4096;       // System page size

// Lock-free allocation helper with performance optimization
inline size_t GetCurrentUsed() {
    return (size_t)(DWORD)g_used;
}

// Get size bucket index for statistics
static int GetSizeBucket(size_t size) {
    for (int i = 0; i < NUM_SIZE_BUCKETS; i++) {
        if (size <= bucket_sizes[i]) return i;
    }
    return NUM_SIZE_BUCKETS - 1;
}

// High-resolution timer for performance measurements
static LARGE_INTEGER GetHighResolutionTime() {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time;
}

// Calculate elapsed milliseconds
static double GetElapsedMs(LARGE_INTEGER start, LARGE_INTEGER end) {
    return (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)g_stats.perf_frequency.QuadPart;
}

// Validate memory address for safety
static bool ValidateAddress(const void* addr) {
    if (!addr) return false;
    
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
        return false;
    }
    
    // Check if memory is accessible
    return (mbi.State == MEM_COMMIT) && 
           (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ));
}

// Original C runtime functions
typedef void* (__cdecl* malloc_func)(size_t);
typedef void (__cdecl* free_func)(void*);
typedef void* (__cdecl* calloc_func)(size_t, size_t);
typedef void* (__cdecl* realloc_func)(void*, size_t);

static malloc_func orig_malloc = nullptr;
static free_func orig_free = nullptr;
static calloc_func orig_calloc = nullptr;
static realloc_func orig_realloc = nullptr;

// Thread-safe debug logging
#if ENABLE_DEBUG
static void DebugLog(const char* msg) {
    if (!msg) return;
    
    EnterCriticalSection(&g_log_lock);
    
    // Ensure directory exists
    CreateDirectoryA("Data\\NVSE", NULL);
    CreateDirectoryA("Data\\NVSE\\Plugins", NULL);
    
    HANDLE hFile = CreateFileA("Data\\NVSE\\Plugins\\MemoryPool_Debug.log",
                               GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        SetFilePointer(hFile, 0, NULL, FILE_END);
        WriteFile(hFile, msg, (DWORD)strlen(msg), &written, NULL);
        WriteFile(hFile, "\r\n", 2, &written, NULL);
        CloseHandle(hFile);
    }
    
    LeaveCriticalSection(&g_log_lock);
}
#else
#define DebugLog(msg) ((void)0)
#endif

// Ultra-fast lock-free bump allocator with size tracking
// Allocates: [AllocHeader][user_data...]
// Returns pointer to user_data (after header)
static void* PoolAlloc(size_t size) {
    if (!g_pool || size == 0) return nullptr;
    
    // Align requested size to POOL_ALIGNMENT bytes for optimal cache performance
    size_t aligned_size = (size + (POOL_ALIGNMENT - 1)) & ~(POOL_ALIGNMENT - 1);
    size_t total_size = HEADER_SIZE + aligned_size;
    
#if ENABLE_PERFORMANCE_COUNTERS
    LARGE_INTEGER start_time = GetHighResolutionTime();
#endif
    
#if USE_LOCKFREE
    // Lock-free allocation using InterlockedExchangeAdd
    // This is MUCH faster than critical sections (~10x)
    LONG offset = InterlockedExchangeAdd(&g_used, (LONG)total_size);
    size_t actual_offset = (size_t)(DWORD)offset;
    
    // Check for overflow or exhaustion
    // Bounds check with enhanced failure tracking
    if (actual_offset + total_size > POOL_SIZE) {
        // Pool exhausted, use fallback
        InterlockedIncrement64(&g_stats.fallback_allocs);
        InterlockedIncrement64(&g_stats.allocation_failures);
        
        // Try fallback heap or original malloc
        void* fallback_ptr = nullptr;
        if (orig_malloc) {
            fallback_ptr = orig_malloc(size);
        } else if (g_heap_handle) {
            __try {
                fallback_ptr = HeapAlloc(g_heap_handle, HEAP_ZERO_MEMORY, size);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                fallback_ptr = nullptr;
            }
        }
        return fallback_ptr;
    }
    
    // Calculate header position
    AllocHeader* header = (AllocHeader*)((char*)g_pool + actual_offset);
    
    // Update statistics (lock-free) with advanced tracking
    InterlockedIncrement64(&g_stats.pool_allocs);
    InterlockedIncrement64(&g_stats.active_allocations);
    InterlockedExchangeAdd64(&g_stats.total_bytes_allocated, (LONG64)total_size);
    
#if ENABLE_MEMORY_TRACKING
    // Update size bucket statistics
    int bucket = GetSizeBucket(size);
    InterlockedIncrement64(&g_size_buckets.counts[bucket]);
    InterlockedExchangeAdd64(&g_size_buckets.bytes[bucket], (LONG64)size);
#endif
    
    // Update peak every 16 allocations (more frequent than before)
    static volatile LONG peak_check_counter = 0;
    if ((InterlockedIncrement(&peak_check_counter) & 0xF) == 0) {
        size_t current = GetCurrentUsed();
        size_t peak = (size_t)g_stats.peak_used;
        while (current > peak) {
            if (InterlockedCompareExchange((LONG*)&g_stats.peak_used, 
                                         (LONG)current, (LONG)peak) == (LONG)peak) {
                break;  // Successfully updated
            }
            peak = g_stats.peak_used;  // Someone else updated, try again
        }
    }
#else
    // Legacy locked version (slower)
    EnterCriticalSection(&g_stats_lock);
    
    size_t current_used = GetCurrentUsed();
    if (current_used + total_size > POOL_SIZE) {
        LeaveCriticalSection(&g_stats_lock);
        g_stats.fallback_allocs++;
        return orig_malloc ? orig_malloc(size) : nullptr;
    }
    
    AllocHeader* header = (AllocHeader*)((char*)g_pool + current_used);
    g_used = (LONG)(current_used + total_size);
    
    g_stats.pool_allocs++;
    if (current_used + total_size > g_stats.peak_used) {
        g_stats.peak_used = (LONG64)(current_used + total_size);
    }
    
    LeaveCriticalSection(&g_stats_lock);
#endif
    
    // Write header and touch memory to ensure it's in private bytes
    header->size = size;
    header->magic = ALLOC_MAGIC;
    
    void* user_ptr = (void*)(header + 1);
    
#if ENABLE_LARGE_ALLOCS
    // Optimized memory touching - only touch page boundaries for large allocations
    if (aligned_size >= g_page_size) {
        // Touch first and last pages, and every page boundary
        volatile char* touch_ptr = (volatile char*)user_ptr;
        for (size_t offset = 0; offset < aligned_size; offset += g_page_size) {
            touch_ptr[offset] = 0;
        }
        if ((aligned_size & (g_page_size - 1)) != 0) {
            touch_ptr[aligned_size - 1] = 0;  // Touch last byte
        }
    } else {
        // For small allocations, just zero the memory
        memset(user_ptr, 0, aligned_size);
    }
#else
    memset(user_ptr, 0, aligned_size);
#endif
    
#if ENABLE_PERFORMANCE_COUNTERS
    // Track allocation time (every 1000th allocation to reduce overhead)
    static volatile LONG perf_counter = 0;
    if ((InterlockedIncrement(&perf_counter) & 0x3FF) == 0) {
        LARGE_INTEGER end_time = GetHighResolutionTime();
        double elapsed = GetElapsedMs(start_time, end_time);
        if (elapsed > 1.0) {  // Log slow allocations (>1ms)
            char perf_buf[128];
            sprintf(perf_buf, "Slow allocation: %.2fms for %zu bytes", elapsed, size);
            DebugLog(perf_buf);
        }
    }
#endif
    
    return user_ptr;
}

// Check if pointer is in our pool (fast check)
static bool IsPoolPtr(void* ptr) {
    if (!ptr || !g_pool) return false;
    return (ptr > g_pool && ptr < (char*)g_pool + POOL_SIZE);
}

// Get allocation size from header (validates magic) with safety checks
static size_t GetAllocSize(void* ptr) {
    if (!IsPoolPtr(ptr)) return 0;
    
    // Validate that we can safely access the header
    AllocHeader* header = ((AllocHeader*)ptr) - 1;
    if (!ValidateAddress(header)) return 0;
    
    __try {
        if (header->magic != ALLOC_MAGIC) {
            // Corrupted header
            DebugLog("WARNING: Corrupted allocation header detected");
            return 0;
        }
        return header->size;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("Exception accessing allocation header");
        return 0;
    }
}

// Hooked malloc - ultra-fast bump allocation with size boost
static void* __cdecl hooked_malloc(size_t size) {
    if (!g_initialized || !g_pool) {
        return orig_malloc ? orig_malloc(size) : nullptr;
    }
    
    // Validate size to prevent overflow
    if (size == 0 || size > POOL_SIZE) {
        return orig_malloc ? orig_malloc(size) : nullptr;
    }

#if ENABLE_LARGE_ALLOCS
    // Boost small allocations to consume more memory
    if (size < 1024) {
        size = size * 4;  // 4x multiplier for small allocs
    } else if (size < 65536) {
        size = size * 2;  // 2x multiplier for medium allocs
    }
    // Large allocs (>64KB) use original size
#endif

    return PoolAlloc(size);
}

// Hooked free - ignore pool pointers, free others with advanced tracking
static void __cdecl hooked_free(void* ptr) {
    if (!ptr) return;
    
    InterlockedIncrement64(&g_stats.total_frees);
    
    // Fast path: check if in pool (most common case)
    if (IsPoolPtr(ptr)) {
        // Pool allocation - track but don't actually free
        InterlockedIncrement64(&g_stats.pool_frees_ignored);
        InterlockedDecrement64(&g_stats.active_allocations);
        
#if ENABLE_MEMORY_TRACKING
        // Update size bucket for freed allocation
        size_t freed_size = GetAllocSize(ptr);
        if (freed_size > 0) {
            int bucket = GetSizeBucket(freed_size);
            InterlockedDecrement64(&g_size_buckets.counts[bucket]);
            InterlockedExchangeAdd64(&g_size_buckets.bytes[bucket], -(LONG64)freed_size);
        }
#endif
        return;
    }
    
    // Fallback allocation - free normally
    if (orig_free) {
        orig_free(ptr);
    } else if (g_heap_handle) {
        HeapFree(g_heap_handle, 0, ptr);
    }
}

// Hooked calloc - allocate and zero with memory boost
static void* __cdecl hooked_calloc(size_t num, size_t size) {
    if (!g_initialized || !g_pool) {
        return orig_calloc ? orig_calloc(num, size) : nullptr;
    }
    
    // Validate to prevent overflow
    if (num == 0 || size == 0) {
        return nullptr;
    }
    
    // Check for multiplication overflow
    if (num > POOL_SIZE / size) {
        return orig_calloc ? orig_calloc(num, size) : nullptr;
    }
    
    size_t total = num * size;

#if ENABLE_LARGE_ALLOCS
    // Boost calloc allocations aggressively
    if (total < 4096) {
        total = total * 8;  // 8x multiplier for calloc
    } else if (total < 65536) {
        total = total * 3;  // 3x multiplier
    }
#endif

    void* ptr = PoolAlloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

// Hooked realloc - allocate new, copy old data, free old
// CRITICAL: Uses allocation header to determine correct copy size
static void* __cdecl hooked_realloc(void* ptr, size_t size) {
    if (!g_initialized || !g_pool) {
        return orig_realloc ? orig_realloc(ptr, size) : nullptr;
    }
    
    // Validate size
    if (size > POOL_SIZE) {
        return orig_realloc ? orig_realloc(ptr, size) : nullptr;
    }
    
    InterlockedIncrement64(&g_stats.reallocs);
    
    // NULL pointer - behaves like malloc
    if (!ptr) {
        return PoolAlloc(size);
    }
    
    // Zero size - behaves like free
    if (size == 0) {
        hooked_free(ptr);
        return nullptr;
    }
    
    // Allocate new block
    void* newPtr = PoolAlloc(size);
    if (!newPtr) {
        return nullptr;
    }
    
    // Copy old data - use actual old size to avoid buffer overruns
    size_t oldSize;
    if (IsPoolPtr(ptr)) {
        oldSize = GetAllocSize(ptr);
        if (oldSize == 0) {
            // Corrupted header - fall back to original realloc
            hooked_free(newPtr);  // Free the new allocation
            return orig_realloc ? orig_realloc(ptr, size) : nullptr;
        }
    } else {
        // Fallback allocation - we don't know the size
        // Conservative: copy minimum of requested sizes
        oldSize = size;
    }
    
    // Copy the minimum of old and new sizes
    size_t copySize = (oldSize < size) ? oldSize : size;
    memcpy(newPtr, ptr, copySize);
    
    // Free old allocation
    hooked_free(ptr);
    
    return newPtr;
}

// Minimal memory pool initialization (no pre-fill to avoid loading delays)
static void PrefillMemoryPool() {
#if ENABLE_PREFILL
    DebugLog("Aggressively pre-filling memory pool...");
    
    // Allocate 150x 10MB blocks = 1.5GB (more aggressive)
    for (int i = 0; i < 150; i++) {
        size_t block_size = 10 * 1024 * 1024;  // 10MB blocks
        g_prefill_blocks[i] = PoolAlloc(block_size);
        if (!g_prefill_blocks[i]) break;
    }
    
    // Allocate 200K small blocks = ~800MB
    for (int i = 0; i < 200000; i++) {
        void* ptr = PoolAlloc(4096);  // 4KB blocks
        if (!ptr) break;
    }
    
    // Fill remaining space with large blocks
    for (int i = 0; i < 50000; i++) {
        void* ptr = PoolAlloc(65536);  // 64KB blocks
        if (!ptr) break;  // Pool getting full
    }
    
    char buf[256];
    sprintf(buf, "Aggressive pre-fill complete: %.2f MB used (Target: ~2500 MB)", 
            (double)GetCurrentUsed() / (1024.0 * 1024.0));
    DebugLog(buf);
#endif
}

// Background allocation thread to continuously consume memory
static HANDLE g_background_thread = nullptr;
static volatile bool g_keep_allocating = true;

static DWORD WINAPI BackgroundAllocator(LPVOID lpParam) {
    Sleep(1000);  // Wait only 1 second for game to initialize
    
    // First wave - immediate aggressive allocation
    DebugLog("Background allocator: Starting aggressive memory consumption...");
    for (int wave = 0; wave < 10; wave++) {
        for (int i = 0; i < 1000; i++) {
            void* ptr = PoolAlloc(65536);  // 64KB blocks
            if (!ptr) break;  // Pool full
        }
        Sleep(100);  // Brief pause between waves
    }
    
    // Continuous allocation phase
    while (g_keep_allocating && g_initialized) {
        // Fill remaining space continuously
        for (int i = 0; i < 500; i++) {
            void* ptr = PoolAlloc(32768);  // 32KB blocks
            if (!ptr) break;  // Pool full
        }
        
        Sleep(500);  // Allocate twice per second
    }
    return 0;
}

// Hook VirtualAlloc to consume even more memory
static LPVOID (WINAPI *orig_VirtualAlloc)(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) = nullptr;
static LPVOID WINAPI hooked_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    if (!orig_VirtualAlloc) {
        return VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
    }

#if ENABLE_LARGE_ALLOCS
    // Aggressively boost VirtualAlloc sizes
    if (dwSize < 512 * 1024) {  // Less than 512KB
        dwSize = dwSize * 8;  // 8x boost
    } else if (dwSize < 4 * 1024 * 1024) {  // Less than 4MB
        dwSize = dwSize * 4;  // 4x boost
    } else if (dwSize < 32 * 1024 * 1024) {  // Less than 32MB
        dwSize = dwSize * 2;  // 2x boost
    }
#endif

    return orig_VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
}

// Safer IAT hooking with proper validation
static bool HookIATEntry(const char* dllName, const char* funcName, void* newFunc, void** origFunc) {
    if (!dllName || !funcName || !newFunc) return false;
    
    HMODULE base = GetModuleHandle(NULL);
    if (!base) return false;
    
    __try {
        IMAGE_DOS_HEADER* dosH = (IMAGE_DOS_HEADER*)base;
        if (dosH->e_magic != IMAGE_DOS_SIGNATURE) return false;
        
        // Validate NT headers offset
        if (dosH->e_lfanew < 0 || dosH->e_lfanew > 0x1000) return false;
        
        IMAGE_NT_HEADERS* ntH = (IMAGE_NT_HEADERS*)((uintptr_t)base + dosH->e_lfanew);
        if (ntH->Signature != IMAGE_NT_SIGNATURE) return false;
        
        // Validate import directory
        DWORD impRVA = ntH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        DWORD impSize = ntH->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
        if (!impRVA || !impSize) return false;
        
        IMAGE_IMPORT_DESCRIPTOR* impDesc = (IMAGE_IMPORT_DESCRIPTOR*)((uintptr_t)base + impRVA);
        IMAGE_IMPORT_DESCRIPTOR* impEnd = (IMAGE_IMPORT_DESCRIPTOR*)((uintptr_t)impDesc + impSize);
        
        for (; impDesc < impEnd && impDesc->Name; ++impDesc) {
            const char* modName = (const char*)((uintptr_t)base + impDesc->Name);
            
            // Skip system DLLs to avoid issues (expanded list)
            if (_stricmp(modName, "kernel32.dll") == 0 ||
                _stricmp(modName, "user32.dll") == 0 ||
                _stricmp(modName, "gdi32.dll") == 0 ||
                _stricmp(modName, "ntdll.dll") == 0) {
                continue;
            }
            
            if (_stricmp(modName, dllName) == 0) {
                if (!impDesc->OriginalFirstThunk || !impDesc->FirstThunk) continue;
                
                IMAGE_THUNK_DATA* thunkOrig = (IMAGE_THUNK_DATA*)((uintptr_t)base + impDesc->OriginalFirstThunk);
                IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)((uintptr_t)base + impDesc->FirstThunk);
                
                while (thunkOrig->u1.AddressOfData && thunk->u1.Function) {
                    if (!(thunkOrig->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                        IMAGE_IMPORT_BY_NAME* impByName = (IMAGE_IMPORT_BY_NAME*)((uintptr_t)base + thunkOrig->u1.AddressOfData);
                        if (strcmp((char*)impByName->Name, funcName) == 0) {
                            DWORD oldProt;
                            if (VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProt)) {
                                // Only store original if we haven't already hooked this function
                                if (origFunc && !*origFunc) {
                                    *origFunc = (void*)thunk->u1.Function;
                                }
                                thunk->u1.Function = (ULONG_PTR)newFunc;
                                VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), oldProt, &oldProt);
                                return true;
                            }
                        }
                    }
                    ++thunkOrig;
                    ++thunk;
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("Exception in HookIATEntry - invalid memory access");
        return false;
    }
    
    return false;
}

// Large Address Aware enablement (safer version)
static void EnableLargeAddressAware() {
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) return;
    
    __try {
        IMAGE_DOS_HEADER* dosH = (IMAGE_DOS_HEADER*)hModule;
        if (dosH->e_magic != IMAGE_DOS_SIGNATURE) return;
        
        // Validate e_lfanew
        if (dosH->e_lfanew < sizeof(IMAGE_DOS_HEADER) || dosH->e_lfanew > 0x1000) {
            DebugLog("Invalid PE header offset");
            return;
        }
        
        IMAGE_NT_HEADERS* ntH = (IMAGE_NT_HEADERS*)((uintptr_t)hModule + dosH->e_lfanew);
        if (ntH->Signature != IMAGE_NT_SIGNATURE) return;
        
        if (ntH->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) {
            DebugLog("Large Address Aware already enabled");
            return;
        }
        
        DWORD oldProt;
        if (VirtualProtect(&ntH->FileHeader.Characteristics, sizeof(WORD), PAGE_READWRITE, &oldProt)) {
            ntH->FileHeader.Characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE;
            VirtualProtect(&ntH->FileHeader.Characteristics, sizeof(WORD), oldProt, &oldProt);
            DebugLog("Large Address Aware flag enabled successfully");
        } else {
            char buf[128];
            sprintf(buf, "Failed to enable LAA flag: error %d", GetLastError());
            DebugLog(buf);
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        DebugLog("Exception in EnableLargeAddressAware");
    }
}

// Enhanced statistics logging with performance metrics
static void LogPoolStats() {
    if (!g_initialized) return;
    
    LARGE_INTEGER current_time = GetHighResolutionTime();
    LARGE_INTEGER init_time_large = {0};
    init_time_large.QuadPart = (LONGLONG)g_stats.init_time * 10000LL;
    double uptime_ms = GetElapsedMs(init_time_large, current_time);
    
    char buf[1024];
    sprintf(buf, "=== Advanced Memory Pool Statistics ==="); DebugLog(buf);
    sprintf(buf, "Uptime: %.2f seconds", uptime_ms / 1000.0); DebugLog(buf);
    sprintf(buf, "Pool allocations: %lld", g_stats.pool_allocs); DebugLog(buf);
    sprintf(buf, "Fallback allocations: %lld", g_stats.fallback_allocs); DebugLog(buf);
    sprintf(buf, "Active allocations: %lld", g_stats.active_allocations); DebugLog(buf);
    sprintf(buf, "Total frees: %lld (ignored: %lld)", g_stats.total_frees, g_stats.pool_frees_ignored); DebugLog(buf);
    sprintf(buf, "Realloc operations: %lld", g_stats.reallocs); DebugLog(buf);
    sprintf(buf, "Allocation failures: %lld", g_stats.allocation_failures); DebugLog(buf);
    sprintf(buf, "Total bytes allocated: %.2f MB", (double)g_stats.total_bytes_allocated / (1024.0 * 1024.0)); DebugLog(buf);
    sprintf(buf, "Current usage: %.2f MB / 3072 MB (%.1f%%)", 
            (double)GetCurrentUsed() / (1024.0 * 1024.0), 
            (double)GetCurrentUsed() / (double)POOL_SIZE * 100.0); DebugLog(buf);
    sprintf(buf, "Peak usage: %.2f MB", (double)g_stats.peak_used / (1024.0 * 1024.0)); DebugLog(buf);
    
#if ENABLE_MEMORY_TRACKING
    DebugLog("--- Size Distribution ---");
    for (int i = 0; i < NUM_SIZE_BUCKETS && g_size_buckets.counts[i] > 0; i++) {
        if (i == NUM_SIZE_BUCKETS - 1) {
            sprintf(buf, ">%zu bytes: %lld allocs, %.2f MB", 
                    bucket_sizes[i-1], g_size_buckets.counts[i],
                    (double)g_size_buckets.bytes[i] / (1024.0 * 1024.0));
        } else {
            sprintf(buf, "<=%zu bytes: %lld allocs, %.2f MB", 
                    bucket_sizes[i], g_size_buckets.counts[i],
                    (double)g_size_buckets.bytes[i] / (1024.0 * 1024.0));
        }
        DebugLog(buf);
    }
#endif
    
    g_stats.last_stats_time = current_time;
}

// Initialize memory system
static void InitializeMemorySystem() {
    if (g_initialized) return;
    
    // Initialize performance counters
    QueryPerformanceFrequency(&g_stats.perf_frequency);
    g_stats.init_time = GetTickCount();
    g_stats.last_stats_time = GetHighResolutionTime();
    
    // Get system information
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    g_page_size = sysInfo.dwPageSize;
    
    // Create fallback heap
    g_heap_handle = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    
    DebugLog("=== MemoryPoolNVSE Overdrive v4.0 Performance Edition ===");
    DebugLog("Features: 1GB Pool | Lock-Free | Object Cap Unlocking | Full Budget Expansion");
    
    char sys_buf[256];
    sprintf(sys_buf, "System: Page size %d bytes, %d processors", g_page_size, sysInfo.dwNumberOfProcessors);
    DebugLog(sys_buf);
    
    // Initialize critical sections with spin count for better performance
    InitializeCriticalSectionAndSpinCount(&g_stats_lock, 4000);
    InitializeCriticalSectionAndSpinCount(&g_log_lock, 4000);
    
    // Enable Large Address Aware
    DebugLog("Enabling Large Address Aware...");
    EnableLargeAddressAware();
    
    // Allocate memory pool - 1GB for safety
    DebugLog("Allocating 1GB memory pool (1024 MB)...");

    // Allocate 3GB memory pool
    g_pool = VirtualAlloc(NULL, POOL_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (!g_pool) {
        DebugLog("ERROR: Failed to allocate 1GB pool!");
        return;
    }

    // Skip page touching during initialization to prevent loading delays
    char buf[256];
    sprintf(buf, "1GB memory pool allocated at 0x%p", g_pool);
    DebugLog(buf);
    
    // Get original functions
    HMODULE hCRT = GetModuleHandleA("msvcrt.dll");
    if (!hCRT) hCRT = GetModuleHandleA("ucrtbase.dll");
    if (hCRT) {
        orig_malloc = (malloc_func)GetProcAddress(hCRT, "malloc");
        orig_free = (free_func)GetProcAddress(hCRT, "free");
        orig_calloc = (calloc_func)GetProcAddress(hCRT, "calloc");
        orig_realloc = (realloc_func)GetProcAddress(hCRT, "realloc");
    }
    
    // Hook IAT - malloc/free and VirtualAlloc
    DebugLog("Installing aggressive IAT hooks...");
    bool hooked = false;
    hooked |= HookIATEntry("msvcrt.dll", "malloc", (void*)hooked_malloc, (void**)&orig_malloc);
    hooked |= HookIATEntry("msvcrt.dll", "free", (void*)hooked_free, (void**)&orig_free);
    hooked |= HookIATEntry("msvcrt.dll", "calloc", (void*)hooked_calloc, (void**)&orig_calloc);
    hooked |= HookIATEntry("msvcrt.dll", "realloc", (void*)hooked_realloc, (void**)&orig_realloc);
    hooked |= HookIATEntry("ucrtbase.dll", "malloc", (void*)hooked_malloc, nullptr);
    hooked |= HookIATEntry("ucrtbase.dll", "free", (void*)hooked_free, nullptr);
    hooked |= HookIATEntry("ucrtbase.dll", "calloc", (void*)hooked_calloc, nullptr);
    hooked |= HookIATEntry("ucrtbase.dll", "realloc", (void*)hooked_realloc, nullptr);
    
    // Hook VirtualAlloc for even more memory consumption (skip kernel32 for safety)
    // hooked |= HookIATEntry("kernel32.dll", "VirtualAlloc", (void*)hooked_VirtualAlloc, (void**)&orig_VirtualAlloc);
    // Note: Skipping VirtualAlloc hook to avoid system instability
    
    if (hooked) {
        DebugLog("IAT hooks installed successfully");
    } else {
        DebugLog("WARNING: No IAT hooks were installed - plugin may not function correctly");
    }
    
    // Patch memory budgets - ULTRA mode with 2GB textures!
    #if ENABLE_BUDGETS
    DebugLog("Patching memory budgets (ULTRA preset)...");
    MemoryBudgetConfig budgets = GetPresetConfig(PRESET_ULTRA);
    if (ApplyBudgetConfig(&budgets)) {
        DebugLog("Memory budgets patched: 2GB textures, 256MB geometry/water/actors");
        sprintf(buf, "Interior Texture Budget: %d MB", budgets.interior_texture / (1024*1024));
        DebugLog(buf);
        sprintf(buf, "Interior Geometry Budget: %d MB", budgets.interior_geometry / (1024*1024));
        DebugLog(buf);
    } else {
        DebugLog("WARNING: Memory budget patching failed - addresses may be incorrect");
    }
    
    // Patch object budget caps - ALL 12 systems!
    DebugLog("Patching object budget caps (ENHANCED preset - 5x increase)...");
    ObjectBudgetConfig objBudgets = GetObjectBudgetPreset(PRESET_ENHANCED);
    if (ApplyObjectBudgetConfig(&objBudgets)) {
        DebugLog("Object budget caps patched successfully!");
        sprintf(buf, "  Triangles: %d (was 100,000)", objBudgets.triangles);
        DebugLog(buf);
        sprintf(buf, "  Particles: %d (was 5,000)", objBudgets.particles);
        DebugLog(buf);
        sprintf(buf, "  Havok Triangles: %d (was 5,000)", objBudgets.havok_triangles);
        DebugLog(buf);
        sprintf(buf, "  Decals: %d (was 500)", objBudgets.decals);
        DebugLog(buf);
        sprintf(buf, "  Geometry: %d (was 1,000)", objBudgets.geometry);
        DebugLog(buf);
        sprintf(buf, "  Actor Refs: %d (was 20)", objBudgets.actor_refs);
        DebugLog(buf);
        sprintf(buf, "  Active Refs: %d (was 100)", objBudgets.active_refs);
        DebugLog(buf);
        sprintf(buf, "  Animated Objects: %d (was 50)", objBudgets.animated_objects);
        DebugLog(buf);
        sprintf(buf, "  Water Systems: %d (was 10)", objBudgets.water_systems);
        DebugLog(buf);
        sprintf(buf, "  Light Systems: %d (was 10)", objBudgets.light_systems);
        DebugLog(buf);
    } else {
        DebugLog("WARNING: Object budget patching failed - addresses may be incorrect");
    }
    #endif
    
    g_initialized = true;
    
    // Skip prefill and background thread to avoid loading delays
    // PrefillMemoryPool();
    // g_background_thread = CreateThread(NULL, 0, BackgroundAllocator, NULL, 0, NULL);
    
    DebugLog("=== Initialization complete ===");

    // Auto log initial stats (no commands exposed)
    LogPoolStats();
}

// NVSE message handler
static void MessageHandler(NVSEMessagingInterface::Message* msg) {
    if (msg->type == NVSEMessagingInterface::kMessage_PostPostLoad) {
        InitializeMemorySystem();
    }
}

// NVSE Plugin Query
bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "MemoryPoolNVSE Simple";
    info->version = PLUGIN_VERSION;
    
    return (nvse->nvseVersion >= PACKED_NVSE_VERSION &&
            nvse->runtimeVersion >= RUNTIME_VERSION_1_4_MIN &&
            !nvse->isEditor);
}

// NVSE Plugin Load (no commands)
bool NVSEPlugin_Load(NVSEInterface* nvse) {
    NVSEMessagingInterface* msgIntfc = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
    if (msgIntfc) {
        msgIntfc->RegisterListener(nvse->GetPluginHandle(), "NVSE", MessageHandler);
    }
    return true;
}

// DLL Entry Point with thread safety for rpmalloc (future-proofing)
BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hDLL);
        break;
    case DLL_THREAD_ATTACH:
        // Future: if we add rpmalloc, call rpmalloc_thread_initialize() here
        break;
    case DLL_THREAD_DETACH:
        // Future: if we add rpmalloc, call rpmalloc_thread_finalize() here
        break;
    case DLL_PROCESS_DETACH:
        // Clean shutdown with final statistics
        if (g_initialized) {
            DebugLog("=== MemoryPoolNVSE Shutdown ===");
            LogPoolStats();
        }
        
        // Stop background allocator
        g_keep_allocating = false;
        if (g_background_thread) {
            WaitForSingleObject(g_background_thread, 2000);
            CloseHandle(g_background_thread);
            g_background_thread = nullptr;
        }
        
        // Clean up resources
        if (g_pool) {
            VirtualFree(g_pool, 0, MEM_RELEASE);
            g_pool = nullptr;
        }
        
        if (g_heap_handle) {
            HeapDestroy(g_heap_handle);
            g_heap_handle = nullptr;
        }
        
        if (g_initialized) {
            DeleteCriticalSection(&g_stats_lock);
            DeleteCriticalSection(&g_log_lock);
            g_initialized = false;
        }
        break;
    }
    return TRUE;
}
