#include "lo_semantics/ring_reservation.h"

namespace lo::semantic::gpu::ring_reservation
{
namespace
{
GuestAddress Low(std::uint64_t value)
{
    return static_cast<GuestAddress>(value);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    SynchronizationServices& synchronization, std::uint64_t descriptor,
    std::uint64_t ring, std::uint64_t requested_bytes,
    Registers& registers, std::uint64_t& result)
{
    if (address != 0x82290AB8u)
        return false;
    memory.WriteU32(Low(descriptor), Low(ring));
    registers.r9 = 1;
    registers.r11 = memory.ReadU32(Low(ring) + 24u);
    registers.r10 = registers.r11 + requested_bytes;
    registers.r11 -= 1u;
    registers.r10 -= 1u;
    registers.r11 = registers.r10 & ~registers.r11;
    memory.WriteU32(Low(descriptor) + 8u, Low(registers.r11));
    memory.WriteU32(Low(ring) + 16u, Low(registers.r9));

    for (;;)
    {
        registers.r11 = memory.ReadU32(Low(descriptor));
        registers.r8 = memory.ReadU32(Low(registers.r11) + 20u);
        registers.r10 = memory.ReadU32(Low(registers.r11) + 8u);
        if (Low(registers.r8) > Low(registers.r10))
        {
            registers.r10 = memory.ReadU32(Low(registers.r11) + 8u);
            registers.r9 = memory.ReadU32(Low(descriptor) + 8u);
            registers.r10 += registers.r9;
            if (Low(registers.r10) >= Low(registers.r8))
                continue;
        }
        registers.r10 = memory.ReadU32(Low(registers.r11) + 8u);
        registers.r9 = memory.ReadU32(Low(descriptor) + 8u);
        registers.r7 = memory.ReadU32(Low(registers.r11) + 4u);
        registers.r10 += registers.r9;
        if (Low(registers.r10) <= Low(registers.r7))
            break;

        registers.r10 = memory.ReadU32(Low(registers.r11));
        if (Low(registers.r8) == Low(registers.r10))
            continue;
        registers.r10 = memory.ReadU32(Low(registers.r11) + 8u);
        memory.WriteU32(Low(registers.r11) + 12u, Low(registers.r10));
        synchronization.LightweightSync();
        // The descriptor/ring can alias the stores or synchronization boundary.
        registers.r11 = memory.ReadU32(Low(descriptor));
        registers.r10 = memory.ReadU32(Low(registers.r11));
        memory.WriteU32(Low(registers.r11) + 8u, Low(registers.r10));
    }
    registers.r11 = memory.ReadU32(Low(descriptor));
    registers.r11 = memory.ReadU32(Low(registers.r11) + 8u);
    memory.WriteU32(Low(descriptor) + 4u, Low(registers.r11));
    result = descriptor;
    return true;
}
} // namespace lo::semantic::gpu::ring_reservation
