#pragma once

#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/float_triplet_transfer.h"

namespace lo::semantic::gpu::object_record_update
{
struct Registers : crt_stream_operations::Registers
{
    std::uint64_t f0_bits = 0;
    std::uint32_t cached_fp_control = 0;
};
using NativeServices = float_triplet_transfer::NativeServices;

// Keep the live record snapshot, ordered word additions and conditional output.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& registers);
}
