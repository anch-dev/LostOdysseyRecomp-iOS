#include "lo_semantics/accessor_family.h"

namespace lo::semantic::gpu
{

std::uint64_t ReadField(GuestMemory& memory, GuestAddress base,
                        std::int32_t displacement, IntegerWidth width)
{
    return ReadFieldWith(memory, base, displacement, width);
}

void WriteField(GuestMemory& memory, GuestAddress base, std::int32_t displacement,
                IntegerWidth width, std::uint64_t value)
{
    WriteFieldWith(memory, base, displacement, width, value);
}

} // namespace lo::semantic::gpu
