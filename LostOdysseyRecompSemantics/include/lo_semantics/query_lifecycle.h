#pragma once

#include "lo_semantics/query.h"
#include "lo_semantics/query_pool.h"

namespace lo::semantic::gpu
{

// Compose recovered query creation, slot allocation and release. Only the
// underlying allocation/free/cache services remain supplied by the caller.
// This is a library adapter, not a PPCContext or game-runtime hook.
class PooledQueryServices final : public QueryServices
{
public:
    PooledQueryServices(GuestMemory& memory, QueryPoolServices& services)
        : memory_(memory), services_(services) {}

    GuestAddress Allocate(std::uint32_t bytes, std::uint32_t flags) override
    {
        return services_.Allocate(bytes, flags);
    }

    std::int32_t InitializeSlot(GuestAddress device, GuestAddress owner_slot,
        GuestAddress data_slot) override
    {
        return InitializeQuerySlot(memory_, services_, device, owner_slot, data_slot);
    }

    std::uint32_t Release(GuestAddress query) override
    {
        return ReleaseQuery(memory_, services_, query);
    }

private:
    GuestMemory& memory_;
    QueryPoolServices& services_;
};

} // namespace lo::semantic::gpu
