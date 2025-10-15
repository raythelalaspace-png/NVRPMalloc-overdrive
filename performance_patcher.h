// performance_patcher.h - Performance Budget Constants Patcher
// Patches frame time limits, texture/geometry memory caps, and actor counts
#pragma once

#include <windows.h>
#include <stdint.h>

// Performance budget float constant addresses
#define PERF_MAX_MS_PER_FRAME_ADDR      0x0101F414  // fMaxMsUsagePerFrame (16.67ms = 60fps)
#define PERF_MAX_TEXTURE_MEMORY_ADDR    0x0101F418  // fMaxTotalTextureMemory (MB)
#define PERF_MAX_GEOMETRY_MEMORY_ADDR   0x0101F41C  // fMaxTotalGeometryMemory (MB)
#define PERF_MAX_PARTICLE_SYSTEMS_ADDR  0x0101F420  // fMaxParticleSystems

// Budget handler function addresses (for optional hooking)
#define PERF_CITY_LOD_HANDLER_ADDR      0x00500000  // City LOD budget handler
#define PERF_ACTOR_COUNT_HANDLER_ADDR   0x00500010  // Actor count budget handler

// Frame monitor callback addresses (for disabling)
#define PERF_FRAME_TIME_MONITOR_ADDR    0x00FC9010  // Frame time monitor
#define PERF_LOD_FRAME_ADJUSTER_ADDR    0x00FC9040  // LOD frame adjuster
#define PERF_TEXTURE_MONITOR_ADDR       0x00FC9070  // Texture memory monitor
#define PERF_GEOMETRY_MONITOR_ADDR      0x00FC90A0  // Geometry memory monitor
#define PERF_ACTOR_CULLER_ADDR          0x00FC90D0  // Actor frame culler
#define PERF_PARTICLE_MANAGER_ADDR      0x00FC9100  // Particle system manager

// Performance configuration
struct PerformanceConfig {
    // Frame timing
    float max_ms_per_frame;        // Max milliseconds per frame (default: 16.67 for 60fps)
    
    // Memory budgets
    float max_texture_memory_mb;   // Max texture memory in MB (default: varies)
    float max_geometry_memory_mb;  // Max geometry memory in MB (default: varies)
    
    // Object counts
    float max_particle_systems;    // Max particle systems (default: varies)
    
    // Performance relaxation
    bool relax_frame_limits;       // Allow frames to take longer if needed
    bool disable_aggressive_culling; // Disable aggressive LOD/actor culling
};

// Performance presets
enum PerformancePreset {
    PERF_VANILLA = 0,       // Original game limits
    PERF_RELAXED = 1,       // 2x limits, relaxed culling
    PERF_HIGH_END = 2,      // 4x limits, minimal culling
    PERF_UNLIMITED = 3      // 10x+ limits, culling disabled
};

// Get preset performance configuration
PerformanceConfig GetPerformancePreset(PerformancePreset preset);

// Apply performance configuration (patches float constants)
bool ApplyPerformancePatches(const PerformanceConfig* config);

// Disable aggressive budget enforcement callbacks
bool DisableAggressiveCulling();

// Re-enable budget enforcement
bool EnableAggressiveCulling();
