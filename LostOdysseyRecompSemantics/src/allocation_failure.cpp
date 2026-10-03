#include "lo_semantics/allocation_failure.h"

namespace lo::semantic::gpu
{

std::uint64_t ReportRuntimeError(GuestMemory& memory,
    AllocationFailureServices& services, std::uint64_t error_code)
{
    constexpr GuestAddress message_table = 0x83214ff0u;
    for (std::uint32_t index = 0; index != 23; ++index)
    {
        const GuestAddress entry = message_table + index * 8u;
        if (memory.ReadU32(entry) == static_cast<std::uint32_t>(error_code))
            return services.OutputErrorMessage(memory.ReadU32(entry + 4u));
    }
    return error_code;
}

std::uint64_t ReportMissingHeapBanner(GuestMemory& memory,
    AllocationFailureServices& services)
{
    (void)ReportRuntimeError(memory, services, 252);
    return ReportRuntimeError(memory, services, 255);
}

std::uint64_t TerminateAllocationFailure(AllocationFailureServices& services)
{
    return services.BugCheck(0);
}

std::uint64_t InvokeNewHandler(GuestMemory& memory,
    AllocationFailureServices& services, std::uint64_t requested_bytes)
{
    constexpr GuestAddress handler_global = 0x832d3ae8u;
    const GuestAddress handler = memory.ReadU32(handler_global);
    if (handler == 0)
        return 0;
    return static_cast<std::uint32_t>(services.CallNewHandler(handler & ~3u,
                                                             requested_bytes)) != 0;
}

std::uint64_t GetAllocationErrorAddress(AllocationFailureServices& services)
{
    const std::uint64_t thread_data = services.GetThreadData();
    return static_cast<GuestAddress>(thread_data) == 0 ?
        0xffffffff83215210ull : thread_data + 8u;
}

void RecoveredRawAllocationServices::EnterMissingHeapPath()
{
    (void)ReportMissingHeapBanner(memory_, services_);
}

void RecoveredRawAllocationServices::ReportMissingHeap(std::uint32_t code)
{
    (void)ReportRuntimeError(memory_, services_, code);
}

void RecoveredRawAllocationServices::TerminateMissingHeap(std::uint32_t code)
{
    (void)code;
    (void)TerminateAllocationFailure(services_);
}

std::int32_t RecoveredRawAllocationServices::RetryAllocation(std::uint64_t bytes)
{
    return static_cast<std::int32_t>(InvokeNewHandler(memory_, services_, bytes));
}

GuestAddress RecoveredRawAllocationServices::GetErrorAddress()
{
    return static_cast<GuestAddress>(GetAllocationErrorAddress(services_));
}

} // namespace lo::semantic::gpu
