#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::caller_frame_unlock
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    bool operator==(const Condition&) const = default;
};
struct Registers
{
    std::uint64_t sp = 0, lr = 0, ctr = 0;
    std::uint64_t r3 = 0, r4 = 0, r10 = 0, r11 = 0, r12 = 0, r31 = 0;
    std::uint8_t xer_ca = 0, xer_so = 0;
    Condition cr0{}, cr6{};
};
class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void LeaveCriticalSection(GuestMemory& memory, Registers& state) = 0;
};

// Dereference the caller-owned lock record and release its critical section.
// Seven unwind funclets address that record relative to the incoming r12 frame.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
}
