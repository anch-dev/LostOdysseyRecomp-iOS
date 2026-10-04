#pragma once
#include "lo_semantics/crt_stream_byte_read_context.h"
namespace lo::semantic::gpu::crt_reader_next61 {
using Registers=crt_async_status_transfer::Registers;
using Dependencies=crt_stream_close_shared_lower::Dependencies;
// Exact bounded line reader and external cleanup, with actual lock/handle
// integer leaves. Byte-reader, errno, unlock and native imports retain their
// accepted selected ABI; runtime concurrency/fault behavior remains bounded.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
