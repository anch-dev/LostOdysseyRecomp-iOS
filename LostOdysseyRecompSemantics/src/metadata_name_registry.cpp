#include "lo_semantics/metadata_name_registry.h"

namespace lo::semantic::gpu::metadata_name_registry
{
namespace
{
struct Entry { GuestAddress name; std::uint32_t id; };
constexpr Entry kRecords[] = {
#include "metadata_name_registry_entries.inc"
};
static_assert(sizeof(kRecords) / sizeof(kRecords[0]) == 446);
constexpr GuestAddress kCrcGuard = 0x83371A98u;
constexpr GuestAddress kCrcTable = 0x832EE168u;
constexpr GuestAddress kBuckets = 0x832EE568u;
constexpr GuestAddress kIdIndex = 0x833690D0u;
constexpr GuestAddress kReady = 0x83246260u;

void InitializeTables(GuestMemory& memory)
{
    const std::uint32_t guard = memory.ReadU32(kCrcGuard);
    if ((guard & 1u) == 0)
    {
        memory.WriteU32(kCrcGuard, guard | 1u);
        for (std::uint32_t index = 0; index != 256; ++index)
        {
            std::uint32_t value = index << 24u;
            for (unsigned bit = 0; bit != 8; ++bit)
            {
                const bool high = (value & 0x80000000u) != 0;
                value <<= 1u;
                if (high) value ^= 0x04C11DB7u;
            }
            memory.WriteU32(kCrcTable + index * 4u, value);
        }
    }
    memory.WriteU32(kIdIndex, 0);
    memory.WriteU32(kIdIndex + 4u, 0);
    memory.WriteU32(kReady, 1);
    memory.WriteU32(kIdIndex + 8u, 0);
    for (std::uint32_t index = 0; index != 4096; ++index)
        memory.WriteU32(kBuckets + index * 4u, 0);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& records, ArrayResizeServices& arrays,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    if (address != 0x823F4700u)
        return false;
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    memory.WriteU32(sp - 96u, sp);
    const std::uint64_t nested_sp = caller_sp - 96u;
    InitializeTables(memory);
    std::uint32_t ordinal = 0;
    for (const auto& entry : kRecords)
    {
        // All original parameter-load variants are immediate-only and leave
        // these same r3-r6 values before the call. Volatile scratch is outside
        // this boundary; the exact variants are retained in the manifest.
        frame.lr = 0x823F47D4u + ordinal * 28u;
        std::uint64_t record = 0;
        (void)metadata_name_record::Apply(0x823F7B08u, memory, records,
            0xFFFFFFFF00000000ull | entry.name, entry.id, 0, 0,
            nested_sp, frame, record);
        metadata_name_index::FrameRegisters index_frame{
            0x823F47D8u + ordinal * 28u, frame.r27, frame.r28,
            frame.r29, frame.r30, frame.r31, 0, 0};
        (void)metadata_name_index::Apply(0x823F44E8u, memory, arrays,
            record, nested_sp, index_frame, result);
        frame = {index_frame.lr, index_frame.r27, index_frame.r28,
            index_frame.r29, index_frame.r30, index_frame.r31};
        ++ordinal;
    }
    frame.lr = memory.ReadU32(sp - 8u);
    return true;
}
} // namespace lo::semantic::gpu::metadata_name_registry
