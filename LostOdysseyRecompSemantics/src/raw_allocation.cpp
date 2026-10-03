#include "lo_semantics/raw_allocation.h"

#include "lo_semantics/memory_services.h"

namespace lo::semantic::gpu
{

std::uint64_t AllocateRawMemory(GuestMemory& memory,
    RawAllocationServices& services, std::uint64_t requested_bytes)
{
    constexpr std::uint32_t kOutOfMemory = 12;
    constexpr GuestAddress kRetryEnabled = 0x832d3aec;
    const auto size = static_cast<std::uint32_t>(requested_bytes);
    if (size > 0xfffff000u)
    {
        (void)services.RetryAllocation(requested_bytes);
        memory.WriteU32(services.GetErrorAddress(), kOutOfMemory);
        return 0;
    }

    for (;;)
    {
        if (GetProcessHeap(memory) == 0)
        {
            services.EnterMissingHeapPath();
            services.ReportMissingHeap(30);
            services.TerminateMissingHeap(255);
        }

        // Re-read the heap after the missing-heap calls, as the original does.
        const GuestAddress heap = GetProcessHeap(memory);
        const std::uint64_t allocation_bytes = size == 0 ? 1 : requested_bytes;
        const std::uint64_t allocated = services.AllocateHeap(heap, 0, allocation_bytes);
        if (static_cast<std::uint32_t>(allocated) != 0)
            return allocated;

        if (memory.ReadU32(kRetryEnabled) != 0)
        {
            if (services.RetryAllocation(requested_bytes) != 0)
                continue;
        }
        else
        {
            // Both lookups and stores are observable: the error address can
            // change between calls, even when both receive the same code.
            memory.WriteU32(services.GetErrorAddress(), kOutOfMemory);
        }
        memory.WriteU32(services.GetErrorAddress(), kOutOfMemory);
        return allocated;
    }
}

} // namespace lo::semantic::gpu
