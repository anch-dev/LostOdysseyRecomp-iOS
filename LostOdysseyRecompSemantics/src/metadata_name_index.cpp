#include "lo_semantics/metadata_name_index.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::metadata_name_index
{
namespace
{
constexpr GuestAddress kFoldTable = 0x82297010u;
constexpr GuestAddress kHashTable = 0x832ee168u;
constexpr GuestAddress kBuckets = 0x832ee568u;
constexpr GuestAddress kIndexArray = 0x833690d0u;
constexpr std::uint64_t kFullIndexArray = 0xffffffff833690d0ull;

std::int32_t Signed(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

void WriteU64(GuestMemory& memory, GuestAddress address,
    std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

void SaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame, std::uint32_t first_register)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (first_register == 27u)
    {
        WriteU64(memory, sp - 48u, frame.r27);
        WriteU64(memory, sp - 40u, frame.r28);
    }
    WriteU64(memory, sp - 32u, frame.r29);
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    const std::uint32_t frame_size = first_register == 27u ? 128u : 112u;
    memory.WriteU32(sp - frame_size, sp);
}

void RestoreFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint32_t first_register)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (first_register == 27u)
    {
        frame.r27 = ReadU64(memory, sp - 48u);
        frame.r28 = ReadU64(memory, sp - 40u);
    }
    frame.r29 = ReadU64(memory, sp - 32u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

std::uint32_t GrowthCapacity(std::uint64_t new_count)
{
    const auto low_count = static_cast<std::uint32_t>(new_count);
    const std::uint32_t triple = low_count + (low_count << 1u);
    const std::int64_t rounded = Signed(triple) / 8;
    return static_cast<std::uint32_t>(new_count +
        static_cast<std::uint64_t>(rounded) + 32u);
}

std::uint64_t InsertName(GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t record_register,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    SaveFrame(memory, caller_sp, frame, 27u);
    const std::uint64_t nested_sp = caller_sp - 128u;
    const GuestAddress record = static_cast<GuestAddress>(record_register);
    frame.r30 = record_register;
    frame.lr = 0x823f4500u;
    std::uint64_t result = HashName(memory, frame.r30 + 16u,
        nested_sp, frame);

    const GuestAddress bucket = kBuckets +
        ((static_cast<std::uint32_t>(result) << 2u) & 0x3ffcu);
    const std::uint32_t old_head = memory.ReadU32(bucket);
    memory.WriteU32(static_cast<GuestAddress>(frame.r30 + 12u), old_head);
    memory.WriteU32(bucket, static_cast<std::uint32_t>(frame.r30));
    frame.r31 = kFullIndexArray;

    std::uint64_t identifier = memory.ReadU32(record);
    std::uint64_t count = memory.ReadU32(kIndexArray + 4u);
    frame.r28 = count;
    if (Signed(static_cast<std::uint32_t>(frame.r28)) <=
        Signed(static_cast<std::uint32_t>(identifier)))
    {
        frame.r27 = 0;
        do
        {
            frame.r29 = count;
            count += 1u;
            memory.WriteU32(kIndexArray + 4u,
                static_cast<std::uint32_t>(count));
            const std::uint32_t capacity = memory.ReadU32(kIndexArray + 8u);
            if (Signed(static_cast<std::uint32_t>(count)) > Signed(capacity))
            {
                memory.WriteU32(kIndexArray + 8u, GrowthCapacity(count));
                const std::uint32_t old_storage = memory.ReadU32(kIndexArray);
                const std::uint32_t live_capacity =
                    memory.ReadU32(kIndexArray + 8u);
                frame.lr = 0x823f457cu;
                ResizeArray(memory, resize_services, kIndexArray, 4u, 8u);
                // 8229F678's existing service returns a 32-bit guest address.
                // Its early return leaves full r3 at the sign-extended header;
                // a callback returns the storage pointer it writes to header[0].
                result = old_storage == 0u && Signed(live_capacity) == 0 ?
                    frame.r31 : memory.ReadU32(kIndexArray);
                count = memory.ReadU32(kIndexArray + 4u);
            }
            const std::uint64_t storage = memory.ReadU32(kIndexArray);
            const std::uint64_t slot = storage +
                (static_cast<std::uint32_t>(frame.r29) << 2u);
            if (static_cast<std::uint32_t>(slot) != 0u)
            {
                memory.WriteU32(static_cast<GuestAddress>(slot),
                    static_cast<std::uint32_t>(frame.r27));
                count = memory.ReadU32(kIndexArray + 4u);
            }
            identifier = memory.ReadU32(
                static_cast<GuestAddress>(frame.r30));
            frame.r28 += 1u;
        } while (Signed(static_cast<std::uint32_t>(frame.r28)) <=
            Signed(static_cast<std::uint32_t>(identifier)));
    }
    identifier = memory.ReadU32(static_cast<GuestAddress>(frame.r30));
    const GuestAddress storage = memory.ReadU32(kIndexArray);
    const GuestAddress offset = static_cast<std::uint32_t>(identifier) << 2u;
    memory.WriteU32(storage + offset, static_cast<std::uint32_t>(frame.r30));
    RestoreFrame(memory, caller_sp, frame, 27u);
    return result;
}
} // namespace

std::uint64_t FoldUtf16(GuestMemory& memory, std::uint64_t incoming_r3,
    FrameRegisters& frame)
{
    const std::uint32_t unit = static_cast<std::uint32_t>(incoming_r3) & 0xffffu;
    const std::uint32_t index = unit - 156u;
    if (index <= 99u)
    {
        frame.r0 = memory.ReadU32(kFoldTable + (index << 2u));
        frame.ctr = frame.r0;
        if (index == 0u)
            return 140u;
        if (index == 99u)
            return 159u;
        if (index == 52u || index == 67u || index == 84u || index == 91u)
            return incoming_r3;
    }
    if ((unit >= 97u && unit <= 122u) ||
        (unit >= 224u && unit < 255u))
        return (unit - 32u) & 0xffffu;
    return incoming_r3;
}

std::uint64_t HashName(GuestMemory& memory, std::uint64_t source_register,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    SaveFrame(memory, caller_sp, frame, 29u);
    frame.r31 = source_register;
    frame.r30 = 0;
    std::uint16_t unit = memory.ReadU16(static_cast<GuestAddress>(frame.r31));
    if (unit != 0u)
    {
        frame.r29 = 0xffffffff832ee168ull;
        do
        {
            frame.lr = 0x82296f98u;
            const std::uint64_t folded = FoldUtf16(memory, unit, frame);
            const std::uint32_t first_byte =
                static_cast<std::uint32_t>(folded) & 0xffu;
            std::uint32_t carry =
                std::rotl(static_cast<std::uint32_t>(frame.r30), 24) & 0xffffffu;
            const std::uint32_t second_byte =
                std::rotl(static_cast<std::uint32_t>(folded), 24) & 0xffu;
            const std::uint32_t first_index =
                ((first_byte ^ static_cast<std::uint32_t>(frame.r30)) << 2u) &
                0x3fcu;
            frame.r31 += 2u;
            const std::uint32_t first_word = memory.ReadU32(
                kHashTable + first_index);
            carry = first_word ^ carry;
            unit = memory.ReadU16(static_cast<GuestAddress>(frame.r31));
            const std::uint32_t second_index =
                ((second_byte ^ carry) << 2u) & 0x3fcu;
            const std::uint32_t second_carry = std::rotl(carry, 24) & 0xffffffu;
            frame.r30 = memory.ReadU32(kHashTable + second_index) ^
                second_carry;
        } while (unit != 0u);
    }
    const std::uint64_t result = frame.r30;
    RestoreFrame(memory, caller_sp, frame, 29u);
    return result;
}

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t incoming_r3,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    switch (address)
    {
    case 0x82296fe8u:
        result = FoldUtf16(memory, incoming_r3, frame);
        return true;
    case 0x82296f68u:
        result = HashName(memory, incoming_r3, caller_sp, frame);
        return true;
    case 0x823f44e8u:
        result = InsertName(memory, resize_services, incoming_r3,
            caller_sp, frame);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::metadata_name_index
