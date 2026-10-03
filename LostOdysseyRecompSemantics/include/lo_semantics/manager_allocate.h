#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

class ManagerAllocateServices
{
public:
    virtual ~ManagerAllocateServices() = default;
    virtual std::uint64_t TryEnterCriticalSection(
        std::uint64_t section_register) = 0;
    virtual std::uint64_t AllocateVirtual(GuestAddress method,
        std::uint64_t receiver_register, std::uint64_t arg4_register,
        std::uint64_t arg5_register) = 0;
    virtual std::uint64_t LeaveCriticalSection(
        std::uint64_t section_register) = 0;
};

// 823F2670: writes arg5 and 1 to two object fields, calls the vtable +4
// entry with r5 = 8, then clears both fields even if that call changed them.
[[nodiscard]] std::uint64_t AllocateThroughPrimaryManager(
    GuestMemory& memory, ManagerAllocateServices& services,
    std::uint64_t receiver_register, std::uint64_t arg4_register,
    std::uint64_t arg5_register);

// 823F25E0: frame_base is the post-prologue 128-byte frame. Its +80 lock
// holder is live across the callback and is reloaded for Leave. ABI saves and
// backchain stores are supplied by the PPCContext adapter.
[[nodiscard]] std::uint64_t AllocateThroughFallbackManager(
    GuestMemory& memory, ManagerAllocateServices& services,
    GuestAddress frame_base, std::uint64_t arg4_register,
    std::uint64_t arg5_register);

} // namespace lo::semantic::gpu
