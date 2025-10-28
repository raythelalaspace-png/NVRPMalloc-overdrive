#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <algorithm>
#include "AddressDiscovery.h"

namespace AddrDisc {
    static std::mutex g_mtx;
    struct Entry { Pattern pat; std::string key; void* cached = nullptr; };
    static std::unordered_map<uint32_t, Entry> g_map; // by fallback RVA

    static HMODULE s_mod = nullptr;

    HMODULE ModuleBase() {
        if (!s_mod) s_mod = GetModuleHandleA(NULL);
        return s_mod;
    }

    static IMAGE_NT_HEADERS* NtHeaders(HMODULE mod) {
        auto dos = (IMAGE_DOS_HEADER*)mod;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
        auto nt = (IMAGE_NT_HEADERS*)((uint8_t*)mod + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
        return nt;
    }

    bool GetSection(const char* name, uint8_t** start, size_t* size) {
        *start = nullptr; *size = 0;
        HMODULE mod = ModuleBase();
        auto nt = NtHeaders(mod);
        if (!nt) return false;
        auto sec = (IMAGE_SECTION_HEADER*)((uint8_t*)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            char sname[9] = {0}; memcpy(sname, sec[i].Name, 8);
            if (_stricmp(sname, name) == 0) {
                *start = (uint8_t*)mod + sec[i].VirtualAddress;
                *size  = (size_t)sec[i].Misc.VirtualSize;
                return true;
            }
        }
        return false;
    }

    static bool MatchAt(const uint8_t* base, const uint8_t* pat, const char* mask, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            if (mask[i] == 'x' && base[i] != pat[i]) return false;
        }
        return true;
    }

    static void* ScanRange(uint8_t* start, size_t size, const uint8_t* pat, const char* mask, size_t len) {
        if (!start || !size || !pat || !mask || !len) return nullptr;
        size_t last = (size >= len) ? (size - len) : 0;
        for (size_t i = 0; i <= last; ++i) {
            if (MatchAt(start + i, pat, mask, len)) return start + i;
        }
        return nullptr;
    }

    void* FindPattern(const uint8_t* pat, const char* mask, const char* section) {
        if (!pat || !mask) return nullptr;
        size_t len = strlen(mask);
        if (section && section[0]) {
            uint8_t* s; size_t n;
            if (GetSection(section, &s, &n)) return ScanRange(s, n, pat, mask, len);
            return nullptr;
        }
        // Scan .text then .rdata then .data as a fallback
        const char* secs[] = { ".text", ".rdata", ".data" };
        for (auto sn : secs) {
            uint8_t* s; size_t n; if (GetSection(sn, &s, &n)) {
                void* p = ScanRange(s, n, pat, mask, len);
                if (p) return p;
            }
        }
        return nullptr;
    }

    void* FindPushImm32(uint32_t imm, const char* section) {
        uint8_t pat[5]; pat[0] = 0x68; memcpy(&pat[1], &imm, 4);
        const char* mask = "x????";
        return FindPattern(pat, mask, section);
    }

    static void* SearchNearExport(const char* exportName, const uint8_t* pat, const char* mask, size_t len, size_t window) {
        if (!exportName) return nullptr;
        HMODULE mod = ModuleBase();
        HMODULE mods[1024]; DWORD needed=0;
        // Get export address directly
        FARPROC exp = GetProcAddress(mod, exportName);
        if (!exp) return nullptr;
        uint8_t* base = (uint8_t*)exp;
        uint8_t* begin = (base > (uint8_t*)window) ? base - window : (uint8_t*)mod;
        uint8_t* end   = base + window;
        // Clamp to module sections by using .text as primary
        uint8_t* secStart; size_t secSize;
        if (GetSection(".text", &secStart, &secSize)) {
            uint8_t* secEnd = secStart + secSize;
            if (begin < secStart) begin = secStart;
            if (end   > secEnd)   end   = secEnd;
        }
        return ScanRange(begin, (size_t)(end - begin), pat, mask, len);
    }

    void Register(uint32_t fallbackRVA, const Pattern& pat, const char* keyName) {
        std::lock_guard<std::mutex> g(g_mtx);
        auto& e = g_map[fallbackRVA];
        e.pat = pat;
        if (keyName) e.key = keyName; else e.key.clear();
    }

    void* ResolveRVA(uint32_t fallbackRVA) {
        {
            std::lock_guard<std::mutex> g(g_mtx);
            auto it = g_map.find(fallbackRVA);
            if (it != g_map.end() && it->second.cached)
                return it->second.cached;
        }
        // Try registered pattern if present
        void* found = nullptr;
        Pattern pat{};
        std::string key;
        {
            std::lock_guard<std::mutex> g(g_mtx);
            auto it = g_map.find(fallbackRVA);
            if (it != g_map.end()) { pat = it->second.pat; key = it->second.key; }
        }
        if (pat.bytes && pat.mask && pat.length) {
            found = FindPattern(pat.bytes, pat.mask, pat.section);
            if (!found && pat.export_hint) {
                found = SearchNearExport(pat.export_hint, pat.bytes, pat.mask, pat.length, 64 * 1024);
            }
        }
        if (!found) {
            // Secondary heuristic for budget constants: scan for push imm of default value if mask was null
            // This path expects mask==nullptr and bytes holds imm32
            if (!pat.mask && pat.bytes && pat.length == 4) {
                uint32_t imm; memcpy(&imm, pat.bytes, 4);
                found = FindPushImm32(imm, pat.section);
            }
        }
        if (!found) {
            // Fallback to RVA
            found = (void*)((uintptr_t)ModuleBase() + fallbackRVA);
        }
        {
            std::lock_guard<std::mutex> g(g_mtx);
            g_map[fallbackRVA].cached = found;
        }
        return found;
    }

    bool ValidateDWORD(void* addr, uint32_t expected, uint32_t tolerance) {
        if (!addr) return false;
        __try {
            uint32_t v = *(volatile uint32_t*)addr;
            if (tolerance == 0) return v == expected;
            uint32_t lo = (expected > tolerance) ? (expected - tolerance) : 0u;
            uint32_t hi = expected + tolerance;
            return v >= lo && v <= hi;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    bool ValidateFloat(void* addr, float expected, float relTolerance) {
        if (!addr) return false;
        if (relTolerance < 0.0f) relTolerance = 0.0f;
        __try {
            float v = *(volatile float*)addr;
            float delta = (v > expected) ? (v - expected) : (expected - v);
            float tol = expected * relTolerance;
            return delta <= tol;
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
}

// Example registration of known addresses (optional). Users can extend this at runtime.
// We register defaults for budget constants using push imm32 heuristics, and leave manager RVAs to fallback.
static void AddrDisc_RegisterDefaults() {
    using namespace AddrDisc;
    // Texture budgets defaults (imm32) — pattern uses push imm32 in .text
    // The Pattern here sets bytes=imm32, mask=null to trigger FindPushImm32 path.
    const uint8_t ext_tex_imm[4] = { 0x00, 0x00, 0x40, 0x01 }; // 0x01400000 (20MB)
    Register(0x00F3DE43u, Pattern{ ext_tex_imm, nullptr, ".text", 4, nullptr }, "BUDGET_EXTERIOR_TEXTURE");

    const uint8_t int_geo_imm[4] = { 0x00, 0x00, 0xA0, 0x00 }; // 0x00A00000 (10MB)
    Register(0x00F3E113u, Pattern{ int_geo_imm, nullptr, ".text", 4, nullptr }, "BUDGET_INTERIOR_GEOMETRY");

    const uint8_t int_tex_imm[4] = { 0x00, 0x00, 0x40, 0x06 }; // 0x06400000 (100MB)
    Register(0x00F3E143u, Pattern{ int_tex_imm, nullptr, ".text", 4, nullptr }, "BUDGET_INTERIOR_TEXTURE");

    const uint8_t int_wat_imm[4] = { 0x00, 0x00, 0xA0, 0x00 }; // 10MB
    Register(0x00F3E173u, Pattern{ int_wat_imm, nullptr, ".text", 4, nullptr }, "BUDGET_INTERIOR_WATER");

    const uint8_t actor_mem_imm[4] = { 0x00, 0x00, 0xA0, 0x00 }; // 10MB
    Register(0x00F3E593u, Pattern{ actor_mem_imm, nullptr, ".text", 4, nullptr }, "BUDGET_ACTOR_MEMORY");
}

struct AddrDisc_AutoInit { AddrDisc_AutoInit(){ AddrDisc_RegisterDefaults(); } } g_addrdisc_autoinit;
