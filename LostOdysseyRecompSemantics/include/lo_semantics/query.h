#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu
{

// Boundaries still implemented by guest functions 827C9D88, 827B72E8 and
// 823CDCA8, respectively. Their host-side behavior is intentionally opaque.
class QueryServices
{
public:
    virtual ~QueryServices() = default;

    virtual GuestAddress Allocate(std::uint32_t bytes, std::uint32_t flags) = 0;
    virtual std::int32_t InitializeSlot(GuestAddress device, GuestAddress owner_slot,
                                        GuestAddress data_slot) = 0;
    virtual std::uint32_t Release(GuestAddress query) = 0;
};

// Semantic recovery of guest function 827B7408. Returns zero when allocation
// or slot initialization fails, as the original PPC routine does.
[[nodiscard]] GuestAddress CreateType9Query(GuestMemory& memory, QueryServices& services,
                                            GuestAddress device);

} // namespace lo::semantic::gpu
