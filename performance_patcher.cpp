// performance_patcher.cpp - Performance Patcher Implementation
#include "performance_patcher.h"
#include <stdio.h>
#include <string.h>

// Game base address
static HMODULE GetGameBase() {
    return GetModuleHandleA(NULL);
}

// Calculate absolute address from RVA
static void* RVA(uint32_t offset) {
    return (void*)((uintptr_t)GetGameBase() + offset);
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

static bool PatchFloat(void* address, float new_value) {
    if (!address) return false;
    
    DWORD oldProtect;
    if (!VirtualProtect(address, sizeof(float), PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    
    *(float*)address = new_value;
    
    VirtualProtect(address, sizeof(float), oldProtect, &oldProtect);
    return true;
}

bool ApplyPerformancePatches(const PerformanceConfig* config) {
    if (!config) return false;
    
    bool success = true;
    
    // Patch frame time limit
    success &= PatchFloat(RVA(PERF_MAX_MS_PER_FRAME_ADDR), config->max_ms_per_frame);
    
    // Patch texture memory limit
    success &= PatchFloat(RVA(PERF_MAX_TEXTURE_MEMORY_ADDR), config->max_texture_memory_mb);
    
    // Patch geometry memory limit
    success &= PatchFloat(RVA(PERF_MAX_GEOMETRY_MEMORY_ADDR), config->max_geometry_memory_mb);
    
    // Patch particle system limit
    success &= PatchFloat(RVA(PERF_MAX_PARTICLE_SYSTEMS_ADDR), config->max_particle_systems);
    
    // Optionally disable aggressive culling
    if (config->disable_aggressive_culling) {
        DisableAggressiveCulling();
    }
    
    return success;
}

// NOP out a function by replacing first instruction with RET
static bool NopFunction(void* address) {
    if (!address) return false;
    
    DWORD oldProtect;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    // Write RET opcode (0xC3) to immediately return from function
    *(uint8_t*)address = 0xC3;
    
    VirtualProtect(address, 1, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), address, 1);
    
    return true;
}

// Store original bytes for restoration
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
