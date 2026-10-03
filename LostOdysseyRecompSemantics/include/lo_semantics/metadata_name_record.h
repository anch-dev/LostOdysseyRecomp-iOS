#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/manager_init.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_name_record
{

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r27 = 0;
    std::uint64_t r28 = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
};

class Services : public ManagerInitServices
{
public:
    // Only the manager vtable+4 implementation remains dynamic. The target
    // has its low two bits cleared; the call sees full PPC register values.
    // Its LR is the 823F7B08 call-site return address and its SP is the live
    // 128-byte guest frame. It may mutate memory and live nonvolatile state.
    virtual std::uint64_t AllocateRecord(GuestAddress target,
        GuestMemory& memory, std::uint64_t manager_r3,
        std::uint64_t bytes_r4, std::uint64_t alignment_r5,
        std::uint64_t full_sp, FrameRegisters& frame) = 0;
};

// 823F7B08. Incoming r3 names the UTF-16 source; r4 is the record id and
// r6 its extra word. Incoming r5 is overwritten before the virtual call.
// Unknown addresses return false without changing the arguments or result.
// Ordinary bounded guest RAM and the existing ManagerInitServices lower ABI
// are the bounds of this interface; it does not implement the virtual method.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Services& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t incoming_r5,
    std::uint64_t incoming_r6, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_name_record
