#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_token_float_cursor
{
struct Registers
{
    crt_stream_operations::Registers integer{};
    std::uint64_t f0_bits = 0, f1_bits = 0;
    std::uint32_t cached_fp_control = 0;
};

struct NumberServices
{
    virtual ~NumberServices() = default;
    // Unrecovered PPC guest callee 82B7D270. This bounded contract supplies
    // f1; its numeric parsing and volatile side effects remain open.
    virtual double Convert(std::uint64_t text, std::uint64_t mode) = 0;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// 82297338 searches a UTF-16 token and stores its parsed float when found;
// 822974A8 sets conversion mode zero and tail-calls that PPC boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    NumberServices& numbers, Registers& registers);
} // namespace lo::semantic::gpu::legacy_token_float_cursor
