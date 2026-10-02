#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// Allocation and range notification remain guest service boundaries. Callbacks
// may change mapped memory; callers must retain the original read/write order.
class QueryPoolServices
{
public:
    virtual ~QueryPoolServices() = default;
    virtual GuestAddress Allocate(std::uint32_t bytes, std::uint32_t flags) = 0;
    virtual void Free(GuestAddress address, std::uint32_t flags) = 0;
    virtual void NotifyRange(GuestAddress begin, GuestAddress end, std::uint32_t flags) = 0;
};

// 827B72E8: obtain a 64-byte slot from the device's linked bitmap pools.
[[nodiscard]] std::int32_t InitializeQuerySlot(GuestMemory& memory, QueryPoolServices& services,
    GuestAddress device, GuestAddress owner_slot, GuestAddress data_slot);

// 823CDCA8: decrement the query reference count and release its owned slots.
[[nodiscard]] std::uint32_t ReleaseQuery(GuestMemory& memory, QueryPoolServices& services,
    GuestAddress query);

} // namespace lo::semantic::gpu
