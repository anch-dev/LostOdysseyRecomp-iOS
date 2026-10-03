#pragma once

#include "lo_semantics/allocation_array.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_descriptor_array
{

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
};

// 823B9268 pops the last 32-bit array entry; 822B3F50 assigns a two-byte
// array; 82400BC0 installs a descriptor in the global slot array and two
// chained hash tables. Inputs and the returned r3 keep their full PPC width.
// Own LR/GPR saves and backchains are observable through ordinary guest RAM.
// The existing array and copy helpers supply their accepted algorithms;
// generic lower ABI register effects and resize-method internals are outside
// this boundary. Unknown addresses return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_descriptor_array
