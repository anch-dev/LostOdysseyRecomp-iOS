#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// The optional heap commit callback, kernel VM allocation, and 827CB658
// uncommitted-range bookkeeping remain explicit guest-service boundaries.
class HeapSegmentServices
{
public:
    virtual ~HeapSegmentServices() = default;
    virtual std::int32_t CommitRange(GuestAddress callback_address,
        GuestAddress heap, GuestAddress base_inout,
        GuestAddress bytes_inout) = 0;
    virtual std::int32_t AllocateVirtualMemory(GuestAddress base_inout,
        GuestAddress size_inout, std::uint32_t type,
        std::uint32_t protect, std::uint32_t zero) = 0;
    virtual void InitializeUncommittedRange(GuestAddress segment,
        GuestAddress committed_end, std::uint32_t bytes) = 0;
};

// 827CB778. frame_base is the 160-byte guest frame. Its +80 slot is the
// candidate commit address passed by pointer to the commit service.
[[nodiscard]] GuestAddress ExtendHeapSegment(GuestMemory& memory,
    HeapSegmentServices& services, GuestAddress heap, GuestAddress segment,
    GuestAddress bytes_inout, GuestAddress required_base,
    GuestAddress frame_base);

// 827CC2C0. frame_base is the 176-byte guest frame; the incoming committed
// end is spilled at frame_base + 236 in the caller's argument area. +80 is the
// VM commit-size output slot. ABI register saves/backchain belong to adapter.
[[nodiscard]] GuestAddress InitializeHeapSegment(GuestMemory& memory,
    HeapSegmentServices& services, GuestAddress heap, GuestAddress base,
    std::uint32_t slot_index, std::uint32_t segment_flags,
    GuestAddress range_begin, GuestAddress committed_end,
    GuestAddress reserved_end, GuestAddress frame_base);

} // namespace lo::semantic::gpu
