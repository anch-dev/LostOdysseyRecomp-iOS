#pragma once

#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/crt_free_context.h"
#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/crt_utf8_conversion_routes.h"
#include "lo_semantics/heap_allocation_context.h"

namespace lo::semantic::gpu::crt_stream_read_routes
{
using Registers = crt_async_status_transfer::Registers;

struct Dependencies
{
    crt_stream_operations::Dependencies stream;
    crt_async_status_transfer::NativeServices& async_native;
    crt_utf8_conversion_routes::NativeServices& conversion_native;
    heap_allocation_context::BoundaryServices& heap;
    crt_free_context::LowerCalls& free_lower;
};

// Actual 82B85448 read body. Accepted stream/error, async, allocation/free,
// and UTF8 conversion bodies are invoked with their documented selected ABI.
// Heap guest helpers and kernel/RTL imports keep the accepted lower boundary;
// only the selected GPR/FPR, LR/CTR, CR0/CR6, XER and CSR state is compared.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_read_routes
