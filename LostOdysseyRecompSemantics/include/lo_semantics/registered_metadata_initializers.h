#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu::registered_metadata_initializers
{

class FpServices
{
public:
    virtual ~FpServices() = default;
    // Called at the first lfs, after any preceding integer memory effects.
    virtual void DisableFlushMode() = 0;
};

// Observable registers of the three leaf initializers. r3 remains unchanged.
// NaN format movement follows the documented PPC bit mapping. The generated
// C++ baseline quiets signaling NaNs in FPRs and is compiler-dependent on stores;
// those inputs remain outside the established generated-code equivalence.
struct Effects
{
    std::uint64_t r10{};
    std::uint64_t r11{};
    double f0{};
    double f13{};
    bool disable_flush_mode{};
};

// Object and source addresses use the low 32 bits of guest registers. Unknown
// addresses return false without touching memory or effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    FpServices& fp_services, std::uint64_t object_register, Effects& effects);

} // namespace lo::semantic::gpu::registered_metadata_initializers
