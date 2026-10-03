#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::ring_reservation
{
class SynchronizationServices
{
public:
    virtual ~SynchronizationServices() = default;
    // The source lwsync occurs after publishing the wrap limit and before
    // resetting the producer cursor. Generated PPC omits this opcode; its
    // native comparison does not establish hardware synchronization.
    virtual void LightweightSync() = 0;
};

struct Registers
{
    std::uint64_t r7;
    std::uint64_t r8;
    std::uint64_t r9;
    std::uint64_t r10;
    std::uint64_t r11;
};

// 82290AB8 reserves aligned space without advancing the producer cursor.
// The descriptor receives ring/start/size at +0/+4/+8. All loop fields are
// reloaded as in PPC, so insufficient space may wait indefinitely. The full
// descriptor register is returned unchanged; ordinary mapped RAM, selected
// GPR effects and the explicit lwsync boundary are the supported scope.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    SynchronizationServices& synchronization, std::uint64_t descriptor,
    std::uint64_t ring, std::uint64_t requested_bytes,
    Registers& registers, std::uint64_t& result);
} // namespace lo::semantic::gpu::ring_reservation
