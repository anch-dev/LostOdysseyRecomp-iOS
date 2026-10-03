#pragma once

#include "lo_semantics/instance_component_initializer_family.h"
#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu::instance_navigation_allocation_family
{

struct FrameRegisters
{
    std::uint64_t lr;
    std::uint64_t r27;
    std::uint64_t r28;
    std::uint64_t r29;
    std::uint64_t r30;
    std::uint64_t r31;
};

// Guest vtable calls in 823B8728 and 825E12A0 remain explicit boundaries.
// The callback receives the complete r3 and SP and may mutate guest memory
// and live nonvolatile registers. The dynamic target itself is not recovered.
class VirtualServices
{
public:
    virtual ~VirtualServices() = default;
    virtual std::uint64_t Call(GuestAddress method, GuestMemory& memory,
        std::uint64_t incoming_r3, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
};

// Five exact entrypoints: registry linking, nested object initialization,
// navigation allocation, a parent object initializer and its null-guard tail.
// f0/f13 record lfs results only when 825AF048 runs. Split 64-bit guest
// stores assume ordinary RAM; optimized baseline sNaN differences remain.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    VirtualServices& virtual_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& f0_bits,
    std::uint64_t& f13_bits, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_navigation_allocation_family
