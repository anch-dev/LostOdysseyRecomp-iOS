#pragma once

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/heap_allocate.h"
#include "lo_semantics/heap_free.h"

namespace lo::semantic::gpu
{

// The existing heap and failure implementations supply the callees below.
// Only their kernel, callback, and invalid-parameter operations remain services.
class CrtAllocationServices : public AllocationFailureServices,
                              public HeapAllocateServices,
                              public HeapFreeServices
{
public:
    virtual void ReportInvalidParameter() = 0;
};

// 82B7FD10: map a runtime status through the 45-entry guest error table.
[[nodiscard]] std::uint32_t TranslateCrtError(GuestMemory& memory,
    std::uint32_t status);

// 82B816A0: checked count*size allocation with new-handler retry. caller_sp
// is the guest stack at entry; the existing heap allocator receives its nested
// 320-byte frame at caller_sp - 448. error_out is a guest address or zero.
[[nodiscard]] std::uint64_t AllocateCrtArray(GuestMemory& memory,
    CrtAllocationServices& services, std::uint32_t count, std::uint32_t size,
    GuestAddress error_out, GuestAddress caller_sp);

// 82B81778: calloc-like wrapper with a local error-out word.
[[nodiscard]] std::uint64_t AllocateCrtRecord(GuestMemory& memory,
    CrtAllocationServices& services, std::uint32_t count, std::uint32_t size,
    GuestAddress caller_sp);

// 823ADDC0: heap release and errno mapping on failure. A zero low-word
// payload returns the original full input register without a heap call.
[[nodiscard]] std::uint64_t FreeCrtRecord(GuestMemory& memory,
    CrtAllocationServices& services, std::uint64_t payload,
    GuestAddress thread_environment, GuestAddress caller_sp);

} // namespace lo::semantic::gpu
