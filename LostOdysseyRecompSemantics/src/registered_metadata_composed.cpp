#include "lo_semantics/registered_metadata_composed.h"

namespace lo::semantic::gpu::registered_metadata_composed
{

bool Apply(GuestAddress address, GuestMemory& memory,
    VirtualServices& services, std::uint64_t incoming_r3,
    TailControl& control, std::uint64_t& result)
{
    if (address != 0x826d6c10u)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    const GuestAddress vtable = memory.ReadU32(object);
    const GuestAddress method = memory.ReadU32(vtable + 292u) & ~3u;
    result = services.TailCall(method, memory, incoming_r3, control);
    return true;
}

std::uint64_t CopyUtf16UntilNull(GuestMemory& memory,
    std::uint64_t destination, std::uint64_t source,
    std::uint64_t& source_after)
{
    std::uint64_t write_cursor = destination;
    std::uint64_t read_cursor = source;
    std::uint16_t code_unit;
    do
    {
        code_unit = memory.ReadU16(static_cast<GuestAddress>(read_cursor));
        read_cursor += 2;
        memory.WriteU16(static_cast<GuestAddress>(write_cursor), code_unit);
        write_cursor += 2;
    } while (code_unit != 0);
    source_after = read_cursor;
    return destination;
}

} // namespace lo::semantic::gpu::registered_metadata_composed
