#pragma once

#include "lo_semantics/legacy_descriptor_recursive_copy.h"

namespace lo::semantic::gpu::legacy_descriptor_recursive_copy_caller
{
using Registers = legacy_descriptor_recursive_copy::Registers;
using Dependencies = legacy_descriptor_recursive_copy::Dependencies;

// Complete 83026E18 iterator/copy caller, its 82FFD208 cursor advance, and
// 82F99F48 diagnostic frame. Accepted recursive/attachment/value semantics
// compose directly; other guest children retain mutable selected context.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_recursive_copy_caller
