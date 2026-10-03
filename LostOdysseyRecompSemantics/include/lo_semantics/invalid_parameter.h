#pragma once

#include "lo_semantics/guest_memory.h"

#include <array>

namespace lo::semantic::gpu
{

// The callback receives the live argument registers and implicit r13 value.
// They are an explicit mutable boundary because the callback may return with
// a new result or thread environment.
struct InvalidParameterCall
{
    std::array<std::uint64_t, 8> arguments{}; // PPC r3 through r10.
    std::uint64_t thread_environment = 0;    // PPC r13.
};

class InvalidParameterServices
{
public:
    virtual ~InvalidParameterServices() = default;
    virtual void CallHandler(GuestMemory& memory, GuestAddress function,
                             InvalidParameterCall& call) = 0;
    // The original instruction traps after the no-handler state change.
    // A returning test boundary observes the generated PPC continuation.
    virtual void Trap(const InvalidParameterCall& call) = 0;
};

// 82B84D88: clear the runtime state word. Its incoming r3 is untouched.
void ClearInvalidParameterState(GuestMemory& memory);

// 82B7FEC0: invoke the configured callback, or clear state and trap.
// A returning callback's full r3 and r13 values remain in call.
[[nodiscard]] std::uint64_t ReportInvalidParameter(GuestMemory& memory,
    InvalidParameterServices& services, InvalidParameterCall& call);

} // namespace lo::semantic::gpu
