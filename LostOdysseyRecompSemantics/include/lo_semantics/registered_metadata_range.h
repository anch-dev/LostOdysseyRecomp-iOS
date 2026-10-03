#pragma once

#include "lo_semantics/registered_metadata_words.h"

#include <cstdint>

namespace lo::semantic::gpu::registered_metadata_range
{

// The bounded entry ABI includes the nonvolatile registers and the guest stack
// touched by these five bodies. Generic volatile registers are adapter state.
struct EntryAbi
{
    std::uint64_t caller_sp;
    std::uint64_t incoming_lr;
    std::uint64_t incoming_r30;
    std::uint64_t incoming_r31;
};

struct Result
{
    std::uint64_t r3;
    std::uint64_t lr;
    std::uint64_t r30;
    std::uint64_t r31;
};

// 8249B130 takes owner/offset/size in r3/r4/r5. The four parent entries take
// their object in r3 and load owner from object[52]. The returned r3 is the
// final AppendMetadataWord index, including its full-width underflow behavior.
// Unknown addresses return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t object_register,
    std::uint64_t offset_register, std::uint64_t size_register,
    const EntryAbi& abi, Result& result);

} // namespace lo::semantic::gpu::registered_metadata_range
