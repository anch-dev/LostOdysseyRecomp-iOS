#pragma once
#include "lo_semantics/crt_stream_close_shared_lower.h"
#include "lo_semantics/crt_stream_flush_context.h"
namespace lo::semantic::gpu::crt_stream_reader_final61 {
using Registers=crt_async_status_transfer::Registers;
struct Dependencies {
    crt_stream_close_shared_lower::Dependencies accepted;
    crt_stream_flush_context::NativeServices& flush_native;
};
// Exact flush wrapper and integer sweep/core/cleanup helpers. Accepted
// stream write/format/lock/flush callees retain their existing selected ABI;
// native imports and unselected floating/control effects remain boundaries.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
// Selected lower bridge for the unchanged original-body oracle, zero credit.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
