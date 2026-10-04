#pragma once

#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/crt_stream_bulk_close_routes.h"
#include "lo_semantics/crt_stream_open_wrapper.h"

namespace lo::semantic::gpu::crt_stream_close_shared_lower
{
using Registers = crt_async_status_transfer::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void EnterCriticalSection(GuestMemory& memory,
        Registers& state) = 0;
    // RtlUnwind normally does not return. A returning service permits the
    // translated epilogue to be compared without claiming native unwind.
    virtual void RtlUnwind(GuestMemory& memory, Registers& state) = 0;
};

struct Dependencies
{
    crt_stream_bulk_close_routes::Dependencies close;
    crt_stream_open_wrapper::Dependencies open;
    NativeServices& native;
};

// Eight exact PPC bodies close the DF2150 -> DF1FC0 direct-call chain.
// DF2150, DF2118 and DF6908 are catalog-external validation funclets.
// Selected GPR/FPR, CR0/CR1/CR6/CR7, LR/CTR, XER and FP control remain live;
// accepted lower and native service boundaries keep their documented scope.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Validation-only composition for already accepted direct callees. Unknown
// addresses leave the selected full context and guest RAM unchanged.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry,
    GuestMemory& memory, Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_close_shared_lower
