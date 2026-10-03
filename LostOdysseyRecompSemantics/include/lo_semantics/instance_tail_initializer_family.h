#pragma once

#include "lo_semantics/allocation_array.h"

namespace lo::semantic::gpu::instance_tail_initializer_family
{

// The five listed null-guarded tail entries and their four newly recovered
// callees. A direct callee has no null guard; the tail entry does. The caller
// stack address is needed by the two staged-word initializers. The full r3 is
// returned. Generic PPC volatile registers and call-frame spills are outside
// this bounded memory API.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_tail_initializer_family
