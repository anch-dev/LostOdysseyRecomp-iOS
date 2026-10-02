#pragma once

#include "lo_semantics/heap.h"

namespace lo::semantic::gpu
{

// The segment extender (827CB778), segment initializer (827CC2C0), and
// kernel virtual-memory calls remain explicit boundaries.
class HeapGrowthServices : public HeapServices
{
public:
    virtual GuestAddress ExtendSegment(GuestAddress heap, GuestAddress segment,
        GuestAddress size_inout, std::uint32_t zero) = 0;
    virtual std::int32_t AllocateVirtualMemory(GuestAddress base_inout,
        GuestAddress size_inout, std::uint32_t type,
        std::uint32_t protect, std::uint32_t zero) = 0;
    virtual GuestAddress InitializeSegment(GuestAddress heap, GuestAddress base,
        std::uint32_t slot_index, std::uint32_t zero,
        GuestAddress range_begin, GuestAddress committed_end,
        GuestAddress reserved_end) = 0;
    virtual std::int32_t FreeVirtualMemory(GuestAddress base_inout,
        GuestAddress size_inout, std::uint32_t type,
        std::uint32_t zero) = 0;
};

// 827CC428. frame_base is its 144-byte guest frame. Its local output slots are
// +80 reserve size, +84 VM base/segment, +88 requested segment size or units,
// and +92 commit size. ABI register saves/backchain are adapter responsibilities.
[[nodiscard]] GuestAddress GrowHeap(GuestMemory& memory,
    HeapGrowthServices& services, GuestAddress heap,
    std::uint32_t rounded_bytes, GuestAddress frame_base);

} // namespace lo::semantic::gpu
