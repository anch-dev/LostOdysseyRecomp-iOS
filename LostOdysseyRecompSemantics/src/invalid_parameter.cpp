#include "lo_semantics/invalid_parameter.h"

namespace lo::semantic::gpu
{

void ClearInvalidParameterState(GuestMemory& memory)
{
    memory.WriteU32(0x83378d64u, 0);
}

std::uint64_t ReportInvalidParameter(GuestMemory& memory,
    InvalidParameterServices& services, InvalidParameterCall& call)
{
    const GuestAddress handler = memory.ReadU32(0x83378e80u);
    if (handler != 0)
    {
        services.CallHandler(memory, handler & ~3u, call);
        return call.arguments[0];
    }

    call.arguments[0] = 2;
    ClearInvalidParameterState(memory);
    // 82B84D88 leaves the sign-extended lis base in r10; r11 holds the zero
    // written to guest memory and is outside InvalidParameterCall.
    call.arguments[7] = 0xffffffff83380000ull;
    services.Trap(call);
    return call.arguments[0];
}

} // namespace lo::semantic::gpu
