#include "lo_semantics/cache.h"

#include <bit>

namespace lo::semantic::gpu
{

void FlushDataCacheRange(CacheServices& services, GuestAddress begin, GuestAddress end)
{
    if (begin - 0x7f100000u <= 0x07efffffu)
        return;

    GuestAddress line = begin & 0xffffff80u;
    const GuestAddress aligned_end = (end + 127u) & 0xffffff80u;
    // Both endpoints are 128-byte aligned, so the original srawi/addze has
    // no discarded low bits. Keep its signed interpretation before converting
    // the loop count back to the original unsigned bit pattern.
    const auto byte_count = std::bit_cast<std::int32_t>(aligned_end - line);
    const auto line_count = static_cast<std::uint32_t>(byte_count / 128);
    for (std::uint32_t index = 0; index < line_count; ++index)
    {
        services.FlushLine(line);
        line += 128;
    }
    services.Sync();
}

} // namespace lo::semantic::gpu
