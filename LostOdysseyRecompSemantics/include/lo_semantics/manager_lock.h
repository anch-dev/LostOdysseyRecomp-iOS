#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// 829664E8 sets the full r3 register to zero.
[[nodiscard]] std::uint64_t ReturnZeroStatus();

class ManagerLockServices
{
public:
    virtual ~ManagerLockServices() = default;
    virtual std::uint64_t TryEnterCriticalSection(
        std::uint64_t section_register) = 0;
    virtual std::uint64_t CallMethod(GuestAddress method,
                                     std::uint64_t receiver_register) = 0;
    virtual std::uint64_t LeaveCriticalSection(
        std::uint64_t section_register) = 0;
};

// 822958F8 stores the lock pointer through holder_register, then tries its
// embedded critical section until the callback returns a nonzero low word.
// ABI saves/backchain are adapter responsibilities.
[[nodiscard]] std::uint64_t StoreLockAndWaitForEnter(GuestMemory& memory,
    ManagerLockServices& services, std::uint64_t holder_register,
    std::uint64_t lock_register);

// 827C5688. frame_base is the post-prologue 112-byte guest frame. Its +80
// slot remains live across the indirect manager call and determines the leave
// address. ABI saves/backchain are adapter responsibilities.
[[nodiscard]] std::uint64_t InvokeManagerUnderLock(GuestMemory& memory,
    ManagerLockServices& services, GuestAddress frame_base);

} // namespace lo::semantic::gpu
