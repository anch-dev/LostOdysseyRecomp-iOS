#pragma once

#include "lo_semantics/object_curve_record_displacement.h"

namespace lo::semantic::gpu::object_curve_node_record_update
{
using Registers = object_curve_record_displacement::Registers;
using NativeServices = object_curve_record_displacement::NativeServices;

// Actual 82628970 node lookup and indexed-record update. All direct curve
// and normalization calls reuse restored bodies; guest vtable+116 remains
// a mutable selected-register boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_curve_node_record_update
