#pragma once

#include "lo_semantics/legacy_float_parse_routes.h"

namespace lo::semantic::gpu::legacy_float_public_parse
{
struct Registers
{
    legacy_float_parse_routes::Registers route{};
    std::uint64_t f0_bits = 0;
    std::uint64_t f31_bits = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void Parse822975B0(GuestMemory&, Registers&) = 0;
    virtual void InvalidArgument82B7FD78(GuestMemory&, Registers&) = 0;
    virtual void InvalidParameter82B7FEC0(GuestMemory&, Registers&) = 0;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// 82B7E200 supplies the omitted mode argument then tail-enters 82B7E098.
// Character flags, UTF-16 length, route and binary conversion use accepted
// lower bodies; the numeric text parser and exceptional calls remain boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_float_public_parse
