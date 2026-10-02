#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// Four allocator implementations are kept explicit until their own heap,
// synchronization and failure contracts have been recovered.
class AllocationServices
{
public:
    virtual ~AllocationServices() = default;
    virtual GuestAddress AllocateSpecial(GuestAddress allocator, std::uint32_t bytes) = 0;
    virtual GuestAddress AllocateGeneral(std::uint32_t bytes, std::uint32_t flags) = 0;
    virtual std::uint32_t FreeSpecial(GuestAddress allocator, GuestAddress address) = 0;
    virtual std::uint32_t FreeGeneral(GuestAddress address, std::uint32_t flags) = 0;
};

// 827C9D88 and 827C9DB0 tail-dispatch by exact flags, preserving low-32-bit
// arguments and returns. Allocation flags are not decoded or normalized here.
[[nodiscard]] GuestAddress AllocateDispatch(AllocationServices& services,
    std::uint32_t bytes, std::uint32_t flags);
std::uint32_t FreeDispatch(AllocationServices& services, GuestAddress address,
    std::uint32_t flags);

} // namespace lo::semantic::gpu
