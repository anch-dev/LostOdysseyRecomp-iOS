#include "lo_semantics/thread_state.h"

namespace lo::semantic::gpu
{

void StoreThreadFailureCode(GuestMemory& memory, GuestAddress thread_state,
    std::uint32_t code)
{
    if (memory.ReadU32(thread_state + 0x150) != 0)
        return;
    const GuestAddress failure_state = memory.ReadU32(thread_state + 0x100);
    memory.WriteU32(failure_state + 0x160, code);
}

void ReportAllocationFailure(GuestMemory& memory, GuestAddress thread_state,
    std::uint32_t code)
{
    StoreThreadFailureCode(memory, thread_state, code);
}

} // namespace lo::semantic::gpu
