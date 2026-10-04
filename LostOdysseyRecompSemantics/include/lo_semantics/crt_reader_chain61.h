#pragma once
#include "lo_semantics/crt_stream_close_shared_lower.h"
namespace lo::semantic::gpu::crt_reader_chain61 {
using Registers=crt_async_status_transfer::Registers;
using Dependencies=crt_stream_close_shared_lower::Dependencies;
// Exact wide pushback including its integer conversion/tail/fill leaves.
// Existing errno/allocation services retain their selected ABI; faults,
// imports, concurrency and gameplay remain separately bounded.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
// Zero-credit external support: complete 82B7BC40 volatile-register/CTR/CR0
// effects and ordinary RAM fill, including the original caller LR.
void ApplySupport_B7BC40(GuestMemory&,Dependencies,Registers&);
// Zero-credit exact accepted conversion and stream-state bodies.
void ApplySupport_B86BE0(GuestMemory&,Dependencies,Registers&);
void ApplySupport_B81648(GuestMemory&,Dependencies,Registers&);
}
