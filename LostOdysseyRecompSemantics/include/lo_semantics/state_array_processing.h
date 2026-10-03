#pragma once

#include "lo_semantics/manager_init.h"

#include <array>

namespace lo::semantic::gpu::state_array_processing
{

struct FrameRegisters
{
    std::uint64_t lr;
    // gpr[0] is r21, through gpr[10] for r31. The hub's real ABI helper
    // saves and restores all eleven registers in guest memory.
    std::array<std::uint64_t, 11> gpr;
};

// The item method and manager vtable methods are dynamic guest callbacks.
// A callback sees its masked target, full input registers, full guest SP and
// live LR/nonvolatile registers. Nested lower helper volatile r4-r7 effects
// are not represented by this bounded API; VisitItem therefore exposes only
// its freshly loaded receiver. Callback implementations remain external.
class StateArrayCallbacks
{
public:
    virtual ~StateArrayCallbacks() = default;
    virtual void VisitItem(GuestAddress method, GuestMemory& memory,
        std::uint64_t receiver, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
    [[nodiscard]] virtual std::uint64_t ResizeStorage(GuestAddress method,
        GuestMemory& memory, std::uint64_t manager_register,
        std::uint64_t storage_register, std::uint64_t bytes_register,
        std::uint64_t argument_register, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
    [[nodiscard]] virtual std::uint64_t ReleaseStorage(GuestAddress method,
        GuestMemory& memory, std::uint64_t manager_register,
        std::uint64_t storage_register, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
};

// 823FDAB0 drains/processes the global queue; 82407AD8 initializes its
// Package owner; 8240A7C0 guards a low32-null tail call. All three preserve
// full incoming r3 and SP where the original body does. Unknown entries have
// no effects. Ordinary mapped guest RAM and the named service callbacks are
// the supported boundary; generic volatile GPR/CR and callback internals are
// not recovered here.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerInitServices& manager_init, StateArrayCallbacks& callbacks,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::state_array_processing
