#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_utf8_conversion_routes
{
using Registers = crt_stream_operations::Registers;

// The non-UTF8 code-page path calls two kernel imports with live selected PPC
// state. The native implementation is supplied by the embedding runtime.
class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void RtlMultiByteToUnicodeN(GuestMemory& memory,
        Registers& state) = 0;
    virtual void RtlNtStatusToDosError(GuestMemory& memory,
        Registers& state) = 0;
};

// 8229C560 dispatches UTF8 to the complete 827CA660 decoder. The direct
// 822CA180 error-store tail uses its accepted guest RAM semantics. Fixed
// conversion tables are read from guest RAM in original instruction order.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::crt_utf8_conversion_routes
