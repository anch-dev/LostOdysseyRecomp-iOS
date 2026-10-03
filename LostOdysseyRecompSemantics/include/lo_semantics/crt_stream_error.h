#pragma once

#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/invalid_parameter.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_stream_error
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
};

struct Registers
{
    std::uint64_t sp = 0, lr = 0;
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    std::uint64_t r8 = 0, r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0;
    std::uint64_t r30 = 0, r31 = 0;
    std::uint8_t xer_so = 0, xer_ca = 0;
    Condition cr0{}, cr6{};
};

// 82B7FDB0 selects the CRT thread's stream-error slot; 82B86228 resolves
// a stream handle or reports error 9; 82B7FDE8 stores both an error code
// and its CRT translation. The three PPC bodies, their own stack frames,
// selected live registers, and ordinary RAM are modeled. The accepted
// thread-data, error-address, invalid-parameter and translation callees
// retain their existing generic lower ABI and external service boundaries.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_error
