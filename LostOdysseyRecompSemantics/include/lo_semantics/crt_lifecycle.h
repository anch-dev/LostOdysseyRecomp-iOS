#pragma once

#include "lo_semantics/crt_allocation.h"
#include "lo_semantics/crt_thread_data.h"

namespace lo::semantic::gpu
{

// Only the kernel TLS operations and guest indirect callbacks stay external.
// Heap, CRT allocation, CRT release, and thread-record initialization use the
// recovered functions. The allocation-service boundaries inherited here cover
// the kernel heap/VM and error-handler calls made by those functions.
class CrtLifecycleServices : public CrtAllocationServices
{
public:
    virtual std::uint64_t GetTlsValue(std::uint32_t index) = 0;
    virtual void SetTlsValue(std::uint32_t index, std::uint64_t value) = 0;
    virtual std::uint64_t CallThreadDataGetter(GuestAddress function,
        std::uint64_t context, CrtThreadDataCall& call) = 0;
    virtual std::uint64_t BindThreadData(GuestAddress function,
        std::uint64_t context, std::uint64_t data,
        CrtThreadDataCall& call) = 0;
};

// 822CA048's 112-byte frame is the caller frame of 82B81778 and 823ADDC0.
// The latter functions build their own guest frames at the original offsets.
[[nodiscard]] std::uint64_t GetCrtThreadDataComposed(GuestMemory& memory,
    CrtLifecycleServices& services, CrtThreadDataCall& call,
    GuestAddress caller_sp);

} // namespace lo::semantic::gpu
