#pragma once

#include "lo_semantics/metadata_name_record.h"
#include "lo_semantics/metadata_name_index.h"

namespace lo::semantic::gpu::metadata_name_registry
{
using FrameRegisters = metadata_name_record::FrameRegisters;

// 823F4700 initializes the guarded CRC table, clears the name/id indices and
// creates/inserts all 446 original records in order. Rows are data, not new
// entry points. The result retains the final insertion's full residual r3.
// Own frame and selected nonvolatile registers are modeled; generic lower
// volatile ABI, dynamic allocator internals and faults/MMIO remain bounded.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& records, ArrayResizeServices& arrays,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_name_registry
