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

    // Unrecovered PPC guest bodies. These callbacks may change selected live
    // registers and guest RAM, just as the corresponding direct calls do.
    virtual void Parse822975B0(GuestMemory&, Registers&) = 0;
    virtual void Classify822981C8(GuestMemory&, Registers&) = 0;
    virtual void InvalidArgument82B7FD78(GuestMemory&, Registers&) = 0;
    virtual void InvalidParameter82B7FEC0(GuestMemory&, Registers&) = 0;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// 82B7D270 skips classified leading UTF-16 whitespace before converting;
// 822974F8 routes the parser flags/classification into a result object.
// Numeric parsing (822975B0) and conversion (822981C8) remain PPC boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    PpcBoundaryServices& services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_float_parse_routes
