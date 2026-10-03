#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_manager_link_family
{

class FpServices
{
public:
    virtual ~FpServices() = default;
    virtual void DisableFlushMode() = 0;
};

struct Registers
{
    std::uint64_t lr{};
    std::uint64_t r9{};
    std::uint64_t r10{};
    std::uint64_t r11{};
    std::uint64_t r30{};
    std::uint64_t r31{};
    double f0{};
    double f13{};
};

// 82700FD8 initializes the 128-byte data subobject. 82700F28 constructs
// its 160-byte owner and calls that exact leaf at full r3+16. The owner
// writes a 112-byte frame; saved LR and nonvolatiles are reloaded from guest
// memory, so aliases with the object remain observable. Unknown addresses
// produce no effects. This API does not implement the larger 827010D0 owner.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    FpServices& fp_services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    Registers& registers, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_manager_link_family
