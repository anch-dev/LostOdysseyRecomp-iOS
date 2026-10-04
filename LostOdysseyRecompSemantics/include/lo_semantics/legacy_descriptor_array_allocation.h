#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_descriptor_array_allocation
{
struct Registers
{
    crt_stream_operations::Registers integer{};
    std::uint64_t f0_bits = 0, f12_bits = 0, f13_bits = 0;
    std::uint32_t cached_fp_control = 0;
};

class Services
{
public:
    virtual ~Services() = default;
    // The exhausted-pool 82FAC428 branch remains an explicit native boundary.
    virtual void AllocateFromPool(GuestMemory& memory,
        crt_stream_operations::Registers& integer) = 0;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// Complete 83058590 array-descriptor constructor and 82FB36B0 pool/free-list
// allocator. Actual 82FAC238, 83056568 and accepted 82B7BC40 are composed.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Services& services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_array_allocation
