#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_float_parse_routes
{
struct Registers
{
    crt_stream_operations::Registers integer{};
    std::uint64_t f1_bits = 0;
    std::uint32_t cached_fp_control = 0;
};

class PpcBoundaryServices
{
public:
    virtual ~PpcBoundaryServices() = default;

    // The parser and invalid-parameter PPC bodies remain explicit boundaries.
    // The classifier hook is retained for historical oracle compatibility;
    // current routes call the recovered 822981C8 lower directly.
    virtual void Parse822975B0(GuestMemory&, Registers&) = 0;
    virtual void Classify822981C8(GuestMemory&, Registers&) = 0;
    virtual void InvalidArgument82B7FD78(GuestMemory&, Registers&) = 0;
    virtual void InvalidParameter82B7FEC0(GuestMemory&, Registers&) = 0;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// 82B7D270 skips classified leading UTF-16 whitespace before converting;
// 822974F8 routes the parser flags/classification into a result object.
// Numeric parsing (822975B0) remains a PPC boundary; conversion (822981C8)
// runs its complete recovered integer/ordinary-RAM control flow.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    PpcBoundaryServices& services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_float_parse_routes
