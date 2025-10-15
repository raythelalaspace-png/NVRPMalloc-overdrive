// Fallout New Vegas Object Budget Caps System
// Complete analysis of all 12 budget cap systems
// Addresses and values for runtime patching

#pragma once
#include <windows.h>

// ============================================================================
// OBJECT BUDGET CAP ADDRESSES
// ============================================================================

// Budget initialization addresses (push immediate values)
#define BUDGET_TRIANGLES_ADDR           0x00FC8DB5  // push 186A0h (100,000)
#define BUDGET_PARTICLES_ADDR           0x00FC8D85  // push 1388h (5,000)
#define BUDGET_HAVOK_TRIANGLES_ADDR     0x00FC8E35  // push 1388h (5,000)
#define BUDGET_DECALS_ADDR              0x00FC8E25  // push 1F4h (500)
#define BUDGET_GEOMETRY_ADDR            0x00FC8DA5  // push 3E8h (1,000)
#define BUDGET_GENERAL_REFS_ADDR        0x00FC8D45  // push 2BCh (700)
#define BUDGET_ACTIVE_REFS_ADDR         0x00FC8D65  // push 64h (100)
#define BUDGET_EMITTERS_ADDR            0x00FC8D95  // push 32h (50)
#define BUDGET_ANIMATED_OBJECTS_ADDR    0x00FC8D75  // push 32h (50)
#define BUDGET_ACTOR_REFS_ADDR          0x00FC8D55  // push 14h (20)
#define BUDGET_WATER_SYSTEMS_ADDR       0x00FC8DC5  // push 0Ah (10)
#define BUDGET_LIGHT_SYSTEMS_ADDR       0x00FC8E05  // push 0Ah (10)

// Management object addresses
#define MANAGER_TRIANGLES               0x011C59EC
#define MANAGER_PARTICLES               0x011C5B3C
#define MANAGER_HAVOK_TRIANGLES         0x011C59A0
#define MANAGER_DECALS                  0x011C5BDC
#define MANAGER_GEOMETRY                0x011C5A58
#define MANAGER_GENERAL_REFS            0x011C5A1C
#define MANAGER_ACTIVE_REFS             0x011C5AF0
#define MANAGER_EMITTERS                0x011C5A64
#define MANAGER_ANIMATED_OBJECTS        0x011C5C24
#define MANAGER_ACTOR_REFS              0x011C5A40
#define MANAGER_WATER_SYSTEMS           0x011C5A10
#define MANAGER_LIGHT_SYSTEMS           0x011C5B80

// ============================================================================
// DEFAULT VALUES (Original Game Limits)
// ============================================================================

#define DEFAULT_TRIANGLES               100000
#define DEFAULT_PARTICLES               5000
#define DEFAULT_HAVOK_TRIANGLES         5000
#define DEFAULT_DECALS                  500
#define DEFAULT_GEOMETRY                1000
#define DEFAULT_GENERAL_REFS            700
#define DEFAULT_ACTIVE_REFS             100
#define DEFAULT_EMITTERS                50
#define DEFAULT_ANIMATED_OBJECTS        50
#define DEFAULT_ACTOR_REFS              20
#define DEFAULT_WATER_SYSTEMS           10
#define DEFAULT_LIGHT_SYSTEMS           10

// ============================================================================
// ENHANCED VALUES (For Modern Hardware - Conservative)
// ============================================================================

#define ENHANCED_TRIANGLES              500000    // 5x increase
#define ENHANCED_PARTICLES              25000     // 5x increase
#define ENHANCED_HAVOK_TRIANGLES        25000     // 5x increase
#define ENHANCED_DECALS                 2500      // 5x increase
#define ENHANCED_GEOMETRY               5000      // 5x increase
#define ENHANCED_GENERAL_REFS           3500      // 5x increase
#define ENHANCED_ACTIVE_REFS            500       // 5x increase
#define ENHANCED_EMITTERS               250       // 5x increase
#define ENHANCED_ANIMATED_OBJECTS       250       // 5x increase
#define ENHANCED_ACTOR_REFS             100       // 5x increase
#define ENHANCED_WATER_SYSTEMS          50        // 5x increase
#define ENHANCED_LIGHT_SYSTEMS          50        // 5x increase

// ============================================================================
// EXTREME VALUES (For High-End Systems - Aggressive)
// ============================================================================

#define EXTREME_TRIANGLES               2000000   // 20x increase
#define EXTREME_PARTICLES               100000    // 20x increase
#define EXTREME_HAVOK_TRIANGLES         100000    // 20x increase
#define EXTREME_DECALS                  10000     // 20x increase
#define EXTREME_GEOMETRY                20000     // 20x increase
#define EXTREME_GENERAL_REFS            14000     // 20x increase
#define EXTREME_ACTIVE_REFS             2000      // 20x increase
#define EXTREME_EMITTERS                1000      // 20x increase
#define EXTREME_ANIMATED_OBJECTS        1000      // 20x increase
#define EXTREME_ACTOR_REFS              400       // 20x increase
#define EXTREME_WATER_SYSTEMS           200       // 20x increase
#define EXTREME_LIGHT_SYSTEMS           200       // 20x increase

// ============================================================================
// CONFIGURATION STRUCTURE
// ============================================================================

typedef struct ObjectBudgetConfig {
    DWORD triangles;
    DWORD particles;
    DWORD havok_triangles;
    DWORD decals;
    DWORD geometry;
    DWORD general_refs;
    DWORD active_refs;
    DWORD emitters;
    DWORD animated_objects;
    DWORD actor_refs;
    DWORD water_systems;
    DWORD light_systems;
} ObjectBudgetConfig;

// ============================================================================
// PRESET CONFIGURATIONS
// ============================================================================

enum ObjectBudgetPresetType {
    OBJ_PRESET_DEFAULT = 0,    // Original game values
    OBJ_PRESET_ENHANCED = 1,   // Conservative 5x increase
    OBJ_PRESET_EXTREME = 2,    // Aggressive 20x increase
    OBJ_PRESET_CUSTOM = 3      // User-defined values
};

// Get preset configuration
inline ObjectBudgetConfig GetObjectBudgetPreset(ObjectBudgetPresetType preset) {
    ObjectBudgetConfig config = {0};
    
    switch (preset) {
        case OBJ_PRESET_ENHANCED:
            config.triangles = ENHANCED_TRIANGLES;
            config.particles = ENHANCED_PARTICLES;
            config.havok_triangles = ENHANCED_HAVOK_TRIANGLES;
            config.decals = ENHANCED_DECALS;
            config.geometry = ENHANCED_GEOMETRY;
            config.general_refs = ENHANCED_GENERAL_REFS;
            config.active_refs = ENHANCED_ACTIVE_REFS;
            config.emitters = ENHANCED_EMITTERS;
            config.animated_objects = ENHANCED_ANIMATED_OBJECTS;
            config.actor_refs = ENHANCED_ACTOR_REFS;
            config.water_systems = ENHANCED_WATER_SYSTEMS;
            config.light_systems = ENHANCED_LIGHT_SYSTEMS;
            break;
            
        case OBJ_PRESET_EXTREME:
            config.triangles = EXTREME_TRIANGLES;
            config.particles = EXTREME_PARTICLES;
            config.havok_triangles = EXTREME_HAVOK_TRIANGLES;
            config.decals = EXTREME_DECALS;
            config.geometry = EXTREME_GEOMETRY;
            config.general_refs = EXTREME_GENERAL_REFS;
            config.active_refs = EXTREME_ACTIVE_REFS;
            config.emitters = EXTREME_EMITTERS;
            config.animated_objects = EXTREME_ANIMATED_OBJECTS;
            config.actor_refs = EXTREME_ACTOR_REFS;
            config.water_systems = EXTREME_WATER_SYSTEMS;
            config.light_systems = EXTREME_LIGHT_SYSTEMS;
            break;
            
        case OBJ_PRESET_DEFAULT:
        default:
            config.triangles = DEFAULT_TRIANGLES;
            config.particles = DEFAULT_PARTICLES;
            config.havok_triangles = DEFAULT_HAVOK_TRIANGLES;
            config.decals = DEFAULT_DECALS;
            config.geometry = DEFAULT_GEOMETRY;
            config.general_refs = DEFAULT_GENERAL_REFS;
            config.active_refs = DEFAULT_ACTIVE_REFS;
            config.emitters = DEFAULT_EMITTERS;
            config.animated_objects = DEFAULT_ANIMATED_OBJECTS;
            config.actor_refs = DEFAULT_ACTOR_REFS;
            config.water_systems = DEFAULT_WATER_SYSTEMS;
            config.light_systems = DEFAULT_LIGHT_SYSTEMS;
            break;
    }
    
    return config;
}

// Patch a single budget value
inline bool PatchObjectBudget(DWORD address, DWORD newValue) {
    DWORD oldProtect;
    
    // Make memory writable
    if (!VirtualProtect((void*)address, 4, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    // Write new value (the PUSH immediate operand, 1 byte after PUSH opcode)
    *(DWORD*)(address + 1) = newValue;
    
    // Restore protection
    VirtualProtect((void*)address, 4, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), (void*)address, 4);
    
    return true;
}

// Apply complete budget configuration
inline bool ApplyObjectBudgetConfig(const ObjectBudgetConfig* config) {
    bool success = true;
    
    success &= PatchObjectBudget(BUDGET_TRIANGLES_ADDR, config->triangles);
    success &= PatchObjectBudget(BUDGET_PARTICLES_ADDR, config->particles);
    success &= PatchObjectBudget(BUDGET_HAVOK_TRIANGLES_ADDR, config->havok_triangles);
    success &= PatchObjectBudget(BUDGET_DECALS_ADDR, config->decals);
    success &= PatchObjectBudget(BUDGET_GEOMETRY_ADDR, config->geometry);
    success &= PatchObjectBudget(BUDGET_GENERAL_REFS_ADDR, config->general_refs);
    success &= PatchObjectBudget(BUDGET_ACTIVE_REFS_ADDR, config->active_refs);
    success &= PatchObjectBudget(BUDGET_EMITTERS_ADDR, config->emitters);
    success &= PatchObjectBudget(BUDGET_ANIMATED_OBJECTS_ADDR, config->animated_objects);
    success &= PatchObjectBudget(BUDGET_ACTOR_REFS_ADDR, config->actor_refs);
    success &= PatchObjectBudget(BUDGET_WATER_SYSTEMS_ADDR, config->water_systems);
    success &= PatchObjectBudget(BUDGET_LIGHT_SYSTEMS_ADDR, config->light_systems);
    
    return success;
}
