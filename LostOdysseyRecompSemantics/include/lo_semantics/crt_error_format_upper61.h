#pragma once
#include "lo_semantics/crt_narrow_formatter61.h"
namespace lo::semantic::gpu::crt_error_format_upper61 {
using Registers=crt_narrow_formatter61::Registers;
using Dependencies=crt_narrow_formatter61::Dependencies;
// Exact stream varargs formatter and error prefix/message upper. Complete
// selected context, with accepted lower/native ABI limits preserved.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
