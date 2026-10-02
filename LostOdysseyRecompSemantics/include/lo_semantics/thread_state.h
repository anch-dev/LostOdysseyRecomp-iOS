#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// 822CA188 receives thread_state implicitly in PPC r13 and code in r3.
// Field names describe the observed failure-reporting call path; the original
// debug type and the meaning of the suppressing word are not established.
void StoreThreadFailureCode(GuestMemory& memory, GuestAddress thread_state,
    std::uint32_t code);

// 822CA180 is a tail-forwarding entry used by the physical allocator's failure
// branch. It has the same implicit r13 input and does not change r3.
void ReportAllocationFailure(GuestMemory& memory, GuestAddress thread_state,
    std::uint32_t code);

} // namespace lo::semantic::gpu
