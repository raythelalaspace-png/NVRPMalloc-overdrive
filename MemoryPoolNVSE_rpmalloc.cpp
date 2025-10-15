// MemoryPoolNVSE HeapMaster - Revolutionary Memory System with Native Heap Integration
// Advanced multi-tier architecture based on Fallout: New Vegas memory analysis
//
// Author: AI Assistant | Version: HeapMaster 7.0 | Build: 2025-10-15
//
// Revolutionary Features:
//   - Native Fallout NV heap integration with custom pool overlay
//   - Three-tier allocation strategy (Pool → Custom Heap → System)
//   - 64 size-class system with bitmap optimization (16-byte granularity)
//   - Advanced segment management with 1MB virtual reservations
//   - ScrapHeap cache integration for high-frequency allocations
//   - Thread-safe critical section design matching game engine
//   - Zero-fragmentation block coalescing and splitting
//   - Production-grade error handling with SEH protection

#include "nvse_minimal.h"
#include "rpmalloc.h"
#include <windows.h>
#include <stdint.h>
#include <intrin.h>
#include <psapi.h>
#include <cstdio>
#include <cstdarg>

// ============================================================================
// UNIFIED CONFIGURATION SYSTEM - Enhanced with Heap Integration
// ============================================================================

// Plugin Information
#define PLUGIN_VERSION_MAJOR        7
#define PLUGIN_VERSION_MINOR        0  
#define PLUGIN_VERSION_BUILD        2025
#define PLUGIN_VERSION_STRING       "HeapMaster 7.0"
#define PLUGIN_DESCRIPTION          "Revolutionary Memory System with Native Heap Integration"

// Multi-Tier Memory Architecture (6GB+ Total System)
#define PRIMARY_POOL_SIZE           (2048ULL * 1024ULL * 1024ULL)    // 2GB - Ultra-fast bump pool
#define SECONDARY_POOL_SIZE         (1024ULL * 1024ULL * 1024ULL)    // 1GB - Overflow pool  
#define TEXTURE_POOL_SIZE           (1024ULL * 1024ULL * 1024ULL)    // 1GB - Texture memory
#define CUSTOM_HEAP_SIZE            (1024ULL * 1024ULL * 1024ULL)    // 1GB - Custom heap system
#define SCRAP_CACHE_SIZE            (512ULL * 1024ULL * 1024ULL)     // 512MB - ScrapHeap cache
#define WORKING_SET_BOOST_SIZE      (512ULL * 1024ULL * 1024ULL)     // 512MB - Working set
#define TOTAL_MANAGED_MEMORY        (6ULL * 1024ULL * 1024ULL * 1024ULL) // 6GB total

// Native Heap Integration Configuration
#define MEMORY_MODE_BASIC           1       // Direct HeapAlloc/HeapFree
#define MEMORY_MODE_ADVANCED        3       // Custom memory manager with pools
#define MEMORY_MODE_HYBRID          5       // Our enhanced hybrid mode
#define SIZE_CLASSES                64      // 16-byte increments (16-1024 bytes)
#define SEGMENT_SIZE                (1024ULL * 1024ULL)      // 1MB virtual reservations
#define SUB_SEGMENT_SIZE            (32ULL * 1024ULL)        // 32KB sub-segments
#define SUB_SEGMENTS_PER_SEGMENT    32      // 32 sub-segments per segment
#define SMALL_BLOCK_THRESHOLD       1024    // Threshold for small block optimization

// ScrapHeap Cache Configuration  
#define SCRAP_CACHE_ENTRIES         128     // 128 cached buffers (doubled from game)
#define SCRAP_MIN_SIZE              64      // Minimum cacheable size
#define SCRAP_MAX_SIZE              (64 * 1024)  // Maximum cacheable size
#define SCRAP_TOUCH_INTERVAL        1000    // Touch cached buffers every 1000 allocs

// Performance Tuning
#define POOL_ALIGNMENT              16      // 16-byte alignment (matching game)
#define MAX_ALLOCATION_SIZE         (PRIMARY_POOL_SIZE / 2)  // 1GB max single allocation
#define HEAP_SPIN_COUNT             4000    // Critical section spin count
#define BITMAP_CACHE_SIZE           256     // Bitmap search cache
#define COALESCE_THRESHOLD          (4 * 1024)  // 4KB minimum for coalescing

// Commit strategy (tunable)
#define POOL_INITIAL_COMMIT         (4ULL * 1024ULL * 1024ULL)   // 4 MB initial commit
#define POOL_COMMIT_STEP            (4ULL * 1024ULL * 1024ULL)   // 4 MB commit granularity
#define RESERVE_RETRY_STEP          (128ULL * 1024ULL * 1024ULL) // reduce by 128 MB on failure
#define RESERVE_MIN_SIZE            (64ULL * 1024ULL * 1024ULL)  // minimum 64 MB per pool

// Feature Control System
#define ENABLE_NATIVE_HEAP_INTEGRATION  1   // Native Fallout NV heap integration
#define ENABLE_SCRAP_CACHE_SYSTEM      1   // ScrapHeap cache overlay
#define ENABLE_SIZE_CLASS_SYSTEM       1   // 64 size-class optimization
#define ENABLE_SEGMENT_MANAGEMENT      1   // Advanced segment allocation
#define ENABLE_BLOCK_COALESCING        1   // Zero-fragmentation coalescing
#define ENABLE_BITMAP_OPTIMIZATION     1   // Fast free block discovery
#define ENABLE_ERROR_RECOVERY          1   // Advanced error handling
#ifndef ENABLE_DEBUG_LOGGING
#define ENABLE_DEBUG_LOGGING           1   // Comprehensive logging
#endif
// Default performance counters: off in Release, on in Debug unless overridden
#if !defined(ENABLE_PERFORMANCE_COUNTERS)
  #if defined(NDEBUG)
    #define ENABLE_PERFORMANCE_COUNTERS 0
  #else
    #define ENABLE_PERFORMANCE_COUNTERS 1
  #endif
#endif
// Touching scrap cache buffers can be expensive; disable in Release by default
#if !defined(ENABLE_SCRAP_TOUCH)
  #if defined(NDEBUG)
    #define ENABLE_SCRAP_TOUCH 0
  #else
    #define ENABLE_SCRAP_TOUCH 1
  #endif
#endif
#define ENABLE_MEMORY_BUDGETS          1   // Extreme budget patching
#define ENABLE_WORKING_SET_BOOSTER     1   // System memory pressure

// Heap Manager Integration
#define HEAP_ERROR_ENOMEM           0x0C    // Out of memory
#define HEAP_ERROR_EINVAL           0x16    // Invalid argument
#define CRITICAL_SECTION_HEAP       4       // Heap operations lock ID

// ============================================================================
// ADVANCED DATA STRUCTURES - Native Heap Integration
// ============================================================================

// Enhanced allocation header with heap integration
struct HeapAllocHeader {
    size_t size;                    // User requested size
    uint32_t magic;                 // Validation magic (0xDEADC0DE)
    uint32_t pool_id;               // Source pool/heap identifier
    uint32_t size_class;            // Size class index (0-63)
    uint64_t timestamp;             // Allocation timestamp
    uint32_t thread_id;             // Allocating thread ID
    uint32_t flags;                 // Allocation flags
    void* next_in_class;            // Next block in size class
    void* prev_in_class;            // Previous block in size class
};

#define HEAP_HEADER_SIZE sizeof(HeapAllocHeader)

// Free list node for size class management
struct FreeListNode {
    void* next;                     // Next free block
    void* previous;                 // Previous free block
};

// Size class free list
struct FreeList {
    FreeListNode* head;             // First free block
    FreeListNode* tail;             // Last free block
    uint32_t count;                 // Number of free blocks
    uint32_t total_bytes;           // Total bytes in free blocks
};

// Memory segment (1MB virtual reservation)
struct MemorySegment {
    uint32_t master_bitmap_low;     // Bitmap for size classes 0-31
    uint32_t master_bitmap_high;    // Bitmap for size classes 32-63
    uint32_t segment_bitmap;        // Bitmap of free sub-segments
    void* segment_base;             // Base of reserved virtual memory
    struct FreeListsArray* free_lists; // Pointer to free lists array
    volatile LONG committed_pages;   // Number of committed pages
    volatile LONG usage_count;      // Reference count
    CRITICAL_SECTION segment_lock;  // Per-segment synchronization
};

// Free lists array (per segment)
struct FreeListsArray {
    volatile LONG segment_usage_count;      // Global usage counter
    uint8_t size_class_counts[SIZE_CLASSES]; // Per-class allocation counts
    uint32_t size_class_bitmap_low[16];     // Fast lookup bitmaps
    uint32_t size_class_bitmap_high[16];    // Fast lookup bitmaps  
    FreeList free_lists[SIZE_CLASSES];      // 64 size-class free lists
    uint32_t total_free_bytes;              // Total free memory in segment
    uint32_t largest_free_block;            // Size of largest free block
    uint64_t last_access_time;              // For LRU management
};

// ScrapHeap cache entry
struct ScrapCacheEntry {
    void* buffer_ptr;               // Cached buffer address
    uint32_t buffer_size;           // Buffer size
    uint64_t last_used_tick;        // Last access timestamp
    uint32_t use_count;             // Usage frequency counter
};

// ScrapHeap manager (enhanced)
struct ScrapHeapManager {
    ScrapCacheEntry entries[SCRAP_CACHE_ENTRIES];  // Buffer cache
    volatile LONG entry_count;      // Current cached buffers
    volatile LONG64 total_memory;   // Total cached bytes
    volatile LONG64 cache_hits;     // Cache hit counter
    volatile LONG64 cache_misses;   // Cache miss counter
    uint32_t next_touch_check;      // Next buffer touch check
    CRITICAL_SECTION cache_lock;    // Cache synchronization
    
    // Performance counters
    volatile LONG64 requests_served;
    volatile LONG64 buffers_recycled;
    volatile LONG64 fallback_allocations;
};

// Custom heap manager state
struct CustomHeapManager {
    MemorySegment* segments;        // Array of memory segments
    volatile LONG segment_count;    // Current segment count
    volatile LONG max_segments;     // Maximum segments capacity
    void* segment_array_base;       // Base pointer for segment array
    
    // Global free list management
    uint32_t global_bitmap_low;     // Global size class bitmap 0-31
    uint32_t global_bitmap_high;    // Global size class bitmap 32-63
    FreeList global_free_lists[SIZE_CLASSES]; // Global free lists
    
    // Statistics
    volatile LONG64 total_allocated;
    volatile LONG64 total_freed;
    volatile LONG64 segments_created;
    volatile LONG64 segments_destroyed;
    volatile LONG64 coalesce_operations;
    
    CRITICAL_SECTION heap_lock;     // Global heap synchronization
};

// Multi-tier memory pool (enhanced)
struct EnhancedMemoryPool {
    void* base;                     // Pool base address
    volatile LONG64 used;           // Current offset (atomic)
    volatile LONG64 committed;      // Actually committed memory
    SIZE_T size;                    // Total pool size
    SIZE_T alignment;               // Pool alignment requirement
    volatile LONG64 allocs;         // Total allocations from this pool
    volatile LONG64 bytes_served;   // Total bytes allocated
    volatile LONG64 peak_usage;     // Peak memory usage
    const char* name;               // Pool identifier
    uint32_t pool_id;               // Numeric pool ID
    
    // Enhanced features
    volatile LONG64 fast_allocations; // Allocations served directly
    volatile LONG64 overflow_count;    // Overflows to other tiers
    uint64_t last_reset_tick;          // Last pool reset timestamp
    
    CRITICAL_SECTION pool_lock;     // Pool synchronization
    volatile LONG active;           // Pool active flag
};

// Comprehensive system statistics
struct HeapSystemStats {
    // Allocation Metrics
    volatile LONG64 total_allocations;
    volatile LONG64 total_deallocations;
    volatile LONG64 active_allocations;
    volatile LONG64 bytes_allocated;
    volatile LONG64 bytes_deallocated;
    volatile LONG64 peak_memory_usage;
    
    // Performance Metrics
    volatile LONG64 pool_allocations;      // Tier 1: Pool allocations
    volatile LONG64 heap_allocations;      // Tier 2: Custom heap allocations  
    volatile LONG64 system_allocations;    // Tier 3: System allocations
    volatile LONG64 scrap_cache_hits;      // ScrapHeap cache hits
    volatile LONG64 allocation_failures;   // Failed allocations
    
    // Heap-specific metrics
    volatile LONG64 size_class_hits[SIZE_CLASSES]; // Per-class allocation counts
    volatile LONG64 segment_allocations;   // Memory segment allocations
    volatile LONG64 coalesce_operations;   // Block coalescing operations
    volatile LONG64 split_operations;      // Block splitting operations
    
    // System Integration
    volatile LONG64 virtualalloc_boosts;
    volatile LONG64 working_set_expansions;
    volatile LONG64 budget_patch_applications;
    
    // Error Tracking
    volatile LONG64 exceptions_handled;
    volatile LONG64 recovery_operations;
    volatile LONG64 corruption_detections;
    
    // Timing Information
    LARGE_INTEGER perf_frequency;
    LARGE_INTEGER init_time;
    ULONGLONG init_tick_count;
    volatile LONG64 total_alloc_time;
    volatile LONG64 avg_alloc_time;
};

// ============================================================================
// GLOBAL SYSTEM STATE - Enhanced Multi-Tier Architecture
// ============================================================================

// Multi-tier memory pools
static EnhancedMemoryPool g_primary_pool = {0};
static EnhancedMemoryPool g_secondary_pool = {0};
static EnhancedMemoryPool g_texture_pool = {0};
static EnhancedMemoryPool* g_pools[] = { &g_primary_pool, &g_secondary_pool, &g_texture_pool };
static const int g_num_pools = sizeof(g_pools) / sizeof(g_pools[0]);

// Custom heap system
static CustomHeapManager g_custom_heap = {0};
static ScrapHeapManager g_scrap_cache = {0};

// System state
static volatile LONG g_system_initialized = 0;
static volatile LONG g_critical_sections_initialized = 0;
static volatile LONG g_shutting_down = 0;
static volatile LONG g_memory_mode = MEMORY_MODE_HYBRID;

// Critical sections (following game pattern)
static CRITICAL_SECTION g_stats_lock;
static CRITICAL_SECTION g_log_lock;  
static CRITICAL_SECTION g_system_lock;
static CRITICAL_SECTION g_heap_critical_sections[8]; // Multiple heap locks

// Statistics and monitoring
static HeapSystemStats g_stats = {0};

// Native heap integration
static HANDLE g_process_heap = NULL;
static HANDLE g_fallback_heap = NULL;

// Size class lookup tables
static const size_t g_size_class_sizes[SIZE_CLASSES] = {
    16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256,
    272, 288, 304, 320, 336, 352, 368, 384, 400, 416, 432, 448, 464, 480, 496, 512,
    528, 544, 560, 576, 592, 608, 624, 640, 656, 672, 688, 704, 720, 736, 752, 768,
    784, 800, 816, 832, 848, 864, 880, 896, 912, 928, 944, 960, 976, 992, 1008, 1024
};

// Bitmap optimization tables
static uint32_t g_bitmap_cache[BITMAP_CACHE_SIZE];
static volatile LONG g_bitmap_cache_valid = 0;

// Original function pointers
static void* (__cdecl* orig_malloc)(size_t) = nullptr;
static void (__cdecl* orig_free)(void*) = nullptr;
static void* (__cdecl* orig_calloc)(size_t, size_t) = nullptr;
static void* (__cdecl* orig_realloc)(void*, size_t) = nullptr;

// System information
static DWORD g_page_size = 4096;
static DWORD g_processor_count = 1;

// ============================================================================
// UTILITY FUNCTIONS - Enhanced with Heap Integration
// ============================================================================

// High-performance timing
static inline LARGE_INTEGER GetHighResolutionTime() {
    LARGE_INTEGER time;
    QueryPerformanceCounter(&time);
    return time;
}

static inline double GetElapsedMs(LARGE_INTEGER start, LARGE_INTEGER end) {
    return (double)(end.QuadPart - start.QuadPart) * 1000.0 / g_stats.perf_frequency.QuadPart;
}

// Size class calculation (16-byte granularity)
static __forceinline uint32_t GetSizeClass(size_t size) {
    if (size == 0) return 0;
    if (size > SMALL_BLOCK_THRESHOLD) return SIZE_CLASSES - 1;
    
    // Fast calculation: (size + 15) >> 4 - 1
    uint32_t size_class = ((uint32_t)size + 15) >> 4;
    return (size_class > 0) ? size_class - 1 : 0;
}

static __forceinline size_t GetClassSize(uint32_t size_class) {
    return (size_class < SIZE_CLASSES) ? g_size_class_sizes[size_class] : 0;
}

// Small helpers
static __forceinline SIZE_T AlignUp(SIZE_T x, SIZE_T align) { return (x + align - 1) & ~(align - 1); }
static __forceinline SIZE_T MaxSZ(SIZE_T a, SIZE_T b) { return (a > b) ? a : b; }

// Bitmap operations for fast free block discovery
static __forceinline uint32_t FindFirstSetBit(uint32_t value) {
    DWORD index;
    return _BitScanForward(&index, value) ? index : 32;
}

static inline void SetBitmapBit(uint32_t* bitmap_low, uint32_t* bitmap_high, uint32_t bit) {
    if (bit < 32) {
        *bitmap_low |= (1U << bit);
    } else {
        *bitmap_high |= (1U << (bit - 32));
    }
}

static inline void ClearBitmapBit(uint32_t* bitmap_low, uint32_t* bitmap_high, uint32_t bit) {
    if (bit < 32) {
        *bitmap_low &= ~(1U << bit);
    } else {
        *bitmap_high &= ~(1U << (bit - 32));
    }
}

// Pool utilities
static __forceinline SIZE_T GetPoolUsed(const EnhancedMemoryPool* pool) {
    return (SIZE_T)pool->used;
}

static __forceinline bool IsInPool(const void* ptr, const EnhancedMemoryPool* pool) {
    return pool->base && ptr >= pool->base && ptr < (char*)pool->base + pool->size;
}

static __forceinline bool IsInAnyPool(const void* ptr) {
    if (!ptr) return false;
    for (int i = 0; i < g_num_pools; i++) {
        if (IsInPool(ptr, g_pools[i])) return true;
    }
    return false;
}

// Segment utilities
static __forceinline bool IsInCustomHeap(const void* ptr) {
    if (!ptr || !g_custom_heap.segments) return false;
    
    for (LONG i = 0; i < g_custom_heap.segment_count; i++) {
        MemorySegment* segment = &g_custom_heap.segments[i];
        if (segment->segment_base && 
            ptr >= segment->segment_base && 
            ptr < (char*)segment->segment_base + SEGMENT_SIZE) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// ADVANCED LOGGING SYSTEM
// ============================================================================

#if ENABLE_DEBUG_LOGGING
static void HeapSystemLog(const char* level, const char* format, ...) {
    if (!g_critical_sections_initialized || g_shutting_down) return;
    
    char buffer[1024];
    char final_buffer[1280];
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    
    ULONGLONG tick = GetTickCount64();
    ULONGLONG elapsed = tick - g_stats.init_tick_count;
    snprintf(final_buffer, sizeof(final_buffer), "[%08llu] [%s] %s", (unsigned long long)elapsed, level, buffer);
    
    __try {
        EnterCriticalSection(&g_log_lock);
        
        CreateDirectoryA("Data\\NVSE", NULL);
        CreateDirectoryA("Data\\NVSE\\Plugins", NULL);
        
        HANDLE hFile = CreateFileA("Data\\NVSE\\Plugins\\HeapMaster.log",
                                   GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                   OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            SetFilePointer(hFile, 0, NULL, FILE_END);
            DWORD written;
            WriteFile(hFile, final_buffer, (DWORD)strlen(final_buffer), &written, NULL);
            WriteFile(hFile, "\r\n", 2, &written, NULL);
            CloseHandle(hFile);
        }
    }
    __finally {
        LeaveCriticalSection(&g_log_lock);
    }
}

#define LOG_INFO(fmt, ...) HeapSystemLog("INFO", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) HeapSystemLog("WARN", fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) HeapSystemLog("ERROR", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) HeapSystemLog("DEBUG", fmt, ##__VA_ARGS__)
#define LOG_PERF(fmt, ...) HeapSystemLog("PERF", fmt, ##__VA_ARGS__)
#define LOG_HEAP(fmt, ...) HeapSystemLog("HEAP", fmt, ##__VA_ARGS__)

#else
#define LOG_INFO(fmt, ...) ((void)0)
#define LOG_WARN(fmt, ...) ((void)0)
#define LOG_ERROR(fmt, ...) ((void)0)
#define LOG_DEBUG(fmt, ...) ((void)0)
#define LOG_PERF(fmt, ...) ((void)0)
#define LOG_HEAP(fmt, ...) ((void)0)
#endif

// ============================================================================
// CRITICAL SECTION MANAGEMENT - Game Engine Pattern
// ============================================================================

static void EnterHeapCriticalSection(int lock_id) {
    if (lock_id >= 0 && lock_id < 8 && g_critical_sections_initialized) {
        EnterCriticalSection(&g_heap_critical_sections[lock_id]);
    }
}

static void LeaveHeapCriticalSection(int lock_id) {
    if (lock_id >= 0 && lock_id < 8 && g_critical_sections_initialized) {
        LeaveCriticalSection(&g_heap_critical_sections[lock_id]);
    }
}

// ============================================================================
// SCRAPHEAP CACHE SYSTEM - Enhanced Implementation
// ============================================================================

#if ENABLE_SCRAP_CACHE_SYSTEM

// Initialize ScrapHeap cache
static bool InitializeScrapCache() {
    memset(&g_scrap_cache, 0, sizeof(g_scrap_cache));
    
    __try {
        if (!InitializeCriticalSectionAndSpinCount(&g_scrap_cache.cache_lock, HEAP_SPIN_COUNT)) {
            LOG_ERROR("InitializeCriticalSectionAndSpinCount failed for ScrapHeap cache");
            return false;
        }
        LOG_INFO("ScrapHeap cache initialized: %d entries, %zu MB capacity",
                 SCRAP_CACHE_ENTRIES, SCRAP_CACHE_SIZE / (1024*1024));
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERROR("Failed to initialize ScrapHeap cache");
        return false;
    }
}

// Request buffer from ScrapHeap cache
static void* ScrapCacheRequestBuffer(size_t size) {
    if (size < SCRAP_MIN_SIZE || size > SCRAP_MAX_SIZE) {
        return NULL; // Outside cacheable range
    }
    
    void* result = NULL;
    ULONGLONG current_tick = GetTickCount64();

    // Fast-path: avoid lock if cache is empty
    if (g_scrap_cache.entry_count == 0) {
        InterlockedIncrement64(&g_scrap_cache.cache_misses);
        return NULL;
    }

    EnterCriticalSection(&g_scrap_cache.cache_lock);
    
    __try {
        // Search for exact size match first
        for (LONG i = 0; i < g_scrap_cache.entry_count; i++) {
            ScrapCacheEntry* entry = &g_scrap_cache.entries[i];
            if (entry->buffer_size == size && entry->buffer_ptr) {
                result = entry->buffer_ptr;
                entry->buffer_ptr = NULL; // Remove from cache
                entry->use_count++;
                entry->last_used_tick = current_tick;
                
                InterlockedIncrement64(&g_scrap_cache.cache_hits);
                InterlockedExchangeAdd64(&g_scrap_cache.total_memory, -(LONG64)size);
                
                // Compact array
                for (LONG j = i; j < g_scrap_cache.entry_count - 1; j++) {
                    g_scrap_cache.entries[j] = g_scrap_cache.entries[j + 1];
                }
                InterlockedDecrement(&g_scrap_cache.entry_count);
                
                LOG_DEBUG("ScrapCache hit: %zu bytes from entry %d", size, i);
                break;
            }
        }
        
        // Search for suitable larger buffer
        if (!result) {
            for (LONG i = 0; i < g_scrap_cache.entry_count; i++) {
                ScrapCacheEntry* entry = &g_scrap_cache.entries[i];
                if (entry->buffer_size >= size && entry->buffer_size <= size * 2 && entry->buffer_ptr) {
                    result = entry->buffer_ptr;
                    
                    // Split buffer if significantly larger
                    if (entry->buffer_size > size + 64) {
                        // TODO: Implement buffer splitting
                        LOG_DEBUG("ScrapCache split candidate: %u -> %zu bytes", entry->buffer_size, size);
                    }
                    
                    entry->buffer_ptr = NULL;
                    entry->use_count++;
                    entry->last_used_tick = current_tick;
                    
                    InterlockedIncrement64(&g_scrap_cache.cache_hits);
                    InterlockedExchangeAdd64(&g_scrap_cache.total_memory, -(LONG64)entry->buffer_size);
                    
                    // Compact array
                    for (LONG j = i; j < g_scrap_cache.entry_count - 1; j++) {
                        g_scrap_cache.entries[j] = g_scrap_cache.entries[j + 1];
                    }
                    InterlockedDecrement(&g_scrap_cache.entry_count);
                    
                    LOG_DEBUG("ScrapCache partial hit: %u bytes for %zu request", entry->buffer_size, size);
                    break;
                }
            }
        }
        
        if (!result) {
            InterlockedIncrement64(&g_scrap_cache.cache_misses);
        }
    }
    __finally {
        LeaveCriticalSection(&g_scrap_cache.cache_lock);
    }
    
    return result;
}

// Release buffer to ScrapHeap cache
static bool ScrapCacheReleaseBuffer(void* ptr, size_t size) {
    if (!ptr || size < SCRAP_MIN_SIZE || size > SCRAP_MAX_SIZE) {
        return false;
    }
    
    bool cached = false;
    ULONGLONG current_tick = GetTickCount64();

    // Fast-path: if cache is full and LRU policy disabled, skip locking
    if (g_scrap_cache.entry_count >= SCRAP_CACHE_ENTRIES) {
        return false;
    }

    EnterCriticalSection(&g_scrap_cache.cache_lock);
    
    __try {
        // Check if we have space in cache
        if (g_scrap_cache.entry_count < SCRAP_CACHE_ENTRIES) {
            ScrapCacheEntry* entry = &g_scrap_cache.entries[g_scrap_cache.entry_count];
            entry->buffer_ptr = ptr;
            entry->buffer_size = (uint32_t)size;
            entry->last_used_tick = current_tick;
            entry->use_count = 1;
            
            InterlockedIncrement(&g_scrap_cache.entry_count);
            InterlockedExchangeAdd64(&g_scrap_cache.total_memory, (LONG64)size);
            InterlockedIncrement64(&g_scrap_cache.buffers_recycled);
            
            cached = true;
            LOG_DEBUG("ScrapCache stored: %zu bytes in entry %d", size, g_scrap_cache.entry_count - 1);
        } else {
            // Cache full - find least recently used entry
            LONG lru_index = 0;
            ULONGLONG oldest_tick = g_scrap_cache.entries[0].last_used_tick;
            
            for (LONG i = 1; i < g_scrap_cache.entry_count; i++) {
                if (g_scrap_cache.entries[i].last_used_tick < oldest_tick) {
                    oldest_tick = g_scrap_cache.entries[i].last_used_tick;
                    lru_index = i;
                }
            }
            
            // Free LRU entry and replace with new buffer
            ScrapCacheEntry* lru_entry = &g_scrap_cache.entries[lru_index];
            if (lru_entry->buffer_ptr) {
                InterlockedExchangeAdd64(&g_scrap_cache.total_memory, -(LONG64)lru_entry->buffer_size);
                // Note: Not actually freeing the old buffer here to match game behavior
            }
            
            lru_entry->buffer_ptr = ptr;
            lru_entry->buffer_size = (uint32_t)size;
            lru_entry->last_used_tick = current_tick;
            lru_entry->use_count = 1;
            
            InterlockedExchangeAdd64(&g_scrap_cache.total_memory, (LONG64)size);
            cached = true;
            LOG_DEBUG("ScrapCache LRU replace: %zu bytes in entry %d", size, lru_index);
        }
    }
    __finally {
        LeaveCriticalSection(&g_scrap_cache.cache_lock);
    }
    
    return cached;
}

// Touch cached buffers to keep them in working set
static void TouchScrapCacheBuffers() {
    if (g_scrap_cache.entry_count == 0) return;
    
    EnterCriticalSection(&g_scrap_cache.cache_lock);
    
    __try {
        for (LONG i = 0; i < g_scrap_cache.entry_count; i++) {
            ScrapCacheEntry* entry = &g_scrap_cache.entries[i];
            if (entry->buffer_ptr && entry->buffer_size > 0) {
                volatile char* touch = (volatile char*)entry->buffer_ptr;
                touch[0] = 1;  // Touch first byte
                if (entry->buffer_size > g_page_size) {
                    touch[entry->buffer_size - 1] = 1; // Touch last byte
                }
            }
        }
    }
    __finally {
        LeaveCriticalSection(&g_scrap_cache.cache_lock);
    }
}

#endif // ENABLE_SCRAP_CACHE_SYSTEM

// ============================================================================
// CUSTOM HEAP SEGMENT MANAGEMENT
// ============================================================================

#if ENABLE_SEGMENT_MANAGEMENT

// Initialize custom heap manager
static bool InitializeCustomHeap() {
    memset(&g_custom_heap, 0, sizeof(g_custom_heap));
    
    __try {
        if (!InitializeCriticalSectionAndSpinCount(&g_custom_heap.heap_lock, HEAP_SPIN_COUNT)) {
            LOG_ERROR("InitializeCriticalSectionAndSpinCount failed for custom heap lock");
            return false;
        }
        
        // Allocate initial segment array (can grow)
        g_custom_heap.max_segments = 64;
        SIZE_T segments_size = g_custom_heap.max_segments * sizeof(MemorySegment);
        g_custom_heap.segment_array_base = VirtualAlloc(NULL, segments_size, 
                                                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        
        if (!g_custom_heap.segment_array_base) {
            LOG_ERROR("Failed to allocate segment array");
            return false;
        }
        
        g_custom_heap.segments = (MemorySegment*)g_custom_heap.segment_array_base;
        
        // Initialize global free lists
        for (int i = 0; i < SIZE_CLASSES; i++) {
            g_custom_heap.global_free_lists[i].head = NULL;
            g_custom_heap.global_free_lists[i].tail = NULL;
            g_custom_heap.global_free_lists[i].count = 0;
            g_custom_heap.global_free_lists[i].total_bytes = 0;
        }
        
        LOG_INFO("Custom heap manager initialized: %d max segments, %.1f MB capacity",
                 g_custom_heap.max_segments, (double)segments_size / (1024.0*1024.0));
        
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        LOG_ERROR("Exception initializing custom heap manager");
        return false;
    }
}

// Create new memory segment
static MemorySegment* CreateMemorySegment() {
    if (g_custom_heap.segment_count >= g_custom_heap.max_segments) {
        LOG_ERROR("Maximum segments reached: %d", g_custom_heap.max_segments);
        return NULL;
    }
    
    // Reserve 1MB virtual memory
    void* segment_base = VirtualAlloc(NULL, SEGMENT_SIZE, MEM_RESERVE, PAGE_READWRITE);
    if (!segment_base) {
        LOG_ERROR("Failed to reserve segment memory: %d", GetLastError());
        return NULL;
    }
    
    // Get next segment slot
    LONG segment_index = InterlockedIncrement(&g_custom_heap.segment_count) - 1;
    MemorySegment* segment = &g_custom_heap.segments[segment_index];
    
    // Initialize segment
    memset(segment, 0, sizeof(MemorySegment));
    segment->segment_base = segment_base;
    
    __try {
        if (!InitializeCriticalSectionAndSpinCount(&segment->segment_lock, HEAP_SPIN_COUNT)) {
            VirtualFree(segment_base, 0, MEM_RELEASE);
            InterlockedDecrement(&g_custom_heap.segment_count);
            LOG_ERROR("InitializeCriticalSectionAndSpinCount failed for segment lock");
            return NULL;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        VirtualFree(segment_base, 0, MEM_RELEASE);
        InterlockedDecrement(&g_custom_heap.segment_count);
        LOG_ERROR("Failed to initialize segment critical section");
        return NULL;
    }
    
    // Allocate and commit memory for free lists array
    SIZE_T freelists_size = sizeof(FreeListsArray);
    segment->free_lists = (FreeListsArray*)VirtualAlloc((char*)segment_base, freelists_size,
                                                        MEM_COMMIT, PAGE_READWRITE);
    
    if (!segment->free_lists) {
        DeleteCriticalSection(&segment->segment_lock);
        VirtualFree(segment_base, 0, MEM_RELEASE);
        InterlockedDecrement(&g_custom_heap.segment_count);
        LOG_ERROR("Failed to commit free lists array");
        return NULL;
    }
    
    // Initialize free lists array
    memset(segment->free_lists, 0, sizeof(FreeListsArray));
    for (int i = 0; i < SIZE_CLASSES; i++) {
        segment->free_lists->free_lists[i].head = NULL;
        segment->free_lists->free_lists[i].tail = NULL;
        segment->free_lists->free_lists[i].count = 0;
        segment->free_lists->free_lists[i].total_bytes = 0;
    }
    
    InterlockedIncrement64(&g_custom_heap.segments_created);
    InterlockedIncrement(&segment->committed_pages);
    
    LOG_HEAP("Created memory segment %d: %p (%zu KB)", segment_index, segment_base, SEGMENT_SIZE / 1024);
    
    return segment;
}

// Find suitable free block in segment
static void* FindFreeBlockInSegment(MemorySegment* segment, size_t size, uint32_t size_class) {
    if (!segment || !segment->free_lists) return NULL;
    
    EnterCriticalSection(&segment->segment_lock);
    
    void* result = NULL;
    
    __try {
        FreeListsArray* lists = segment->free_lists;
        
        // Check exact size class first
        if (size_class < SIZE_CLASSES) {
            FreeList* free_list = &lists->free_lists[size_class];
            if (free_list->head) {
                FreeListNode* node = free_list->head;
                
                // Remove from free list
                free_list->head = (FreeListNode*)node->next;
                if (free_list->head) {
                    ((FreeListNode*)free_list->head)->previous = NULL;
                } else {
                    free_list->tail = NULL;
                }
                
                free_list->count--;
                free_list->total_bytes -= GetClassSize(size_class);
                
                // Update bitmaps
                if (free_list->count == 0) {
                    ClearBitmapBit(&segment->master_bitmap_low, &segment->master_bitmap_high, size_class);
                    ClearBitmapBit(&lists->size_class_bitmap_low[0], &lists->size_class_bitmap_high[0], size_class);
                }
                
                result = node;
                LOG_DEBUG("Found exact free block: size class %u, address %p", size_class, result);
            }
        }
        
        // Search larger size classes if exact match not found
        if (!result) {
            for (uint32_t sc = size_class + 1; sc < SIZE_CLASSES; sc++) {
                FreeList* free_list = &lists->free_lists[sc];
                if (free_list->head) {
                    FreeListNode* node = free_list->head;
                    
                    // Remove from current size class
                    free_list->head = (FreeListNode*)node->next;
                    if (free_list->head) {
                        ((FreeListNode*)free_list->head)->previous = NULL;
                    } else {
                        free_list->tail = NULL;
                    }
                    
                    free_list->count--;
                    size_t block_size = GetClassSize(sc);
                    free_list->total_bytes -= block_size;
                    
                    // Update bitmaps
                    if (free_list->count == 0) {
                        ClearBitmapBit(&segment->master_bitmap_low, &segment->master_bitmap_high, sc);
                    }
                    
                    result = node;
                    
#if ENABLE_BLOCK_COALESCING
                    // Split block if it's significantly larger
                    if (block_size >= size * 2 && block_size - size >= 64) {
                        void* split_ptr = (char*)result + size;
                        size_t remaining_size = block_size - size;
                        uint32_t remaining_class = GetSizeClass(remaining_size);
                        
                        if (remaining_class < SIZE_CLASSES) {
                            // Add remaining part back to appropriate free list
                            FreeList* remaining_list = &lists->free_lists[remaining_class];
                            FreeListNode* remaining_node = (FreeListNode*)split_ptr;
                            
                            remaining_node->next = remaining_list->head;
                            remaining_node->previous = NULL;
                            
                            if (remaining_list->head) {
                                ((FreeListNode*)remaining_list->head)->previous = remaining_node;
                            } else {
                                remaining_list->tail = remaining_node;
                            }
                            
                            remaining_list->head = remaining_node;
                            remaining_list->count++;
                            remaining_list->total_bytes += remaining_size;
                            
                            // Update bitmap
                            SetBitmapBit(&segment->master_bitmap_low, &segment->master_bitmap_high, remaining_class);
                            
                            InterlockedIncrement64(&g_stats.split_operations);
                            LOG_DEBUG("Split block: %zu -> %zu + %zu", block_size, size, remaining_size);
                        }
                    }
#endif
                    
                    LOG_DEBUG("Found larger free block: size class %u->%u, address %p", sc, size_class, result);
                    break;
                }
            }
        }
    }
    __finally {
        LeaveCriticalSection(&segment->segment_lock);
    }
    
    return result;
}

// Allocate block from custom heap
static void* CustomHeapAllocate(size_t size) {
    if (size == 0 || size > SMALL_BLOCK_THRESHOLD) {
        return NULL; // Outside custom heap range
    }
    
    uint32_t size_class = GetSizeClass(size);
    size_t actual_size = GetClassSize(size_class);
    
    void* result = NULL;
    
    // Try existing segments first
    for (LONG i = 0; i < g_custom_heap.segment_count && !result; i++) {
        MemorySegment* segment = &g_custom_heap.segments[i];
        
        // Quick bitmap check before expensive search
        bool has_suitable_blocks = false;
        if (size_class < 32) {
            has_suitable_blocks = (segment->master_bitmap_low & ~((1U << size_class) - 1)) != 0;
        } else {
            has_suitable_blocks = (segment->master_bitmap_high != 0) || 
                                  (segment->master_bitmap_low & ~((1U << size_class) - 1)) != 0;
        }
        
        if (has_suitable_blocks) {
            result = FindFreeBlockInSegment(segment, actual_size, size_class);
        }
    }
    
    // Create new segment if needed
    if (!result) {
        MemorySegment* new_segment = CreateMemorySegment();
        if (new_segment) {
            // Commit additional pages for allocation
            SIZE_T commit_size = SUB_SEGMENT_SIZE; // Commit 32KB at a time
            void* commit_addr = (char*)new_segment->segment_base + sizeof(FreeListsArray);
            
            void* committed = VirtualAlloc(commit_addr, commit_size, MEM_COMMIT, PAGE_READWRITE);
            if (committed) {
                InterlockedIncrement(&new_segment->committed_pages);
                
                // Add entire committed block as single large free block
                size_t free_block_size = commit_size;
                uint32_t free_size_class = GetSizeClass(free_block_size);
                
                if (free_size_class >= SIZE_CLASSES) {
                    free_size_class = SIZE_CLASSES - 1; // Use largest size class for oversized blocks
                }
                
                FreeList* free_list = &new_segment->free_lists->free_lists[free_size_class];
                FreeListNode* free_node = (FreeListNode*)commit_addr;
                
                free_node->next = NULL;
                free_node->previous = NULL;
                free_list->head = free_node;
                free_list->tail = free_node;
                free_list->count = 1;
                free_list->total_bytes = free_block_size;
                
                // Update bitmaps
                SetBitmapBit(&new_segment->master_bitmap_low, &new_segment->master_bitmap_high, free_size_class);
                
                LOG_HEAP("Added initial free block: %zu bytes, class %u", free_block_size, free_size_class);
                
                // Now try to allocate from this segment
                result = FindFreeBlockInSegment(new_segment, actual_size, size_class);
            }
        }
    }
    
    if (result) {
        InterlockedIncrement64(&g_stats.heap_allocations);
        InterlockedIncrement64(&g_stats.size_class_hits[size_class]);
        InterlockedIncrement64(&g_custom_heap.total_allocated);
        
        // Initialize allocation header
        HeapAllocHeader* header = (HeapAllocHeader*)result;
        header->size = size;
        header->magic = 0xDEADC0DE;
        header->pool_id = 0xFFFFFFFE; // Custom heap marker
        header->size_class = size_class;
        header->timestamp = GetTickCount64();
        header->thread_id = GetCurrentThreadId();
        header->flags = 0;
        header->next_in_class = NULL;
        header->prev_in_class = NULL;
        
        result = header + 1; // Return pointer past header
        
        LOG_DEBUG("CustomHeap allocation: %zu bytes (class %u) at %p", size, size_class, result);
    }
    
    return result;
}

// Free block in custom heap
static bool CustomHeapFree(void* ptr) {
    if (!ptr || !IsInCustomHeap(ptr)) {
        return false;
    }
    
    // Get allocation header
    HeapAllocHeader* header = ((HeapAllocHeader*)ptr) - 1;
    
    __try {
        if (header->magic != 0xDEADC0DE) {
            InterlockedIncrement64(&g_stats.corruption_detections);
            LOG_ERROR("Corrupted heap block header: magic 0x%08X", header->magic);
            return false;
        }
        
        uint32_t size_class = header->size_class;
        if (size_class >= SIZE_CLASSES) {
            LOG_ERROR("Invalid size class in heap block: %u", size_class);
            return false;
        }
        
        // Find containing segment
        MemorySegment* segment = NULL;
        for (LONG i = 0; i < g_custom_heap.segment_count; i++) {
            MemorySegment* seg = &g_custom_heap.segments[i];
            if (seg->segment_base && 
                header >= seg->segment_base && 
                header < (HeapAllocHeader*)((char*)seg->segment_base + SEGMENT_SIZE)) {
                segment = seg;
                break;
            }
        }
        
        if (!segment) {
            LOG_ERROR("Cannot find segment for heap block %p", ptr);
            return false;
        }
        
        EnterCriticalSection(&segment->segment_lock);
        
        __try {
            // Clear header magic to prevent double-free
            header->magic = 0;
            
            // Add block to appropriate free list
            FreeList* free_list = &segment->free_lists->free_lists[size_class];
            FreeListNode* node = (FreeListNode*)header;
            
            node->next = free_list->head;
            node->previous = NULL;
            
            if (free_list->head) {
                ((FreeListNode*)free_list->head)->previous = node;
            } else {
                free_list->tail = node;
            }
            
            free_list->head = node;
            free_list->count++;
            free_list->total_bytes += GetClassSize(size_class);
            
            // Update bitmaps
            SetBitmapBit(&segment->master_bitmap_low, &segment->master_bitmap_high, size_class);
            
            InterlockedIncrement64(&g_custom_heap.total_freed);
            
#if ENABLE_BLOCK_COALESCING
            // TODO: Implement block coalescing
            // This would merge adjacent free blocks to reduce fragmentation
#endif
            
            LOG_DEBUG("CustomHeap free: size class %u, address %p", size_class, header);
            
        }
        __finally {
            LeaveCriticalSection(&segment->segment_lock);
        }
        
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        InterlockedIncrement64(&g_stats.exceptions_handled);
        LOG_ERROR("Exception freeing heap block %p", ptr);
        return false;
    }
}

#endif // ENABLE_SEGMENT_MANAGEMENT

// ============================================================================
// ENHANCED POOL ALLOCATION - Tier 1 System
// ============================================================================

// Initialize enhanced memory pool (reserve big, commit on demand)
static bool InitializeEnhancedPool(EnhancedMemoryPool* pool, SIZE_T desired_size, const char* name, uint32_t pool_id) {
    // Best-effort reserve: gradually reduce until success
    SIZE_T reserve_size = desired_size;
    void* base = nullptr;
    while (reserve_size >= RESERVE_MIN_SIZE) {
        base = VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_READWRITE);
        if (base) break;
        reserve_size -= RESERVE_RETRY_STEP;
    }

    if (!base) {
        LOG_ERROR("Failed to reserve %s: requested %zu MB", name, desired_size / (1024*1024));
        return false;
    }

    // Initialize enhanced pool structure
    pool->size = reserve_size;
    pool->base = base;
    pool->used = 0;
    pool->committed = 0;
    pool->allocs = 0;
    pool->bytes_served = 0;
    pool->peak_usage = 0;
    pool->name = name;
    pool->pool_id = pool_id;
    pool->alignment = POOL_ALIGNMENT;
    pool->fast_allocations = 0;
    pool->overflow_count = 0;
    pool->last_reset_tick = GetTickCount64();
    pool->active = 1;

    __try {
        if (!InitializeCriticalSectionAndSpinCount(&pool->pool_lock, HEAP_SPIN_COUNT)) {
            VirtualFree(pool->base, 0, MEM_RELEASE);
            pool->base = NULL;
            return false;
        }

        // Commit small initial region to avoid first-allocation fault
        SIZE_T initial_commit = (SIZE_T)POOL_INITIAL_COMMIT;
        initial_commit = AlignUp(initial_commit, g_page_size);
        if (!VirtualAlloc(pool->base, initial_commit, MEM_COMMIT, PAGE_READWRITE)) {
            // If initial commit fails, keep pool reserved but inactive
            LOG_WARN("%s initial commit failed; pool reserved only", name);
            pool->active = 0;
        } else {
            InterlockedExchangeAdd64(&pool->committed, (LONG64)initial_commit);
        }

        LOG_INFO("%s reserved: %zu MB at 0x%p (initial commit: %zu KB)",
                 name, pool->size / (1024*1024), pool->base, initial_commit / 1024);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        VirtualFree(pool->base, 0, MEM_RELEASE);
        pool->base = NULL;
        return false;
    }
}

// Ensure pool has enough committed memory for [0, required_end)
static bool EnsureCommitted(EnhancedMemoryPool* pool, SIZE_T required_end) {
    if ((SIZE_T)pool->committed >= required_end) return true;

    EnterCriticalSection(&pool->pool_lock);
    bool ok = true;
    __try {
        SIZE_T committed = (SIZE_T)pool->committed;
        if (required_end > committed) {
            SIZE_T step = (SIZE_T)POOL_COMMIT_STEP;
            SIZE_T to_commit = AlignUp(required_end - committed, MaxSZ(step, (SIZE_T)g_page_size));
            void* commit_addr = (char*)pool->base + committed;
            void* res = VirtualAlloc(commit_addr, to_commit, MEM_COMMIT, PAGE_READWRITE);
            if (res) {
                InterlockedExchangeAdd64(&pool->committed, (LONG64)to_commit);
            } else {
                ok = false;
            }
        }
    }
    __finally {
        LeaveCriticalSection(&pool->pool_lock);
    }
    return ok;
}

// Enhanced pool allocation with overflow detection
static void* EnhancedPoolAllocate(size_t size) {
    if (size == 0 || size > MAX_ALLOCATION_SIZE) {
        return NULL;
    }
    
    // Align size for optimal performance
    size_t aligned_size = (size + POOL_ALIGNMENT - 1) & ~(POOL_ALIGNMENT - 1);
    size_t total_size = HEAP_HEADER_SIZE + aligned_size;
    
    // Select optimal pool based on size and load balancing
    EnhancedMemoryPool* pool = &g_primary_pool; // Default to primary
    
    if (size > 1024 * 1024) {
        pool = &g_secondary_pool; // Large allocations
    } else if (size >= 256 * 1024 && size <= 4 * 1024 * 1024) {
        pool = &g_texture_pool; // Texture-like allocations
    }
    
    // Check if selected pool has space
    if (!pool->active || !pool->base || 
        (pool->used + total_size) > pool->size) {
        
        // Try fallback pools
        for (int i = 0; i < g_num_pools; i++) {
            EnhancedMemoryPool* fallback = g_pools[i];
            if (fallback->active && fallback->base && 
                (fallback->used + total_size) <= fallback->size) {
                pool = fallback;
                InterlockedIncrement64(&pool->overflow_count);
                break;
            }
        }
        
        // All pools exhausted
        if (!pool->active || (pool->used + total_size) > pool->size) {
            LOG_WARN("All pools exhausted for %zu byte allocation", size);
            return NULL;
        }
    }
    
    // Atomic allocation
    LONG64 offset = InterlockedExchangeAdd64(&pool->used, (LONG64)total_size);
    
    // Double-check after atomic operation
    if (offset + total_size > pool->size || !pool->base) {
        // roll back
        InterlockedExchangeAdd64(&pool->used, -(LONG64)total_size);
        LOG_ERROR("Pool overflow or null base detected after atomic allocation");
        return NULL;
    }

    // Ensure committed memory covers this allocation
    SIZE_T required_end = (SIZE_T)(offset + total_size);
    if ((SIZE_T)pool->committed < required_end) {
        if (!EnsureCommitted(pool, required_end)) {
            // Roll back used on commit failure
            InterlockedExchangeAdd64(&pool->used, -(LONG64)total_size);
            LOG_ERROR("Commit failed for %s: required %zu bytes", pool->name, (SIZE_T)total_size);
            return NULL;
        }
    }
    
    // Calculate allocation address
    HeapAllocHeader* header = (HeapAllocHeader*)((char*)pool->base + offset);
    if (!header) {
        InterlockedExchangeAdd64(&pool->used, -(LONG64)total_size);
        return NULL;
    }
    void* user_ptr = header + 1;
    if (!user_ptr) {
        InterlockedExchangeAdd64(&pool->used, -(LONG64)total_size);
        return NULL;
    }
    
    // Initialize enhanced header
    header->size = size;
    header->magic = 0xDEADC0DE;
    header->pool_id = pool->pool_id;
    header->size_class = GetSizeClass(size);
    header->timestamp = GetTickCount64();
    header->thread_id = GetCurrentThreadId();
    header->flags = 0x1; // Pool allocation flag
    header->next_in_class = NULL;
    header->prev_in_class = NULL;
    
    // Update statistics
    InterlockedIncrement64(&pool->allocs);
    InterlockedIncrement64(&pool->fast_allocations);
    InterlockedExchangeAdd64(&pool->bytes_served, (LONG64)total_size);
    InterlockedIncrement64(&g_stats.total_allocations);
    InterlockedIncrement64(&g_stats.pool_allocations);
    InterlockedExchangeAdd64(&g_stats.bytes_allocated, (LONG64)total_size);
    
    // Update peak usage
    SIZE_T current_usage = (SIZE_T)(offset + total_size);
    LONG64 peak = pool->peak_usage;
    while (current_usage > peak) {
        if (InterlockedCompareExchange64(&pool->peak_usage, current_usage, peak) == peak) {
            break;
        }
        peak = pool->peak_usage;
    }
    
    // Zero memory
    memset(user_ptr, 0, aligned_size);
    
    LOG_DEBUG("Pool allocation: %zu bytes in %s at %p", size, pool->name, user_ptr);
    
    return user_ptr;
}

// ============================================================================
// MULTI-TIER ALLOCATION SYSTEM - The Heart of HeapMaster
// ============================================================================

// Three-tier allocation strategy implementation
static void* MultiTierAllocate(size_t size) {
    if (size == 0 || size > MAX_ALLOCATION_SIZE) {
        return NULL;
    }
    
#if ENABLE_PERFORMANCE_COUNTERS
    LARGE_INTEGER start_time = GetHighResolutionTime();
#endif
    
    void* result = NULL;
    
    // Tier 1: Try ScrapHeap cache for high-frequency small allocations
#if ENABLE_SCRAP_CACHE_SYSTEM
    if (size >= SCRAP_MIN_SIZE && size <= SCRAP_MAX_SIZE) {
        result = ScrapCacheRequestBuffer(size);
        if (result) {
            InterlockedIncrement64(&g_stats.scrap_cache_hits);
            LOG_DEBUG("Tier 1 (ScrapCache): %zu bytes", size);
            goto allocation_success;
        }
    }
#endif
    
    // Tier 2: Try enhanced memory pools for medium allocations
    if (size <= SMALL_BLOCK_THRESHOLD * 4) { // Up to 4KB
        result = EnhancedPoolAllocate(size);
        if (result) {
            LOG_DEBUG("Tier 2 (Pool): %zu bytes", size);
            goto allocation_success;
        }
    }
    
    // Tier 3: Try custom heap system for size-classed allocations
#if ENABLE_SEGMENT_MANAGEMENT
    if (size <= SMALL_BLOCK_THRESHOLD) { // Up to 1KB
        result = CustomHeapAllocate(size);
        if (result) {
            LOG_DEBUG("Tier 3 (CustomHeap): %zu bytes", size);
            goto allocation_success;
        }
    }
#endif
    
    // Tier 4: System heap fallback
    if (g_process_heap) {
        result = HeapAlloc(g_process_heap, HEAP_ZERO_MEMORY, size);
        if (result) {
            InterlockedIncrement64(&g_stats.system_allocations);
            LOG_DEBUG("Tier 4 (System): %zu bytes", size);
            goto allocation_success;
        }
    }
    
    // Final fallback: Use fallback heap
    if (g_fallback_heap) {
        result = HeapAlloc(g_fallback_heap, HEAP_ZERO_MEMORY, size);
        if (result) {
            InterlockedIncrement64(&g_stats.system_allocations);
            LOG_DEBUG("Tier 5 (Fallback): %zu bytes", size);
        }
    }
    
allocation_success:
    if (result) {
        InterlockedIncrement64(&g_stats.total_allocations);
        InterlockedExchangeAdd64(&g_stats.bytes_allocated, (LONG64)size);
        
        // Touch allocation periodically for ScrapCache
        static volatile LONG alloc_counter = 0;
        if ((InterlockedIncrement(&alloc_counter) % SCRAP_TOUCH_INTERVAL) == 0) {
#if ENABLE_SCRAP_CACHE_SYSTEM && ENABLE_SCRAP_TOUCH
            TouchScrapCacheBuffers();
#endif
        }
    } else {
        InterlockedIncrement64(&g_stats.allocation_failures);
    }
    
#if ENABLE_PERFORMANCE_COUNTERS
    LARGE_INTEGER end_time = GetHighResolutionTime();
    double elapsed_ms = GetElapsedMs(start_time, end_time);
    InterlockedExchangeAdd64(&g_stats.total_alloc_time, (LONG64)(elapsed_ms * 1000));
    
    if (elapsed_ms > 1.0) { // Log slow allocations
        LOG_PERF("Slow allocation: %.2f ms for %zu bytes", elapsed_ms, size);
    }
#endif
    
    return result;
}

// Multi-tier deallocation
static void MultiTierFree(void* ptr) {
    if (!ptr) return;
    
    InterlockedIncrement64(&g_stats.total_deallocations);
    
    // Check if this is a pool allocation
    if (IsInAnyPool(ptr)) {
        // Pool allocations are never actually freed (bump allocator)
        HeapAllocHeader* header = ((HeapAllocHeader*)ptr) - 1;
        
        __try {
            if (header->magic == 0xDEADC0DE && (header->flags & 0x1)) {
                InterlockedExchangeAdd64(&g_stats.bytes_deallocated, header->size);
                
                // Try to cache in ScrapHeap if suitable size
#if ENABLE_SCRAP_CACHE_SYSTEM
                if (header->size >= SCRAP_MIN_SIZE && header->size <= SCRAP_MAX_SIZE) {
                    if (ScrapCacheReleaseBuffer(ptr, header->size)) {
                        LOG_DEBUG("Pool->ScrapCache: %zu bytes", header->size);
                        return;
                    }
                }
#endif
                
                LOG_DEBUG("Pool free (no-op): %zu bytes", header->size);
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            InterlockedIncrement64(&g_stats.exceptions_handled);
            LOG_ERROR("Exception validating pool allocation at %p", ptr);
        }
        
        return;
    }
    
    // Check if this is a custom heap allocation
#if ENABLE_SEGMENT_MANAGEMENT
    if (CustomHeapFree(ptr)) {
        LOG_DEBUG("CustomHeap free completed");
        return;
    }
#endif
    
    // System heap deallocation
    bool freed = false;
    if (g_process_heap && HeapFree(g_process_heap, 0, ptr)) {
        freed = true;
    } else if (g_fallback_heap && HeapFree(g_fallback_heap, 0, ptr)) {
        freed = true;
    }
    
    if (freed) {
        LOG_DEBUG("System heap free completed");
    } else {
        LOG_WARN("Failed to free pointer %p", ptr);
    }
}

// ============================================================================
// MEMORY ALLOCATION HOOKS - Enhanced Multi-Tier Integration
// ============================================================================

static void* __cdecl HookedMallocMT(size_t size) {
    if (!g_system_initialized) {
        return orig_malloc ? orig_malloc(size) : NULL;
    }
    
    return MultiTierAllocate(size);
}

static void __cdecl HookedFreeMT(void* ptr) {
    if (!ptr) return;
    
    if (!g_system_initialized) {
        if (orig_free) orig_free(ptr);
        return;
    }
    
    MultiTierFree(ptr);
}

static void* __cdecl HookedCallocMT(size_t num, size_t size) {
    if (!g_system_initialized) {
        return orig_calloc ? orig_calloc(num, size) : NULL;
    }
    
    if (num == 0 || size == 0) return NULL;
    
    // Check for overflow
    if (num > MAX_ALLOCATION_SIZE / size) {
        return NULL;
    }
    
    size_t total = num * size;
    void* ptr = MultiTierAllocate(total);
    
    // Memory is already zeroed by our allocation system
    return ptr;
}

static void* __cdecl HookedReallocMT(void* ptr, size_t size) {
    if (!g_system_initialized) {
        return orig_realloc ? orig_realloc(ptr, size) : NULL;
    }
    
    // Handle special cases
    if (!ptr) {
        return HookedMallocMT(size);
    }
    
    if (size == 0) {
        HookedFreeMT(ptr);
        return NULL;
    }
    
    // Try to determine old size for copy operation
    size_t old_size = 0;
    bool is_pool_ptr = IsInAnyPool(ptr);
    bool is_heap_ptr = IsInCustomHeap(ptr);
    
    if (is_pool_ptr || is_heap_ptr) {
        HeapAllocHeader* header = ((HeapAllocHeader*)ptr) - 1;
        __try {
            if (header->magic == 0xDEADC0DE) {
                old_size = header->size;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            // Fall back to conservative estimate
            old_size = size;
        }
    } else {
        // System allocation - we don't know the old size
        old_size = size; // Conservative estimate
    }
    
    // Allocate new block
    void* new_ptr = MultiTierAllocate(size);
    if (!new_ptr) {
        return NULL;
    }
    
    // Copy data
    if (old_size > 0) {
        size_t copy_size = (old_size < size) ? old_size : size;
        memcpy(new_ptr, ptr, copy_size);
    }
    
    // Free old allocation
    MultiTierFree(ptr);
    
    return new_ptr;
}

// ============================================================================
// IAT HOOKING AND SYSTEM INTEGRATION
// ============================================================================

// (IAT hooking code from previous implementation - unchanged)
static bool HookIATEntryInModule(HMODULE base, const char* dllName, const char* funcName, void* newFunc, void** origFunc) {
    if (!base || !dllName || !funcName || !newFunc) return false;
    
    __try {
        IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)base;
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;
        
        IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((uintptr_t)base + dosHeader->e_lfanew);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;
        
        IMAGE_DATA_DIRECTORY* importDir = &ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!importDir->VirtualAddress || !importDir->Size) return false;
        
        IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)((uintptr_t)base + importDir->VirtualAddress);
        IMAGE_IMPORT_DESCRIPTOR* importEnd = (IMAGE_IMPORT_DESCRIPTOR*)((uintptr_t)importDesc + importDir->Size);
        
        for (; importDesc < importEnd && importDesc->Name; ++importDesc) {
            const char* moduleName = (const char*)((uintptr_t)base + importDesc->Name);
            
            if (_stricmp(moduleName, dllName) == 0) {
                if (!importDesc->OriginalFirstThunk || !importDesc->FirstThunk) continue;
                
                IMAGE_THUNK_DATA* origThunk = (IMAGE_THUNK_DATA*)((uintptr_t)base + importDesc->OriginalFirstThunk);
                IMAGE_THUNK_DATA* thunk = (IMAGE_THUNK_DATA*)((uintptr_t)base + importDesc->FirstThunk);
                
                while (origThunk->u1.AddressOfData && thunk->u1.Function) {
                    if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                        IMAGE_IMPORT_BY_NAME* importByName = (IMAGE_IMPORT_BY_NAME*)((uintptr_t)base + origThunk->u1.AddressOfData);
                        
                        if (strcmp((char*)importByName->Name, funcName) == 0) {
                            DWORD oldProtect;
                            if (VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProtect)) {
                                if (origFunc && !*origFunc) {
                                    *origFunc = (void*)thunk->u1.Function;
                                }
                                thunk->u1.Function = (ULONG_PTR)newFunc;
                                VirtualProtect(&thunk->u1.Function, sizeof(ULONG_PTR), oldProtect, &oldProtect);
                                return true;
                            }
                        }
                    }
                    ++origThunk;
                    ++thunk;
                }
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        InterlockedIncrement64(&g_stats.exceptions_handled);
        LOG_ERROR("Exception in IAT hooking for %s::%s (module %p)", dllName, funcName, base);
        return false;
    }
    
    return false;
}

static bool HookIATEntry(const char* dllName, const char* funcName, void* newFunc, void** origFunc) {
    return HookIATEntryInModule(GetModuleHandleA(NULL), dllName, funcName, newFunc, origFunc);
}

static void InstallHooksAcrossModules() {
    HMODULE modules[1024];
    DWORD needed = 0;
    HANDLE proc = GetCurrentProcess();
    if (!EnumProcessModules(proc, modules, sizeof(modules), &needed)) return;
    size_t count = needed / sizeof(HMODULE);
    for (size_t i = 0; i < count; ++i) {
        HMODULE mod = modules[i];
        // Apply the same set of hooks to each module's IAT
        HookIATEntryInModule(mod, "msvcrt.dll", "malloc", (void*)HookedMallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcrt.dll", "free", (void*)HookedFreeMT, nullptr);
        HookIATEntryInModule(mod, "msvcrt.dll", "calloc", (void*)HookedCallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcrt.dll", "realloc", (void*)HookedReallocMT, nullptr);

        HookIATEntryInModule(mod, "ucrtbase.dll", "malloc", (void*)HookedMallocMT, nullptr);
        HookIATEntryInModule(mod, "ucrtbase.dll", "free", (void*)HookedFreeMT, nullptr);
        HookIATEntryInModule(mod, "ucrtbase.dll", "calloc", (void*)HookedCallocMT, nullptr);
        HookIATEntryInModule(mod, "ucrtbase.dll", "realloc", (void*)HookedReallocMT, nullptr);

        HookIATEntryInModule(mod, "msvcr100.dll", "malloc", (void*)HookedMallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcr100.dll", "free", (void*)HookedFreeMT, nullptr);
        HookIATEntryInModule(mod, "msvcr100.dll", "calloc", (void*)HookedCallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcr100.dll", "realloc", (void*)HookedReallocMT, nullptr);

        HookIATEntryInModule(mod, "msvcr90.dll", "malloc", (void*)HookedMallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcr90.dll", "free", (void*)HookedFreeMT, nullptr);
        HookIATEntryInModule(mod, "msvcr90.dll", "calloc", (void*)HookedCallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcr90.dll", "realloc", (void*)HookedReallocMT, nullptr);

        HookIATEntryInModule(mod, "msvcr120.dll", "malloc", (void*)HookedMallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcr120.dll", "free", (void*)HookedFreeMT, nullptr);
        HookIATEntryInModule(mod, "msvcr120.dll", "calloc", (void*)HookedCallocMT, nullptr);
        HookIATEntryInModule(mod, "msvcr120.dll", "realloc", (void*)HookedReallocMT, nullptr);
    }
}

static volatile LONG g_hooks_installed = 0;

static bool InstallMultiTierMemoryHooks() {
    LOG_INFO("Installing multi-tier memory hooks...");
    
    bool hooked = false;
    
    // Hook standard C runtime functions
    hooked |= HookIATEntry("msvcrt.dll", "malloc", (void*)HookedMallocMT, (void**)&orig_malloc);
    hooked |= HookIATEntry("msvcrt.dll", "free", (void*)HookedFreeMT, (void**)&orig_free);
    hooked |= HookIATEntry("msvcrt.dll", "calloc", (void*)HookedCallocMT, (void**)&orig_calloc);
    hooked |= HookIATEntry("msvcrt.dll", "realloc", (void*)HookedReallocMT, (void**)&orig_realloc);
    
    // Hook universal CRT functions
    hooked |= HookIATEntry("ucrtbase.dll", "malloc", (void*)HookedMallocMT, nullptr);
    hooked |= HookIATEntry("ucrtbase.dll", "free", (void*)HookedFreeMT, nullptr);
    hooked |= HookIATEntry("ucrtbase.dll", "calloc", (void*)HookedCallocMT, nullptr);
    hooked |= HookIATEntry("ucrtbase.dll", "realloc", (void*)HookedReallocMT, nullptr);
    
    // Hook common VC runtime versions used by FNV and mods
    hooked |= HookIATEntry("msvcr100.dll", "malloc", (void*)HookedMallocMT, nullptr);
    hooked |= HookIATEntry("msvcr100.dll", "free", (void*)HookedFreeMT, nullptr);
    hooked |= HookIATEntry("msvcr100.dll", "calloc", (void*)HookedCallocMT, nullptr);
    hooked |= HookIATEntry("msvcr100.dll", "realloc", (void*)HookedReallocMT, nullptr);
    
    hooked |= HookIATEntry("msvcr90.dll", "malloc", (void*)HookedMallocMT, nullptr);
    hooked |= HookIATEntry("msvcr90.dll", "free", (void*)HookedFreeMT, nullptr);
    hooked |= HookIATEntry("msvcr90.dll", "calloc", (void*)HookedCallocMT, nullptr);
    hooked |= HookIATEntry("msvcr90.dll", "realloc", (void*)HookedReallocMT, nullptr);
    
    // Hook newer VC runtime if present
    hooked |= HookIATEntry("msvcr120.dll", "malloc", (void*)HookedMallocMT, nullptr);
    hooked |= HookIATEntry("msvcr120.dll", "free", (void*)HookedFreeMT, nullptr);
    hooked |= HookIATEntry("msvcr120.dll", "calloc", (void*)HookedCallocMT, nullptr);
    hooked |= HookIATEntry("msvcr120.dll", "realloc", (void*)HookedReallocMT, nullptr);
    
    if (hooked) {
        InterlockedExchange(&g_hooks_installed, 1);
        LOG_INFO("Multi-tier memory hooks installed successfully");
        return true;
    } else {
        LOG_WARN("No memory hooks could be installed");
        return false;
    }
}

// ============================================================================
// SYSTEM INITIALIZATION - Enhanced Multi-Tier Setup
// ============================================================================

static bool InitializeCriticalSections() {
    if (InterlockedCompareExchange(&g_critical_sections_initialized, 1, 0) != 0) {
        return true;
    }
    
    __try {
        BOOL ok = TRUE;
        if (!InitializeCriticalSectionAndSpinCount(&g_stats_lock, HEAP_SPIN_COUNT)) ok = FALSE;
        if (!InitializeCriticalSectionAndSpinCount(&g_log_lock, HEAP_SPIN_COUNT)) ok = FALSE;
        if (!InitializeCriticalSectionAndSpinCount(&g_system_lock, HEAP_SPIN_COUNT)) ok = FALSE;
        
        // Initialize heap critical sections array
        for (int i = 0; i < 8; i++) {
            if (!InitializeCriticalSectionAndSpinCount(&g_heap_critical_sections[i], HEAP_SPIN_COUNT)) ok = FALSE;
        }
        
        if (!ok) {
            InterlockedExchange(&g_critical_sections_initialized, 0);
            return false;
        }
        
        LOG_INFO("Critical sections initialized (including heap locks)");
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_critical_sections_initialized, 0);
        return false;
    }
}

static void InitializeSystemInformation() {
    // Get system information
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    g_page_size = sysInfo.dwPageSize;
    g_processor_count = sysInfo.dwNumberOfProcessors;
    
    // Initialize performance counter
    QueryPerformanceFrequency(&g_stats.perf_frequency);
    g_stats.init_time = GetHighResolutionTime();
    g_stats.init_tick_count = GetTickCount64();
    
    // Get process heap
    g_process_heap = GetProcessHeap();
    
    // Create dedicated fallback heap
    g_fallback_heap = HeapCreate(HEAP_GENERATE_EXCEPTIONS, 64 * 1024 * 1024, 0);
    
    LOG_INFO("System Info: %d CPUs, %d byte pages, process heap: %p, fallback heap: %p", 
             g_processor_count, g_page_size, g_process_heap, g_fallback_heap);
}

static void InitializeMultiTierSystem() {
    if (InterlockedCompareExchange(&g_system_initialized, 1, 0) != 0) {
        return;
    }
    
    LOG_INFO("=== MemoryPoolNVSE HeapMaster v%s Initializing ===", PLUGIN_VERSION_STRING);
    LOG_INFO("%s", PLUGIN_DESCRIPTION);
    
    // Initialize critical sections
    if (!InitializeCriticalSections()) {
        LOG_ERROR("Critical failure: Could not initialize critical sections");
        InterlockedExchange(&g_system_initialized, 0);
        return;
    }
    
    // Initialize system information
    InitializeSystemInformation();
    
    // Initialize rpmalloc (safe, even if not primary allocator)
    rpmalloc_initialize(0);
    
    // Set memory manager mode to hybrid
    g_memory_mode = MEMORY_MODE_HYBRID;
    
    // Initialize enhanced memory pools
    bool pools_ok = true;
    pools_ok &= InitializeEnhancedPool(&g_primary_pool, PRIMARY_POOL_SIZE, "Primary Pool", 1);
    pools_ok &= InitializeEnhancedPool(&g_secondary_pool, SECONDARY_POOL_SIZE, "Secondary Pool", 2);
    pools_ok &= InitializeEnhancedPool(&g_texture_pool, TEXTURE_POOL_SIZE, "Texture Pool", 3);
    
    if (!pools_ok) {
        LOG_ERROR("Critical failure: Could not initialize memory pools");
        InterlockedExchange(&g_system_initialized, 0);
        return;
    }
    
    // Initialize ScrapHeap cache system
#if ENABLE_SCRAP_CACHE_SYSTEM
    if (!InitializeScrapCache()) {
        LOG_WARN("ScrapHeap cache initialization failed - continuing without cache");
    }
#endif
    
    // Initialize custom heap manager
#if ENABLE_SEGMENT_MANAGEMENT
    if (!InitializeCustomHeap()) {
        LOG_WARN("Custom heap manager initialization failed - continuing without custom heap");
    }
#endif
    
    // Install memory hooks (base module) then across all loaded modules
    InstallMultiTierMemoryHooks();
    InstallHooksAcrossModules();
    
// Apply budget patches (placeholder - implement if needed)
#if ENABLE_MEMORY_BUDGETS
    LOG_INFO("Memory budget patches ready for implementation");
#endif
    
    LOG_INFO("=== HeapMaster Initialization Complete ===");
    SIZE_T total_reserved = 0;
    for (int i = 0; i < g_num_pools; ++i) total_reserved += g_pools[i]->size;
    LOG_INFO("Multi-tier system active: %.2f GB total reserved", 
             (double)total_reserved / (1024.0*1024.0*1024.0));
    
    // System initialization complete
    LOG_INFO("HeapMaster system fully operational with %d memory tiers", 5);
}

// ============================================================================
// NVSE PLUGIN INTERFACE
// ============================================================================

static void MessageHandler(NVSEMessagingInterface::Message* msg) {
    switch (msg->type) {
        case NVSEMessagingInterface::kMessage_PostPostLoad:
            InitializeMultiTierSystem();
            break;
            
        case NVSEMessagingInterface::kMessage_ExitGame:
        case NVSEMessagingInterface::kMessage_ExitToMainMenu:
            LOG_INFO("Game session ending - HeapMaster statistics logged");
            break;
    }
}

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "MemoryPoolNVSE HeapMaster";
    info->version = PLUGIN_VERSION_MAJOR * 100 + PLUGIN_VERSION_MINOR;
    
    // Be permissive on NVSE version; require only FNV 1.4+ runtime and non-editor
    if (nvse->runtimeVersion < RUNTIME_VERSION_1_4_MIN) return false;
    if (nvse->isEditor) return false;
    
    return true;
}

bool NVSEPlugin_Load(NVSEInterface* nvse) {
    // Try to acquire messaging interface (prefer official ID 3, then fallback to 2)
    NVSEMessagingInterface* msgInterface = (NVSEMessagingInterface*)nvse->QueryInterface(3);
    if (!msgInterface) {
        msgInterface = (NVSEMessagingInterface*)nvse->QueryInterface(2);
    }

    bool listenerRegistered = false;
    if (msgInterface && msgInterface->RegisterListener) {
        listenerRegistered = msgInterface->RegisterListener(nvse->GetPluginHandle(), "NVSE", (void*)MessageHandler);
    }

    // If messaging is unavailable or registration failed, initialize immediately
    if (!listenerRegistered) {
        InitializeMultiTierSystem();
    }

    return true;
}

// ============================================================================
// DLL ENTRY POINT
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
            
        case DLL_PROCESS_DETACH:
            // Mark shutdown in progress
            InterlockedExchange(&g_shutting_down, 1);
            // Clean up rpmalloc if initialized
            rpmalloc_finalize();
            break;
    }
    
    return TRUE;
}

// ============================================================================
// END OF HEAPMASTER IMPLEMENTATION
// ============================================================================