#pragma once

#include "lo_semantics/allocation_array.h"

namespace lo::semantic::gpu::metadata_descriptor_membership
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r27 = 0;
    std::uint64_t r28 = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
    std::uint64_t ctr = 0;
};

class Services : public ArrayResizeServices
{
public:
    virtual std::uint64_t Dispatch(GuestAddress target,
        std::uint64_t receiver, std::uint64_t argument,
        std::uint64_t caller_sp, FrameRegisters& frame) = 0;
};

// 824080A8 removes a matching descriptor slot and its empty owner's global
// memberships. 823AAE00 follows the owner chain, changes the slot index and
// installs a free slot. Observer vtable+4/+8/+12 calls are explicit services.
// Own save27/save29 frames, selected live registers, full inputs and full r3
// results are modeled. RemoveArrayRange/AppendMetadataWord reuse accepted
// algorithms; their unexposed generic ABI/frame effects and dynamic target
// internals, faults/MMIO and concurrent mutation remain outside this boundary.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Services& services, std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_descriptor_membership
