#pragma once

#include "lo_semantics/crt_allocation.h"
#include "lo_semantics/heap_reallocate.h"

namespace lo::semantic::gpu::crt_reallocate
{
using Registers = heap_reallocate::Registers;

// 823ACAD8 handles allocation/free shortcuts, the recovered heap realloc,
// new-handler retries and thread-error translation. Its own 128-byte frame
// and save27 slots are retained. Existing lower models keep their documented
// native/ABI boundaries; this interface does not establish guest unwinding,
// fault/MMIO, concurrency or runtime replacement.
// An unknown address leaves registers, memory and services untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    heap_reallocate::Services& heap, CrtAllocationServices& crt,
    Registers& registers);
} // namespace lo::semantic::gpu::crt_reallocate
