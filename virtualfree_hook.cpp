// virtualfree_hook.cpp - VirtualFree Hook Implementation
#include "virtualfree_hook.h"
#include <stdio.h>
#include <string.h>

// Original VirtualFree function pointer
typedef BOOL (WINAPI* VirtualFree_t)(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType);
static VirtualFree_t g_original_VirtualFree = nullptr;

// Configuration
static VirtualFreeHookConfig g_config = {};
static VirtualFreeStats g_stats = {};
static bool g_hook_active = false;

// Delayed free queue (simple ring buffer)
#define MAX_DELAYED_FREES 1024
struct DelayedFree {
    LPVOID address;
    SIZE_T size;
    DWORD freeType;
    DWORD timestamp;
};
static DelayedFree g_delayed_queue[MAX_DELAYED_FREES];
static volatile LONG g_queue_head = 0;
static volatile LONG g_queue_tail = 0;
static CRITICAL_SECTION g_queue_lock;

static void LogVirtualFree(const char* msg) {
    if (!g_config.log_operations) return;
    
    HANDLE hFile = CreateFileA("Data\\NVSE\\Plugins\\VirtualFree_Debug.log", 
                               GENERIC_WRITE, FILE_SHARE_READ, NULL, 
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        SetFilePointer(hFile, 0, NULL, FILE_END);
        WriteFile(hFile, msg, (DWORD)strlen(msg), &written, NULL);
        WriteFile(hFile, "\r\n", 2, &written, NULL);
        CloseHandle(hFile);
    }
}

static bool ShouldBlockDecommit(LPVOID address, SIZE_T size) {
    // Block decommit for allocations larger than min_keep_size
    if (g_config.delay_decommit && size >= g_config.min_keep_size) {
        return true;
    }
    return false;
}

static bool ShouldBlockRelease(LPVOID address, SIZE_T size) {
    // Optionally prevent release of certain memory regions
    if (g_config.prevent_release && size >= g_config.min_keep_size) {
        return true;
    }
    return false;
}

static void QueueDelayedFree(LPVOID address, SIZE_T size, DWORD freeType) {
    EnterCriticalSection(&g_queue_lock);
    
    LONG next = (g_queue_head + 1) % MAX_DELAYED_FREES;
    if (next != g_queue_tail) {  // Queue not full
        g_delayed_queue[g_queue_head].address = address;
        g_delayed_queue[g_queue_head].size = size;
        g_delayed_queue[g_queue_head].freeType = freeType;
        g_delayed_queue[g_queue_head].timestamp = GetTickCount();
        g_queue_head = next;
        
        InterlockedIncrement(&g_stats.decommit_delayed);
    }
    
    LeaveCriticalSection(&g_queue_lock);
}

static void ProcessDelayedFrees(bool force) {
    if (!g_original_VirtualFree) return;
    
    EnterCriticalSection(&g_queue_lock);
    
    DWORD now = GetTickCount();
    while (g_queue_tail != g_queue_head) {
        DelayedFree* item = &g_delayed_queue[g_queue_tail];
        
        // Check if delay period has elapsed
        DWORD elapsed = now - item->timestamp;
        if (elapsed >= g_config.delay_ms || force) {
            // Actually free the memory now
            g_original_VirtualFree(item->address, item->size, item->freeType);
            
            g_queue_tail = (g_queue_tail + 1) % MAX_DELAYED_FREES;
        } else {
            break;  // Not ready yet
        }
    }
    
    LeaveCriticalSection(&g_queue_lock);
}

// Hooked VirtualFree function
static BOOL WINAPI Hooked_VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    if (!g_hook_active || !g_original_VirtualFree) {
        return g_original_VirtualFree ? g_original_VirtualFree(lpAddress, dwSize, dwFreeType) : FALSE;
    }
    
    InterlockedIncrement(&g_stats.total_calls);
    
    // Log the operation
    if (g_config.log_operations) {
        char buf[256];
        sprintf(buf, "VirtualFree(%p, %zu, 0x%X) %s", 
                lpAddress, (size_t)dwSize, dwFreeType,
                (dwFreeType & MEM_DECOMMIT) ? "MEM_DECOMMIT" : 
                (dwFreeType & MEM_RELEASE) ? "MEM_RELEASE" : "UNKNOWN");
        LogVirtualFree(buf);
    }
    
    // Handle MEM_DECOMMIT
    if (dwFreeType & MEM_DECOMMIT) {
        if (ShouldBlockDecommit(lpAddress, dwSize)) {
            InterlockedIncrement(&g_stats.decommit_blocked);
            g_stats.bytes_kept_committed += dwSize;
            
            if (g_config.log_operations) {
                LogVirtualFree("  -> BLOCKED (keeping memory committed)");
            }
            return TRUE;  // Pretend success
        }
        
        // Queue for delayed processing
        if (g_config.delay_decommit && g_config.delay_ms > 0) {
            QueueDelayedFree(lpAddress, dwSize, dwFreeType);
            return TRUE;  // Pretend success
        }
    }
    
    // Handle MEM_RELEASE
    if (dwFreeType & MEM_RELEASE) {
        if (ShouldBlockRelease(lpAddress, dwSize)) {
            InterlockedIncrement(&g_stats.release_blocked);
            g_stats.bytes_kept_committed += dwSize;
            
            if (g_config.log_operations) {
                LogVirtualFree("  -> BLOCKED (preventing release)");
            }
            return TRUE;  // Pretend success
        }
    }
    
    // Process any delayed frees that are ready
    ProcessDelayedFrees(false);
    
    // Allow the operation to proceed
    return g_original_VirtualFree(lpAddress, dwSize, dwFreeType);
}

// IAT hooking helper
static bool HookIAT_VirtualFree() {
    HMODULE hModule = GetModuleHandleA(NULL);
    if (!hModule) return false;
    
    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;
    
    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;
    
    IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)hModule + 
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    
    // Find kernel32.dll
    while (importDesc->Name != 0) {
        const char* dllName = (const char*)((BYTE*)hModule + importDesc->Name);
        
        if (_stricmp(dllName, "kernel32.dll") == 0) {
            IMAGE_THUNK_DATA* originalThunk = (IMAGE_THUNK_DATA*)((BYTE*)hModule + importDesc->OriginalFirstThunk);
            IMAGE_THUNK_DATA* firstThunk = (IMAGE_THUNK_DATA*)((BYTE*)hModule + importDesc->FirstThunk);
            
            while (originalThunk->u1.AddressOfData != 0) {
                IMAGE_IMPORT_BY_NAME* importByName = (IMAGE_IMPORT_BY_NAME*)((BYTE*)hModule + originalThunk->u1.AddressOfData);
                
                if (strcmp((char*)importByName->Name, "VirtualFree") == 0) {
                    DWORD oldProtect;
                    if (VirtualProtect(&firstThunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
                        g_original_VirtualFree = (VirtualFree_t)firstThunk->u1.Function;
                        firstThunk->u1.Function = (DWORD_PTR)Hooked_VirtualFree;
                        VirtualProtect(&firstThunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                        return true;
                    }
                }
                
                originalThunk++;
                firstThunk++;
            }
        }
        importDesc++;
    }
    
    return false;
}

bool InitVirtualFreeHook(const VirtualFreeHookConfig* config) {
    if (g_hook_active) return false;
    
    if (config) {
        g_config = *config;
    } else {
        // Default configuration: conservative blocking
        g_config.delay_decommit = true;
        g_config.prevent_release = false;
        g_config.delay_ms = 2000;  // 2 second delay (reduced from 5)
        g_config.min_keep_size = 256 * 1024;  // 256 KB minimum (reduced from 1MB)
        g_config.log_operations = false;
    }
    
    InitializeCriticalSection(&g_queue_lock);
    memset(&g_stats, 0, sizeof(g_stats));
    memset(g_delayed_queue, 0, sizeof(g_delayed_queue));
    
    if (!HookIAT_VirtualFree()) {
        DeleteCriticalSection(&g_queue_lock);
        return false;
    }
    
    g_hook_active = true;
    
    if (g_config.log_operations) {
        LogVirtualFree("=== VirtualFree Hook Initialized ===");
        char buf[256];
        sprintf(buf, "Config: delay=%d, prevent_release=%d, delay_ms=%u, min_size=%zu",
                g_config.delay_decommit, g_config.prevent_release, 
                g_config.delay_ms, (size_t)g_config.min_keep_size);
        LogVirtualFree(buf);
    }
    
    return true;
}

static void RestoreIAT_VirtualFree() {
    if (!g_original_VirtualFree) return;
    HMODULE hModule = GetModuleHandleA(NULL);
    if (!hModule) return;

    IMAGE_DOS_HEADER* dosHeader = (IMAGE_DOS_HEADER*)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;

    IMAGE_NT_HEADERS* ntHeaders = (IMAGE_NT_HEADERS*)((BYTE*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return;

    IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)hModule + 
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    while (importDesc->Name != 0) {
        const char* dllName = (const char*)((BYTE*)hModule + importDesc->Name);
        if (_stricmp(dllName, "kernel32.dll") == 0) {
            IMAGE_THUNK_DATA* originalThunk = (IMAGE_THUNK_DATA*)((BYTE*)hModule + importDesc->OriginalFirstThunk);
            IMAGE_THUNK_DATA* firstThunk = (IMAGE_THUNK_DATA*)((BYTE*)hModule + importDesc->FirstThunk);
            while (originalThunk->u1.AddressOfData != 0) {
                IMAGE_IMPORT_BY_NAME* importByName = (IMAGE_IMPORT_BY_NAME*)((BYTE*)hModule + originalThunk->u1.AddressOfData);
                if (strcmp((char*)importByName->Name, "VirtualFree") == 0) {
                    DWORD oldProtect;
                    if (VirtualProtect(&firstThunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
                        firstThunk->u1.Function = (DWORD_PTR)g_original_VirtualFree;
                        VirtualProtect(&firstThunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                    }
                    return;
                }
                originalThunk++;
                firstThunk++;
            }
        }
        importDesc++;
    }
}

void ShutdownVirtualFreeHook() {
    if (!g_hook_active) return;
    
    FlushDelayedFrees();

    // Restore IAT entry back to original before disabling
    RestoreIAT_VirtualFree();
    
    g_hook_active = false;
    DeleteCriticalSection(&g_queue_lock);
    
    if (g_config.log_operations) {
        LogVirtualFree("=== VirtualFree Hook Shutdown ===");
    }
}

void GetVirtualFreeStats(VirtualFreeStats* stats) {
    if (stats) {
        *stats = g_stats;
    }
}

void FlushDelayedFrees() {
    ProcessDelayedFrees(true);  // Force flush
}
