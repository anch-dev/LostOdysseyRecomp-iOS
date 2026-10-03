#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// Two indirect manager methods remain guest ABI boundaries.
class ManagerResizeServices
{
public:
    virtual ~ManagerResizeServices() = default;
    [[nodiscard]] virtual std::uint64_t AllocateStorage(
        GuestAddress method, std::uint64_t manager_register,
        std::uint64_t bytes_register, std::uint64_t flags_register) = 0;
    virtual void FreeStorage(
        GuestAddress method, std::uint64_t manager_register,
        std::uint64_t address_register) = 0;
};

// 822A0738. Returns the full r3 register produced by the original address
// arithmetic. Inputs correspond to original r3, r4, r5, r6 respectively.
[[nodiscard]] std::uint64_t FindPrimaryResizeNode(GuestMemory& memory,
    std::uint64_t manager_register, std::uint64_t requested_register,
    std::uint64_t mode_register, std::uint64_t group_register);

// 82295950. frame_base is the post-prologue 144-byte guest r1. The direct
// forward copy uses frame_base-8 as its observable spill/reload slot.
[[nodiscard]] std::uint64_t ResizePrimaryManagerStorage(GuestMemory& memory,
    ManagerResizeServices& services, std::uint64_t manager_register,
    std::uint64_t old_storage_register, std::uint64_t new_bytes_register,
    std::uint64_t flags_register, GuestAddress frame_base);

} // namespace lo::semantic::gpu
