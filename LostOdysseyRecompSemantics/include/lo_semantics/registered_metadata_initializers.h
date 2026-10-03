#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu::registered_metadata_initializers
{

// Observable registers of the three leaf initializers. r3 remains unchanged.
// The caller must disable host FP flushing before Apply, as each PPC body does
// before its first lfs. Apply reports that FPSCR effect for a future adapter.
// Known 825A8448 differential: with source 0x7F800001, the original generated
// C++ at /O2 stores 0x7F800001 while this conversion stores 0x7FC00001. At
// /Od both store 0x7FC00001. This API does not claim sNaN payload equivalence.
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
    std::uint64_t object_register, Effects& effects);

} // namespace lo::semantic::gpu::registered_metadata_initializers
