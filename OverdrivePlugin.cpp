#include <windows.h>
#include <stdint.h>
#include <intrin.h>
#include <psapi.h>
#include <cstdio>
#include <cstdarg>

#define NVSE_NO_EXPORT_DECL
#include "nvse_minimal.h"
#include "rpmalloc.h"
#include "OverdriveConfig.h"
#include "memory_budgets.h"
#include "performance_patcher.h"
#include "virtualfree_hook.h"

// Simple logging
static CRITICAL_SECTION g_log_lock;
static volatile LONG g_log_init = 0;
static void LogInit() {
    if (InterlockedCompareExchange(&g_log_init, 1, 0) == 0) {
        InitializeCriticalSection(&g_log_lock);
        CreateDirectoryA("Data\\NVSE", NULL);
        CreateDirectoryA("Data\\NVSE\\Plugins", NULL);
    }
}
static void LOG(const char* level, const char* fmt, ...) {
    if (!g_log_init) LogInit();
    char buf[1024];
    va_list args; va_start(args, fmt);
    _vsnprintf_s(buf, _TRUNCATE, fmt, args);
    va_end(args);
    char line[1152];
    _snprintf_s(line, _TRUNCATE, "[%s] %s\r\n", level, buf);
    EnterCriticalSection(&g_log_lock);
    HANDLE h = CreateFileA("Data\\NVSE\\Plugins\\Overdrive.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        SetFilePointer(h, 0, NULL, FILE_END);
        DWORD w=0; WriteFile(h, line, (DWORD)strlen(line), &w, NULL);
        CloseHandle(h);
    }
    LeaveCriticalSection(&g_log_lock);
}
#define LOGI(...) LOG("INFO", __VA_ARGS__)
#define LOGW(...) LOG("WARN", __VA_ARGS__)
#define LOGE(...) LOG("ERROR", __VA_ARGS__)

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

// Hooked CRT allocators -> rpmalloc
static void* (__cdecl* orig_malloc)(size_t) = nullptr;
static void (__cdecl* orig_free)(void*) = nullptr;
static void* (__cdecl* orig_calloc)(size_t, size_t) = nullptr;
static void* (__cdecl* orig_realloc)(void*, size_t) = nullptr;

// Hooked Win32 heap/VA API
static LPVOID (WINAPI* orig_HeapAlloc)(HANDLE, DWORD, SIZE_T) = nullptr;
static LPVOID (WINAPI* orig_HeapReAlloc)(HANDLE, DWORD, LPVOID, SIZE_T) = nullptr;
static BOOL   (WINAPI* orig_HeapFree)(HANDLE, DWORD, LPVOID) = nullptr;
static LPVOID (WINAPI* orig_VirtualAlloc)(LPVOID, SIZE_T, DWORD, DWORD) = nullptr;

static void* __cdecl hk_malloc(size_t sz) {
    if (!g_initialized) return orig_malloc ? orig_malloc(sz) : nullptr;
    void* p = rpmalloc(sz);
    if (p) { InterlockedIncrement64(&g_allocs); InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)sz); }
    return p;
}
static void __cdecl hk_free(void* p) {
    if (!p) return;
    if (!g_initialized) { if (orig_free) orig_free(p); return; }
    size_t s = rpmalloc_usable_size(p);
    rpfree(p);
    InterlockedIncrement64(&g_frees);
    if (s) InterlockedExchangeAdd64(&g_bytes_free, (LONG64)s);
}
static void* __cdecl hk_calloc(size_t n, size_t sz) {
    if (!g_initialized) return orig_calloc ? orig_calloc(n, sz) : nullptr;
    if (!n || !sz || n > SIZE_MAX / sz) return nullptr;
    void* p = rpcalloc(n, sz);
    if (p) { InterlockedIncrement64(&g_allocs); InterlockedExchangeAdd64(&g_bytes_alloc, (LONG64)(n*sz)); }
    return p;
}
static void* __cdecl hk_realloc(void* p, size_t sz) {
    if (!g_initialized) return orig_realloc ? orig_realloc(p, sz) : nullptr;
    if (!p) return hk_malloc(sz);
    if (!sz) { hk_free(p); return nullptr; }
    size_t old = rpmalloc_usable_size(p);
    void* np = rprealloc(p, sz);
    if (np) {
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

// VirtualAlloc hook to prefer top-down
static LPVOID WINAPI hk_VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    if (!g_initialized || !g_cfg.hookVirtualAlloc) return orig_VirtualAlloc ? orig_VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect) : nullptr;
    if (g_cfg.preferTopDownVA) flAllocationType |= MEM_TOP_DOWN;
    return orig_VirtualAlloc ? orig_VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect) : nullptr;
}

// IAT hooking helpers
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
    return HookIATEntryInModule(GetModuleHandleA(NULL), dllName, funcName, newFunc, origFunc);
}
static void InstallHooksAcrossModules() {
    HMODULE mods[1024]; DWORD needed=0; HANDLE proc=GetCurrentProcess();
    if (!EnumProcessModules(proc, mods, sizeof(mods), &needed)) return;
    size_t cnt = needed/sizeof(HMODULE);
    for (size_t i=0;i<cnt;i++) {
        HMODULE m=mods[i];
        HookIATEntryInModule(m, "msvcrt.dll",   "malloc",  (void*)hk_malloc,  nullptr);
        HookIATEntryInModule(m, "msvcrt.dll",   "free",    (void*)hk_free,    nullptr);
        HookIATEntryInModule(m, "msvcrt.dll",   "calloc",  (void*)hk_calloc,  nullptr);
        HookIATEntryInModule(m, "msvcrt.dll",   "realloc", (void*)hk_realloc, nullptr);
        HookIATEntryInModule(m, "ucrtbase.dll", "malloc",  (void*)hk_malloc,  nullptr);
        HookIATEntryInModule(m, "ucrtbase.dll", "free",    (void*)hk_free,    nullptr);
        HookIATEntryInModule(m, "ucrtbase.dll", "calloc",  (void*)hk_calloc,  nullptr);
        HookIATEntryInModule(m, "ucrtbase.dll", "realloc", (void*)hk_realloc, nullptr);
        if (g_cfg.hookHeapAPI) {
            HookIATEntryInModule(m, "kernel32.dll", "HeapAlloc",   (void*)hk_HeapAlloc,   nullptr);
            HookIATEntryInModule(m, "kernel32.dll", "HeapReAlloc", (void*)hk_HeapReAlloc, nullptr);
            HookIATEntryInModule(m, "kernel32.dll", "HeapFree",    (void*)hk_HeapFree,    nullptr);
        }
        if (g_cfg.hookVirtualAlloc) {
            HookIATEntryInModule(m, "kernel32.dll", "VirtualAlloc", (void*)hk_VirtualAlloc, nullptr);
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
        case NVSEMessagingInterface::kMessage_PostPostLoad: {
            if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0) return;
            LOGI("Overdrive init start");
            LoadOverdriveConfig(g_cfg);
            if (g_cfg.useVanillaHeaps) { LOGI("Safe mode: vanilla heaps"); return; }
            // Initialize timers
            QueryPerformanceFrequency(&g_qpf);
            QueryPerformanceCounter(&g_last_tick);
            g_ema_ms = g_cfg.targetMsPerFrame;
            // rpmalloc
            rpmalloc_initialize(0);
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
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "RPNVSE Overdrive";
    info->version = 100; // 1.00
    if (nvse->runtimeVersion < RUNTIME_VERSION_1_4_MIN) return false;
    if (nvse->isEditor) return false;
    return true;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(NVSEInterface* nvse) {
    // Register commands
    if (nvse->RegisterCommand) {
        static CommandInfo kReload = {"OverdriveReload","odreload",0,"Reload Overdrive INI",0,0,nullptr,Cmd_ReloadOverdrive_Execute};
        static CommandInfo kBudgets= {"OverdriveGetBudgets","odbudgets",0,"Log current budgets",0,0,nullptr,Cmd_GetBudgets_Execute};
        static CommandInfo kHeaps  = {"OverdriveDumpHeaps","odheaps",0,"Log heap counters",0,0,nullptr,Cmd_DumpHeaps_Execute};
        nvse->RegisterCommand(&kReload);
        nvse->RegisterCommand(&kBudgets);
        nvse->RegisterCommand(&kHeaps);
    }
    // Messaging
    NVSEMessagingInterface* msg = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
    if (msg && msg->RegisterListener) msg->RegisterListener(nvse->GetPluginHandle(), "NVSE", (void*)MessageHandler);
    else {
        // Fallback: initialize immediately
        NVSEMessagingInterface::Message fake{ "NVSE", NVSEMessagingInterface::kMessage_PostPostLoad, 0, nullptr };
        MessageHandler(&fake);
    }
    return true;
}

// DllMain
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: DisableThreadLibraryCalls(hinst); break;
        case DLL_PROCESS_DETACH:
            FlushDelayedFrees();
            ShutdownVirtualFreeHook();
            if (g_initialized) rpmalloc_finalize();
            break;
    }
    return TRUE;
}
