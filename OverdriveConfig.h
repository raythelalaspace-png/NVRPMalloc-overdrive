#pragma once
#include <windows.h>
#include <stdint.h>

struct OverdriveConfig {
    // General
    bool useVanillaHeaps = false;
    int budgetPreset = 2; // aggressive preset by default for performance
    // Diagnostics
    bool detectCrossModuleMismatch = false;
    uint32_t stackTraceDepth = 12; // for mismatch logging

    // High VA arena
    bool enableArena = true;
    uint32_t arenaMB = 1024; // 1GB default
    bool topDownOnNonArena = true; // steer non-arena reserves top-down when effective LAA

    // Custom budgets (MB); if zero, ignored when preset != custom
    uint32_t exteriorTextureMB = 0;
    uint32_t interiorGeometryMB = 0;
    uint32_t interiorTextureMB = 0;
    uint32_t interiorWaterMB = 0;
    uint32_t actorMemoryMB = 0;

    // Performance
    float maxMsPerFrame = 16.67f;
    float maxTextureMB = 2048.0f;     // allow larger texture pool
    float maxGeometryMB = 1024.0f;    // more geometry headroom
    float maxParticleSystems = 500.0f; // higher effects ceiling
    bool relaxFrameLimits = true;
    bool disableAggressiveCulling = false; // keep culling for FPS

    // Dynamic scaling (budget cuts under load)
    bool dynamicBudgets = true;
    float targetMsPerFrame = 16.67f;  // aim ~60 fps
    float scaleDownAggressiveness = 0.20f; // faster cuts under load
    float scaleUpRate = 0.02f;        // slower recovery to avoid oscillation
    uint32_t adjustPeriodFrames = 30; // react ~2x per second
    // Minimum/maximum caps (MB)
    uint32_t minExteriorTextureMB = 128;
    uint32_t minInteriorTextureMB = 128;
    uint32_t minInteriorGeometryMB = 64;
    uint32_t minInteriorWaterMB = 32;
    uint32_t minActorMemoryMB = 32;
    uint32_t maxExteriorTextureMB = 4096;
    uint32_t maxInteriorTextureMB = 4096;
    uint32_t maxInteriorGeometryMB = 2048;
    uint32_t maxInteriorWaterMB = 1024;
    uint32_t maxActorMemoryMB = 1024;

    // VirtualFree hook
    bool vfDelayDecommit = true;
    bool vfPreventRelease = false;
    uint32_t vfDelayMs = 1000; // reduce churn but still responsive
    uint32_t vfMinKeepKB = 1024; // keep >=1MB committed to avoid re-commit stalls
    bool vfLog = false;
    uint32_t vfMaxKeptCommittedMB = 256; // quota for delayed kept committed
    uint32_t vfLowVATriggerMB = 64;      // flush if largest high-VA free < threshold

    // Hook coverage
    bool hookHeapAPI = true;            // Hook HeapAlloc/HeapReAlloc/HeapFree
    bool hookVirtualAlloc = true;       // Hook VirtualAlloc (reserve/commit tweaks)
    uint32_t heapHookThresholdKB = 128; // Route <= this (KB) to rpmalloc
    bool preferTopDownVA = false;       // Legacy: Add MEM_TOP_DOWN to VirtualAlloc (overridden by topDownOnNonArena when arena is enabled)
    bool hookChainExisting = true;      // If an import already hooked, chain through existing target
    char hookWhitelist[1024] = {0};     // CSV of module basenames to hook; blank = main exe only

    // Telemetry
    bool telemetryEnabled = true;
    uint32_t telemetryPeriodFrames = 300; // ~5s @60fps
    char telemetryFile[MAX_PATH] = "Data\\NVSE\\Plugins\\OverdriveMetrics.csv";

    // Allocation tuning
    uint32_t largeAllocThresholdMB = 8; // > threshold -> direct VirtualAlloc
};

bool LoadOverdriveConfig(OverdriveConfig& outCfg);
