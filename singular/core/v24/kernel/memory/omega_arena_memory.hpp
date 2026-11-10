#pragma once
// SINGULAR Ω v24 — 64B-Aligned Bounded State-Table & Causal Arena Memory
#include <array>
#include <cstddef>
#include <cstdint>

namespace singular::v24::kernel {

struct alignas(64) BoundedCausalArena64K {
    static constexpr size_t CAPACITY_BYTES = 65536;
    std::array<uint8_t, CAPACITY_BYTES> storage{};
    size_t offset = 0;

    void* allocate_aligned(size_t bytes) noexcept {
        size_t aligned = (bytes + 63u) & ~size_t(63u);
        if (offset + aligned > CAPACITY_BYTES) return nullptr;
        void* p = storage.data() + offset;
        offset += aligned;
        return p;
    }
    void reset() noexcept { offset = 0; }
};

} // namespace singular::v24::kernel
