#pragma once

#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu::owned_state_initializer
{

struct FrameRegisters
{
    std::uint64_t lr;
    std::uint64_t r31;
};

// 823FA008 dispatches the old state's live vtable[0] with r4=1. The
// implementation remains a guest callback, including memory and the exposed
// nonvolatile/LR effects. Its returned r3 is discarded by the caller.
class StateServices
{
public:
    virtual ~StateServices() = default;
    virtual void DestroyState(GuestAddress method, GuestMemory& memory,
        std::uint64_t receiver, std::uint64_t argument,
        std::uint64_t caller_sp, FrameRegisters& frame) = 0;
};

// 82406A38 initializes a 56-byte state using its owner; 823FA008 destroys an
// existing state, allocates its replacement and sets owner flag bit 57.
// The second owner+52 read is unconditional, even when owner low32 is zero.
// Ordinary RAM is modeled; split ld/std accesses, volatile GPR/CR and generic
// allocation ABI frames remain outside this API. Unknown entries are inert.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services, StateServices& state_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::owned_state_initializer
