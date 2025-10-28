#include "OverdriveConfig.h"
#include <windows.h>
#include <shlwapi.h>
#include <cstdio>

static void ReadString(const char* path, const char* section, const char* key, const char* def, char* out, DWORD outSize) {
    GetPrivateProfileStringA(section, key, def, out, outSize, path);
}
static int ReadInt(const char* path, const char* section, const char* key, int def) {
    return GetPrivateProfileIntA(section, key, def, path);
}
static float ReadFloat(const char* path, const char* section, const char* key, float def) {
    char buf[64];
    GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), path);
    if (!buf[0]) return def;
    return (float)atof(buf);
}

bool LoadOverdriveConfig(OverdriveConfig& c) {
    char iniPath[MAX_PATH] = {0};
    // Default INI under game dir/Data/NVSE/Plugins
    GetModuleFileNameA(NULL, iniPath, MAX_PATH);
    char* slash = strrchr(iniPath, '\\');
    if (slash) *slash = '\0';
    strcat_s(iniPath, "\\Data\\NVSE\\Plugins\\RPNVSEOverdrive.ini");

    // Ensure directories exist
    CreateDirectoryA("Data", NULL);
    CreateDirectoryA("Data\\NVSE", NULL);
    CreateDirectoryA("Data\\NVSE\\Plugins", NULL);

    // General
    c.useVanillaHeaps = ReadInt(iniPath, "General", "bUseVanillaHeaps", c.useVanillaHeaps ? 1 : 0) != 0;
    c.budgetPreset = ReadInt(iniPath, "General", "iBudgetPreset", c.budgetPreset);
    c.detectCrossModuleMismatch = ReadInt(iniPath, "General", "bDetectCrossModuleMismatch", c.detectCrossModuleMismatch ? 1 : 0) != 0;
    c.stackTraceDepth = (uint32_t)ReadInt(iniPath, "General", "iStackTraceDepth", (int)c.stackTraceDepth);

    // Address space / arena
    c.enableArena = ReadInt(iniPath, "AddressSpace", "bEnableArena", c.enableArena ? 1 : 0) != 0;
    c.arenaMB = (uint32_t)ReadInt(iniPath, "AddressSpace", "iArenaMB", (int)c.arenaMB);
    c.topDownOnNonArena = ReadInt(iniPath, "AddressSpace", "bTopDownOnNonArena", c.topDownOnNonArena ? 1 : 0) != 0;

    // Custom budgets (MB)
    c.exteriorTextureMB = ReadInt(iniPath, "Budgets", "ExteriorTextureMB", c.exteriorTextureMB);
    c.interiorGeometryMB = ReadInt(iniPath, "Budgets", "InteriorGeometryMB", c.interiorGeometryMB);
    c.interiorTextureMB = ReadInt(iniPath, "Budgets", "InteriorTextureMB", c.interiorTextureMB);
    c.interiorWaterMB   = ReadInt(iniPath, "Budgets", "InteriorWaterMB", c.interiorWaterMB);
    c.actorMemoryMB     = ReadInt(iniPath, "Budgets", "ActorMemoryMB", c.actorMemoryMB);

    // Performance
    c.maxMsPerFrame = ReadFloat(iniPath, "Performance", "MaxMsPerFrame", c.maxMsPerFrame);
    c.maxTextureMB = ReadFloat(iniPath, "Performance", "MaxTextureMemoryMB", c.maxTextureMB);
    c.maxGeometryMB = ReadFloat(iniPath, "Performance", "MaxGeometryMemoryMB", c.maxGeometryMB);
    c.maxParticleSystems = ReadFloat(iniPath, "Performance", "MaxParticleSystems", c.maxParticleSystems);
    c.relaxFrameLimits = ReadInt(iniPath, "Performance", "bRelaxFrameLimits", c.relaxFrameLimits ? 1 : 0) != 0;
    c.disableAggressiveCulling = ReadInt(iniPath, "Performance", "bDisableAggressiveCulling", c.disableAggressiveCulling ? 1 : 0) != 0;

    // Dynamic budgets
    c.dynamicBudgets = ReadInt(iniPath, "DynamicBudgets", "bEnabled", c.dynamicBudgets ? 1 : 0) != 0;
    c.targetMsPerFrame = ReadFloat(iniPath, "DynamicBudgets", "TargetMsPerFrame", c.targetMsPerFrame);
    c.scaleDownAggressiveness = ReadFloat(iniPath, "DynamicBudgets", "ScaleDownAggressiveness", c.scaleDownAggressiveness);
    c.scaleUpRate = ReadFloat(iniPath, "DynamicBudgets", "ScaleUpRate", c.scaleUpRate);
    c.adjustPeriodFrames = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "AdjustPeriodFrames", (int)c.adjustPeriodFrames);
    c.minExteriorTextureMB = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MinExteriorTextureMB", (int)c.minExteriorTextureMB);
    c.minInteriorTextureMB = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MinInteriorTextureMB", (int)c.minInteriorTextureMB);
    c.minInteriorGeometryMB = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MinInteriorGeometryMB", (int)c.minInteriorGeometryMB);
    c.minInteriorWaterMB   = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MinInteriorWaterMB", (int)c.minInteriorWaterMB);
    c.minActorMemoryMB     = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MinActorMemoryMB", (int)c.minActorMemoryMB);
    c.maxExteriorTextureMB = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MaxExteriorTextureMB", (int)c.maxExteriorTextureMB);
    c.maxInteriorTextureMB = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MaxInteriorTextureMB", (int)c.maxInteriorTextureMB);
    c.maxInteriorGeometryMB = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MaxInteriorGeometryMB", (int)c.maxInteriorGeometryMB);
    c.maxInteriorWaterMB   = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MaxInteriorWaterMB", (int)c.maxInteriorWaterMB);
    c.maxActorMemoryMB     = (uint32_t)ReadInt(iniPath, "DynamicBudgets", "MaxActorMemoryMB", (int)c.maxActorMemoryMB);

    // VirtualFree
    c.vfDelayDecommit = ReadInt(iniPath, "VirtualFree", "bDelayDecommit", c.vfDelayDecommit ? 1 : 0) != 0;
    c.vfPreventRelease = ReadInt(iniPath, "VirtualFree", "bPreventRelease", c.vfPreventRelease ? 1 : 0) != 0;
    c.vfDelayMs = (uint32_t)ReadInt(iniPath, "VirtualFree", "iDelayMs", (int)c.vfDelayMs);
    c.vfMinKeepKB = (uint32_t)ReadInt(iniPath, "VirtualFree", "iMinKeepKB", (int)c.vfMinKeepKB);
    c.vfLog = ReadInt(iniPath, "VirtualFree", "bLog", c.vfLog ? 1 : 0) != 0;
    c.vfMaxKeptCommittedMB = (uint32_t)ReadInt(iniPath, "VirtualFree", "MaxKeptCommittedMB", (int)c.vfMaxKeptCommittedMB);
    c.vfLowVATriggerMB = (uint32_t)ReadInt(iniPath, "VirtualFree", "LowVATriggerMB", (int)c.vfLowVATriggerMB);

    // Hooks
    c.hookHeapAPI = ReadInt(iniPath, "Hooks", "bHookHeapAPI", c.hookHeapAPI ? 1 : 0) != 0;
    c.hookVirtualAlloc = ReadInt(iniPath, "Hooks", "bHookVirtualAlloc", c.hookVirtualAlloc ? 1 : 0) != 0;
    c.heapHookThresholdKB = (uint32_t)ReadInt(iniPath, "Hooks", "iHeapHookThresholdKB", (int)c.heapHookThresholdKB);
    c.preferTopDownVA = ReadInt(iniPath, "Hooks", "bPreferTopDownVA", c.preferTopDownVA ? 1 : 0) != 0;
    c.hookChainExisting = ReadInt(iniPath, "Hooks", "bHookChainExisting", c.hookChainExisting ? 1 : 0) != 0;
    ReadString(iniPath, "Hooks", "sHookWhitelist", "", c.hookWhitelist, (DWORD)sizeof(c.hookWhitelist));
    c.largeAllocThresholdMB = (uint32_t)ReadInt(iniPath, "Hooks", "LargeAllocThresholdMB", (int)c.largeAllocThresholdMB);

    // Telemetry
    c.telemetryEnabled = ReadInt(iniPath, "Telemetry", "bEnabled", c.telemetryEnabled ? 1 : 0) != 0;
    c.telemetryPeriodFrames = (uint32_t)ReadInt(iniPath, "Telemetry", "iPeriodFrames", (int)c.telemetryPeriodFrames);
    ReadString(iniPath, "Telemetry", "sOutput", c.telemetryFile, c.telemetryFile, (DWORD)sizeof(c.telemetryFile));

    return true;
}