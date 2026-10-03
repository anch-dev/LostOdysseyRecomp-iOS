#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/raw_allocation.h"

namespace lo::semantic::gpu
{

class AllocationFailureServices
{
public:
    virtual ~AllocationFailureServices() = default;
    virtual std::uint64_t OutputErrorMessage(GuestAddress message) = 0;
    virtual std::uint64_t BugCheck(std::uint32_t code) = 0;
    virtual std::uint64_t CallNewHandler(GuestAddress handler,
                                       std::uint64_t requested_bytes) = 0;
    virtual std::uint64_t GetThreadData() = 0;
};

// 82B7FC98: search the 23-entry error-message table. A missing code leaves the
// full incoming return register unchanged; a match forwards the output result.
[[nodiscard]] std::uint64_t ReportRuntimeError(GuestMemory& memory,
    AllocationFailureServices& services, std::uint64_t error_code);

// 82B7FCE0: emit entries 252 and 255 in that order, if their lookups succeed.
[[nodiscard]] std::uint64_t ReportMissingHeapBanner(GuestMemory& memory,
    AllocationFailureServices& services);

// 82B7BF20 ignores the caller's code and tail-calls KeBugCheck with zero.
// Real termination need not return; a returning boundary result is preserved.
[[nodiscard]] std::uint64_t TerminateAllocationFailure(AllocationFailureServices& services);

// 82B7FE68: invoke the live new-handler pointer and normalize its low-word result.
[[nodiscard]] std::uint64_t InvokeNewHandler(GuestMemory& memory,
    AllocationFailureServices& services, std::uint64_t requested_bytes);

// 82B7FD78: select thread-data +8, or the sign-extended static errno address.
[[nodiscard]] std::uint64_t GetAllocationErrorAddress(AllocationFailureServices& services);

// Compose the recovered failure helpers into AllocateRawMemory. Subclasses
// provide AllocateHeap; only message output, termination, the new handler and
// thread-data acquisition remain external service boundaries.
class RecoveredRawAllocationServices : public RawAllocationServices
{
public:
    RecoveredRawAllocationServices(GuestMemory& memory, AllocationFailureServices& services)
        : memory_(memory), services_(services) {}

    void EnterMissingHeapPath() override;
    void ReportMissingHeap(std::uint32_t code) override;
    void TerminateMissingHeap(std::uint32_t code) override;
    std::int32_t RetryAllocation(std::uint64_t bytes) override;
    GuestAddress GetErrorAddress() override;

private:
    GuestMemory& memory_;
    AllocationFailureServices& services_;
};

} // namespace lo::semantic::gpu
