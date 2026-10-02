#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// 827CB498, NtFreeVirtualMemory, and 827CB658 remain service boundaries.
// The VM call receives guest addresses of the original stack output locals.
class HeapDecommitServices
{
public:
    virtual ~HeapDecommitServices() = default;
    virtual GuestAddress AllocateRangeNode(GuestAddress segment) = 0;
    virtual std::int32_t FreeVirtualMemory(GuestAddress base_out,
        GuestAddress size_out, std::uint32_t type, std::uint32_t zero) = 0;
    virtual void InsertRangeRecord(GuestAddress segment, GuestAddress base,
        std::uint32_t bytes) = 0;
};

// 827CC668. frame_base names its 208-byte guest frame. The caller supplies
// the frame and preserves its saved registers; locals +80/+84 are the VM size
// and base output parameters. Guest exception unwinding is outside this API.
void DecommitFreeBlock(GuestMemory& memory, HeapDecommitServices& services,
    GuestAddress heap, GuestAddress block, std::uint32_t units,
    GuestAddress frame_base);

} // namespace lo::semantic::gpu
