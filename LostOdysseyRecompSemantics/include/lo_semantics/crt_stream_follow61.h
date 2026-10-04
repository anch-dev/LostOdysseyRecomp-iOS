#pragma once
#include "lo_semantics/crt_stream_reader_final61.h"
namespace lo::semantic::gpu::crt_stream_follow61 {
using Registers=crt_flush_full61::Registers;
struct Dependencies {crt_stream_reader_final61::Dependencies accepted;crt_flush_full61::Dependencies full_flush;};
// Mapped core composes verified mutable Full native flush; other accepted
// helpers retain their documented selected ABI and actual D80/E90 tables.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
