// performance_patcher.cpp - Performance Patcher Implementation
#include "performance_patcher.h"
#include "overdrive_log.h"
#include <stdio.h>
#include <string.h>

// Game base address
static HMODULE GetGameBase() {
    return GetModuleHandleA(NULL);
}

#include "AddressDiscovery.h"
// Calculate absolute address from RVA (pattern-aware)
static void* RVA(uint32_t offset) {
    return AddrDisc::ResolveRVA(offset);
}

static bool PatchFloatSafe(uint32_t rva, float newValue, float minVal, float maxVal, float expectedDefault, const char* name) {
    void* addr = AddrDisc::ResolveRVA(rva);
    if (!addr) { LOG_ERROR("Perf patch: failed to resolve %s (RVA=0x%08X)", name, rva); return false; }
    if (newValue < minVal) newValue = minVal;
    if (newValue > maxVal) newValue = maxVal;
    // Validate expected default within 10% tolerance to avoid patching wrong place
    if (expectedDefault > 0.0f && !AddrDisc::ValidateFloat(addr, expectedDefault, 0.10f)) {
        LOG_WARN("Perf patch: %s validation weak at %p (expected ~%.2f)", name, addr, expectedDefault);
    }
    DWORD oldProt;
    if (!VirtualProtect(addr, sizeof(float), PAGE_EXECUTE_READWRITE, &oldProt)) {
        LOG_ERROR("Perf patch: unprotect failed for %s at %p", name, addr);
        return false;
    }
    *(volatile float*)addr = newValue;
    DWORD tmp; VirtualProtect(addr, sizeof(float), oldProt, &tmp);
    FlushInstructionCache(GetCurrentProcess(), addr, sizeof(float));
    LOG_INFO("Perf patch: %s -> %.2f", name, newValue);
    return true;
}

static bool NopFunction(void* address) {
    if (!address) return false;
    DWORD oldProt;
    if (!VirtualProtect(address, 16, PAGE_EXECUTE_READWRITE, &oldProt)) return false;
    // Write RET followed by NOP sled to neutralize function prologue safely
    uint8_t bytes[16]; bytes[0] = 0xC3; for (int i=1;i<16;i++) bytes[i]=0x90;
    memcpy(address, bytes, sizeof(bytes));
    DWORD tmp; VirtualProtect(address, 16, oldProt, &tmp);
    FlushInstructionCache(GetCurrentProcess(), address, 16);
    return true;
}

PerformanceConfig GetPerformancePreset(PerformancePreset preset) {
    PerformanceConfig config = {};
    
    switch (preset) {
        case PERF_VANILLA:
            config.max_ms_per_frame = 16.67f;      // 60 FPS
            config.max_texture_memory_mb = 512.0f;  // 512 MB
            config.max_geometry_memory_mb = 256.0f; // 256 MB
            config.max_particle_systems = 100.0f;
            config.relax_frame_limits = false;
            config.disable_aggressive_culling = false;
            break;
            
        case PERF_RELAXED:
            config.max_ms_per_frame = 20.0f;       // Allow 50 FPS
            config.max_texture_memory_mb = 1024.0f; // 1 GB
            config.max_geometry_memory_mb = 512.0f; // 512 MB
            config.max_particle_systems = 200.0f;
            config.relax_frame_limits = true;
            config.disable_aggressive_culling = false;
            break;
            
        case PERF_HIGH_END:
            config.max_ms_per_frame = 33.33f;      // Allow 30 FPS minimum
            config.max_texture_memory_mb = 2048.0f; // 2 GB
            config.max_geometry_memory_mb = 1024.0f; // 1 GB
            config.max_particle_systems = 500.0f;
            config.relax_frame_limits = true;
            config.disable_aggressive_culling = true;
            break;
            
        case PERF_UNLIMITED:
            config.max_ms_per_frame = 1000.0f;     // Effectively unlimited
            // Reduce caps slightly to leave VA headroom
            config.max_texture_memory_mb = 12288.0f; // 12 GB
            config.max_geometry_memory_mb = 6144.0f;  // 6 GB
            config.max_particle_systems = 3000.0f;
            config.relax_frame_limits = true;
            config.disable_aggressive_culling = true;
            break;
    }
    
    return config;
}

bool ApplyPerformancePatches(const PerformanceConfig* config) {
    if (!config) return false;
    
    bool success = true;
    
    // Patch frame time limit (expect ~16.67 default)
    success &= PatchFloatSafe(PERF_MAX_MS_PER_FRAME_ADDR,     config->max_ms_per_frame,      5.0f,   2000.0f, 16.67f, "Frame time limit");
    // Patch texture memory limit (MB)
    success &= PatchFloatSafe(PERF_MAX_TEXTURE_MEMORY_ADDR,   config->max_texture_memory_mb, 64.0f,  32768.0f, 512.0f, "Texture memory limit");
    // Patch geometry memory limit (MB)
    success &= PatchFloatSafe(PERF_MAX_GEOMETRY_MEMORY_ADDR,  config->max_geometry_memory_mb,32.0f,  16384.0f, 256.0f, "Geometry memory limit");
    // Patch particle system limit
    success &= PatchFloatSafe(PERF_MAX_PARTICLE_SYSTEMS_ADDR, config->max_particle_systems,  10.0f,  10000.0f, 100.0f, "Particle system limit");
    
    // Optionally disable aggressive culling
    if (config->disable_aggressive_culling) {
        DisableAggressiveCulling();
    }
    
    return success;
}

static uint8_t g_original_bytes[6][16];
static bool g_bytes_saved = false;

static bool SaveFunctionBytes(void* address, int index) {
    if (index >= 6) return false;
    memcpy(g_original_bytes[index], address, 16);
    return true;
}

static bool RestoreFunctionBytes(void* address, int index) {
    if (index >= 6 || !address) return false;
    
    DWORD oldProtect;
    if (!VirtualProtect(address, 16, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    memcpy(address, g_original_bytes[index], 16);
    
    VirtualProtect(address, 16, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), address, 16);
    
    return true;
}

bool DisableAggressiveCulling() {
    if (!g_bytes_saved) {
        // Save original bytes before patching
        SaveFunctionBytes(RVA(PERF_FRAME_TIME_MONITOR_ADDR), 0);
        SaveFunctionBytes(RVA(PERF_LOD_FRAME_ADJUSTER_ADDR), 1);
        SaveFunctionBytes(RVA(PERF_TEXTURE_MONITOR_ADDR), 2);
        SaveFunctionBytes(RVA(PERF_GEOMETRY_MONITOR_ADDR), 3);
        SaveFunctionBytes(RVA(PERF_ACTOR_CULLER_ADDR), 4);
        SaveFunctionBytes(RVA(PERF_PARTICLE_MANAGER_ADDR), 5);
        g_bytes_saved = true;
    }
    
    bool success = true;
    
    // NOP out aggressive culling callbacks
    // These functions reduce quality when budgets are exceeded
    success &= NopFunction(RVA(PERF_FRAME_TIME_MONITOR_ADDR));
    success &= NopFunction(RVA(PERF_LOD_FRAME_ADJUSTER_ADDR));
    success &= NopFunction(RVA(PERF_TEXTURE_MONITOR_ADDR));
    success &= NopFunction(RVA(PERF_GEOMETRY_MONITOR_ADDR));
    success &= NopFunction(RVA(PERF_ACTOR_CULLER_ADDR));
    success &= NopFunction(RVA(PERF_PARTICLE_MANAGER_ADDR));
    
    return success;
}

bool EnableAggressiveCulling() {
    if (!g_bytes_saved) return false;
    
    bool success = true;
    
    // Restore original function bytes
    success &= RestoreFunctionBytes(RVA(PERF_FRAME_TIME_MONITOR_ADDR), 0);
    success &= RestoreFunctionBytes(RVA(PERF_LOD_FRAME_ADJUSTER_ADDR), 1);
    success &= RestoreFunctionBytes(RVA(PERF_TEXTURE_MONITOR_ADDR), 2);
    success &= RestoreFunctionBytes(RVA(PERF_GEOMETRY_MONITOR_ADDR), 3);
    success &= RestoreFunctionBytes(RVA(PERF_ACTOR_CULLER_ADDR), 4);
    success &= RestoreFunctionBytes(RVA(PERF_PARTICLE_MANAGER_ADDR), 5);
    
    return success;
}
