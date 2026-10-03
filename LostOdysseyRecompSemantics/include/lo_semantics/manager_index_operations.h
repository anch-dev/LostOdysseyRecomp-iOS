#pragma once

#include "lo_semantics/manager_index_tables.h"

namespace lo::semantic::gpu::manager_index_operations
{

class FpServices
{
public:
    virtual ~FpServices() = default;
    virtual void DisableFlushMode() = 0;
};

struct FrameRegisters
{
    std::uint64_t lr{};
    std::uint64_t r28{};
    std::uint64_t r29{};
    std::uint64_t r30{};
    std::uint64_t r31{};
    std::uint64_t f31_bits{};
};

// 82326978 upserts a single-precision value and 82713CF8 a word value in
// the 16-byte-entry manager index. The key is full r4; the integer value is
// low32 r5. For 82326978, f1_bits contains full FPR f1 and the caller's f31
// is saved/restored in guest memory. Unknown addresses produce no effects.
// Existing ResizeArray and manager-index rebuild APIs bound generic lower
// callback register redirection; ordinary mapped RAM is the memory contract.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager, ArrayResizeServices& arrays,
    FpServices& fp_services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t incoming_r5,
    std::uint64_t f1_bits, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::manager_index_operations
