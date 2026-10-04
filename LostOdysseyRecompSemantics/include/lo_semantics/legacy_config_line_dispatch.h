#pragma once

#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/manager_facade.h"
#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::legacy_config_line_dispatch
{
using Registers = crt_stream_operations::Registers;

class VirtualCalls
{
public:
    virtual ~VirtualCalls() = default;
    // The vtable+260, vtable+284 and fallback method targets receive selected
    // live PPC state and may change mapped RAM and subsequent dispatch.
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

struct Dependencies
{
    CrtThreadDataServices& thread;
    InvalidParameterServices& invalid;
    ManagerFacadeServices& manager;
    registered_constructor_family::RegistrationServices& registration;
    VirtualCalls& virtual_calls;
};

// 82295DA8 dispatches each bounded UTF-16 configuration line. Its 8231A220
// manager-query lower is also complete; the registered singleton getter is
// reused. Only the three dynamic vtable calls remain explicit guest edges.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_config_line_dispatch
