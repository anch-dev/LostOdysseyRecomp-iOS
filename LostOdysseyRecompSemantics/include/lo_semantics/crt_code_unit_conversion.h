#pragma once

#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/invalid_parameter.h"

#include <array>
#include <cstdint>

namespace lo::semantic::gpu::crt_code_unit_conversion
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
};

struct Registers
{
    std::uint64_t sp = 0, lr = 0;
    // Selected PPC r3 through r13; the indices match the register numbers.
    std::array<std::uint64_t, 14> r{};
    std::uint8_t xer_so = 0;
    Condition cr0{}, cr6{};
};

struct Dependencies
{
    CrtThreadDataServices& thread;
    InvalidParameterServices& invalid;
};

// 82B86AB8 converts a code unit to one byte; 82B86BE0 clears r7 and tail
// enters that body. Unknown addresses leave the selected state and RAM intact.
// The accepted CRT lower models retain their existing native/ABI boundaries.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_code_unit_conversion
