// Memory Budget System for Fallout: New Vegas
// Based on comprehensive game analysis of budget initialization

#pragma once
#include <windows.h>
#include <stdint.h>

// Helper to convert RVA to actual address (account for module base/ASLR)
static inline HMODULE __mb_GetGameBase() {
    return GetModuleHandleA(NULL);
}
static inline void* __mb_RVA(uint32_t offset) {
    return (void*)((uintptr_t)__mb_GetGameBase() + (uintptr_t)offset);
}

// Memory budget RVAs (initialization sites in code/data)
#define BUDGET_EXTERIOR_TEXTURE_ADDR    0x00F3DE43  // push 1400000h (20MB)
#define BUDGET_INTERIOR_GEOMETRY_ADDR   0x00F3E113  // push 0A00000h (10MB)
#define BUDGET_INTERIOR_TEXTURE_ADDR    0x00F3E143  // push 6400000h (100MB)
#define BUDGET_INTERIOR_WATER_ADDR      0x00F3E173  // push 0A00000h (10MB)
#define BUDGET_ACTOR_MEMORY_ADDR        0x00F3E593  // push 0A00000h (10MB)

// Manager global RVAs (runtime values)
#define MANAGER_EXTERIOR_GEOMETRY       0x011C5BBC  // Dynamic via sub_500000
#define MANAGER_EXTERIOR_TEXTURE        0x011C5B5C  // 20MB static
#define MANAGER_EXTERIOR_WATER          0x011C5C50  // Dynamic via sub_500000
#define MANAGER_INTERIOR_GEOMETRY       0x011C5C80  // 10MB static
#define MANAGER_INTERIOR_TEXTURE        0x011C5C60  // 100MB static
#define MANAGER_INTERIOR_WATER          0x011C5A4C  // 10MB static
#define MANAGER_ACTOR_MEMORY            0x011C59E0  // 10MB static

// Default values (original game limits)
#define DEFAULT_EXTERIOR_TEXTURE        0x1400000   // 20 MB
#define DEFAULT_INTERIOR_GEOMETRY       0x0A00000   // 10 MB
#define DEFAULT_INTERIOR_TEXTURE        0x6400000   // 100 MB
#define DEFAULT_INTERIOR_WATER          0x0A00000   // 10 MB
#define DEFAULT_ACTOR_MEMORY            0x0A00000   // 10 MB

// Recommended increased values (for modern systems)
#define INCREASED_EXTERIOR_TEXTURE      0x4000000   // 64 MB (3.2x)
#define INCREASED_INTERIOR_GEOMETRY     0x2000000   // 32 MB (3.2x)
#define INCREASED_INTERIOR_TEXTURE      0x10000000  // 256 MB (2.56x)
#define INCREASED_INTERIOR_WATER        0x2000000   // 32 MB (3.2x)
#define INCREASED_ACTOR_MEMORY          0x2000000   // 32 MB (3.2x)

// Aggressive values (for heavily modded games) - INCREASED
#define AGGRESSIVE_EXTERIOR_TEXTURE     0xC000000   // 192 MB (9.6x)
#define AGGRESSIVE_INTERIOR_GEOMETRY    0x6000000   // 96 MB (9.6x)
#define AGGRESSIVE_INTERIOR_TEXTURE     0x30000000  // 768 MB (7.68x)
#define AGGRESSIVE_INTERIOR_WATER       0x6000000   // 96 MB (9.6x)
#define AGGRESSIVE_ACTOR_MEMORY         0x6000000   // 96 MB (9.6x)

// Ultra-aggressive values (for extreme modding with massive texture packs)
#define ULTRA_EXTERIOR_TEXTURE          0x20000000  // 512 MB (25.6x)
#define ULTRA_INTERIOR_GEOMETRY         0x10000000  // 256 MB (25.6x)
#define ULTRA_INTERIOR_TEXTURE          0x80000000  // 2048 MB / 2GB (20.48x)
#define ULTRA_INTERIOR_WATER            0x10000000  // 256 MB (25.6x)
#define ULTRA_ACTOR_MEMORY              0x10000000  // 256 MB (25.6x)

// EXTREME values (for absolute maximum texture usage - pushes to 3GB limit)
#define EXTREME_EXTERIOR_TEXTURE        0x40000000  // 1024 MB / 1GB (51.2x)
#define EXTREME_INTERIOR_GEOMETRY       0x20000000  // 512 MB (51.2x)
#define EXTREME_INTERIOR_TEXTURE        0xC0000000  // 3072 MB / 3GB (30.72x) - MAXIMUM
#define EXTREME_INTERIOR_WATER          0x20000000  // 512 MB (51.2x)
#define EXTREME_ACTOR_MEMORY            0x20000000  // 512 MB (51.2x)

// Configuration structure
typedef struct MemoryBudgetConfig {
    DWORD exterior_texture;
    DWORD interior_geometry;
    DWORD interior_texture;
    DWORD interior_water;
    DWORD actor_memory;
} MemoryBudgetConfig;

// Preset configurations
enum BudgetPreset {
    PRESET_DEFAULT = 0,      // Original game limits
    PRESET_RECOMMENDED = 1,  // Balanced increase for modern systems
    PRESET_AGGRESSIVE = 2,   // Maximum for heavily modded games
    PRESET_ULTRA = 3,        // Extreme for massive texture packs (2GB textures)
    PRESET_EXTREME = 4,      // MAXIMUM texture usage (3GB textures) - highest possible
    PRESET_CUSTOM = 5        // User-defined values
};

// Budget patching functions
inline MemoryBudgetConfig GetPresetConfig(BudgetPreset preset) {
    MemoryBudgetConfig config;
    
    switch (preset) {
        case PRESET_RECOMMENDED:
            config.exterior_texture = INCREASED_EXTERIOR_TEXTURE;
            config.interior_geometry = INCREASED_INTERIOR_GEOMETRY;
            config.interior_texture = INCREASED_INTERIOR_TEXTURE;
            config.interior_water = INCREASED_INTERIOR_WATER;
            config.actor_memory = INCREASED_ACTOR_MEMORY;
            break;
            
        case PRESET_AGGRESSIVE:
            config.exterior_texture = AGGRESSIVE_EXTERIOR_TEXTURE;
            config.interior_geometry = AGGRESSIVE_INTERIOR_GEOMETRY;
            config.interior_texture = AGGRESSIVE_INTERIOR_TEXTURE;
            config.interior_water = AGGRESSIVE_INTERIOR_WATER;
            config.actor_memory = AGGRESSIVE_ACTOR_MEMORY;
            break;
            
        case PRESET_ULTRA:
            config.exterior_texture = ULTRA_EXTERIOR_TEXTURE;
            config.interior_geometry = ULTRA_INTERIOR_GEOMETRY;
            config.interior_texture = ULTRA_INTERIOR_TEXTURE;
            config.interior_water = ULTRA_INTERIOR_WATER;
            config.actor_memory = ULTRA_ACTOR_MEMORY;
            break;
            
        case PRESET_EXTREME:
            config.exterior_texture = EXTREME_EXTERIOR_TEXTURE;
            config.interior_geometry = EXTREME_INTERIOR_GEOMETRY;
            config.interior_texture = EXTREME_INTERIOR_TEXTURE;
            config.interior_water = EXTREME_INTERIOR_WATER;
            config.actor_memory = EXTREME_ACTOR_MEMORY;
            break;
            
        case PRESET_DEFAULT:
        default:
            config.exterior_texture = DEFAULT_EXTERIOR_TEXTURE;
            config.interior_geometry = DEFAULT_INTERIOR_GEOMETRY;
            config.interior_texture = DEFAULT_INTERIOR_TEXTURE;
            config.interior_water = DEFAULT_INTERIOR_WATER;
            config.actor_memory = DEFAULT_ACTOR_MEMORY;
            break;
    }
    
    return config;
}

// Patch a single budget value in memory/code (RVA -> absolute)
inline bool PatchBudgetValue(uint32_t rva, DWORD newValue) {
    void* addr = __mb_RVA(rva);
    DWORD oldProtect;
    
    if (!VirtualProtect(addr, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    *(DWORD*)addr = newValue;
    
    VirtualProtect(addr, sizeof(DWORD), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), addr, sizeof(DWORD));
    return true;
}

// Patch a manager runtime value (RVA -> absolute)
inline bool PatchManagerValue(uint32_t rva, DWORD newValue) {
    void* addr = __mb_RVA(rva);
    DWORD oldProtect;
    if (!VirtualProtect(addr, sizeof(DWORD), PAGE_READWRITE, &oldProtect)) {
        return false;
    }
    *(DWORD*)addr = newValue;
    VirtualProtect(addr, sizeof(DWORD), oldProtect, &oldProtect);
    return true;
}

// Apply complete budget configuration (both init sites and live manager values)
inline bool ApplyBudgetConfig(const MemoryBudgetConfig* config) {
    bool success = true;
    
    // Patch code/data init constants (affects future inits)
    success &= PatchBudgetValue(BUDGET_EXTERIOR_TEXTURE_ADDR, config->exterior_texture);
    success &= PatchBudgetValue(BUDGET_INTERIOR_GEOMETRY_ADDR, config->interior_geometry);
    success &= PatchBudgetValue(BUDGET_INTERIOR_TEXTURE_ADDR, config->interior_texture);
    success &= PatchBudgetValue(BUDGET_INTERIOR_WATER_ADDR, config->interior_water);
    success &= PatchBudgetValue(BUDGET_ACTOR_MEMORY_ADDR, config->actor_memory);

    // Patch live manager runtime values (affects current session immediately)
    success &= PatchManagerValue(MANAGER_EXTERIOR_TEXTURE, config->exterior_texture);
    success &= PatchManagerValue(MANAGER_EXTERIOR_GEOMETRY, config->interior_geometry);
    success &= PatchManagerValue(MANAGER_EXTERIOR_WATER, config->interior_water);
    success &= PatchManagerValue(MANAGER_INTERIOR_TEXTURE, config->interior_texture);
    success &= PatchManagerValue(MANAGER_INTERIOR_GEOMETRY, config->interior_geometry);
    success &= PatchManagerValue(MANAGER_INTERIOR_WATER, config->interior_water);
    success &= PatchManagerValue(MANAGER_ACTOR_MEMORY, config->actor_memory);
    
    return success;
}

// Get current budget values from runtime managers
inline void GetCurrentBudgets(MemoryBudgetConfig* config) {
    config->exterior_texture = *(DWORD*)__mb_RVA(MANAGER_EXTERIOR_TEXTURE);
    config->interior_geometry = *(DWORD*)__mb_RVA(MANAGER_INTERIOR_GEOMETRY);
    config->interior_texture = *(DWORD*)__mb_RVA(MANAGER_INTERIOR_TEXTURE);
    config->interior_water = *(DWORD*)__mb_RVA(MANAGER_INTERIOR_WATER);
    config->actor_memory = *(DWORD*)__mb_RVA(MANAGER_ACTOR_MEMORY);
}
