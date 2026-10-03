#pragma once

#include "lo_semantics/crt_stream_error.h"
#include "lo_semantics/crt_stream_io.h"
#include "lo_semantics/crt_stream_locks.h"
#include "lo_semantics/crt_stream_pointer_unlock.h"
#include "lo_semantics/crt_stream_state.h"
#include "lo_semantics/guest_memory.h"

#include <array>
#include <cstdint>

namespace lo::semantic::gpu::crt_stream_operations
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
};

// The selected live PPC context includes all nonvolatile registers touched by
// the save helpers. Native callbacks in the accepted dependencies remain live.
struct Registers
{
    std::uint64_t sp = 0, lr = 0, ctr = 0;
    std::array<std::uint64_t, 32> r{};
    std::uint8_t xer_so = 0, xer_ca = 0;
    Condition cr0{}, cr6{};
};

struct Dependencies
{
    CrtThreadDataServices& thread;
    InvalidParameterServices& invalid;
    RawAllocationServices& raw;
    crt_stream_io::NativeServices& io;
    crt_stream_locks::Dependencies locks;
    crt_stream_pointer_unlock::NativeServices& unlock;
    crt_stream_state::NativeServices& state;
};

// Five recovered stream entry points. Unknown addresses leave all inputs
// unchanged. Ordinary guest RAM and the selected live context are modeled;
// native internals, faults, MMIO and concurrent mutation remain external.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);

// Explicit adapters for the accepted PPC callees. Original-body oracles and
// callers use the same selected-state conversion as the five entries.
[[nodiscard]] bool ApplyAcceptedCallee(GuestAddress address,
    GuestMemory& memory, Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_operations
