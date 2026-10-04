#pragma once
#include "lo_semantics/crt_stream_operations.h"
namespace lo::semantic::gpu::crt_thread_error_routes
{
using Registers = crt_stream_operations::Registers;
class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void KeTlsGetValue(GuestMemory&, Registers&) = 0;
    virtual void KeTlsSetValue(GuestMemory&, Registers&) = 0;
};
// 822CA128 obtains TLS state or installs the current guest default. 822CA188
// and its real 822CA180 tail entry store a thread error only when +336 is zero.
// System TLS internals, faults, MMIO and concurrency remain service boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& services, Registers& state);
}
