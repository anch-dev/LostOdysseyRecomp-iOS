#pragma once
#include "lo_semantics/crt_narrow_formatter61.h"
namespace lo::semantic::gpu::crt_narrow_callers61 {
using Registers=crt_narrow_formatter61::Registers;
using Dependencies=crt_narrow_formatter61::Dependencies;
// Actual narrow buffer/stdout wrappers and cleanup. Narrow engine retains
// Full context; other accepted lock/output/errno lowers retain selected ABI.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
