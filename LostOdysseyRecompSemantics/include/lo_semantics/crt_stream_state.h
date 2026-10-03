#pragma once

#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/invalid_parameter.h"
#include "lo_semantics/raw_allocation.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_stream_state
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r31 = 0;
    std::uint64_t sp = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // Imported RtlInitializeCriticalSection is the irreducible operation
    // inside the recovered 82B82180 PPC wrapper. The callback sees live
    // full-width argument registers and selected frame fields, and may
    // modify guest memory and the guest stack.
    virtual std::uint64_t InitializeCriticalSection(GuestMemory& memory,
        InvalidParameterCall& call, FrameRegisters& frame) = 0;
    // 82B821B0 may use a mutable function pointer other than 82B82180.
    virtual std::uint64_t CallIndirect(GuestAddress target,
        GuestMemory& memory, InvalidParameterCall& call,
        FrameRegisters& frame) = 0;
};

// 82B81648 reads a stream field or reports an invalid argument;
// 82B85C40 initializes a stream buffer through recovered 823ACBD0;
// 82B82180 wraps the native critical-section initializer; and 82B821B0
// installs/calls its mutable global target. The returned r3, SP, LR, r31,
// guest stack and ordered RAM writes are exposed. CRT/native lower volatile
// ABI effects beyond the explicit service call are outside this boundary.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    RawAllocationServices& allocation_services,
    NativeServices& native_services, InvalidParameterCall& call,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result);
} // namespace lo::semantic::gpu::crt_stream_state
