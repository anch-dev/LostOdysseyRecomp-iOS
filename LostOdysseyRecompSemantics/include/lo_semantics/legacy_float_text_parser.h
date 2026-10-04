#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_float_text_parser
{
using Registers = crt_stream_operations::Registers;

class PpcBoundaryServices
{
public:
    virtual ~PpcBoundaryServices() = default;
    // Pending direct PPC callee 82297F38. It converts at most 24 unpacked
    // decimal digits at r3/r4 into the 12-byte extended record at r5.
    virtual void DigitsToExtended82297F38(GuestMemory&, Registers&) = 0;
    virtual void InvalidArgument82B7FD78(GuestMemory&, Registers&) = 0;
    virtual void InvalidParameter82B7FEC0(GuestMemory&, Registers&) = 0;
};

// 822975B0 is the complete UTF-16/extended-decimal parser. It reads the
// guest locale separator and power-of-ten tables, writes the end pointer and
// 12-byte extended record, and returns the parser status in r3. The pending
// digit-conversion PPC callee remains a mutable selected-state boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    PpcBoundaryServices& services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_float_text_parser
