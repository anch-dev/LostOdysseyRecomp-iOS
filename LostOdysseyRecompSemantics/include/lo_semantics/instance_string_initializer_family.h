#pragma once

#include "lo_semantics/registered_metadata_string.h"

#include <cstdint>

namespace lo::semantic::gpu::instance_string_initializer_family
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

// 82407300 initializes the object and its string at +72 from its constant
// source. 8240AE28 is its null-guarded tail entry. Both overwrite r4. The lower
// string allocator/copy behavior is the recovered InitializeString contract.
// This entry's frame saves/reloads are retained; the lower helper's generic
// ABI spills, volatile registers and fault/MMIO widths are excluded. U64
// stack saves are represented by two ordinary word accesses.
// Unknown addresses return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, const EntryAbi& abi, Result& result);

} // namespace lo::semantic::gpu::instance_string_initializer_family
