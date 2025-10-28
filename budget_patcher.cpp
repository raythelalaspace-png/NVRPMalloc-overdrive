// budget_patcher.cpp - Object Budget Patcher Implementation
#include "budget_patcher.h"
#include <stdio.h>

// Game base address (FalloutNV.exe usually loads at 0x00400000)
static HMODULE GetGameBase() {
    return GetModuleHandleA(NULL);
}

#include "AddressDiscovery.h"
// Calculate absolute address from RVA (pattern-aware)
static void* RVA(uint32_t offset) {
    return AddrDisc::ResolveRVA(offset);
}

ObjectBudgetConfig GetObjectBudgetPreset(int preset) {
    ObjectBudgetConfig config = {};
    
    switch (preset) {
        case 0: // PRESET_DEFAULT
            config.triangles = 100000;
            config.particles = 5000;
            config.havok_triangles = 5000;
            config.decals = 500;
            config.geometry = 1000;
            config.general_refs = 700;
            config.active_refs = 100;
            config.emitters = 50;
            config.animated_objects = 50;
            config.actor_refs = 20;
            config.water_systems = 10;
            config.light_systems = 10;
            break;
            
        case 1: // PRESET_RECOMMENDED
            config.triangles = 400000;      // 4x
            config.particles = 20000;       // 4x
            config.havok_triangles = 20000; // 4x
            config.decals = 2000;           // 4x
            config.geometry = 4000;         // 4x
            config.general_refs = 2800;     // 4x
            config.active_refs = 400;       // 4x
            config.emitters = 200;          // 4x
            config.animated_objects = 200;  // 4x
            config.actor_refs = 80;         // 4x
            config.water_systems = 40;      // 4x
            config.light_systems = 40;      // 4x
            break;
            
        case 2: // PRESET_AGGRESSIVE
            config.triangles = 800000;      // 8x (increased from 6x)
            config.particles = 40000;       // 8x
            config.havok_triangles = 40000; // 8x
            config.decals = 4000;           // 8x
            config.geometry = 8000;         // 8x
            config.general_refs = 5600;     // 8x
            config.active_refs = 800;       // 8x
            config.emitters = 400;          // 8x
            config.animated_objects = 400;  // 8x
            config.actor_refs = 160;        // 8x
            config.water_systems = 80;      // 8x
            config.light_systems = 80;      // 8x
            break;
            
        case 3: // PRESET_ULTRA
            config.triangles = 1000000;     // 10x
            config.particles = 50000;       // 10x
            config.havok_triangles = 50000; // 10x
            config.decals = 5000;           // 10x
            config.geometry = 10000;        // 10x
            config.general_refs = 7000;     // 10x
            config.active_refs = 1000;      // 10x
            config.emitters = 500;          // 10x
            config.animated_objects = 500;  // 10x
            config.actor_refs = 200;        // 10x
            config.water_systems = 100;     // 10x
            config.light_systems = 100;     // 10x
            break;
            
        case 4: // PRESET_EXTREME
            config.triangles = 1500000;     // 15x - EXTREME triangle count
            config.particles = 75000;       // 15x - EXTREME particle count
            config.havok_triangles = 75000; // 15x
            config.decals = 7500;           // 15x
            config.geometry = 15000;        // 15x
            config.general_refs = 10500;    // 15x
            config.active_refs = 1500;      // 15x
            config.emitters = 750;          // 15x
            config.animated_objects = 750;  // 15x
            config.actor_refs = 300;        // 15x
            config.water_systems = 150;     // 15x
            config.light_systems = 150;     // 15x
            break;
    }
    
    return config;
}

static bool PatchMemoryValue(void* address, uint32_t new_value) {
    if (!address) return false;
    
    DWORD oldProtect;
    if (!VirtualProtect(address, sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    
    *(uint32_t*)address = new_value;
    
    VirtualProtect(address, sizeof(uint32_t), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), address, sizeof(uint32_t));
    
    return true;
}

bool ApplyObjectBudgetPatches(const ObjectBudgetConfig* config) {
    if (!config) return false;
    
    bool success = true;
    
    // OBJECT BUDGET PATCHES (runtime manager data)
    // These patch the manager objects that hold the limits
    // Structure: first DWORD is the budget cap value
    
    success &= PatchMemoryValue(RVA(OBJ_TRIANGLES_ADDR), config->triangles);
    success &= PatchMemoryValue(RVA(OBJ_PARTICLES_ADDR), config->particles);
    success &= PatchMemoryValue(RVA(OBJ_HAVOK_TRIANGLES_ADDR), config->havok_triangles);
    success &= PatchMemoryValue(RVA(OBJ_DECALS_ADDR), config->decals);
    success &= PatchMemoryValue(RVA(OBJ_GEOMETRY_ADDR), config->geometry);
    success &= PatchMemoryValue(RVA(OBJ_GENERAL_REFS_ADDR), config->general_refs);
    success &= PatchMemoryValue(RVA(OBJ_ACTIVE_REFS_ADDR), config->active_refs);
    success &= PatchMemoryValue(RVA(OBJ_EMITTERS_ADDR), config->emitters);
    success &= PatchMemoryValue(RVA(OBJ_ANIMATED_OBJECTS_ADDR), config->animated_objects);
    success &= PatchMemoryValue(RVA(OBJ_ACTOR_REFS_ADDR), config->actor_refs);
    success &= PatchMemoryValue(RVA(OBJ_WATER_SYSTEMS_ADDR), config->water_systems);
    success &= PatchMemoryValue(RVA(OBJ_LIGHT_SYSTEMS_ADDR), config->light_systems);
    
    return success;
}
