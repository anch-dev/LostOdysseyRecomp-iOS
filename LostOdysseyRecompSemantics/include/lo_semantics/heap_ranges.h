#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// The two virtual-memory kernel calls remain explicit service boundaries.
// Their pointer arguments are guest addresses in the original caller frame.
class HeapRangeServices
{
public:
    virtual ~HeapRangeServices() = default;
    virtual std::int32_t AllocateVirtualMemory(GuestAddress base_inout,
        GuestAddress size_inout, std::uint32_t type, std::uint32_t protect,
        std::uint32_t zero) = 0;
    virtual std::int32_t FreeVirtualMemory(GuestAddress base_inout,
        GuestAddress size_inout, std::uint32_t type, std::uint32_t zero) = 0;
};

// 827CB498. frame_base names the original 128-byte guest frame. Its VM
// output locals are at +80, +84, +88, and +92.
[[nodiscard]] GuestAddress AllocateRangeNode(GuestMemory& memory,
    HeapRangeServices& services, GuestAddress segment, GuestAddress frame_base);

// 827CB658. frame_base names this function's 128-byte guest frame. When it
// calls AllocateRangeNode, that callee uses the next 128 bytes below it.
void InsertRangeRecord(GuestMemory& memory, HeapRangeServices& services,
    GuestAddress segment, GuestAddress base, std::uint32_t bytes,
    GuestAddress frame_base);

} // namespace lo::semantic::gpu
