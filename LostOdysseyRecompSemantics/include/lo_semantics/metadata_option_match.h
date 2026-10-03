#pragma once

#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/invalid_parameter.h"

#include <array>
#include <cstdint>

namespace lo::semantic::gpu::metadata_option_match
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::array<std::uint64_t, 7> r25_through_r31{};
};

// 82296858 compares a bounded UTF-16 prefix with ASCII case folding;
// 82297390 finds an option token after an ASCII-alphanumeric boundary;
// 8247C0C0 scans slash/dash-separated options and tests the token suffix.
// The recovered errno/invalid-parameter helpers are composed on invalid
// compare arguments. Guest RAM uses 32-bit addresses, while input registers,
// frame values, stack arithmetic, and the returned r3 keep their full width.
// Other volatile effects of nested CRT helpers are outside this interface.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_option_match
