#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_fill_initializer_family
{

struct EntryAbi
{
    std::uint64_t caller_sp;
    std::uint64_t incoming_lr;
    std::uint64_t incoming_r31;
};

struct Result
{
    std::uint64_t r3;
    std::uint64_t lr;
    std::uint64_t r31;
};

// Four UI initializers clear a 100-byte field through FillGuestMemory.
// The two framed entries preserve their 96-byte frame writes and reloads,
// including aliases with the object. Non-null returns full r3 + field offset.
// Unknown addresses leave memory and result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, const EntryAbi& abi, Result& result);

} // namespace lo::semantic::gpu::instance_fill_initializer_family
