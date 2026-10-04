#pragma once
#include "lo_semantics/crt_reader_float61.h"
namespace lo::semantic::gpu::crt_reader_sort_float61 {
using Registers=crt_reader_float61::Registers;
using Dependencies=crt_reader_float61::Dependencies;
// Exact floating threshold/reset and mutable table+12 guest cleanup.
// Host flush control uses the same native boundary as accepted float append.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
