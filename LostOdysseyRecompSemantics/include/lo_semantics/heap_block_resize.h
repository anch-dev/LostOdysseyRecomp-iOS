#pragma once

#include "lo_semantics/heap.h"
#include "lo_semantics/heap_segment.h"

#include <array>
#include <cstdint>

namespace lo::semantic::gpu::heap_block_resize
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t sp = 0;
    std::array<std::uint64_t, 10> r22_through_r31{};
};

// The two direct 830DA08C imports compare free-fill bytes. Their return
// values are discarded, but their ordered memory/frame effects are visible.
class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual std::uint64_t CompareMemoryUlong(GuestMemory& memory,
        GuestAddress source, std::uint32_t bytes, std::uint32_t pattern,
        std::uint64_t caller_sp, FrameRegisters& frame) = 0;
};

// 827CBCA8 resizes a heap block in place through segment extension, free
// block coalescing/splitting, and optional zero-fill. r3-r7 and caller_sp are
// full incoming registers. Own save22/rest22 slots, live frame registers,
// return r3 and guest RAM are modeled; the recovered lower helpers retain
// their documented guest32/ABI and external service boundaries.
// Unknown addresses have no effects and return false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    HeapSegmentServices& segment_services,
    HeapServices& coalesce_services, NativeServices& native_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::heap_block_resize
