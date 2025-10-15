#include "nvse_minimal.h"
#include <Windows.h>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <winnt.h>
#include <psapi.h>

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

// PE Section management
struct CodeSection {
    void* baseAddress;
    size_t size;
    DWORD originalProtection;
    bool isActive;
};

CodeSection g_codeSection = { nullptr, 0, 0, false };

// Get the main executable module information
static bool GetExecutableInfo(HMODULE* hModule, PIMAGE_DOS_HEADER* dosHeader, PIMAGE_NT_HEADERS* ntHeaders) {
    *hModule = GetModuleHandle(nullptr);  // Get main EXE
    if (!*hModule) {
        gLog.Log("Failed to get main module handle");
        return false;
    }

    *dosHeader = (PIMAGE_DOS_HEADER)*hModule;
    if ((*dosHeader)->e_magic != IMAGE_DOS_SIGNATURE) {
        gLog.Log("Invalid DOS signature in main executable");
        return false;
    }

    *ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)*hModule + (*dosHeader)->e_lfanew);
    if ((*ntHeaders)->Signature != IMAGE_NT_SIGNATURE) {
        gLog.Log("Invalid NT signature in main executable");
        return false;
    }

    return true;
}

// Create a new executable section in the process
static bool CreateCodeSection(size_t sectionSize = 0x10000) {  // Default 64KB
    gLog.Log("MemoryPoolNVSE: Creating new executable code section (%zu bytes)...", sectionSize);

    HMODULE hModule;
    PIMAGE_DOS_HEADER dosHeader;
    PIMAGE_NT_HEADERS ntHeaders;

    if (!GetExecutableInfo(&hModule, &dosHeader, &ntHeaders)) {
        return false;
    }

    // Log current executable information
    gLog.Log("Main executable base address: %p", hModule);
    gLog.Log("Image size: %u bytes", ntHeaders->OptionalHeader.SizeOfImage);
    gLog.Log("Number of sections: %u", ntHeaders->FileHeader.NumberOfSections);

    // Find a suitable address for our new section
    // We'll allocate after the existing image but within the same address space
    BYTE* baseAddr = (BYTE*)hModule + ntHeaders->OptionalHeader.SizeOfImage;
    
    // Round up to page boundary
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    size_t pageSize = sysInfo.dwPageSize;
    baseAddr = (BYTE*)((ULONG_PTR)(baseAddr + pageSize - 1) & ~(pageSize - 1));

    gLog.Log("Attempting to allocate code section at: %p", baseAddr);

    // Try to allocate memory at the calculated address
    void* allocatedMemory = VirtualAlloc(
        baseAddr,
        sectionSize,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );

    if (!allocatedMemory) {
        // If that fails, let Windows choose the address
        gLog.Log("Fixed address allocation failed, letting Windows choose address...");
        allocatedMemory = VirtualAlloc(
            nullptr,
            sectionSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );
    }

    if (!allocatedMemory) {
        gLog.Log("ERROR - Failed to allocate executable memory: %u", GetLastError());
        return false;
    }

    // Store section information
    g_codeSection.baseAddress = allocatedMemory;
    g_codeSection.size = sectionSize;
    g_codeSection.originalProtection = PAGE_EXECUTE_READWRITE;
    g_codeSection.isActive = true;

    gLog.Log("SUCCESS - Code section created at: %p (size: %zu bytes)", 
             allocatedMemory, sectionSize);

    // Initialize the section with a signature pattern for debugging
    memset(allocatedMemory, 0xCC, sectionSize);  // Fill with INT3 breakpoints
    
    // Write a small signature at the beginning
    const char signature[] = "MEMORYPOOLNVSE_CODESECTION";
    memcpy(allocatedMemory, signature, min(sizeof(signature), sectionSize));

    gLog.Log("Code section initialized with debug signature");
    return true;
}

// Clean up the code section
static void DestroyCodeSection() {
    if (g_codeSection.isActive && g_codeSection.baseAddress) {
        gLog.Log("Destroying code section at: %p", g_codeSection.baseAddress);
        
        if (VirtualFree(g_codeSection.baseAddress, 0, MEM_RELEASE)) {
            gLog.Log("Code section successfully freed");
        } else {
            gLog.Log("WARNING - Failed to free code section: %u", GetLastError());
        }

        g_codeSection.baseAddress = nullptr;
        g_codeSection.size = 0;
        g_codeSection.originalProtection = 0;
        g_codeSection.isActive = false;
    }
}

// Get information about our code section
static void LogCodeSectionInfo() {
    if (!g_codeSection.isActive) {
        gLog.Log("No active code section");
        return;
    }

    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(g_codeSection.baseAddress, &mbi, sizeof(mbi))) {
        gLog.Log("Code section info:");
        gLog.Log("  Base Address: %p", mbi.BaseAddress);
        gLog.Log("  Size: %zu bytes", mbi.RegionSize);
        gLog.Log("  State: %s", (mbi.State == MEM_COMMIT) ? "Committed" : "Reserved");
        gLog.Log("  Protection: 0x%X", mbi.Protect);
        gLog.Log("  Type: %s", (mbi.Type == MEM_PRIVATE) ? "Private" : "Other");
    } else {
        gLog.Log("Failed to query code section memory info");
    }
}

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
    gLog.Log("MemoryPoolNVSE: Starting initialization...");

    // Step 1: Create executable code section for future IAT hooking
    gLog.Log("=== PHASE 1: Code Section Creation ===");
    if (CreateCodeSection()) {
        gLog.Log("MemoryPoolNVSE: SUCCESS - Code section created and ready for injection");
        LogCodeSectionInfo();
    } else {
        gLog.Log("MemoryPoolNVSE: WARNING - Code section creation failed, IAT features will be disabled");
    }

    // Step 2: Initialize jemalloc memory allocation
    gLog.Log("=== PHASE 2: Memory Allocator Initialization ===");
    if (LoadJEmalloc()) {
        gLog.Log("MemoryPoolNVSE: SUCCESS - jemalloc.dll loaded successfully");

        if (TestJEmalloc()) {
            gLog.Log("MemoryPoolNVSE: SUCCESS - jemalloc functionality verified");
        }
        else {
            gLog.Log("MemoryPoolNVSE: WARNING - jemalloc loaded but functionality test failed");
        }
    }
    else {
        gLog.Log("MemoryPoolNVSE: INFO - jemalloc.dll not found, using system allocator");
    }

    // Step 3: Final status
    gLog.Log("=== INITIALIZATION COMPLETE ===");
    gLog.Log("Status Summary:");
    gLog.Log("  Code Section: %s", g_codeSection.isActive ? "ACTIVE" : "INACTIVE");
    gLog.Log("  Memory Allocator: %s", g_je_malloc ? "jemalloc" : "system");
    gLog.Log("MemoryPoolNVSE: All systems operational");
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
        gLog.Log("MemoryPoolNVSE: Game exiting, shutting down...");
        
        // Cleanup code section
        DestroyCodeSection();
        
        // Cleanup jemalloc
        if (g_jemallocDLL) {
            gLog.Log("Unloading jemalloc.dll...");
            FreeLibrary(g_jemallocDLL);
            g_jemallocDLL = nullptr;
        }
        
        gLog.Log("MemoryPoolNVSE: Shutdown complete");
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
            return "MemoryPoolNVSE: Operational - Using jemalloc";
        }
        return "MemoryPoolNVSE: Fallback - Using system allocator";
    }

    // Code section access functions
    __declspec(dllexport) void* GetCodeSectionBase() {
        return g_codeSection.isActive ? g_codeSection.baseAddress : nullptr;
    }

    __declspec(dllexport) size_t GetCodeSectionSize() {
        return g_codeSection.isActive ? g_codeSection.size : 0;
    }

    __declspec(dllexport) bool IsCodeSectionActive() {
        return g_codeSection.isActive;
    }

    __declspec(dllexport) const char* GetCodeSectionStatus() {
        if (g_codeSection.isActive) {
            static char statusBuffer[256];
            sprintf_s(statusBuffer, "Code Section: Active at %p (%zu bytes)", 
                     g_codeSection.baseAddress, g_codeSection.size);
            return statusBuffer;
        }
        return "Code Section: Inactive";
    }
}
