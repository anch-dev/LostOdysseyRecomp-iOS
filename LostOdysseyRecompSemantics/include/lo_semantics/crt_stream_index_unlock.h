#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::crt_stream_index_unlock
{
struct Registers
{
    std::uint64_t sp = 0;
    std::uint64_t lr = 0;
    std::uint64_t r3 = 0;
    std::uint64_t r10 = 0;
    std::uint64_t r11 = 0;
    std::uint64_t r12 = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r31 = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // The real RtlLeaveCriticalSection import. The leaf reaches it by a tail
    // branch, so this callback sees the caller's live LR, SP and registers.
    virtual void LeaveCriticalSection(GuestMemory& memory,
        Registers& registers) = 0;
};

// Recover 82B819C8 and its 82B863B8 wrapper on ordinary guest RAM.
// Unknown addresses return false without effects. The native callback may
// edit live registers or RAM before the wrapper's epilogue. Faults, MMIO,
// concurrency and registers outside this selected state are out of scope.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_index_unlock
