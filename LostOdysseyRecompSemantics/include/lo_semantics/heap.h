#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// 830DA08C remains an external debug comparison boundary. The caller in
// 823AE108 ignores its return value, including when links are damaged.
class HeapServices
{
public:
    virtual ~HeapServices() = default;
    virtual std::uint32_t CompareMemoryUlong(GuestAddress source,
        std::uint32_t bytes, std::uint32_t value) = 0;
};

// 827CBA60: split and insert free blocks into the heap's size lists.
void InsertFreeBlocks(GuestMemory& memory, GuestAddress heap, GuestAddress block,
    std::uint32_t units);

// 823AE108: merge with adjacent free blocks when the combined size fits.
[[nodiscard]] GuestAddress CoalesceFreeBlocks(GuestMemory& memory,
    HeapServices& services, GuestAddress heap, GuestAddress block,
    GuestAddress units_inout, std::uint32_t already_free);

} // namespace lo::semantic::gpu
