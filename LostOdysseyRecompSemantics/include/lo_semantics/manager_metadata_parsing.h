#pragma once

#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/guest_memory.h"
#include "lo_semantics/invalid_parameter.h"

#include <array>
#include <cstdint>

namespace lo::semantic::gpu::manager_metadata_parsing
{

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r13 = 0;
    std::uint64_t r8 = 0;
    std::uint64_t r9 = 0;
    std::array<std::uint64_t, 9> r23_through_r31{};
};

// 82376FA8: decimal value of an accepted Unicode UTF-16 code unit, or -1.
[[nodiscard]] std::uint64_t DecimalDigit(std::uint64_t code_unit);

// 822974B0: guest character-classification table lookup, masked by r4.
[[nodiscard]] std::uint64_t CharacterClass(GuestMemory& memory,
    std::uint64_t code_unit, std::uint64_t mask);

// Applies 822974B0, 82376FA8, 82B7D3E0, 82B7D688, 82376F98, or
// 82296E80. The parser's CRT-thread-data and invalid-parameter lower helpers
// are composed here; their TLS/handler operations remain explicit services.
// r3..r7 are full incoming PPC registers. A known address sets result; an
// unknown address returns false without touching memory, frame or result.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::manager_metadata_parsing
