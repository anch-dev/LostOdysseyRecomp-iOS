#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::metadata_member_dispatch
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
    std::uint64_t ctr = 0;
};

class Services
{
public:
    virtual ~Services() = default;
    virtual std::uint64_t CallMember(GuestAddress method,
        std::uint64_t member, std::uint64_t destination,
        std::uint64_t caller_sp, FrameRegisters& frame) = 0;
};

// 823FD400 follows type+120/member+112 and invokes vtable+340 for
// members without flag bit 41. Full inputs, live selected registers and
// own 112-byte frame are retained. Dynamic method internals and generic
// volatile ABI effects, faults/MMIO and concurrent mutation are external.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Services& services, std::uint64_t receiver, std::uint64_t type,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_member_dispatch
