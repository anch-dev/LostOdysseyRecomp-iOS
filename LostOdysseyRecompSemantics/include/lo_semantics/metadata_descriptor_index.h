#pragma once

#include "lo_semantics/manager_facade.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_descriptor_index
{

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r28 = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
};

// 82523C48 rebuilds descriptor buckets; 8256B910 appends an indexed entry;
// 826BD860 updates a matching entry or appends; 82408D28 initializes the
// enclosing descriptor and retains the nested call's full r3 result. The
// inputs r3-r5 and caller SP are full PPC values. Own save/restore order and
// backchains are observable. The accepted manager facade and array algorithms
// are composed through their existing APIs; generic lower ABI effects beyond
// those APIs and manager virtual-method internals remain external.
// Unknown addresses return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t incoming_r5,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_descriptor_index
