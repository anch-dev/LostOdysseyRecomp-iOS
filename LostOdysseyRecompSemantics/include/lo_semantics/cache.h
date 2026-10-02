#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// Original dcbf/sync instructions are absent from the generated host behavior.
// A runtime implementation must supply their platform-specific effects.
class CacheServices
{
public:
    virtual ~CacheServices() = default;
    virtual void FlushLine(GuestAddress address) = 0;
    virtual void Sync() = 0;
};

// 823EA178. The original third argument is unused. Addresses and arithmetic
// wrap at 32 bits; the excluded address range returns without a sync.
void FlushDataCacheRange(CacheServices& services, GuestAddress begin, GuestAddress end);

} // namespace lo::semantic::gpu
