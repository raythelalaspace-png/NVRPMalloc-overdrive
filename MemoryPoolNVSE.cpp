#include "nvse_minimal.h"
#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <cstdarg>

// Plugin globals
SimpleLog gLog("MemoryPoolNV.log");
PluginHandle g_pluginHandle = kPluginHandle_Invalid;
NVSEMessagingInterface* g_messagingInterface = nullptr;

// Function pointers for jemalloc
typedef void* (*malloc_func)(size_t);
typedef void (*free_func)(void*);
typedef void* (*calloc_func)(size_t, size_t);
typedef void* (*realloc_func)(void*, size_t);

HMODULE g_jemallocDLL = nullptr;
malloc_func g_je_malloc = nullptr;
free_func g_je_free = nullptr;
calloc_func g_je_calloc = nullptr;
realloc_func g_je_realloc = nullptr;

// Load jemalloc DLL and get function pointers
static bool LoadJEmalloc() {
    gLog.Log("MemoryPoolNV: Attempting to load jemalloc.dll...");

    // Try to load jemalloc from the game directory
    g_jemallocDLL = LoadLibraryA("jemalloc.dll");
    if (!g_jemallocDLL) {
        gLog.Log("MemoryPoolNV: ERROR - Failed to load jemalloc.dll - Error code: %d", GetLastError());
        return false;
    }

    gLog.Log("MemoryPoolNV: jemalloc.dll loaded successfully");

    // Get function pointers
    g_je_malloc = (malloc_func)GetProcAddress(g_jemallocDLL, "je_malloc");
    g_je_free = (free_func)GetProcAddress(g_jemallocDLL, "je_free");
    g_je_calloc = (calloc_func)GetProcAddress(g_jemallocDLL, "je_calloc");
    g_je_realloc = (realloc_func)GetProcAddress(g_jemallocDLL, "je_realloc");

    if (!g_je_malloc || !g_je_free || !g_je_calloc || !g_je_realloc) {
        gLog.Log("MemoryPoolNV: ERROR - Failed to get one or more jemalloc function pointers");
        FreeLibrary(g_jemallocDLL);
        g_jemallocDLL = nullptr;
        return false;
    }

    gLog.Log("MemoryPoolNV: Successfully obtained all jemalloc function pointers");
    return true;
}

// Test jemalloc functionality
static bool TestJEmalloc() {
    if (!g_je_malloc) return false;

    gLog.Log("MemoryPoolNV: Testing jemalloc functionality...");

    // Test allocation
    void* test_ptr = g_je_malloc(1024);
    if (!test_ptr) {
        gLog.Log("MemoryPoolNV: ERROR - jemalloc test allocation failed");
        return false;
    }

    // Test reallocation
    void* test_realloc = g_je_realloc(test_ptr, 2048);
    if (!test_realloc) {
        gLog.Log("MemoryPoolNV: ERROR - jemalloc test reallocation failed");
        g_je_free(test_ptr);
        return false;
    }

    // Test calloc
    void* test_calloc = g_je_calloc(10, 64);
    if (!test_calloc) {
        gLog.Log("MemoryPoolNV: ERROR - jemalloc test calloc failed");
        g_je_free(test_realloc);
        return false;
    }

    // Cleanup
    g_je_free(test_realloc);
    g_je_free(test_calloc);

    gLog.Log("MemoryPoolNV: jemalloc functionality test completed successfully");
    return true;
}

// Initialize the memory pool
static void InitializeMemoryPool() {
    gLog.Log("MemoryPoolNV: Starting initialization...");

    if (LoadJEmalloc()) {
        gLog.Log("MemoryPoolNV: SUCCESS - jemalloc.dll loaded successfully");

        if (TestJEmalloc()) {
            gLog.Log("MemoryPoolNV: SUCCESS - jemalloc functionality verified");
            gLog.Log("MemoryPoolNV: Memory pool initialization COMPLETE - All systems operational");
        }
        else {
            gLog.Log("MemoryPoolNV: WARNING - jemalloc loaded but functionality test failed");
        }
    }
    else {
        gLog.Log("MemoryPoolNV: ERROR - Failed to load jemalloc.dll");
        gLog.Log("MemoryPoolNV: Memory pool will use default system allocator");
    }
}

// NVSE message handler
void MessageHandler(NVSEMessagingInterface::Message* msg)
{
    switch (msg->type)
    {
    case NVSEMessagingInterface::kMessage_PostLoad:
        gLog.Log("MemoryPoolNV: Received PostLoad message");
        break;
    case NVSEMessagingInterface::kMessage_PostPostLoad:
        gLog.Log("MemoryPoolNV: Received PostPostLoad message - All plugins loaded");
        InitializeMemoryPool();
        break;
    case NVSEMessagingInterface::kMessage_ExitGame:
        gLog.Log("MemoryPoolNV: Game exiting, shutting down...");
        if (g_jemallocDLL) {
            FreeLibrary(g_jemallocDLL);
            g_jemallocDLL = nullptr;
        }
        break;
    default:
        break;
    }
}

// NVSE Plugin Query - called by NVSE to check plugin compatibility
extern "C" bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
{
    gLog.Log("MemoryPoolNV: Plugin Query called");

    // Fill out plugin info
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "MemoryPoolNV";
    info->version = 1;

    // Version checks
    if (nvse->nvseVersion < PACKED_NVSE_VERSION) {
        gLog.Log("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, PACKED_NVSE_VERSION);
        return false;
    }

    if (!nvse->isEditor) {
        // Check for minimum 1.4.x version (accept any 1.4.x.x version)
        if ((nvse->runtimeVersion & 0xFF000000) < RUNTIME_VERSION_1_4_MIN) {
            gLog.Log("Incorrect runtime version (got %08X need at least 1.4.x.x)", nvse->runtimeVersion);
            return false;
        }
        gLog.Log("Runtime version check passed (got %08X)", nvse->runtimeVersion);

        if (nvse->isNogore) {
            gLog.Log("NoGore is not supported");
            return false;
        }
    }

    gLog.Log("MemoryPoolNV: Plugin Query successful");
    return true;
}

// NVSE Plugin Load - called by NVSE to load the plugin
extern "C" bool NVSEPlugin_Load(NVSEInterface* nvse)
{
    gLog.Log("MemoryPoolNV: Plugin Load called");

    g_pluginHandle = nvse->GetPluginHandle();

    // Register for NVSE messaging
    g_messagingInterface = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
    if (g_messagingInterface) {
        g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", (void*)MessageHandler);
        gLog.Log("MemoryPoolNV: Successfully registered for NVSE messaging");
    }
    else {
        gLog.Log("MemoryPoolNV: WARNING - Failed to get messaging interface");
    }

    gLog.Log("MemoryPoolNV: Plugin Load successful");
    return true;
}

// Export functions for other mods to use
extern "C" {
    __declspec(dllexport) void* MemoryPoolMalloc(size_t size) {
        if (g_je_malloc) {
            return g_je_malloc(size);
        }
        return malloc(size);
    }

    __declspec(dllexport) void MemoryPoolFree(void* ptr) {
        if (g_je_free) {
            g_je_free(ptr);
        }
        else {
            free(ptr);
        }
    }

    __declspec(dllexport) const char* GetMemoryPoolStatus() {
        if (g_jemallocDLL && g_je_malloc) {
            return "MemoryPoolNV: Operational - Using jemalloc";
        }
        return "MemoryPoolNV: Fallback - Using system allocator";
    }
}