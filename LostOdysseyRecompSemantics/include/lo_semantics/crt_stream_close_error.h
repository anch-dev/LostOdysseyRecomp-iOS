#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_stream_close_error
{
using Registers = crt_stream_operations::Registers;

// 82BE1B80 calls the mutable guest function at [0x831e7df4]+4. The
// callback receives the live selected PPC context, including CTR and LR.
class GuestServices
{
public:
    virtual ~GuestServices() = default;
    virtual void CallIndirect(GuestMemory& memory, GuestAddress target,
        Registers& state) = 0;
};

struct Dependencies
{
    crt_stream_operations::Dependencies accepted;
    GuestServices& guest;
};

// Complete generated bodies for 82B87A38, 82BE1B80 and 82B86190.
// Ordinary guest RAM and selected GPR/CR/XER/CTR/LR are modeled. The
// mutable guest target remains an explicit selected-context boundary.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_close_error
