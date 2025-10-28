#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <mutex>
#include <vector>
#include <map>

static_assert(sizeof(void*) == 4, "This code targets 32-bit processes.");

// Simple arena config fed from OverdriveConfig
struct HighVAOptions {
    bool   enable_arena = true;
    size_t arena_size_bytes = 1024ull * 1024ull * 1024ull; // 1GB default
    bool   topdown_on_nonarena = true; // add MEM_TOP_DOWN to non-arena reserves
};

struct HighVASysInfo {
    SIZE_T page_size = 4096;
    SIZE_T alloc_gran = 64 * 1024;
    uintptr_t min_app = 0;
    uintptr_t max_app = 0;
};

inline void HV_GetSysInfo(HighVASysInfo& out) {
    SYSTEM_INFO si{};
    ::GetSystemInfo(&si);
    out.page_size = si.dwPageSize;
    out.alloc_gran = si.dwAllocationGranularity;
    out.min_app = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    out.max_app = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
}

inline bool HV_IsProcessLAA(bool* outEffectiveLAA = nullptr) {
    HMODULE hExe = ::GetModuleHandleW(nullptr);
    if (!hExe) return false;
    auto base = reinterpret_cast<uint8_t*>(hExe);
    auto dos  = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    bool laa = (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) != 0;
    if (outEffectiveLAA) {
        HighVASysInfo s{}; HV_GetSysInfo(s);
        *outEffectiveLAA = laa && (s.max_app > 0x80000000u);
    }
    return laa;
}

inline uintptr_t HV_AlignDown(uintptr_t v, SIZE_T align) { return v & ~(uintptr_t(align) - 1); }
inline uintptr_t HV_AlignUp(uintptr_t v, SIZE_T align)   { return (v + (align - 1)) & ~(uintptr_t(align) - 1); }

// Small VAD-like arena: reserve a big contiguous region, sub-allocate in allocation-granularity units.
class HighVAArena {
public:
    bool init(const HighVAOptions& opt);
    void destroy();

    void* reserve(SIZE_T size);
    bool  commit(void* addr, SIZE_T size, DWORD protect);
    void* alloc(SIZE_T size, DWORD protect);
    bool  decommit(void* addr, SIZE_T size);
    bool  release(void* base);

    bool  contains(void* p) const {
        uintptr_t u = reinterpret_cast<uintptr_t>(p);
        return reserved_ok_ && u >= base_ && u < (base_ + size_bytes_);
    }
    bool  active() const { return reserved_ok_; }
    uintptr_t base() const { return base_; }
    SIZE_T size() const { return size_bytes_; }

private:
    struct FreeSeg { size_t start_units; size_t len_units; };
    struct Reservation { size_t start_units; size_t len_units; };

    bool try_reserve_high(SIZE_T size);
    void merge_free_locked(FreeSeg seg);
    bool carve_from_free_locked(size_t need_units, size_t& out_start_units);

    mutable std::mutex m_;
    uintptr_t base_ = 0;
    SIZE_T size_bytes_ = 0;
    SIZE_T gran_ = 64 * 1024;

    std::vector<FreeSeg> free_;
    std::map<void*, Reservation> reserved_;
    bool reserved_ok_ = false;
};

// Global API used by hooks
namespace HighVAAPI {
    // Initialize once at startup
    void Init(const HighVAOptions& opt);
    void Shutdown();
    bool IsActive();
    bool Contains(void* p);

    // Allocation/commit helpers (used by VirtualAlloc hook)
    void* Reserve(SIZE_T size);
    bool  Commit(void* addr, SIZE_T size, DWORD protect);
    void* Alloc(SIZE_T size, DWORD protect);

    // Free helpers (used by VirtualFree hook)
    bool  Decommit(void* addr, SIZE_T size);
    bool  Release(void* base);

    // LAA status helpers
    bool  HeaderLAA();
    bool  EffectiveLAA();
    void  GetLAA(bool& hdr, bool& effective);

    // Config passthrough
    void  SetTopdownOnNonArena(bool enable);
    bool  TopdownOnNonArena();

    // Info
    bool  GetArenaInfo(uintptr_t& base, SIZE_T& size);
}
