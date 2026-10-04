#pragma once
#include "lo_semantics/crt_close_recursive_buffer_context.h"
#include "lo_semantics/float_triplet_transfer.h"
namespace lo::semantic::gpu::crt_reader_float61 {
using Registers=crt_close_recursive_buffer_context::Registers;
struct Dependencies {crt_close_recursive_buffer_context::GuestServices& guest;float_triplet_transfer::NativeServices& fp;};
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
