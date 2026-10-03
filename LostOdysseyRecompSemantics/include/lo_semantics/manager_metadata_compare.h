#pragma once

#include "lo_semantics/crt_thread_data.h"
#include "lo_semantics/invalid_parameter.h"

namespace lo::semantic::gpu::manager_metadata_compare
{
// 822971E0 compares UTF16 strings with ASCII A-Z folding. Null inputs compose
// the recovered CRT errno lookup and invalid-parameter dispatcher. Exposed
// r3-r10/r13 and this entry's frame/LR are modeled; generic lower volatile ABI,
// lower frame writes, actual trap termination and faults/MMIO are excluded.
// Unknown addresses leave the call, result, LR and guest memory unchanged.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    CrtThreadDataServices& thread_services, InvalidParameterServices& invalid_services,
    InvalidParameterCall& call, std::uint64_t caller_sp,
    std::uint64_t& lr, std::uint64_t& result);
} // namespace lo::semantic::gpu::manager_metadata_compare
