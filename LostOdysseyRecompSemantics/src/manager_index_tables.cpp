#include "lo_semantics/manager_index_tables.h"

#include "lo_semantics/registered_metadata_words.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::manager_index_tables
{
namespace
{

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}

std::int32_t Signed(std::uint32_t word)
{
    return std::bit_cast<std::int32_t>(word);
}

std::uint64_t Rebuild(GuestMemory& memory, ManagerFacadeServices& manager,
    std::uint64_t object_register, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint32_t stride)
{
    const GuestAddress object = static_cast<GuestAddress>(object_register);
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    WriteU64(memory, sp - 16u, frame.r31);
    const std::uint64_t full_nested_sp = caller_sp - 112u;
    const GuestAddress nested_sp = static_cast<GuestAddress>(full_nested_sp);
    memory.WriteU32(nested_sp, sp);

    const std::uint64_t old_storage = memory.ReadU32(object + 12u);
    (void)ReleaseManagerBuffer(memory, manager, old_storage, nested_sp);
    const std::uint32_t requested_count = memory.ReadU32(object + 16u);
    const std::uint64_t bytes = requested_count > 0x3fffffffu ?
        UINT64_MAX : static_cast<std::uint32_t>(requested_count << 2u);
    const std::uint64_t allocated = AllocateManagerBuffer(memory, manager,
        bytes, nested_sp);
    memory.WriteU32(object + 12u, static_cast<GuestAddress>(allocated));

    std::uint64_t bucket_index = 0;
    if (Signed(memory.ReadU32(object + 16u)) > 0)
    {
        std::uint64_t byte_offset = 0;
        do
        {
            const GuestAddress buckets = memory.ReadU32(object + 12u);
            ++bucket_index;
            memory.WriteU32(buckets + static_cast<GuestAddress>(byte_offset),
                UINT32_MAX);
            byte_offset += 4u;
        } while (Signed(static_cast<std::uint32_t>(bucket_index)) <
                 Signed(memory.ReadU32(object + 16u)));
    }

    std::uint64_t entry_index = 0;
    if (Signed(memory.ReadU32(object + 4u)) > 0)
    {
        std::uint64_t byte_offset = 0;
        do
        {
            const GuestAddress entries = memory.ReadU32(object);
            const std::uint64_t current_index = entry_index;
            const std::uint32_t bucket_count = memory.ReadU32(object + 16u);
            ++entry_index;
            const GuestAddress entry = static_cast<GuestAddress>(
                byte_offset + entries);
            const GuestAddress buckets = memory.ReadU32(object + 12u);
            const std::uint32_t bucket_mask = bucket_count - 1u;
            byte_offset += stride;
            const std::uint64_t key = ReadU64(memory, entry + 4u);
            WriteU64(memory, nested_sp + 80u, key);
            const std::uint32_t high_key = memory.ReadU32(nested_sp + 80u);
            const GuestAddress bucket_offset =
                ((bucket_mask & high_key) << 2u) & 0xfffffffcu;
            const std::uint32_t head = memory.ReadU32(buckets + bucket_offset);
            memory.WriteU32(entry, head);
            const GuestAddress live_buckets = memory.ReadU32(object + 12u);
            memory.WriteU32(live_buckets + bucket_offset,
                static_cast<std::uint32_t>(current_index));
        } while (Signed(static_cast<std::uint32_t>(entry_index)) <
                 Signed(memory.ReadU32(object + 4u)));
    }

    frame.lr = memory.ReadU32(sp - 8u);
    frame.r31 = ReadU64(memory, sp - 16u);
    return allocated;
}

std::uint64_t Append(GuestMemory& memory, ManagerFacadeServices& manager,
    ArrayResizeServices& arrays, std::uint64_t array_register,
    std::uint64_t key_register, std::uint64_t value_a,
    std::uint64_t value_b, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress array = static_cast<GuestAddress>(array_register);
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    WriteU64(memory, sp - 32u, frame.r29);
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    const std::uint64_t full_nested_sp = caller_sp - 128u;
    const GuestAddress nested_sp = static_cast<GuestAddress>(full_nested_sp);
    memory.WriteU32(nested_sp, sp);
    WriteU64(memory, nested_sp + 160u, value_a);
    WriteU64(memory, nested_sp + 168u, value_b);

    const std::uint64_t index = registered_metadata_words::AddArrayElements(
        memory, arrays, array, 1u, 28u, 8u);
    const GuestAddress storage = memory.ReadU32(array);
    const std::uint64_t full_entry =
        index * 28u + static_cast<std::uint64_t>(storage);
    std::uint64_t entry_register = full_entry;
    if (static_cast<GuestAddress>(full_entry) != 0)
    {
        WriteU64(memory, static_cast<GuestAddress>(full_entry + 4u), key_register);
        const std::uint32_t a_high = memory.ReadU32(nested_sp + 160u);
        const std::uint32_t a_low = memory.ReadU32(nested_sp + 164u);
        const std::uint32_t b_high = memory.ReadU32(nested_sp + 168u);
        const std::uint32_t b_low = memory.ReadU32(nested_sp + 172u);
        memory.WriteU32(static_cast<GuestAddress>(full_entry + 12u), a_high);
        memory.WriteU32(static_cast<GuestAddress>(full_entry + 16u), a_low);
        memory.WriteU32(static_cast<GuestAddress>(full_entry + 20u), b_high);
        memory.WriteU32(static_cast<GuestAddress>(full_entry + 24u), b_low);
    }
    else
    {
        entry_register = 0;
    }

    const std::uint64_t key = ReadU64(memory,
        static_cast<GuestAddress>(entry_register + 4u));
    const std::uint32_t bucket_mask = memory.ReadU32(array + 16u) - 1u;
    WriteU64(memory, nested_sp + 80u, key);
    const std::uint32_t high_key = memory.ReadU32(nested_sp + 80u);
    const GuestAddress bucket_offset =
        ((bucket_mask & high_key) << 2u) & 0xfffffffcu;
    const GuestAddress buckets = memory.ReadU32(array + 12u);
    const std::uint32_t previous_head = memory.ReadU32(buckets + bucket_offset);
    memory.WriteU32(static_cast<GuestAddress>(entry_register), previous_head);
    const std::uint32_t count = memory.ReadU32(array + 4u);
    const GuestAddress live_buckets = memory.ReadU32(array + 12u);
    memory.WriteU32(live_buckets + bucket_offset, count - 1u);
    const std::uint32_t current_bucket_count = memory.ReadU32(array + 16u);
    const std::uint32_t current_count = memory.ReadU32(array + 4u);
    const std::uint32_t threshold = ((current_bucket_count + 4u) << 1u) &
        0xfffffffeu;
    if (Signed(threshold) < Signed(current_count))
    {
        memory.WriteU32(array + 16u,
            (current_bucket_count << 1u) & 0xfffffffeu);
        FrameRegisters nested_frame{0x82326968u, frame.r28,
            key_register, entry_register, array_register};
        (void)Rebuild(memory, manager, array_register, full_nested_sp,
            nested_frame, 28u);
    }

    const std::uint64_t result = entry_register + 12u;
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r29 = ReadU64(memory, sp - 32u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
    return result;
}

std::uint64_t FindOrAppend(GuestMemory& memory, ManagerFacadeServices& manager,
    ArrayResizeServices& arrays, std::uint64_t array_register,
    std::uint64_t key_register, std::uint64_t value_a,
    std::uint64_t value_b, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress array = static_cast<GuestAddress>(array_register);
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    WriteU64(memory, sp - 40u, frame.r28);
    WriteU64(memory, sp - 32u, frame.r29);
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    const std::uint64_t full_nested_sp = caller_sp - 128u;
    const GuestAddress nested_sp = static_cast<GuestAddress>(full_nested_sp);
    memory.WriteU32(nested_sp, sp);

    const GuestAddress original_buckets = memory.ReadU32(array + 12u);
    WriteU64(memory, nested_sp + 152u, key_register);
    WriteU64(memory, nested_sp + 160u, value_a);
    WriteU64(memory, nested_sp + 168u, value_b);
    if (original_buckets == 0)
    {
        FrameRegisters nested_frame{0x823266d8u, value_b, value_a,
            key_register, array_register};
        (void)Rebuild(memory, manager, array_register, full_nested_sp,
            nested_frame, 28u);
    }

    std::uint64_t result = 0;
    if (Signed(memory.ReadU32(array + 4u)) > 0)
    {
        const std::uint32_t bucket_count = memory.ReadU32(array + 16u);
        const std::uint32_t key_high = memory.ReadU32(nested_sp + 152u);
        const std::uint32_t mask = bucket_count - 1u;
        const GuestAddress buckets = memory.ReadU32(array + 12u);
        const GuestAddress offset = ((mask & key_high) << 2u) & 0xfffffffcu;
        std::uint32_t entry_index = memory.ReadU32(buckets + offset);
        if (entry_index != UINT32_MAX)
        {
            const GuestAddress entries = memory.ReadU32(array);
            const std::uint32_t key_low = memory.ReadU32(nested_sp + 156u);
            while (entry_index != UINT32_MAX)
            {
                const std::uint64_t entry = std::uint64_t{entry_index} * 28u +
                    entries;
                if (memory.ReadU32(static_cast<GuestAddress>(entry + 4u)) ==
                        key_high &&
                    memory.ReadU32(static_cast<GuestAddress>(entry + 8u)) ==
                        key_low)
                {
                    const GuestAddress payload =
                        static_cast<GuestAddress>(entry + 12u);
                    const GuestAddress source = nested_sp + 160u;
                    const std::uint32_t w0 = memory.ReadU32(source);
                    const std::uint32_t w1 = memory.ReadU32(source + 4u);
                    const std::uint32_t w2 = memory.ReadU32(source + 8u);
                    const std::uint32_t w3 = memory.ReadU32(source + 12u);
                    memory.WriteU32(payload, w0);
                    memory.WriteU32(payload + 4u, w1);
                    memory.WriteU32(payload + 8u, w2);
                    memory.WriteU32(payload + 12u, w3);
                    result = std::uint64_t{memory.ReadU32(array)} +
                        std::uint64_t{entry_index} * 28u + 12u;
                    goto done;
                }
                entry_index = memory.ReadU32(static_cast<GuestAddress>(entry));
            }
        }
    }

    {
        FrameRegisters nested_frame{0x82326750u, value_b, value_a,
            key_register, array_register};
        result = Append(memory, manager, arrays, array_register, key_register,
            value_a, value_b, full_nested_sp, nested_frame);
    }
done:
    frame.r28 = ReadU64(memory, sp - 40u);
    frame.r29 = ReadU64(memory, sp - 32u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
    return result;
}

} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager, ArrayResizeServices& arrays,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result)
{
    if (address == 0x82326b08u || address == 0x823267a0u)
    {
        result = Rebuild(memory, manager, incoming_r3, caller_sp, frame,
            address == 0x82326b08u ? 16u : 28u);
        return true;
    }
    if (address == 0x82326890u)
    {
        result = Append(memory, manager, arrays, incoming_r3, incoming_r4,
            incoming_r5, incoming_r6, caller_sp, frame);
        return true;
    }
    if (address == 0x823266a0u)
    {
        result = FindOrAppend(memory, manager, arrays, incoming_r3,
            incoming_r4, incoming_r5, incoming_r6, caller_sp, frame);
        return true;
    }
    return false;
}

} // namespace lo::semantic::gpu::manager_index_tables
