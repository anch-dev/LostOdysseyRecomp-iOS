#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// Explicit CRT failure and heap boundaries of 823ACBD0. Missing-heap calls
// may not return in the live runtime; if they return, the original continues.
class RawAllocationServices
{
public:
    virtual ~RawAllocationServices() = default;
    [[nodiscard]] virtual std::uint64_t AllocateHeap(GuestAddress heap,
        std::uint32_t flags, std::uint64_t bytes) = 0;
    virtual void EnterMissingHeapPath() = 0; // 82B7FCE0
    virtual void ReportMissingHeap(std::uint32_t code) = 0; // 82B7FC98
    virtual void TerminateMissingHeap(std::uint32_t code) = 0; // 82B7BF20
    [[nodiscard]] virtual std::int32_t RetryAllocation(std::uint64_t bytes) = 0; // 82B7FE68
    [[nodiscard]] virtual GuestAddress GetErrorAddress() = 0; // 82B7FD78
};

// 823ACBD0. Threshold/zero decisions use the low 32 bits, while callbacks
// receive the saved full size register. No arbitrary retry limit is imposed.
[[nodiscard]] std::uint64_t AllocateRawMemory(GuestMemory& memory,
    RawAllocationServices& services, std::uint64_t requested_bytes);

} // namespace lo::semantic::gpu
