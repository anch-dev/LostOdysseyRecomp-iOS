#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::crt_stream_pointer_unlock
{
struct Registers
{
    std::uint64_t sp = 0;
    std::uint64_t lr = 0;
    std::uint64_t r3 = 0;
    std::uint64_t r9 = 0;
    std::uint64_t r10 = 0;
    std::uint64_t r11 = 0;
    std::uint64_t r12 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
    std::uint8_t xer_ca = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // The leaf tail-branches to imported RtlLeaveCriticalSection. The native
    // callback sees the computed full argument, live selected registers and
    // RAM; it may mutate wrapper frame/backchain and saved slots.
    virtual void LeaveCriticalSection(GuestMemory& memory,
        Registers& registers) = 0;
};

// 82B863F0 computes the per-stream lock pointer and tail-calls the import.
// 82B81F38 / 82B860CC wrap it with their own distinct LR call site and
// 112-byte frame. 82B81AF8 loads the lock pointer from live r30+80 and calls
// the same import from a 96-byte frame. Unknown addresses leave inputs untouched. The model
// covers ordinary RAM and selected registers, not other native ABI effects,
// faults, MMIO, concurrency or runtime integration.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_pointer_unlock
