#pragma once
#include "lo_semantics/crt_stream_close_shared_lower.h"
namespace lo::semantic::gpu::crt_format_frame61 {
using Registers=crt_stream_close_shared_lower::Registers;
using Dependencies=crt_stream_close_shared_lower::Dependencies;
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
void ApplySupport_DF2AC8(GuestMemory&,Dependencies,Registers&);
}
