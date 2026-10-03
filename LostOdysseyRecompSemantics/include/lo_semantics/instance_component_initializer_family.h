#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_component_initializer_family
{

class ComponentFpServices
{
public:
    virtual ~ComponentFpServices() = default;

    // The non-null PPC path disables flush mode immediately before its first
    // lfs. A runtime adapter supplies the caller's FP control state.
    virtual void DisableFlushMode() = 0;
};

struct ComponentEffects
{
    std::uint64_t r10{};
    std::uint64_t r11{};
    double f0{};
    double f13{};
};

// Initialize the common component defaults and vtable in original store
// order. Guest addresses use the low word of r3; result preserves full r3.
// Unknown addresses change nothing. Null r3 changes result only. Signaling
// NaN memory movement follows LoadedSingle's ISA mapping; the generated PPC
// C++ baseline can differ there. Fault/MMIO width of the original 64-bit
// zero store remains outside this ordinary-memory model.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ComponentFpServices& fp_services, std::uint64_t incoming_r3,
    ComponentEffects& effects, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_component_initializer_family
