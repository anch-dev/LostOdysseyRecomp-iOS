#pragma once
#include "lo_semantics/crt_stream_close_shared_lower.h"
namespace lo::semantic::gpu::crt_flush_full61 {
using Registers=crt_async_status_transfer::Registers;
class NativeServices {
public:
    virtual ~NativeServices()=default;
    virtual void EnterCriticalSection(GuestMemory&,Registers&)=0;
    virtual void LeaveCriticalSection(GuestMemory&,Registers&)=0;
    virtual void NtFlushBuffersFile(GuestMemory&,Registers&)=0;
    virtual void RtlNtStatusToDosError(GuestMemory&,Registers&)=0;
};
struct Dependencies {crt_stream_close_shared_lower::Dependencies accepted;NativeServices& native;};
// Zero-credit selected Full ABI extension of already mapped 82B81F78.
// Exact flush, status, lock and cleanup bodies keep mutable native register
// state live. Lock initialization/indexed-lock and errno getters retain
// existing selected lower ABI; import internals/faults/concurrency stay open.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
