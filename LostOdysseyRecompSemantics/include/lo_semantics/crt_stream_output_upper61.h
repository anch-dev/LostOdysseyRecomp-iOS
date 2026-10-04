#pragma once
#include "lo_semantics/crt_reader_next61.h"
namespace lo::semantic::gpu::crt_stream_output_upper61 {
using Registers=crt_async_status_transfer::Registers;
using Dependencies=crt_stream_close_shared_lower::Dependencies;
// Actual prompt wrapper and format shim. Formatting/puts retain established
// selected GPR ABI; fgets uses its validated Full context. No format returns
// or IO outcomes are synthesized. Faults/concurrency/native internals remain open.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
