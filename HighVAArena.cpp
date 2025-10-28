#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <algorithm>
#include "HighVAArena.h"

static HighVAArena g_arena;
static HighVAOptions g_opt;
static bool g_hdrLAA = false;
static bool g_effLAA = false;
static bool g_inited = false;

static SIZE_T g_gran = 64 * 1024;

bool HighVAArena::try_reserve_high(SIZE_T size) {
    HighVASysInfo s{}; HV_GetSysInfo(s);
    gran_ = s.alloc_gran;

    // Try direct MEM_TOP_DOWN first
    void* p = ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS);
    if (p) {
        base_ = reinterpret_cast<uintptr_t>(p);
        size_bytes_ = size;
        reserved_ok_ = true;
        return true;
    }

    // Manual scan downward from high VA
    uintptr_t maxA = s.max_app;
    uintptr_t minA = HV_AlignUp(s.min_app, s.alloc_gran);
    uintptr_t scan = HV_AlignDown(maxA + 1 - size, s.alloc_gran);
    MEMORY_BASIC_INFORMATION mbi{};
    while (scan >= minA) {
        if (!VirtualQuery(reinterpret_cast<void*>(scan), &mbi, sizeof(mbi)))
            break;
        if (mbi.State == MEM_FREE) {
            uintptr_t region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            SIZE_T region_size = mbi.RegionSize;
            uintptr_t region_end = region_base + region_size;
            uintptr_t candidate = HV_AlignDown(region_end - size, s.alloc_gran);
            if (candidate >= region_base) {
                void* r = ::VirtualAlloc(reinterpret_cast<void*>(candidate), size, MEM_RESERVE, PAGE_NOACCESS);
                if (r) {
                    base_ = reinterpret_cast<uintptr_t>(r);
                    size_bytes_ = size;
                    reserved_ok_ = true;
                    return true;
                }
            }
        }
        uintptr_t next = (reinterpret_cast<uintptr_t>(mbi.BaseAddress) >= s.alloc_gran) ?
                         reinterpret_cast<uintptr_t>(mbi.BaseAddress) - s.alloc_gran : 0;
        if (scan <= next) break;
        scan = next;
    }
    return false;
}

bool HighVAArena::init(const HighVAOptions& opt) {
    std::lock_guard<std::mutex> g(m_);
    if (!opt.enable_arena || opt.arena_size_bytes == 0) return false;
    HighVASysInfo s{}; HV_GetSysInfo(s);
    SIZE_T size = HV_AlignUp(opt.arena_size_bytes, s.alloc_gran);
    if (!try_reserve_high(size)) {
        reserved_ok_ = false; base_ = 0; size_bytes_ = 0;
        return false;
    }
    free_.clear();
    reserved_.clear();
    free_.push_back(FreeSeg{ 0, size_bytes_ / gran_ });
    return true;
}

void HighVAArena::destroy() {
    std::lock_guard<std::mutex> g(m_);
    if (reserved_ok_) {
        ::VirtualFree(reinterpret_cast<void*>(base_), 0, MEM_RELEASE);
    }
    reserved_ok_ = false; base_ = 0; size_bytes_ = 0; free_.clear(); reserved_.clear();
}

void HighVAArena::merge_free_locked(FreeSeg seg) {
    auto it = free_.begin();
    while (it != free_.end() && it->start_units < seg.start_units) ++it;
    free_.insert(it, seg);
    std::vector<FreeSeg> merged;
    merged.reserve(free_.size());
    for (auto& s : free_) {
        if (!merged.empty()) {
            auto& back = merged.back();
            if (back.start_units + back.len_units == s.start_units) {
                back.len_units += s.len_units;
                continue;
            }
        }
        merged.push_back(s);
    }
    free_.swap(merged);
}

bool HighVAArena::carve_from_free_locked(size_t need_units, size_t& out_start_units) {
    // first-fit
    for (size_t i = 0; i < free_.size(); ++i) {
        auto& f = free_[i];
        if (f.len_units >= need_units) {
            out_start_units = f.start_units;
            f.start_units += need_units;
            f.len_units -= need_units;
            if (f.len_units == 0) free_.erase(free_.begin() + i);
            return true;
        }
    }
    return false;
}

void* HighVAArena::reserve(SIZE_T size) {
    if (!reserved_ok_) return nullptr;
    std::lock_guard<std::mutex> g(m_);
    size_t need_units = (HV_AlignUp(size, gran_)) / gran_;
    size_t start_units = 0;
    if (!carve_from_free_locked(need_units, start_units)) return nullptr;
    void* addr = reinterpret_cast<void*>(base_ + start_units * gran_);
    reserved_.emplace(addr, Reservation{ start_units, need_units });
    return addr;
}

bool HighVAArena::commit(void* addr, SIZE_T size, DWORD protect) {
    if (!reserved_ok_ || !addr || size == 0) return false;
    uintptr_t u = reinterpret_cast<uintptr_t>(addr);
    if (u < base_ || (u + size) > (base_ + size_bytes_)) return false;
    void* r = ::VirtualAlloc(addr, size, MEM_COMMIT, protect);
    return r != nullptr;
}

void* HighVAArena::alloc(SIZE_T size, DWORD protect) {
    void* p = reserve(size);
    if (!p) return nullptr;
    if (!commit(p, size, protect)) {
        release(p);
        return nullptr;
    }
    return p;
}

bool HighVAArena::decommit(void* addr, SIZE_T size) {
    if (!reserved_ok_ || !addr || size == 0) return false;
    uintptr_t u = reinterpret_cast<uintptr_t>(addr);
    if (u < base_ || (u + size) > (base_ + size_bytes_)) return false;
    return ::VirtualFree(addr, size, MEM_DECOMMIT) != 0;
}

bool HighVAArena::release(void* baseptr) {
    if (!reserved_ok_ || !baseptr) return false;
    std::lock_guard<std::mutex> g(m_);
    auto it = reserved_.find(baseptr);
    if (it == reserved_.end()) return false;
    auto res = it->second;
    // best-effort decommit
    ::VirtualFree(baseptr, res.len_units * gran_, MEM_DECOMMIT);
    merge_free_locked(FreeSeg{ res.start_units, res.len_units });
    reserved_.erase(it);
    return true;
}

namespace HighVAAPI {
    static std::mutex s_lock;
    static bool s_topdown_nonarena = true;

    void Init(const HighVAOptions& opt) {
        std::lock_guard<std::mutex> g(s_lock);
        if (g_inited) return;
        g_opt = opt;
        g_hdrLAA = HV_IsProcessLAA(&g_effLAA);
        HighVASysInfo si{}; HV_GetSysInfo(si);
        g_gran = si.alloc_gran;
        if (opt.enable_arena && opt.arena_size_bytes) {
            g_arena.init(opt);
        }
        s_topdown_nonarena = opt.topdown_on_nonarena;
        g_inited = true;
    }
    void Shutdown() {
        std::lock_guard<std::mutex> g(s_lock);
        if (!g_inited) return;
        g_arena.destroy();
        g_inited = false;
    }
    bool IsActive() { return g_inited && g_arena.active(); }
    bool Contains(void* p) { return g_arena.contains(p); }

    void* Reserve(SIZE_T size) { return g_arena.reserve(size); }
    bool  Commit(void* addr, SIZE_T size, DWORD protect) { return g_arena.commit(addr, size, protect); }
    void* Alloc(SIZE_T size, DWORD protect) { return g_arena.alloc(size, protect); }

    bool  Decommit(void* addr, SIZE_T size) { return g_arena.decommit(addr, size); }
    bool  Release(void* base) { return g_arena.release(base); }

    bool  HeaderLAA() { return g_hdrLAA; }
    bool  EffectiveLAA() { return g_effLAA; }
    void  GetLAA(bool& hdr, bool& effective) { hdr = g_hdrLAA; effective = g_effLAA; }

    void  SetTopdownOnNonArena(bool enable) { s_topdown_nonarena = enable; }
    bool  TopdownOnNonArena() { return s_topdown_nonarena; }

    bool  GetArenaInfo(uintptr_t& base, SIZE_T& size) {
        if (!IsActive()) return false;
        base = g_arena.base();
        size = g_arena.size();
        return true;
    }
}
