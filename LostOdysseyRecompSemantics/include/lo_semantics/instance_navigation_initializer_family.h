#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu::instance_navigation_initializer_family
{

// Incoming nonvolatile registers are needed for the original stack saves.
// The caller stack pointer addresses ordinary mapped guest memory.
struct EntryAbi
{
    GuestAddress caller_sp;
    std::uint64_t incoming_lr;
    std::uint64_t incoming_r30;
    std::uint64_t incoming_r31;
};

// Construct an RPNavi instance through its real recovered base initializer,
// then bind the derived vtable. Eight null-guarded public wrappers and their
// eight direct callees are accepted. Returns false for an unknown address.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, const EntryAbi& abi, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_navigation_initializer_family
