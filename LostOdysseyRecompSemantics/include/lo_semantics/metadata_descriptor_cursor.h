#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_descriptor_cursor
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r31 = 0;
    std::uint64_t ctr = 0;
};

// The vtable+284 target is dynamic. The service sees its masked address,
// zero-extended node receiver, full r4, current guest SP/LR/CTR/r31 and
// mutable live guest RAM. It returns full r3; only its low word is stored.
class VirtualServices
{
public:
    virtual ~VirtualServices() = default;
    virtual std::uint64_t CallNext(GuestAddress target,
        GuestMemory& memory, std::uint64_t node_r3,
        std::uint64_t incoming_r4, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
};

// 822A6EF8 advances a two-word cursor across eligible +64 links, asking
// vtable+284 for another node when the chain ends. 82406568 initializes the
// cursor then calls that actual recovered child. Full r3/r4/SP/result and
// the two 96-byte owned frames are observable. Unknown addresses have no
// effects. Other volatile ABI state, callback internals, MMIO and infinite
// guest lists are outside the selected ordinary-RAM contract.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    VirtualServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_descriptor_cursor
