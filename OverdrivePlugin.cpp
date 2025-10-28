#include <windows.h>
#include <stdint.h>
#include <intrin.h>
#include <psapi.h>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <algorithm>
#include <emmintrin.h>

#define NVSE_NO_EXPORT_DECL
#include "nvse_compat.h"
#include "overdrive_log.h"
#include "rpmalloc.h"
#include "OverdriveConfig.h"
#include "memory_budgets.h"
#include "performance_patcher.h"
#include "virtualfree_hook.h"
#include "HighVAArena.h"

// Enhanced logging system
static CRITICAL_SECTION g_log_cs;
static volatile LONG g_log_initialized = 0;
static HANDLE g_log_file = INVALID_HANDLE_VALUE;
static const char* const LOG_LEVELS[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };

static void LogShutdown() {
    if (g_log_file != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_log_file);
        CloseHandle(g_log_file);
        g_log_file = INVALID_HANDLE_VALUE;
    }
    if (g_log_initialized) {
        DeleteCriticalSection(&g_log_cs);
        g_log_initialized = 0;
    }
}

static void LogInitialize() {
    if (InterlockedCompareExchange(&g_log_initialized, 1, 0) != 0) {
        return; // Already initialized
    }

    InitializeCriticalSectionAndSpinCount(&g_log_cs, 1000);

    // Build paths relative to the game executable directory
    char exeDir[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, exeDir, MAX_PATH)) {
        char* last = strrchr(exeDir, '\\');
        if (last) *last = '\0';
    }

    char dataDir[MAX_PATH];
    char nvseDir[MAX_PATH];
    char pluginsDir[MAX_PATH];
    char logPath[MAX_PATH];

    if (exeDir[0]) {
        _snprintf_s(dataDir, _TRUNCATE, "%s\\Data", exeDir);
    } else {
        strcpy_s(dataDir, "Data");
    }
    _snprintf_s(nvseDir, _TRUNCATE, "%s\\NVSE", dataDir);
    _snprintf_s(pluginsDir, _TRUNCATE, "%s\\Plugins", nvseDir);

    // Ensure directories exist
    CreateDirectoryA(dataDir, NULL);
    CreateDirectoryA(nvseDir, NULL);
    CreateDirectoryA(pluginsDir, NULL);

    // Open log file once
    _snprintf_s(logPath, _TRUNCATE, "%s\\Overdrive.log", pluginsDir);
    g_log_file = CreateFileA(
        logPath,
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL
    );

    if (g_log_file == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("Overdrive: Failed to open log file\n");
        return;
    }

    // Write UTF-8 BOM if new file
    LARGE_INTEGER fileSize = {0};
    if (GetFileSizeEx(g_log_file, &fileSize) && fileSize.QuadPart == 0) {
        const BYTE utf8_bom[] = {0xEF, 0xBB, 0xBF};
        DWORD written = 0;
        WriteFile(g_log_file, utf8_bom, sizeof(utf8_bom), &written, NULL);
    }

    // Register cleanup
    atexit(LogShutdown);
    // Initial line for troubleshooting
    LogWrite(2, __FILE__, __LINE__, "Overdrive logging initialized");
}

void LogWrite(int level, const char* file, int line, const char* fmt, ...) {
    // Lazy initialization
    if (!g_log_initialized) {
        LogInitialize();
    }

    if (level < 0 || level >= (int)(sizeof(LOG_LEVELS)/sizeof(LOG_LEVELS[0]))) {
        level = 2; // Default to INFO for invalid levels
    }

    // Get timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    // Format message
    char message[4096];
    va_list args;
    va_start(args, fmt);
    int msg_len = _vsnprintf_s(message, _TRUNCATE, fmt, args);
    va_end(args);

    if (msg_len < 0) {
        strcpy_s(message, "[Message too long]");
        msg_len = (int)strlen(message);
    }

    // Format full log line
    char buffer[8192];
    const char* filename = strrchr(file, '\\');
    filename = filename ? filename + 1 : file;
    
    int total = _snprintf_s(buffer, _TRUNCATE, 
        "%04d-%02d-%02d %02d:%02d:%02d.%03d [%5s] [%s:%d] %s\r\n",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        LOG_LEVELS[level], filename, line, message);

    if (total <= 0) {
        return; // Failed to format
    }

    // Thread-safe writing
    EnterCriticalSection(&g_log_cs);
    
    // Write to file
    if (g_log_file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_log_file, buffer, (DWORD)total, &written, NULL);
    }
    
    // Also output to debug console
    OutputDebugStringA(buffer);
    
    LeaveCriticalSection(&g_log_cs);
}

// Logging provided by overdrive_log.h macros (LOG_*, LOGI, LOGW)

// Config and telemetry
static OverdriveConfig g_cfg{};
static volatile LONG g_initialized = 0;
static volatile LONG64 g_allocs = 0, g_frees = 0, g_bytes_alloc = 0, g_bytes_free = 0;
static volatile LONG g_frame = 0;

// Baseline/current budgets for dynamic scaling
static MemoryBudgetConfig g_budgetBase{};
static MemoryBudgetConfig g_budgetCur{};

// Frame timing
static LARGE_INTEGER g_qpf{};
static LARGE_INTEGER g_last_tick{};
static double g_ema_ms = 16.0; // EWMA of frame time

// Hooked CRT allocators -> rpmalloc (hybrid for large allocs)
static void* (__cdecl* orig_malloc)(size_t) = nullptr;
static void (__cdecl* orig_free)(void*) = nullptr;
static void* (__cdecl* orig_calloc)(size_t, size_t) = nullptr;
static void* (__cdecl* orig_realloc)(void*, size_t) = nullptr;

// Hooked Win32 heap/VA API
static LPVOID (WINAPI* orig_HeapAlloc)(HANDLE, DWORD, SIZE_T) = nullptr;
static LPVOID (WINAPI* orig_HeapReAlloc)(HANDLE, DWORD, LPVOID, SIZE_T) = nullptr;
static BOOL   (WINAPI* orig_HeapFree)(HANDLE, DWORD, LPVOID) = nullptr;
static LPVOID (WINAPI* orig_VirtualAlloc)(LPVOID, SIZE_T, DWORD, DWORD) = nullptr;
static SIZE_T g_largeThresholdBytes = 8 * 1024 * 1024; // configurable

// Hybrid big allocation header
struct BigHdr { uint32_t magic; uint32_t reserved; SIZE_T size; };
static const uint32_t BIG_MAGIC = 0xB16B00B5u;
static inline bool IsBigPtr(void* p) {
    __try {
        BigHdr* h = (BigHdr*)((uint8_t*)p - sizeof(BigHdr));
        return h && h->magic == BIG_MAGIC;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static void* BigAlloc(SIZE_T sz, bool zero) {
    SIZE_T total = sz + sizeof(BigHdr);
    DWORD at = MEM_RESERVE | MEM_COMMIT | (HighVAAPI::EffectiveLAA() ? MEM_TOP_DOWN : 0);
    void* base = orig_VirtualAlloc ? orig_VirtualAlloc(nullptr, total, at, PAGE_READWRITE) : VirtualAlloc(nullptr, total, at, PAGE_READWRITE);
    if (!base) return nullptr;
    BigHdr* h = (BigHdr*)base; h->magic = BIG_MAGIC; h->reserved = 0; h->size = sz;
    void* user = (void*)((uint8_t*)base + sizeof(BigHdr));
    if (zero && user) memset(user, 0, sz);
    return user;
}
static void BigFree(void* p) {
    BigHdr* h = (BigHdr*)((uint8_t*)p - sizeof(BigHdr));
    VirtualFree(h, 0, MEM_RELEASE);
}
static void* BigRealloc(void* p, SIZE_T sz) {
    if (!p) return BigAlloc(sz, false);
    BigHdr* h = (BigHdr*)((uint8_t*)p - sizeof(BigHdr));
    if (h->magic != BIG_MAGIC) return nullptr;
    if (sz == h->size) return p;
    void* np = BigAlloc(sz, false);
    if (!np) return nullptr;
    SIZE_T copy = (sz < h->size) ? sz : h->size;
    memcpy(np, p, copy);
    BigFree(p);
    return np;
}

// Cross-module mismatch detection (optional)
static CRITICAL_SECTION g_alloc_meta_lock;
struct AllocMeta { HMODULE mod; void* trace[16]; USHORT depth; size_t size; };
#include <unordered_map>
static std::unordered_map<void*, AllocMeta> g_alloc_meta;
static bool g_alloc_meta_inited = false;
static void AllocMetaInit(){ if(!g_alloc_meta_inited){ InitializeCriticalSection(&g_alloc_meta_lock); g_alloc_meta_inited=true; }}
static HMODULE ModuleFromAddr(void* addr){ HMODULE m=nullptr; GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)addr,&m); return m; }
static HMODULE CallerModule() {
    void* stack[32]; USHORT skip=2; USHORT depth = (USHORT)min((uint32_t)32, g_cfg.stackTraceDepth ? g_cfg.stackTraceDepth : 12);
    USHORT n = RtlCaptureStackBackTrace(skip, depth, stack, NULL);
    for (USHORT i=0;i<n;i++){ HMODULE m=ModuleFromAddr(stack[i]); if (m) return m; }
    return ModuleFromAddr(_ReturnAddress());
}
static void LogMismatch(void* p, const AllocMeta* meta, HMODULE freeMod) {
    if (!g_cfg.detectCrossModuleMismatch) return;
    char allocPath[MAX_PATH]={0}, freePath[MAX_PATH]={0};
    GetModuleFileNameA(meta->mod, allocPath, MAX_PATH); GetModuleFileNameA(freeMod, freePath, MAX_PATH);
    LOGW("Cross-module free: ptr=%p alloc_mod=%s free_mod=%s size=%zu", p, allocPath, freePath, meta->size);
}

static void* __cdecl hk_malloc(size_t sz) {
    if (!g_initialized) return orig_malloc ? orig_malloc(sz) : nullptr;
    AllocMetaInit();
    if (g_largeThresholdBytes && sz >= g_largeThresholdBytes) {
        void* bp = BigAlloc(sz, false);
        if (bp) return bp;
    }
    void* p = rpmalloc(sz);
    if (p) {
        if (g_cfg.detectCrossModuleMismatch) {
            AllocMeta m{}; m.mod = CallerModule(); m.depth = 0; m.size = sz;
            EnterCriticalSection(&g_alloc_meta_lock); g_alloc_meta[p] = m; LeaveCriticalSection(&g_alloc_meta_lock);
        }
        InterlockedIncrement64(&g_allocs); InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)sz);
    }
    return p;
}
static void __cdecl hk_free(void* p) {
    if (!p) return;
    if (!g_initialized) { if (orig_free) orig_free(p); return; }
    // Big block free
    if (IsBigPtr(p)) {
        BigFree(p);
        InterlockedIncrement64(&g_frees);
        return;
    }
    size_t s = 0; __try { s = rpmalloc_usable_size(p); } __except(EXCEPTION_EXECUTE_HANDLER) { s = 0; }
    if (!s) {
        // Not an rpmalloc pointer; fall back to original to avoid crashes
        if (g_cfg.detectCrossModuleMismatch) {
            EnterCriticalSection(&g_alloc_meta_lock);
            auto it = g_alloc_meta.find(p);
            HMODULE fm = CallerModule();
            if (it != g_alloc_meta.end() && it->second.mod != fm) LogMismatch(p, &it->second, fm);
            if (it != g_alloc_meta.end()) g_alloc_meta.erase(it);
            LeaveCriticalSection(&g_alloc_meta_lock);
        }
        if (orig_free) { orig_free(p); return; }
        return;
    }
    if (g_cfg.detectCrossModuleMismatch) {
        EnterCriticalSection(&g_alloc_meta_lock);
        auto it = g_alloc_meta.find(p);
        HMODULE fm = CallerModule();
        if (it != g_alloc_meta.end() && it->second.mod != fm) LogMismatch(p, &it->second, fm);
        if (it != g_alloc_meta.end()) g_alloc_meta.erase(it);
        LeaveCriticalSection(&g_alloc_meta_lock);
    }
    rpfree(p);
    InterlockedIncrement64(&g_frees);
    if (s) InterlockedExchangeAdd64(&g_bytes_free, (LONG64)s);
}
static void* __cdecl hk_calloc(size_t n, size_t sz) {
    if (!g_initialized) return orig_calloc ? orig_calloc(n, sz) : nullptr;
    if (!n || !sz || n > SIZE_MAX / sz) return nullptr;
    AllocMetaInit();
    SIZE_T req = n * sz;
    if (g_largeThresholdBytes && req >= g_largeThresholdBytes) {
        void* bp = BigAlloc(req, true);
        if (bp) return bp;
    }
    void* p = rpcalloc(n, sz);
    if (p) {
        if (g_cfg.detectCrossModuleMismatch) {
            AllocMeta m{}; m.mod = CallerModule(); m.depth = 0; m.size = n*sz;
            EnterCriticalSection(&g_alloc_meta_lock); g_alloc_meta[p] = m; LeaveCriticalSection(&g_alloc_meta_lock);
        }
        InterlockedIncrement64(&g_allocs); InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)(n*sz));
    }
    return p;
}
static void* __cdecl hk_realloc(void* p, size_t sz) {
    if (!g_initialized) return orig_realloc ? orig_realloc(p, sz) : nullptr;
    if (!p) return hk_malloc(sz);
    if (!sz) { hk_free(p); return nullptr; }

    // Big block path
    if (IsBigPtr(p)) {
        if (g_largeThresholdBytes && sz >= g_largeThresholdBytes) {
            void* np_big = BigRealloc(p, sz);
            if (np_big) return np_big;
        } else {
            // Move big->small into rpmalloc block
            void* np_small = rpmalloc(sz);
            if (np_small) {
                BigHdr* h = (BigHdr*)((uint8_t*)p - sizeof(BigHdr));
                SIZE_T copy = (sz < h->size) ? sz : h->size; memcpy(np_small, p, copy);
                BigFree(p);
                InterlockedIncrement64(&g_allocs);
                InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)sz);
                return np_small;
            }
        }
        return nullptr;
    }

    size_t old = 0; __try { old = rpmalloc_usable_size(p); } __except(EXCEPTION_EXECUTE_HANDLER) { old = 0; }
    if (g_largeThresholdBytes && sz >= g_largeThresholdBytes) {
        // small->big: allocate big, copy, free small
        void* np_big = BigAlloc(sz, false);
        if (np_big) {
            SIZE_T copy = (sz < old) ? sz : old; if (copy) memcpy(np_big, p, copy);
            rpfree(p);
            InterlockedIncrement64(&g_frees); if (old) InterlockedExchangeAdd64(&g_bytes_free, (LONG64)old);
            InterlockedIncrement64(&g_allocs); InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)sz);
            return np_big;
        }
        // fallthrough to rprealloc if BigAlloc failed
    }
    void* np = rprealloc(p, sz);
    if (np) {
        if (g_cfg.detectCrossModuleMismatch) {
            AllocMeta m{}; m.mod = CallerModule(); m.depth = 0; m.size = sz;
            EnterCriticalSection(&g_alloc_meta_lock); g_alloc_meta[np] = m; g_alloc_meta.erase(p); LeaveCriticalSection(&g_alloc_meta_lock);
        }
        InterlockedIncrement64(&g_frees);
        if (old) InterlockedExchangeAdd64(&g_bytes_free, (LONG64)old);
        InterlockedIncrement64(&g_allocs);
        InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)sz);
    }
    return np;
}

// Win32 Heap hooks (route small/medium to rpmalloc)
static LPVOID WINAPI hk_HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    if (!g_initialized || !g_cfg.hookHeapAPI) return orig_HeapAlloc ? orig_HeapAlloc(hHeap, dwFlags, dwBytes) : nullptr;
    SIZE_T thr = (SIZE_T)g_cfg.heapHookThresholdKB * 1024ULL;
    if (dwBytes && dwBytes <= thr) {
        void* p = rpmalloc(dwBytes);
        if (p && (dwFlags & HEAP_ZERO_MEMORY)) memset(p, 0, dwBytes);
        if (p) { InterlockedIncrement64(&g_allocs); InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)dwBytes); }
        return p;
    }
    return orig_HeapAlloc ? orig_HeapAlloc(hHeap, dwFlags, dwBytes) : nullptr;
}
static BOOL WINAPI hk_HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);
static LPVOID WINAPI hk_HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes) {
    if (!g_initialized || !g_cfg.hookHeapAPI) return orig_HeapReAlloc ? orig_HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes) : nullptr;
    SIZE_T thr = (SIZE_T)g_cfg.heapHookThresholdKB * 1024ULL;
    if (!lpMem) return hk_HeapAlloc(hHeap, dwFlags, dwBytes);
    if (dwBytes == 0) { hk_HeapFree(hHeap, 0, lpMem); return nullptr; }
    if (dwBytes <= thr) {
        size_t old = 0; __try { old = rpmalloc_usable_size(lpMem); } __except(EXCEPTION_EXECUTE_HANDLER) { old = 0; }
        void* np = rprealloc(lpMem, dwBytes);
        if (np) {
            if (dwFlags & HEAP_ZERO_MEMORY) { if (dwBytes > old) memset((char*)np + old, 0, dwBytes - old); }
            if (old) InterlockedExchangeAdd64(&g_bytes_free, (LONG64)old);
            InterlockedIncrement64(&g_frees);
            InterlockedIncrement64(&g_allocs);
            InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)dwBytes);
        }
        return np;
    }
    return orig_HeapReAlloc ? orig_HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes) : nullptr;
}
static BOOL WINAPI hk_HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    if (!lpMem) return TRUE;
    if (!g_initialized || !g_cfg.hookHeapAPI) return orig_HeapFree ? orig_HeapFree(hHeap, dwFlags, lpMem) : FALSE;
    size_t sz = 0; __try { sz = rpmalloc_usable_size(lpMem); } __except(EXCEPTION_EXECUTE_HANDLER) { sz = 0; }
    if (sz) {
        rpfree(lpMem);
        InterlockedIncrement64(&g_frees);
        InterlockedExchangeAdd64(&g_bytes_free, (LONG64)sz);
        return TRUE;
    }
    return orig_HeapFree ? orig_HeapFree(hHeap, dwFlags, lpMem) : FALSE;
}

// VirtualAlloc hook with arena steering and top-down fallback
static LPVOID WINAPI hk_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    if (!g_initialized || !g_cfg.hookVirtualAlloc) return orig_VirtualAlloc ? orig_VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect) : nullptr;

    // Try arena path first for sizeable allocations
    if (HighVAAPI::IsActive() && dwSize >= 64 * 1024) {
        bool wantReserve = (flAllocationType & MEM_RESERVE) != 0;
        bool wantCommit  = (flAllocationType & MEM_COMMIT)  != 0;
        if (lpAddress == nullptr) {
            if (wantReserve && wantCommit) {
                void* p = HighVAAPI::Alloc(dwSize, flProtect);
                if (p) return p;
            } else if (wantReserve && !wantCommit) {
                void* p = HighVAAPI::Reserve(dwSize);
                if (p) return p;
            } else if (!wantReserve && wantCommit) {
                void* p = HighVAAPI::Alloc(dwSize, flProtect);
                if (p) return p;
            }
        } else if (HighVAAPI::Contains(lpAddress) && wantCommit) {
            if (HighVAAPI::Commit(lpAddress, dwSize, flProtect)) return lpAddress;
        }
        // Fall through to system if arena didn't satisfy
    }

    // Non-arena path steering
    DWORD at = flAllocationType;
    if ((g_cfg.preferTopDownVA || HighVAAPI::TopdownOnNonArena()) && HighVAAPI::EffectiveLAA()) {
        at |= MEM_TOP_DOWN;
    }
    return orig_VirtualAlloc ? orig_VirtualAlloc(lpAddress, dwSize, at, flProtect) : nullptr;
}

// IAT hooking helpers
// Forward decl for extended hook function
static bool HookIATEntryInModuleEx(HMODULE base, const char* dllName, const char* funcName, void* newFunc, void** origFunc, bool chainExisting);
static bool HookIATEntryInModule(HMODULE base, const char* dllName, const char* funcName, void* newFunc, void** origFunc) {
    __try {
        if (!base) return false;
        auto* dos = (IMAGE_DOS_HEADER*)base; if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto* nt = (IMAGE_NT_HEADERS*)((uintptr_t)base + dos->e_lfanew); if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        auto* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]; if (!dir->VirtualAddress || !dir->Size) return false;
        auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)((uintptr_t)base + dir->VirtualAddress);
        for (; imp->Name; ++imp) {
            const char* mod = (const char*)((uintptr_t)base + imp->Name);
            if (_stricmp(mod, dllName) != 0) continue;
            auto* oft = (IMAGE_THUNK_DATA*)((uintptr_t)base + imp->OriginalFirstThunk);
            auto* ft  = (IMAGE_THUNK_DATA*)((uintptr_t)base + imp->FirstThunk);
            while (oft->u1.AddressOfData && ft->u1.Function) {
                if (!(oft->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    auto* ibn = (IMAGE_IMPORT_BY_NAME*)((uintptr_t)base + oft->u1.AddressOfData);
                    if (strcmp((char*)ibn->Name, funcName) == 0) {
                        DWORD oldProt;
                        if (VirtualProtect(&ft->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProt)) {
                            if (origFunc && !*origFunc) *origFunc = (void*)ft->u1.Function;
                            ft->u1.Function = (ULONG_PTR)newFunc;
                            VirtualProtect(&ft->u1.Function, sizeof(ULONG_PTR), oldProt, &oldProt);
                            return true;
                        }
                    }
                }
                ++oft; ++ft;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return false;
}
static bool HookIATEntry(const char* dllName, const char* funcName, void* newFunc, void** origFunc) {
    return HookIATEntryInModuleEx(GetModuleHandleA(NULL), dllName, funcName, newFunc, origFunc, g_cfg.hookChainExisting);
}
// Whitelist parsing
static std::vector<std::string> g_hookWhitelist;
static std::string ToLower(const std::string& s){ std::string r=s; std::transform(r.begin(), r.end(), r.begin(), ::tolower); return r; }
static std::string Basename(const std::string& p){ size_t pos=p.find_last_of("\\/"); return (pos==std::string::npos)?p:p.substr(pos+1); }
static void BuildHookWhitelist() {
    g_hookWhitelist.clear();
    if (g_cfg.hookWhitelist[0]) {
        std::string csv = g_cfg.hookWhitelist; csv += ",";
        size_t start=0; for (size_t i=0;i<csv.size();++i) if (csv[i]==',') { if (i>start){ std::string t=csv.substr(start,i-start); 
            // trim spaces
            size_t a=t.find_first_not_of(" \t"); size_t b=t.find_last_not_of(" \t"); if (a!=std::string::npos) t=t.substr(a,b-a+1); else t.clear();
            if(!t.empty()) g_hookWhitelist.push_back(ToLower(t)); } start=i+1; }
    }
    // Always include main EXE
    char path[MAX_PATH]={0}; GetModuleFileNameA(NULL, path, MAX_PATH);
    g_hookWhitelist.push_back(ToLower(Basename(path)));
}
static bool IsWhitelisted(HMODULE m) {
    char path[MAX_PATH]={0}; GetModuleFileNameExA(GetCurrentProcess(), m, path, MAX_PATH);
    std::string base = ToLower(Basename(path));
    for (auto& w : g_hookWhitelist) if (base == w) return true;
    return false;
}

static bool HookIATEntryInModuleEx(HMODULE base, const char* dllName, const char* funcName, void* newFunc, void** origFunc, bool chainExisting) {
    __try {
        if (!base) return false;
        auto* dos = (IMAGE_DOS_HEADER*)base; if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto* nt = (IMAGE_NT_HEADERS*)((uintptr_t)base + dos->e_lfanew); if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        auto* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]; if (!dir->VirtualAddress || !dir->Size) return false;
        auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)((uintptr_t)base + dir->VirtualAddress);
        HMODULE hDll = GetModuleHandleA(dllName);
        FARPROC gp = hDll ? GetProcAddress(hDll, funcName) : nullptr;
        for (; imp->Name; ++imp) {
            const char* mod = (const char*)((uintptr_t)base + imp->Name);
            if (_stricmp(mod, dllName) != 0) continue;
            auto* oft = (IMAGE_THUNK_DATA*)((uintptr_t)base + imp->OriginalFirstThunk);
            auto* ft  = (IMAGE_THUNK_DATA*)((uintptr_t)base + imp->FirstThunk);
            while (oft->u1.AddressOfData && ft->u1.Function) {
                if (!(oft->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                    auto* ibn = (IMAGE_IMPORT_BY_NAME*)((uintptr_t)base + oft->u1.AddressOfData);
                    if (strcmp((char*)ibn->Name, funcName) == 0) {
                        void* cur = (void*)ft->u1.Function;
                        if (cur == newFunc) return true; // already ours
                        // Detect existing hook: cur != export proc
                        bool alreadyHooked = (gp && cur != (void*)gp);
                        if (alreadyHooked && !chainExisting) {
                            LOGW("Skip hooking %s!%s in module (already hooked)", dllName, funcName);
                            return false;
                        }
                        DWORD oldProt;
                        if (VirtualProtect(&ft->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &oldProt)) {
                            if (origFunc && !*origFunc) *origFunc = cur; // chain through whatever is there
                            ft->u1.Function = (ULONG_PTR)newFunc;
                            VirtualProtect(&ft->u1.Function, sizeof(ULONG_PTR), oldProt, &oldProt);
                            return true;
                        }
                    }
                }
                ++oft; ++ft;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return false;
}

static void InstallHooksAcrossModules() {
    BuildHookWhitelist();
    HMODULE mods[1024]; DWORD needed=0; HANDLE proc=GetCurrentProcess();
    if (!EnumProcessModules(proc, mods, sizeof(mods), &needed)) return;
    size_t cnt = needed/sizeof(HMODULE);
    for (size_t i=0;i<cnt;i++) {
        HMODULE m=mods[i]; if (!IsWhitelisted(m)) continue;
        HookIATEntryInModuleEx(m, "msvcrt.dll",   "malloc",  (void*)hk_malloc,  nullptr, g_cfg.hookChainExisting);
        HookIATEntryInModuleEx(m, "msvcrt.dll",   "free",    (void*)hk_free,    nullptr, g_cfg.hookChainExisting);
        HookIATEntryInModuleEx(m, "msvcrt.dll",   "calloc",  (void*)hk_calloc,  nullptr, g_cfg.hookChainExisting);
        HookIATEntryInModuleEx(m, "msvcrt.dll",   "realloc", (void*)hk_realloc, nullptr, g_cfg.hookChainExisting);
        HookIATEntryInModuleEx(m, "ucrtbase.dll", "malloc",  (void*)hk_malloc,  nullptr, g_cfg.hookChainExisting);
        HookIATEntryInModuleEx(m, "ucrtbase.dll", "free",    (void*)hk_free,    nullptr, g_cfg.hookChainExisting);
        HookIATEntryInModuleEx(m, "ucrtbase.dll", "calloc",  (void*)hk_calloc,  nullptr, g_cfg.hookChainExisting);
        HookIATEntryInModuleEx(m, "ucrtbase.dll", "realloc", (void*)hk_realloc, nullptr, g_cfg.hookChainExisting);
        if (g_cfg.hookHeapAPI) {
            HookIATEntryInModuleEx(m, "kernel32.dll", "HeapAlloc",   (void*)hk_HeapAlloc,   nullptr, g_cfg.hookChainExisting);
            HookIATEntryInModuleEx(m, "kernel32.dll", "HeapReAlloc", (void*)hk_HeapReAlloc, nullptr, g_cfg.hookChainExisting);
            HookIATEntryInModuleEx(m, "kernel32.dll", "HeapFree",    (void*)hk_HeapFree,    nullptr, g_cfg.hookChainExisting);
        }
        if (g_cfg.hookVirtualAlloc) {
            HookIATEntryInModuleEx(m, "kernel32.dll", "VirtualAlloc", (void*)hk_VirtualAlloc, nullptr, g_cfg.hookChainExisting);
        }
    }
}
static bool InstallAllocatorHooks() {
    bool ok=false;
    ok |= HookIATEntry("msvcrt.dll",   "malloc",  (void*)hk_malloc,  (void**)&orig_malloc);
    ok |= HookIATEntry("msvcrt.dll",   "free",    (void*)hk_free,    (void**)&orig_free);
    ok |= HookIATEntry("msvcrt.dll",   "calloc",  (void*)hk_calloc,  (void**)&orig_calloc);
    ok |= HookIATEntry("msvcrt.dll",   "realloc", (void*)hk_realloc, (void**)&orig_realloc);
    ok |= HookIATEntry("ucrtbase.dll", "malloc",  (void*)hk_malloc,  nullptr);
    ok |= HookIATEntry("ucrtbase.dll", "free",    (void*)hk_free,    nullptr);
    ok |= HookIATEntry("ucrtbase.dll", "calloc",  (void*)hk_calloc,  nullptr);
    ok |= HookIATEntry("ucrtbase.dll", "realloc", (void*)hk_realloc, nullptr);
    if (g_cfg.hookHeapAPI) {
        ok |= HookIATEntry("kernel32.dll", "HeapAlloc",   (void*)hk_HeapAlloc,   (void**)&orig_HeapAlloc);
        ok |= HookIATEntry("kernel32.dll", "HeapReAlloc", (void*)hk_HeapReAlloc, (void**)&orig_HeapReAlloc);
        ok |= HookIATEntry("kernel32.dll", "HeapFree",    (void*)hk_HeapFree,    (void**)&orig_HeapFree);
    }
    if (g_cfg.hookVirtualAlloc) {
        ok |= HookIATEntry("kernel32.dll", "VirtualAlloc", (void*)hk_VirtualAlloc, (void**)&orig_VirtualAlloc);
    }
    return ok;
}

// Config application
static void ApplyLoadedConfig() {
    // Budgets
    if (g_cfg.budgetPreset >= 0 && g_cfg.budgetPreset <= 4) {
        MemoryBudgetConfig b = GetPresetConfig((BudgetPreset)g_cfg.budgetPreset);
        auto MB = [](uint32_t mb){ return (DWORD)((uint64_t)mb * 1024ULL * 1024ULL); };
        if (g_cfg.exteriorTextureMB) b.exterior_texture = MB(g_cfg.exteriorTextureMB);
        if (g_cfg.interiorGeometryMB) b.interior_geometry = MB(g_cfg.interiorGeometryMB);
        if (g_cfg.interiorTextureMB) b.interior_texture = MB(g_cfg.interiorTextureMB);
        if (g_cfg.interiorWaterMB) b.interior_water = MB(g_cfg.interiorWaterMB);
        if (g_cfg.actorMemoryMB) b.actor_memory = MB(g_cfg.actorMemoryMB);
        ApplyBudgetConfig(&b);
        g_budgetBase = b;
        g_budgetCur = b;
    }
    // Performance
    PerformanceConfig pc{};
    pc.max_ms_per_frame = g_cfg.maxMsPerFrame;
    pc.max_texture_memory_mb = g_cfg.maxTextureMB;
    pc.max_geometry_memory_mb = g_cfg.maxGeometryMB;
    pc.max_particle_systems = g_cfg.maxParticleSystems;
    pc.relax_frame_limits = g_cfg.relaxFrameLimits;
    pc.disable_aggressive_culling = g_cfg.disableAggressiveCulling;
    ApplyPerformancePatches(&pc);
    if (pc.disable_aggressive_culling) DisableAggressiveCulling();
    // VirtualFree hook
    ShutdownVirtualFreeHook();
    VirtualFreeHookConfig vfc{};
    vfc.delay_decommit = g_cfg.vfDelayDecommit;
    vfc.prevent_release = g_cfg.vfPreventRelease;
    vfc.delay_ms = g_cfg.vfDelayMs;
    vfc.min_keep_size = (size_t)g_cfg.vfMinKeepKB * 1024ULL;
    vfc.log_operations = g_cfg.vfLog;
    vfc.max_kept_committed_bytes = (size_t)g_cfg.vfMaxKeptCommittedMB * 1024ull * 1024ull;
    vfc.low_va_trigger_mb = g_cfg.vfLowVATriggerMB;
    InitVirtualFreeHook(&vfc);
}

// Dynamic budget scaling
static void AdjustBudgetsDynamically(double ema_ms) {
    if (!g_cfg.dynamicBudgets) return;
    if (g_cfg.targetMsPerFrame <= 0.0f) return;
    // Determine scaling direction
    double over = ema_ms - g_cfg.targetMsPerFrame;
    double factor = 0.0;
    if (over > 0.5) {
        factor = -g_cfg.scaleDownAggressiveness; // cut
    } else if (over < -1.0) {
        factor = g_cfg.scaleUpRate; // recover slowly
    } else {
        return; // within band
    }
    auto clampMB = [](uint32_t v, uint32_t lo, uint32_t hi){ if (v<lo) return lo; if (v>hi) return hi; return v; };
    auto apply = [&](uint32_t curMB, uint32_t minMB, uint32_t maxMB)->uint32_t {
        double nv = (double)curMB * (1.0 + factor);
        if (nv < (double)minMB) nv = (double)minMB;
        if (nv > (double)maxMB) nv = (double)maxMB;
        return (uint32_t)nv;
    };
    // Current MB snapshot
    uint32_t extTex = g_budgetCur.exterior_texture / (1024*1024);
    uint32_t intGeo = g_budgetCur.interior_geometry / (1024*1024);
    uint32_t intTex = g_budgetCur.interior_texture / (1024*1024);
    uint32_t intWat = g_budgetCur.interior_water / (1024*1024);
    uint32_t actor  = g_budgetCur.actor_memory / (1024*1024);
    // Apply scaling
    extTex = apply(extTex, g_cfg.minExteriorTextureMB, g_cfg.maxExteriorTextureMB);
    intGeo = apply(intGeo, g_cfg.minInteriorGeometryMB, g_cfg.maxInteriorGeometryMB);
    intTex = apply(intTex, g_cfg.minInteriorTextureMB, g_cfg.maxInteriorTextureMB);
    intWat = apply(intWat, g_cfg.minInteriorWaterMB, g_cfg.maxInteriorWaterMB);
    actor  = apply(actor,  g_cfg.minActorMemoryMB,     g_cfg.maxActorMemoryMB);
    auto MB = [](uint32_t mb){ return (DWORD)((uint64_t)mb * 1024ULL * 1024ULL); };
    MemoryBudgetConfig nb{};
    nb.exterior_texture = MB(extTex);
    nb.interior_geometry= MB(intGeo);
    nb.interior_texture = MB(intTex);
    nb.interior_water   = MB(intWat);
    nb.actor_memory     = MB(actor);
    if (memcmp(&nb, &g_budgetCur, sizeof(nb)) != 0) {
        ApplyBudgetConfig(&nb);
        g_budgetCur = nb;
        LOGI("DynamicBudgets: ms=%.2f extTex=%u intTex=%u intGeo=%u intWat=%u actor=%u",
             ema_ms, extTex, intTex, intGeo, intWat, actor);
    }
}

// Telemetry
static void WriteTelemetryIfDue() {
    if (!g_cfg.telemetryEnabled) return;
    LONG f = InterlockedIncrement(&g_frame);
    uint32_t period = g_cfg.telemetryPeriodFrames ? g_cfg.telemetryPeriodFrames : 300;
    if ((uint32_t)f % period != 0) return;
    VirtualFreeStats vfs{}; GetVirtualFreeStats(&vfs);
    HANDLE h = CreateFileA(g_cfg.telemetryFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written=0; SetFilePointer(h, 0, NULL, FILE_END);
        if (GetFileSize(h, NULL) == 0) {
            const char* header = "allocs,frees,bytes_alloc,bytes_free,vfree_calls,decommit_blocked,decommit_delayed,bytes_kept\r\n";
            WriteFile(h, header, (DWORD)strlen(header), &written, NULL);
        }
        char line[512];
        _snprintf_s(line, _TRUNCATE, "%lld,%lld,%lld,%lld,%ld,%ld,%ld,%zu\r\n",
                    (long long)g_allocs, (long long)g_frees, (long long)g_bytes_alloc, (long long)g_bytes_free,
                    vfs.total_calls, vfs.decommit_blocked, vfs.decommit_delayed, vfs.bytes_kept_committed);
        WriteFile(h, line, (DWORD)strlen(line), &written, NULL);
        CloseHandle(h);
    }
}

// NVSE messaging
static void MessageHandler(NVSEMessagingInterface::Message* msg) {
    switch (msg->type) {
        case NVSEMessagingInterface::kMessage_PostPostLoad:
        case NVSEMessagingInterface::kMessage_PostQueryPlugins: {
            if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0) return;
            LOGI("Overdrive init start");
            LoadOverdriveConfig(g_cfg);
            if (g_cfg.useVanillaHeaps) { LOGI("Safe mode: vanilla heaps"); return; }
            // Initialize timers
            QueryPerformanceFrequency(&g_qpf);
            QueryPerformanceCounter(&g_last_tick);
            g_ema_ms = g_cfg.targetMsPerFrame;
            // rpmalloc
            // rpmalloc (tuned); disable decommit handled by compile flags; keep default page size
            rpmalloc_config_t rcfg{}; memset(&rcfg, 0, sizeof(rcfg));
            rcfg.enable_huge_pages = 0; rcfg.disable_decommit = 1; rcfg.unmap_on_finalize = 0; rcfg.page_name = "Overdrive";
            rpmalloc_initialize_config(nullptr, &rcfg);
            g_largeThresholdBytes = (SIZE_T)g_cfg.largeAllocThresholdMB * 1024ull * 1024ull;

            // High VA arena init
            HighVAOptions hv{};
            hv.enable_arena = g_cfg.enableArena;
            hv.arena_size_bytes = (size_t)g_cfg.arenaMB * 1024ull * 1024ull;
            hv.topdown_on_nonarena = g_cfg.topDownOnNonArena;
            HighVAAPI::Init(hv);
            bool hdr=false, eff=false; HighVAAPI::GetLAA(hdr, eff);
            LOGI("LAA: header=%d effective=%d", hdr?1:0, eff?1:0);
            if (HighVAAPI::IsActive()) {
                uintptr_t base; SIZE_T size; HighVAAPI::GetArenaInfo(base, size);
                LOGI("Arena active: base=0x%08X size=%u MB", (unsigned)base, (unsigned)(size/(1024*1024)));
            } else {
                LOGW("Arena not active (reserve failed or disabled)");
            }

            InstallAllocatorHooks();
            InstallHooksAcrossModules();
            ApplyLoadedConfig();
            LOGI("Overdrive init complete");
        } break;
        case NVSEMessagingInterface::kMessage_MainGameLoop: {
            // Frame timing
            LARGE_INTEGER now; QueryPerformanceCounter(&now);
            double dt_ms = (double)(now.QuadPart - g_last_tick.QuadPart) * 1000.0 / (double)g_qpf.QuadPart;
            g_last_tick = now;
            // EWMA update
            g_ema_ms = 0.90 * g_ema_ms + 0.10 * dt_ms;
            // Periodic adjust
            uint32_t period = g_cfg.adjustPeriodFrames ? g_cfg.adjustPeriodFrames : 60;
            LONG f = InterlockedIncrement(&g_frame);
            if ((uint32_t)f % period == 0) AdjustBudgetsDynamically(g_ema_ms);
            // Housekeeping
            WriteTelemetryIfDue();
            // Backpressure: if kept committed exceeds quota, flush
            {
                VirtualFreeStats vfs{}; GetVirtualFreeStats(&vfs);
                size_t quota = (size_t)g_cfg.vfMaxKeptCommittedMB * 1024ull * 1024ull;
                if (quota && vfs.kept_committed_current > quota) FlushDelayedFrees();
            }
            FlushDelayedFrees();
        } break;
        case NVSEMessagingInterface::kMessage_ExitGame:
        case NVSEMessagingInterface::kMessage_ExitToMainMenu:
            LOGI("Overdrive session end");
            break;
    }
}

// NVSE commands
static bool Cmd_ReloadOverdrive_Execute(COMMAND_ARGS) { LoadOverdriveConfig(g_cfg); ApplyLoadedConfig(); if (result) *result=1.0; return true; }
static bool Cmd_GetBudgets_Execute(COMMAND_ARGS) {
    MemoryBudgetConfig cur{}; GetCurrentBudgets(&cur);
    LOGI("Budgets: extTex=%uMB intTex=%uMB intGeo=%uMB intWater=%uMB actor=%uMB",
         (unsigned)(cur.exterior_texture/ (1024*1024)), (unsigned)(cur.interior_texture/ (1024*1024)),
         (unsigned)(cur.interior_geometry/ (1024*1024)), (unsigned)(cur.interior_water/ (1024*1024)), (unsigned)(cur.actor_memory/ (1024*1024)));
    if (result) *result = (double)(cur.exterior_texture / (1024*1024));
    return true;
}
static bool Cmd_DumpHeaps_Execute(COMMAND_ARGS) {
    VirtualFreeStats vfs{}; GetVirtualFreeStats(&vfs);
    LOGI("Heaps: allocs=%lld frees=%lld bytes_alloc=%lld bytes_free=%lld vfree_calls=%ld kept=%zu",
         (long long)g_allocs,(long long)g_frees,(long long)g_bytes_alloc,(long long)g_bytes_free,vfs.total_calls,vfs.bytes_kept_committed);
    if (result) *result=1.0; return true;
}

// NVSE exports
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
    // Ensure logging is available as early as possible
    LogInitialize();

    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "RPNVSE Overdrive";
    info->version = 100; // 1.00

    LOGI("NVSEPlugin_Query: nvseVersion=%u runtime=%08X isEditor=%u", nvse ? nvse->nvseVersion : 0, nvse ? nvse->runtimeVersion : 0, nvse ? nvse->isEditor : 0);
    if (!nvse) return false;
    if (nvse->runtimeVersion < RUNTIME_VERSION_1_4_MIN) { LOGW("Unsupported runtime version: %08X", nvse->runtimeVersion); return false; }
    if (nvse->isEditor) { LOGW("Editor detected - disabling"); return false; }
    return true;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(NVSEInterface* nvse) {
    LOGI("NVSEPlugin_Load: nvseVersion=%u", nvse ? nvse->nvseVersion : 0);
    // Register commands
    if (nvse && nvse->RegisterCommand) {
        static CommandInfo kReload = {"OverdriveReload","odreload",0,"Reload Overdrive INI",0,0,nullptr,Cmd_ReloadOverdrive_Execute};
        static CommandInfo kBudgets= {"OverdriveGetBudgets","odbudgets",0,"Log current budgets",0,0,nullptr,Cmd_GetBudgets_Execute};
        static CommandInfo kHeaps  = {"OverdriveDumpHeaps","odheaps",0,"Log heap counters",0,0,nullptr,Cmd_DumpHeaps_Execute};
        nvse->RegisterCommand(&kReload);
        nvse->RegisterCommand(&kBudgets);
        nvse->RegisterCommand(&kHeaps);
    }
    // Messaging
    NVSEMessagingInterface* msg = nvse ? (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging) : nullptr;
    if (msg && msg->RegisterListener && nvse && nvse->GetPluginHandle) {
        PluginHandle ph = nvse->GetPluginHandle();
        #ifdef RPNVSE_OVERDRIVE_NVSE_OFFICIAL
        msg->RegisterListener(ph, "NVSE", (NVSEMessagingInterface::EventCallback)MessageHandler);
        msg->RegisterListener(ph, "xNVSE", (NVSEMessagingInterface::EventCallback)MessageHandler);
        #else
        msg->RegisterListener(ph, "NVSE", (void*)MessageHandler);
        msg->RegisterListener(ph, "xNVSE", (void*)MessageHandler);
        #endif
    } else {
        // Fallback: initialize immediately
        NVSEMessagingInterface::Message fake{ "NVSE", NVSEMessagingInterface::kMessage_PostPostLoad, 0, nullptr };
        MessageHandler(&fake);
    }
    return true;
}

// DllMain
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    switch (reason) {
case DLL_PROCESS_ATTACH: DisableThreadLibraryCalls(hinst); LogInitialize(); break;
        case DLL_PROCESS_DETACH:
            FlushDelayedFrees();
            ShutdownVirtualFreeHook();
            if (g_initialized) rpmalloc_finalize();
            break;
    }
    return TRUE;
}
