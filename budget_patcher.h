// budget_patcher.h - Object Budget Patcher for Fallout New Vegas
// Patches object count budgets (triangles, particles, actors, etc.)
#pragma once

#include <windows.h>
#include <stdint.h>

// Object Budget Cap Manager Addresses (runtime data)
#define OBJ_TRIANGLES_ADDR        0x011C59EC  // 100,000
#define OBJ_PARTICLES_ADDR        0x011C5B3C  // 5,000
#define OBJ_HAVOK_TRIANGLES_ADDR  0x011C59A0  // 5,000
#define OBJ_DECALS_ADDR           0x011C5BDC  // 500
#define OBJ_GEOMETRY_ADDR         0x011C5A58  // 1,000
#define OBJ_GENERAL_REFS_ADDR     0x011C5A1C  // 700
#define OBJ_ACTIVE_REFS_ADDR      0x011C5AF0  // 100
#define OBJ_EMITTERS_ADDR         0x011C5A64  // 50
#define OBJ_ANIMATED_OBJECTS_ADDR 0x011C5C24  // 50
#define OBJ_ACTOR_REFS_ADDR       0x011C5A40  // 20
#define OBJ_WATER_SYSTEMS_ADDR    0x011C5A10  // 10
#define OBJ_LIGHT_SYSTEMS_ADDR    0x011C5B80  // 10

// Object budget configuration (counts, not bytes)
struct ObjectBudgetConfig {
    uint32_t triangles;
    uint32_t particles;
    uint32_t havok_triangles;
    uint32_t decals;
    uint32_t geometry;
    uint32_t general_refs;
    uint32_t active_refs;
    uint32_t emitters;
    uint32_t animated_objects;
    uint32_t actor_refs;
    uint32_t water_systems;
    uint32_t light_systems;
};

// Get preset object budget configuration (compatible with memory_budgets.h BudgetPreset)
ObjectBudgetConfig GetObjectBudgetPreset(int preset);

// Apply object budget configuration (patches game runtime managers)
bool ApplyObjectBudgetPatches(const ObjectBudgetConfig* config);
