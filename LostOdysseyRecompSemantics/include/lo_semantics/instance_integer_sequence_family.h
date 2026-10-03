#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_integer_sequence_family
{

// Apply one of fourteen reviewed integer-only instance initializers. The
// original store order, including caller-stack staging, is retained. Unknown
// addresses leave memory and result untouched. The caller supplies the guest
// stack pointer only for XeAudioDevice (824772F8); that entry writes r3 at
// caller_sp+20 even when the object is null. The full incoming r3 is returned.
// U64 loads/stores use two ordinary-memory words; fault/MMIO access width and
// volatile PPCContext registers are outside this library API.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, GuestAddress caller_sp, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_integer_sequence_family
