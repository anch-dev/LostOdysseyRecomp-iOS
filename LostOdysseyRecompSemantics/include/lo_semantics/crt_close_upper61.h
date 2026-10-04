#pragma once
#include "lo_semantics/crt_close_reader_callers_context.h"
namespace lo::semantic::gpu::crt_close_upper61 {
using Registers = crt_close_reader_callers_context::Registers;
using Dependencies = crt_close_reader_callers_context::Dependencies;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}
