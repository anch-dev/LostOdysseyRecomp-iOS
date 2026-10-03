#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::crt_last_error
{
struct Registers
{
    std::uint64_t r3 = 0;
    std::uint64_t r11 = 0;
    std::uint64_t r13 = 0;
    std::uint8_t xer_so = 0;
    struct Condition { std::uint8_t lt = 0, gt = 0, eq = 0, so = 0; } cr6;
};

// 822CA108 returns the thread's error word unless its +336 marker is set;
// 822CA100 is its real tail entry. r11 and CR6 reflect the original reads
// and comparison; all remaining context and ordinary RAM stay unchanged.
// Faults, MMIO and concurrent thread-state changes require separate evidence.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::crt_last_error
