#pragma once
#include "lo_semantics/crt_stream_scan_context.h"
namespace lo::semantic::gpu::crt_numeric_upper61_context
{
using Registers=crt_stream_scan_context::Registers;
struct Dependencies
{
    crt_stream_scan_context::Dependencies accepted;
    crt_stream_scan_context::GuestServices& callback;
};
// Exact string-scanner caller; fixed DF4AF8 executes the accepted actual scanner.
// Other live callback targets retain a full mutable guest boundary. Native ABI,
// fault/MMIO/concurrency/runtime behavior remains bounded.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state);
} // namespace lo::semantic::gpu::crt_numeric_upper61_context
