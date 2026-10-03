#pragma once

#include "lo_semantics/allocation_array.h"

#include <cstdint>

namespace lo::semantic::gpu::registered_metadata_words
{

// 822C42D8: advance the array count and grow capacity when the signed count
// exceeds signed capacity. Return the original count, zero extended.
// Array headers and resize-service arguments follow the existing guest32 API.
[[nodiscard]] std::uint64_t AddArrayElements(GuestMemory& memory,
    ArrayResizeServices& services, GuestAddress array, std::uint32_t count,
    std::uint32_t element_size, std::uint32_t argument);

// 825F41E8: append the live word at value, after any resize callback. A zero
// destination skips the word copy. Return the reloaded count minus one in r3.
[[nodiscard]] std::uint64_t AppendMetadataWord(GuestMemory& memory,
    ArrayResizeServices& services, GuestAddress array, GuestAddress value);

// Append a reviewed constant sequence to the array at object[52]+364.
// caller_sp is entry r1; the explicit outgoing word at frame+80 remains live.
// Unknown entries return false without effects. ABI saves are adapter concerns.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t object_register,
    GuestAddress caller_sp, std::uint64_t& result);

} // namespace lo::semantic::gpu::registered_metadata_words
