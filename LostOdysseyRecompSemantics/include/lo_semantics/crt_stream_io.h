#pragma once

#include "lo_semantics/crt_status_error.h"
#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_stream_io
{
using Condition = crt_status_error::Condition;

// Selected PPC state at the two stream I/O entry boundaries. A service may
// change the live registers and stack pointer before control returns here.
struct Registers
{
    std::uint64_t sp = 0, lr = 0, ctr = 0;
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    std::uint64_t r8 = 0, r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0;
    std::uint64_t r28 = 0, r29 = 0, r30 = 0, r31 = 0;
    std::uint8_t xer_so = 0;
    Condition cr0, cr6;
};

class NativeServices : public crt_status_error::NativeServices
{
public:
    virtual void NtWriteFile(GuestMemory& memory, Registers& state) = 0;
    virtual void NtWaitForSingleObjectEx(GuestMemory& memory, Registers& state) = 0;
    // The target is loaded from the mutable table and masked by the PPC call
    // boundary. Its implementation is deliberately supplied by the caller.
    virtual void CallIndirect(GuestMemory& memory, GuestAddress target,
        Registers& state) = 0;
};

// Returns false for an unknown address without effects. Ordinary guest RAM,
// selected register state and accepted lower models form the recovery scope.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_io
