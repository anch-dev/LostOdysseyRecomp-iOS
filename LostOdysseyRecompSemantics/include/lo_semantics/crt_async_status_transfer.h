#pragma once

#include "lo_semantics/crt_status_error.h"

#include <array>

namespace lo::semantic::gpu::crt_async_status_transfer
{
using Condition = crt_status_error::Condition;
struct Registers
{
    std::array<std::uint64_t, 32> r{};
    std::array<std::uint64_t, 32> fpr_bits{};
    std::uint64_t lr = 0, ctr = 0;
    std::uint32_t cached_fp_control = 0;
    std::uint8_t xer_so = 0, xer_ca = 0;
    Condition cr0{}, cr6{};
};
class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void CallIndirect(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
    virtual void NtWaitForSingleObjectEx(GuestMemory& memory,
        Registers& state) = 0;
    virtual void NtStatusToDosError(GuestMemory& memory,
        Registers& state) = 0;
};

// Actual 82BE2DD8 async status handling. 827CA628 is the accepted direct
// lower body. The mutable import and function-table targets remain services.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::crt_async_status_transfer
