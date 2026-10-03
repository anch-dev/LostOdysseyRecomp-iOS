#pragma once

#include "lo_semantics/manager_init.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_manager_dispatch
{

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
    std::uint64_t ctr = 0;
};

class ManagerDispatchServices : public ManagerInitServices
{
public:
    // 823F3298 dispatches manager vtable+4. The target's low two bits are
    // cleared; r3 is the zero-extended manager, r4/r5 retain full inputs.
    // The target implementation is outside this recovered entry.
    virtual std::uint64_t CallDescriptorMethod(GuestAddress masked_target,
        GuestMemory& memory, std::uint64_t manager_r3,
        std::uint64_t argument_r4, std::uint64_t argument_r5,
        std::uint64_t full_sp, std::uint64_t full_lr,
        FrameRegisters& frame) = 0;
};

// 823F3298: initialize a missing manager with the already recovered
// InitializeManager algorithm, reload its global pointer, then dispatch its
// vtable+4 method. Own savegprlr_29 order, 112-byte backchain, live CTR and
// the target's full r3 result are explicit. Generic lower InitializeManager
// register/frame side effects are outside its existing API. Unknown addresses
// return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerDispatchServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_manager_dispatch
