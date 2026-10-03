#pragma once

#include "lo_semantics/metadata_name_lookup.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_name_dispatch
{

using FrameRegisters = metadata_name_lookup::FrameRegisters;

class DispatchServices
{
public:
    virtual ~DispatchServices() = default;

    // The 825E7600 vtable+264 call sees the loaded full r3 receiver and the
    // live full r31 in r4. Its target is masked with ~3; SP and LR are the
    // live 112-byte frame and 0x825E7688. The callback may change memory and
    // the exposed live register state, including r31 before the final mr r3.
    // The lower 82296D30 API does not expose its residual r5-r7, so those
    // generic volatile inputs and the target implementation are out of scope.
    virtual void CallMethod(GuestAddress masked_target, GuestMemory& memory,
        std::uint64_t receiver_r3, std::uint64_t owner_r4,
        std::uint64_t full_sp, std::uint64_t full_lr,
        FrameRegisters& frame) = 0;
};

// Restore 825E7600 with its ordered object writes, actual name lookup,
// vtable+264 callback, and save/rest GPR29-31/LR frame. r3-r7 and caller_sp
// are full incoming PPC values. Unknown addresses return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& record_services,
    ArrayResizeServices& resize_services,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    DispatchServices& dispatch_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_name_dispatch
