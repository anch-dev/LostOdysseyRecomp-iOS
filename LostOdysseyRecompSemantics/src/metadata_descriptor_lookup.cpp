#include "lo_semantics/metadata_descriptor_lookup.h"

#include <array>
#include <limits>

namespace lo::semantic::gpu::metadata_descriptor_lookup
{
namespace
{
constexpr GuestAddress kOwnedBuckets = 0x832f2568u;
constexpr GuestAddress kGlobalBuckets = 0x832fa568u;
constexpr std::uint64_t kSentinelName = 0xffffffff8218d870ull;

std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}
void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
auto SavedRegisters(FrameRegisters& frame)
{
    return std::array{&frame.r24, &frame.r25, &frame.r26, &frame.r27,
        &frame.r28, &frame.r29, &frame.r30, &frame.r31};
}
void SaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first, std::uint32_t size)
{
    const auto sp = static_cast<GuestAddress>(caller_sp);
    const auto registers = SavedRegisters(frame);
    for (unsigned index = first - 24u; index < registers.size(); ++index)
        Write64(memory, sp - (72u - index * 8u), *registers[index]);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - size, sp);
}
void RestoreFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, unsigned first)
{
    const auto sp = static_cast<GuestAddress>(caller_sp);
    const auto registers = SavedRegisters(frame);
    for (unsigned index = first - 24u; index < registers.size(); ++index)
        *registers[index] = Read64(memory, sp - (72u - index * 8u));
    frame.lr = memory.ReadU32(sp - 8u);
}
struct Services
{
    metadata_name_record::Services& records;
    ArrayResizeServices& arrays;
    CrtThreadDataServices& threads;
    InvalidParameterServices& invalid;
};

GuestAddress CandidatePair(GuestMemory& memory, Services services,
    std::uint64_t sp, bool owned, FrameRegisters& frame)
{
    if (memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 4u)) != 0xffffffffu)
        return static_cast<GuestAddress>(frame.r31 + 44u);
    frame.lr = owned ? 0x8229d1e8u : 0x8229d2f4u;
    const std::uint64_t name = owned ? frame.r28 : frame.r29;
    std::uint64_t result = 0;
    (void)metadata_name_lookup::Apply(0x82296d30u, memory,
        services.records, services.arrays, services.threads, services.invalid,
        sp + 88u, name, 0, 1, 1, sp, frame, result);
    return static_cast<GuestAddress>(sp + 88u);
}

bool MatchesType(GuestMemory& memory, const FrameRegisters& frame)
{
    const auto requested = static_cast<GuestAddress>(frame.r26);
    if (requested == 0u) return true;
    GuestAddress type = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 52u));
    if (static_cast<std::uint32_t>(frame.r24) != 0u)
        return type == requested;
    while (type != 0u)
    {
        if (type == requested) return true;
        type = memory.ReadU32(type + 60u);
    }
    return false;
}

std::uint64_t LookupDescriptor(GuestMemory& memory, Services services,
    std::uint64_t type, std::uint64_t owner, std::uint64_t pair,
    std::uint64_t exact, std::uint64_t allow_owned, std::uint64_t mask,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    SaveFrame(memory, caller_sp, frame, 24u, 176u);
    const std::uint64_t sp = caller_sp - 176u;
    frame.r30 = owner;
    Write64(memory, static_cast<GuestAddress>(sp + 208u), pair);
    frame.r26 = type;
    frame.r24 = exact;
    frame.r27 = allow_owned;
    frame.r25 = mask;
    frame.r8 = mask;
    const bool owned = static_cast<std::uint32_t>(frame.r30) != 0u;
    std::uint32_t high = memory.ReadU32(static_cast<GuestAddress>(sp + 208u));
    std::uint32_t low = memory.ReadU32(static_cast<GuestAddress>(sp + 212u));
    std::uint32_t hash = high ^ low;
    if (owned)
    {
        frame.r27 = low;
        frame.r29 = high;
        hash ^= static_cast<std::uint32_t>(frame.r30);
    }
    else
    {
        frame.r28 = low;
        frame.r30 = high;
    }
    frame.r31 = memory.ReadU32((owned ? kOwnedBuckets : kGlobalBuckets) +
        ((hash << 2u) & 0x7ffcu));
    if (frame.r31 != 0u)
    {
        if (owned) frame.r28 = kSentinelName;
        else frame.r29 = kSentinelName;
        do
        {
            const GuestAddress source = CandidatePair(memory, services, sp, owned, frame);
            Write64(memory, static_cast<GuestAddress>(sp + 80u), Read64(memory, source));
            high = memory.ReadU32(static_cast<GuestAddress>(sp + 80u));
            low = memory.ReadU32(static_cast<GuestAddress>(sp + 84u));
            const bool same_pair = high == static_cast<std::uint32_t>(owned ? frame.r29 : frame.r30) &&
                low == static_cast<std::uint32_t>(owned ? frame.r27 : frame.r28);
            bool allowed = same_pair;
            if (allowed)
            {
                allowed = (Read64(memory, static_cast<GuestAddress>(frame.r31 + 8u)) & frame.r25) == 0u &&
                    frame.r25 != std::numeric_limits<std::uint64_t>::max();
            }
            if (allowed)
            {
                const std::uint32_t actual_owner = memory.ReadU32(
                    static_cast<GuestAddress>(frame.r31 + 40u));
                allowed = owned ? actual_owner == static_cast<std::uint32_t>(frame.r30) :
                    (static_cast<std::uint32_t>(frame.r27) != 0u || actual_owner == 0u);
            }
            if (allowed && MatchesType(memory, frame))
            {
                const std::uint64_t result = frame.r31;
                RestoreFrame(memory, caller_sp, frame, 24u);
                return result;
            }
            frame.r31 = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + (owned ? 20u : 16u)));
        } while (frame.r31 != 0u);
    }
    RestoreFrame(memory, caller_sp, frame, 24u);
    return 0;
}

std::uint64_t NextUnusedPair(GuestMemory& memory, Services services,
    std::uint64_t output, std::uint64_t owner, std::uint64_t descriptor,
    std::uint64_t pair, std::uint64_t caller_sp, FrameRegisters& frame)
{
    SaveFrame(memory, caller_sp, frame, 27u, 144u);
    const std::uint64_t sp = caller_sp - 144u;
    Write64(memory, static_cast<GuestAddress>(sp + 184u), pair);
    frame.r31 = output;
    frame.r27 = owner;
    frame.r30 = descriptor;
    frame.r29 = memory.ReadU32(static_cast<GuestAddress>(sp + 184u));
    if (frame.r29 == 0u && memory.ReadU32(static_cast<GuestAddress>(sp + 188u)) == 0u)
    {
        GuestAddress source = static_cast<GuestAddress>(frame.r30 + 44u);
        if (memory.ReadU32(static_cast<GuestAddress>(frame.r30 + 4u)) == 0xffffffffu)
        {
            frame.lr = 0x82400a94u;
            std::uint64_t result = 0;
            (void)metadata_name_lookup::Apply(0x82296d30u, memory,
                services.records, services.arrays, services.threads, services.invalid,
                sp + 80u, kSentinelName, 0, 1, 1, sp, frame, result);
            source = static_cast<GuestAddress>(sp + 80u);
        }
        Write64(memory, static_cast<GuestAddress>(sp + 80u), Read64(memory, source));
        frame.r29 = memory.ReadU32(static_cast<GuestAddress>(sp + 80u));
    }
    frame.r28 = static_cast<std::uint32_t>(frame.r27) == 0xffffffffu ? 1u : 0u;
    std::uint64_t found = 0;
    do
    {
        const std::uint32_t next = memory.ReadU32(static_cast<GuestAddress>(frame.r30 + 188u)) + 1u;
        frame.r8 = 0;
        memory.WriteU32(static_cast<GuestAddress>(frame.r31), static_cast<std::uint32_t>(frame.r29));
        memory.WriteU32(static_cast<GuestAddress>(frame.r31 + 4u), next);
        const std::uint64_t live_pair = Read64(memory, static_cast<GuestAddress>(frame.r31));
        memory.WriteU32(static_cast<GuestAddress>(frame.r30 + 188u), next);
        frame.lr = 0x82400ae4u;
        found = LookupDescriptor(memory, services, 0, frame.r27, live_pair, 0,
            frame.r28, 0, sp, frame);
    } while (static_cast<std::uint32_t>(found) != 0u);
    const std::uint64_t result = frame.r31;
    RestoreFrame(memory, caller_sp, frame, 27u);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& records, ArrayResizeServices& arrays,
    CrtThreadDataServices& threads, InvalidParameterServices& invalid,
    std::uint64_t r3, std::uint64_t r4, std::uint64_t r5,
    std::uint64_t r6, std::uint64_t r7, std::uint64_t r8,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const Services services{records, arrays, threads, invalid};
    switch (address)
    {
    case 0x8229d160u:
        result = LookupDescriptor(memory, services, r3, r4, r5, r6, r7, r8, caller_sp, frame);
        return true;
    case 0x82400a30u:
        result = NextUnusedPair(memory, services, r3, r4, r5, r6, caller_sp, frame);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::metadata_descriptor_lookup
