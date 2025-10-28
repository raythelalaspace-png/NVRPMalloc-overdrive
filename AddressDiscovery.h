#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <stddef.h>
#include <mutex>
#include <unordered_map>
#include <string>

// Address discovery API: pattern-scans module sections to resolve addresses robustly.
// Falls back to RVA -> base+offset if pattern not found.
namespace AddrDisc {
    struct Pattern {
        const uint8_t* bytes;   // pattern bytes
        const char*     mask;    // mask string: 'x' = match, '?' = wildcard
        const char*     section; // optional PE section hint (".text", ".rdata", etc.), nullable
        size_t          length;  // pattern length
        const char*     export_hint; // optional exported function name to search near as a secondary strategy
    };

    // Register a pattern for a fallback RVA. Thread-safe; can be called multiple times.
    void Register(uint32_t fallbackRVA, const Pattern& pat, const char* keyName);

    // Resolve an address: tries pattern(s) -> export-adjacent scan -> fallback to base + rva.
    void* ResolveRVA(uint32_t fallbackRVA);

    // Utility: direct pattern find across module or single section.
    void* FindPattern(const uint8_t* pat, const char* mask, const char* section /*nullable*/);

    // Helpers for immediate scans commonly used by budget constants in code: push imm32 (0x68 imm32)
    void* FindPushImm32(uint32_t imm, const char* section /*nullable*/);

    // Validation before patching
    bool ValidateDWORD(void* addr, uint32_t expected, uint32_t tolerance /*abs*/);
    bool ValidateFloat(void* addr, float expected, float relTolerance /*e.g., 0.05f = 5%*/);

    // Base and section helpers
    HMODULE ModuleBase();
    bool GetSection(const char* name, uint8_t** start, size_t* size);
}
