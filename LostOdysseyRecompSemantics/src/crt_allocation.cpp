#include "lo_semantics/crt_allocation.h"

#include "lo_semantics/memory_services.h"

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kOutOfMemory = 12;
constexpr GuestAddress kRetryEnabled = 0x832d3aecu;

std::uint32_t CurrentThreadError(GuestMemory& memory,
    GuestAddress thread_environment)
{
    if (memory.ReadU32(thread_environment + 336u) != 0)
        return 0;
    return memory.ReadU32(memory.ReadU32(thread_environment + 256u) + 352u);
}

} // namespace

std::uint32_t TranslateCrtError(GuestMemory& memory, std::uint32_t status)
{
    constexpr GuestAddress table = 0x832150a8u;
    for (std::uint32_t index = 0; index != 45; ++index)
    {
        const GuestAddress entry = table + index * 8u;
        if (memory.ReadU32(entry) == status)
            return memory.ReadU32(entry + 4u);
    }
    if (status - 19u <= 17u)
        return 13;
    return status - 188u <= 14u ? 8 : 22;
}

std::uint64_t AllocateCrtArray(GuestMemory& memory,
    CrtAllocationServices& services, std::uint32_t count, std::uint32_t size,
    GuestAddress error_out, GuestAddress caller_sp)
{
    if (count != 0 && 0xfffff000u / count < size)
    {
        memory.WriteU32(static_cast<GuestAddress>(
            GetAllocationErrorAddress(services)), kOutOfMemory);
        services.ReportInvalidParameter();
        return 0;
    }

    const std::int64_t product = static_cast<std::int64_t>(
        static_cast<std::int32_t>(count)) * static_cast<std::int32_t>(size);
    const std::uint64_t requested = static_cast<std::uint32_t>(product) == 0 ?
        1 : static_cast<std::uint64_t>(product);
    for (;;)
    {
        if (static_cast<GuestAddress>(requested) <= 0xfffff000u)
        {
            const GuestAddress heap = GetProcessHeap(memory);
            const GuestAddress allocated = AllocateHeapBlock(memory, services,
                heap, 8, static_cast<GuestAddress>(requested), caller_sp - 448u);
            if (allocated != 0)
                return allocated;
        }

        if (memory.ReadU32(kRetryEnabled) != 0 &&
            InvokeNewHandler(memory, services, requested) != 0)
            continue;

        if (error_out != 0)
            memory.WriteU32(error_out, kOutOfMemory);
        return 0;
    }
}

std::uint64_t AllocateCrtRecord(GuestMemory& memory,
    CrtAllocationServices& services, std::uint32_t count, std::uint32_t size,
    GuestAddress caller_sp)
{
    const GuestAddress error_out = caller_sp - 32u;
    memory.WriteU32(error_out, 0);
    const std::uint64_t allocated = AllocateCrtArray(memory, services,
        count, size, error_out, caller_sp - 112u);
    if (static_cast<GuestAddress>(allocated) == 0)
    {
        const std::uint32_t error = memory.ReadU32(error_out);
        if (error != 0 &&
            static_cast<GuestAddress>(GetAllocationErrorAddress(services)) != 0)
            memory.WriteU32(static_cast<GuestAddress>(
                GetAllocationErrorAddress(services)), error);
    }
    return allocated;
}

std::uint64_t FreeCrtRecord(GuestMemory& memory,
    CrtAllocationServices& services, std::uint64_t payload,
    GuestAddress thread_environment, GuestAddress caller_sp)
{
    if (static_cast<GuestAddress>(payload) == 0)
        return payload;

    const GuestAddress heap = GetProcessHeap(memory);
    const std::uint32_t freed = FreeHeapBlock(memory, services, heap, 0,
        static_cast<GuestAddress>(payload), caller_sp - 272u);
    if (freed != 0)
        return freed;

    const GuestAddress error_address = static_cast<GuestAddress>(
        GetAllocationErrorAddress(services));
    const std::uint32_t mapped = TranslateCrtError(memory,
        CurrentThreadError(memory, thread_environment));
    memory.WriteU32(error_address, mapped);
    return mapped;
}

} // namespace lo::semantic::gpu
