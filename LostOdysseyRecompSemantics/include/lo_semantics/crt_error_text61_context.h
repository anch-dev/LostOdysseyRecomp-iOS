#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
namespace lo::semantic::gpu::crt_error_text61_context
{
using Registers=crt_async_status_transfer::Registers;
// Exact signed error index lookup and catalog-external pointer leaves.
// Ordinary RAM only; native/fault/MMIO/concurrency/runtime unvalidated.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,Registers& state);
} // namespace lo::semantic::gpu::crt_error_text61_context
