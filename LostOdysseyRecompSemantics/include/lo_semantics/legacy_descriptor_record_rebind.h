#pragma once
#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_descriptor_record_rebind
{
using Registers = crt_stream_operations::Registers;
// Complete indexed/tagged descriptor rebinding, including caller-visible scratch.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
