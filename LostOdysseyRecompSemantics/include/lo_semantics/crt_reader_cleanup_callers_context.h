#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
namespace lo::semantic::gpu::crt_reader_cleanup_callers_context {
using Registers = crt_close_recursive_buffer_context::Registers;
using GuestServices = crt_close_recursive_buffer_context::GuestServices;
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, GuestServices&, Registers&);
}
