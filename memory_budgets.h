// Memory Budget System for Fallout: New Vegas
// Based on comprehensive game analysis of budget initialization

#pragma once
#include <windows.h>
#include <stdint.h>
#include "AddressDiscovery.h"
#include "overdrive_log.h"

// Helper to convert RVA to actual address (account for module base/ASLR)
static inline HMODULE __mb_GetGameBase() {
    return GetModuleHandleA(NULL);
}
static inline void* __mb_RVA(uint32_t offset) {
    // Try robust resolver (pattern/exports), fallback to base+RVA
    return AddrDisc::ResolveRVA(offset);
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

// Result structure for patch operations
struct BudgetPatchResult {
    bool success;
    DWORD old_value;
    DWORD new_value;
    const char* name;
    DWORD error_code;
};

// Patch a single budget value in memory/code (RVA -> absolute)
inline BudgetPatchResult PatchBudgetValue(uint32_t rva, DWORD newValue, const char* name) {
    BudgetPatchResult result = { false, 0, newValue, name, 0 };
    
    // Get address from RVA
    void* addr = __mb_RVA(rva);
    if (!addr) {
        result.error_code = GetLastError();
        LOG_ERROR("Failed to resolve RVA 0x%08X for %s (error: 0x%08X)", rva, name, result.error_code);
        return result;
    }

    // Read current value
    result.old_value = *(DWORD*)addr;
    
    // Skip if already patched
    if (result.old_value == newValue) {
        result.success = true;
        LOG_DEBUG("%s already set to 0x%08X", name, newValue);
        return result;
    }

    // Validate against expected default if known
    uint32_t expected = 0;
    switch (rva) {
        case BUDGET_EXTERIOR_TEXTURE_ADDR: expected = DEFAULT_EXTERIOR_TEXTURE; break;
        case BUDGET_INTERIOR_GEOMETRY_ADDR: expected = DEFAULT_INTERIOR_GEOMETRY; break;
        case BUDGET_INTERIOR_TEXTURE_ADDR: expected = DEFAULT_INTERIOR_TEXTURE; break;
        case BUDGET_INTERIOR_WATER_ADDR: expected = DEFAULT_INTERIOR_WATER; break;
        case BUDGET_ACTOR_MEMORY_ADDR: expected = DEFAULT_ACTOR_MEMORY; break;
    }

    if (expected && !AddrDisc::ValidateDWORD(addr, expected, 0) && 
        !AddrDisc::ValidateDWORD(addr, newValue, 0)) {
        result.error_code = ERROR_INVALID_DATA;
        LOG_ERROR("Validation failed for %s at 0x%p: expected ~0x%08X, got 0x%08X", 
                 name, addr, expected, result.old_value);
        return result;
    }

    // Change memory protection
    DWORD oldProtect;
    if (!VirtualProtect(addr, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        result.error_code = GetLastError();
        LOG_ERROR("Failed to unprotect memory at 0x%p for %s (error: 0x%08X)", 
                 addr, name, result.error_code);
        return result;
    }

    // Patch the value
    *(DWORD*)addr = newValue;
    
    // Verify the write
    DWORD verify = *(DWORD*)addr;
    if (verify != newValue) {
        result.error_code = ERROR_WRITE_FAULT;
        LOG_ERROR("Verification failed for %s: wrote 0x%08X but got 0x%08X", 
                 name, newValue, verify);
    } else {
        result.success = true;
        LOG_INFO("Patched %s: 0x%08X -> 0x%08X", name, result.old_value, newValue);
    }

    // Restore protection
    DWORD temp;
    VirtualProtect(addr, sizeof(DWORD), oldProtect, &temp);
    FlushInstructionCache(GetCurrentProcess(), addr, sizeof(DWORD));
    
    return result;
}

// Patch a manager runtime value (RVA -> absolute)
inline BudgetPatchResult PatchManagerValue(uint32_t rva, DWORD newValue, const char* name) {
    BudgetPatchResult result = { false, 0, newValue, name, 0 };
    
    // Get address from RVA
    void* addr = __mb_RVA(rva);
    if (!addr) {
        result.error_code = GetLastError();
        LOG_ERROR("Failed to resolve manager RVA 0x%08X for %s (error: 0x%08X)", 
                 rva, name, result.error_code);
        return result;
    }

    // Read current value
    result.old_value = *(DWORD*)addr;
    
    // Skip if already set
    if (result.old_value == newValue) {
        result.success = true;
        LOG_DEBUG("Manager %s already set to 0x%08X", name, newValue);
        return result;
    }

    // Change memory protection
    DWORD oldProtect;
    if (!VirtualProtect(addr, sizeof(DWORD), PAGE_READWRITE, &oldProtect)) {
        result.error_code = GetLastError();
        LOG_ERROR("Failed to unprotect manager memory at 0x%p for %s (error: 0x%08X)", 
                 addr, name, result.error_code);
        return result;
    }

    // Patch the value
    *(DWORD*)addr = newValue;
    
    // Verify the write
    DWORD verify = *(DWORD*)addr;
    if (verify != newValue) {
        result.error_code = ERROR_WRITE_FAULT;
        LOG_ERROR("Manager update failed for %s: wrote 0x%08X but got 0x%08X", 
                 name, newValue, verify);
    } else {
        result.success = true;
        LOG_INFO("Updated manager %s: 0x%08X -> 0x%08X", name, result.old_value, newValue);
    }

    // Restore protection
    DWORD temp;
    VirtualProtect(addr, sizeof(DWORD), oldProtect, &temp);
    
    return result;
}

// Structure to track patch results
struct BudgetPatchResults {
    int total_patches;
    int successful_patches;
    int failed_patches;
    bool all_succeeded;
};

// Forward declaration for current budget read
inline bool GetCurrentBudgets(MemoryBudgetConfig* config);

// Apply complete budget configuration with validation
inline BudgetPatchResults ApplyBudgetConfig(const MemoryBudgetConfig* config) {
    BudgetPatchResults results = {0};
    if (!config) {
        LOG_ERROR("Invalid config pointer");
        return results;
    }

    LOG_INFO("Applying budget configuration...");
    LOG_DEBUG("Exterior Texture: 0x%08X (%u MB)", 
             config->exterior_texture, config->exterior_texture / (1024 * 1024));
    LOG_DEBUG("Interior Geometry: 0x%08X (%u MB)", 
             config->interior_geometry, config->interior_geometry / (1024 * 1024));
    LOG_DEBUG("Interior Texture: 0x%08X (%u MB)", 
             config->interior_texture, config->interior_texture / (1024 * 1024));
    LOG_DEBUG("Interior Water: 0x%08X (%u MB)", 
             config->interior_water, config->interior_water / (1024 * 1024));
    LOG_DEBUG("Actor Memory: 0x%08X (%u MB)", 
             config->actor_memory, config->actor_memory / (1024 * 1024));

    // Define all patches to apply
    struct PatchDef {
        uint32_t rva;
        DWORD value;
        const char* name;
        bool is_manager;
    };

    PatchDef patches[] = {
        // Code patches (affect future initializations)
        {BUDGET_EXTERIOR_TEXTURE_ADDR, config->exterior_texture, "Exterior Texture (Code)", false},
        {BUDGET_INTERIOR_GEOMETRY_ADDR, config->interior_geometry, "Interior Geometry (Code)", false},
        {BUDGET_INTERIOR_TEXTURE_ADDR, config->interior_texture, "Interior Texture (Code)", false},
        {BUDGET_INTERIOR_WATER_ADDR, config->interior_water, "Interior Water (Code)", false},
        {BUDGET_ACTOR_MEMORY_ADDR, config->actor_memory, "Actor Memory (Code)", false},
        
        // Manager patches (affect current session)
        {MANAGER_EXTERIOR_TEXTURE, config->exterior_texture, "Exterior Texture (Manager)", true},
        {MANAGER_EXTERIOR_GEOMETRY, config->interior_geometry, "Exterior Geometry (Manager)", true},
        {MANAGER_EXTERIOR_WATER, config->interior_water, "Exterior Water (Manager)", true},
        {MANAGER_INTERIOR_TEXTURE, config->interior_texture, "Interior Texture (Manager)", true},
        {MANAGER_INTERIOR_GEOMETRY, config->interior_geometry, "Interior Geometry (Manager)", true},
        {MANAGER_INTERIOR_WATER, config->interior_water, "Interior Water (Manager)", true},
        {MANAGER_ACTOR_MEMORY, config->actor_memory, "Actor Memory (Manager)", true}
    };

    // Apply all patches
    for (const auto& patch : patches) {
        results.total_patches++;
        
        BudgetPatchResult result = patch.is_manager ?
            PatchManagerValue(patch.rva, patch.value, patch.name) :
            PatchBudgetValue(patch.rva, patch.value, patch.name);
            
        if (result.success) {
            results.successful_patches++;
        } else {
            results.failed_patches++;
            LOG_ERROR("Failed to patch %s: error 0x%08X", patch.name, result.error_code);
        }
    }

    // Verify all manager values
    MemoryBudgetConfig current;
    GetCurrentBudgets(&current);
    
    bool verified = 
        current.exterior_texture == config->exterior_texture &&
        current.interior_geometry == config->interior_geometry &&
        current.interior_texture == config->interior_texture &&
        current.interior_water == config->interior_water &&
        current.actor_memory == config->actor_memory;

    if (!verified) {
        LOG_ERROR("Budget verification failed! Some values were not applied correctly");
        LOG_INFO("Expected: Tex=0x%X Geo=0x%X Water=0x%X Actor=0x%X",
                config->exterior_texture, config->interior_geometry,
                config->interior_water, config->actor_memory);
        LOG_INFO("Got:      Tex=0x%X Geo=0x%X Water=0x%X Actor=0x%X",
                current.exterior_texture, current.interior_geometry,
                current.interior_water, current.actor_memory);
    } else {
        LOG_INFO("Budget configuration applied and verified successfully");
    }

    results.all_succeeded = (results.failed_patches == 0) && verified;
    
    LOG_INFO("Budget patching complete: %d/%d successful, %d failed", 
            results.successful_patches, results.total_patches, results.failed_patches);
            
    return results;
}

// Get current budget values from runtime managers with validation
inline bool GetCurrentBudgets(MemoryBudgetConfig* config) {
    if (!config) return false;
    
    bool success = true;
    
    #define SAFE_READ_RVA(rva, dest) \
        do { \
            void* addr = __mb_RVA(rva); \
            if (addr) { \
                __try { \
                    (dest) = *(DWORD*)addr; \
                } __except(EXCEPTION_EXECUTE_HANDLER) { \
                    LOG_ERROR("Exception reading RVA 0x%08X", rva); \
                    success = false; \
                } \
            } else { \
                LOG_ERROR("Failed to resolve RVA 0x%08X", rva); \
                success = false; \
            } \
        } while(0)
    
    SAFE_READ_RVA(MANAGER_EXTERIOR_TEXTURE, config->exterior_texture);
    SAFE_READ_RVA(MANAGER_INTERIOR_GEOMETRY, config->interior_geometry);
    SAFE_READ_RVA(MANAGER_INTERIOR_TEXTURE, config->interior_texture);
    SAFE_READ_RVA(MANAGER_INTERIOR_WATER, config->interior_water);
    SAFE_READ_RVA(MANAGER_ACTOR_MEMORY, config->actor_memory);
    
    #undef SAFE_READ_RVA
    
    return success;
}
