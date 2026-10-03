#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/manager_facade.h"

#include <cstdint>

namespace lo::semantic::gpu::manager_object_registration
{

struct FrameRegisters
{
    std::uint64_t lr;
    std::uint64_t r31;
};

// External callbacks reached through the already recovered constructor and
// registration helpers. The stack address is the full guest r1 at that call.
class Services
{
public:
    virtual ~Services() = default;
    virtual std::uint64_t Register(GuestAddress target,
        std::uint64_t incoming_r3, std::uint64_t caller_sp) = 0;
    virtual std::uint64_t CallExternalGetter(GuestAddress target,
        std::uint64_t incoming_r3, std::uint64_t caller_sp) = 0;
    virtual std::uint64_t CallExternalRegistration(GuestAddress target,
        std::uint64_t incoming_r3, std::uint64_t caller_sp) = 0;
    virtual std::uint64_t CallReadyMethod(GuestAddress method,
        std::uint64_t receiver, std::uint64_t caller_sp) = 0;
};

// 82376EE8: obtain the shared registration object. The initial singleton
// load, construction, callback, and final reload retain their original order.
// The lower family helpers are reused; their own volatile ABI effects are not
// part of this boundary. Unknown addresses have no effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services, Services& services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

struct Utf16CopyResult
{
    std::uint64_t r3;
    std::uint64_t r4;
    std::uint64_t r5;
};

// 8232D318: copy up to low32(count) UTF-16 code units, then pad after an
// early terminator. GuestMemory bounds the requested transfer.
[[nodiscard]] Utf16CopyResult CopyUtf16Padded(GuestMemory& memory,
    std::uint64_t destination_r3, std::uint64_t source_r4,
    std::uint64_t count_r5);

} // namespace lo::semantic::gpu::manager_object_registration
