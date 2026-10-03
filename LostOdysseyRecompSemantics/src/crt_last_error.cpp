#include "lo_semantics/crt_last_error.h"

namespace lo::semantic::gpu::crt_last_error
{
bool Apply(GuestAddress address, GuestMemory& memory, Registers& registers)
{
    if (address != 0x822ca108u && address != 0x822ca100u) return false;
    registers.r11 = memory.ReadU32(static_cast<GuestAddress>(registers.r13 + 336u));
    registers.cr6 = {0, static_cast<std::uint8_t>(registers.r11 != 0u),
        static_cast<std::uint8_t>(registers.r11 == 0u), registers.xer_so};
    if (registers.r11 != 0u) registers.r3 = 0;
    else
    {
        registers.r11 = memory.ReadU32(static_cast<GuestAddress>(registers.r13 + 256u));
        registers.r3 = memory.ReadU32(static_cast<GuestAddress>(registers.r11 + 352u));
    }
    return true;
}
} // namespace lo::semantic::gpu::crt_last_error
