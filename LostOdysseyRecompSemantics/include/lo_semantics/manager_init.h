#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// Guest implementations of the raw allocator, two constructors, and the two
// vtable calls remain outside this recovered 827C5F38 control flow.
class ManagerInitServices
{
public:
    virtual ~ManagerInitServices() = default;

    virtual std::uint64_t AllocateRaw(std::uint32_t bytes) = 0;
    virtual std::uint64_t ConstructPrimary(std::uint64_t allocation_register) = 0;
    virtual std::uint64_t ConstructFallback(std::uint64_t allocation_register,
                                            GuestAddress current_manager) = 0;
    virtual std::uint64_t CallMethod(GuestAddress method,
                                     std::uint64_t receiver_register) = 0;
};

// 827C5F38. frame_base is its post-prologue 112-byte guest frame. The +80
// allocation slot is observable by callees. ABI saves and the backchain are
// adapter responsibilities, as in the other recovered heap functions.
[[nodiscard]] std::uint64_t InitializeManager(GuestMemory& memory,
    ManagerInitServices& services, GuestAddress frame_base);

} // namespace lo::semantic::gpu
