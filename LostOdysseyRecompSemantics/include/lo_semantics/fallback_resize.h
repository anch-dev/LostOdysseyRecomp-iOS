#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/manager_lock.h"

namespace lo::semantic::gpu
{

// Indirect calls outside the recovered pair. The caller supplies the full
// guest registers, including high halves that the original passes unchanged.
class FallbackResizeServices
{
public:
    virtual ~FallbackResizeServices() = default;
    // Historical boundary name: 827C4FA0 flushes cached pointers, not capacity.
    virtual std::uint64_t GrowPointerVector(
        std::uint64_t vector_register, std::uint64_t value_register) = 0;
    virtual std::uint64_t ReallocateThroughManager(
        GuestAddress method, std::uint64_t manager_register,
        std::uint64_t old_register, std::uint64_t size_register,
        std::uint64_t argument_register) = 0;
    virtual std::uint64_t ReleaseThroughManager(
        GuestAddress method, std::uint64_t manager_register,
        std::uint64_t old_register) = 0;
};

// 827C5050. The optional 827C4FA0 call flushes a full pointer cache through
// the historically named GrowPointerVector boundary.
// Its full r3 return remains live; count and backing pointer are reloaded
// after that callback. ABI frame saves/backchain are adapter responsibilities.
[[nodiscard]] std::uint64_t AppendPointerToVector(
    GuestMemory& memory, FallbackResizeServices& services,
    std::uint64_t vector_register, std::uint64_t value_register);

// 82295530. frame_base is the post-prologue 176-byte guest r1; frame+80
// remains live across lock and manager callbacks. r13 points to the thread
// state. Original incoming r3 is unused. ABI saves/backchain are external.
[[nodiscard]] std::uint64_t ResizeFallbackAllocation(
    GuestMemory& memory, ManagerLockServices& lock_services,
    FallbackResizeServices& resize_services,
    std::uint64_t old_register, std::uint64_t size_register,
    std::uint64_t argument_register, GuestAddress r13,
    GuestAddress frame_base);

} // namespace lo::semantic::gpu
