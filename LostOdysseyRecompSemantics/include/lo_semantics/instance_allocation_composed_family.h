#pragma once

#include "lo_semantics/string_property_initializer.h"

namespace lo::semantic::gpu::instance_allocation_composed_family
{
using FrameRegisters = string_property_initializer::FrameRegisters;

// The bit-word resize dispatch is a guest vtable boundary. Preserve its full
// return register, live nonvolatile state, LR and full caller SP; callbacks may
// mutate guest memory. Other volatile registers are outside this contract.
class BitWordResizeServices
{
public:
    virtual ~BitWordResizeServices() = default;
    virtual std::uint64_t Resize(GuestAddress method, GuestMemory& memory,
        std::uint64_t manager, std::uint64_t storage, std::uint64_t bytes,
        std::uint64_t alignment, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
};

// 823058F0 resizes the storage for a bit array, 825BA620 initializes a
// bit-array holder, and 825BA858 composes a World property and two holders.
// Unknown entries leave memory, registers and the result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& string_resize_services,
    ManagerFacadeServices& manager_services,
    BitWordResizeServices& bit_resize_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::instance_allocation_composed_family
