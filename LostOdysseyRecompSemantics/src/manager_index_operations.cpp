#include "lo_semantics/manager_index_operations.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::manager_index_operations
{
namespace
{

std::int32_t Signed(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}

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

void StoreValue(GuestMemory& memory, GuestAddress address,
    bool floating, std::uint64_t integer_value, std::uint64_t f31_bits,
    FpServices& fp_services)
{
    if (floating)
    {
        fp_services.DisableFlushMode();
        const float single = static_cast<float>(
            std::bit_cast<double>(f31_bits));
        memory.WriteU32(address, std::bit_cast<std::uint32_t>(single));
    }
    else
    {
        memory.WriteU32(address, static_cast<std::uint32_t>(integer_value));
    }
}

} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager, ArrayResizeServices& arrays,
    FpServices& fp_services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t incoming_r5,
    std::uint64_t f1_bits, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != 0x82326978u && address != 0x82713cf8u)
        return false;
    const bool floating = address == 0x82326978u;
    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (!floating)
        WriteU64(memory, sp - 40u, frame.r28);
    WriteU64(memory, sp - 32u, frame.r29);
    WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    if (floating)
    {
        fp_services.DisableFlushMode();
        WriteU64(memory, sp - 40u, frame.f31_bits);
    }
    const std::uint64_t nested_full_sp = caller_sp - 128u;
    const GuestAddress nested_sp = static_cast<GuestAddress>(nested_full_sp);
    memory.WriteU32(nested_sp, sp);

    const GuestAddress original_buckets = memory.ReadU32(object + 12u);
    WriteU64(memory, nested_sp + 152u, incoming_r4);
    if (original_buckets == 0)
    {
        manager_index_tables::FrameRegisters nested_frame{
            floating ? 0x823269a8u : 0x82713d24u,
            floating ? frame.r28 : incoming_r4,
            floating ? incoming_r4 : incoming_r5,
            frame.r30, incoming_r3};
        std::uint64_t ignored = 0;
        (void)manager_index_tables::Apply(0x82326b08u, memory, manager, arrays,
            incoming_r3, incoming_r4, incoming_r5, 0, nested_full_sp,
            nested_frame, ignored);
    }

    bool hit = false;
    const std::uint32_t old_count = memory.ReadU32(object + 4u);
    if (Signed(old_count) > 0)
    {
        const std::uint32_t bucket_count = memory.ReadU32(object + 16u);
        const std::uint32_t high_key = memory.ReadU32(nested_sp + 152u);
        const GuestAddress buckets = memory.ReadU32(object + 12u);
        const GuestAddress bucket_offset =
            (((bucket_count - 1u) & high_key) << 2u) & 0xfffffffcu;
        std::uint32_t entry_index = memory.ReadU32(buckets + bucket_offset);
        if (entry_index != UINT32_MAX)
        {
            const GuestAddress entries = memory.ReadU32(object);
            const std::uint32_t low_key = memory.ReadU32(nested_sp + 156u);
            while (entry_index != UINT32_MAX)
            {
                const GuestAddress offset = (entry_index << 4u) & 0xfffffff0u;
                const GuestAddress entry = entries + offset;
                if (memory.ReadU32(entry + 4u) == high_key &&
                    memory.ReadU32(entry + 8u) == low_key)
                {
                    StoreValue(memory, entry + 12u, floating,
                        incoming_r5, f1_bits, fp_services);
                    result = std::uint64_t{memory.ReadU32(object)} +
                        offset + 12u;
                    hit = true;
                    goto restore;
                }
                entry_index = memory.ReadU32(entry);
            }
        }
    }

    {
        const std::uint32_t new_count = old_count + 1u;
        const std::uint32_t capacity = memory.ReadU32(object + 8u);
        memory.WriteU32(object + 4u, new_count);
        if (Signed(new_count) > Signed(capacity))
        {
            const std::uint32_t tripled = new_count + (new_count << 1u);
            const std::uint32_t slack =
                static_cast<std::uint32_t>(Signed(tripled) / 8);
            memory.WriteU32(object + 8u, new_count + slack + 32u);
            ResizeArray(memory, arrays, object, 16u, 8u);
        }
        const GuestAddress entries = memory.ReadU32(object);
        const GuestAddress offset = (old_count << 4u) & 0xfffffff0u;
        const std::uint64_t full_entry = std::uint64_t{entries} + offset;
        std::uint64_t entry_register = full_entry;
        if (static_cast<GuestAddress>(full_entry) != 0)
        {
            if (floating)
            {
                StoreValue(memory, static_cast<GuestAddress>(full_entry + 12u),
                    true, 0, f1_bits, fp_services);
                WriteU64(memory, static_cast<GuestAddress>(full_entry + 4u),
                    incoming_r4);
            }
            else
            {
                WriteU64(memory, static_cast<GuestAddress>(full_entry + 4u),
                    incoming_r4);
                StoreValue(memory, static_cast<GuestAddress>(full_entry + 12u),
                    false, incoming_r5, 0, fp_services);
            }
        }
        else
        {
            entry_register = 0;
        }

        const std::uint64_t key = ReadU64(memory,
            static_cast<GuestAddress>(entry_register + 4u));
        const std::uint32_t bucket_count = memory.ReadU32(object + 16u);
        WriteU64(memory, nested_sp + 80u, key);
        const std::uint32_t high_key = memory.ReadU32(nested_sp + 80u);
        const GuestAddress buckets = memory.ReadU32(object + 12u);
        const GuestAddress bucket_offset =
            (((bucket_count - 1u) & high_key) << 2u) & 0xfffffffcu;
        const std::uint32_t head = memory.ReadU32(buckets + bucket_offset);
        memory.WriteU32(static_cast<GuestAddress>(entry_register), head);
        const std::uint32_t count = memory.ReadU32(object + 4u);
        const GuestAddress live_buckets = memory.ReadU32(object + 12u);
        memory.WriteU32(live_buckets + bucket_offset, count - 1u);

        const std::uint32_t current_bucket_count = memory.ReadU32(object + 16u);
        const std::uint32_t current_count = memory.ReadU32(object + 4u);
        const std::uint32_t threshold =
            ((current_bucket_count + 4u) << 1u) & 0xfffffffeu;
        if (Signed(threshold) < Signed(current_count))
        {
            memory.WriteU32(object + 16u,
                (current_bucket_count << 1u) & 0xfffffffeu);
            manager_index_tables::FrameRegisters nested_frame{
                floating ? 0x82326af8u : 0x82713e70u,
                floating ? frame.r28 : incoming_r4,
                floating ? incoming_r4 : incoming_r5,
                entry_register, incoming_r3};
            std::uint64_t ignored = 0;
            (void)manager_index_tables::Apply(0x82326b08u, memory,
                manager, arrays, incoming_r3, incoming_r4,
                incoming_r5, 0, nested_full_sp, nested_frame, ignored);
        }
        result = entry_register + 12u;
    }

restore:
    if (floating)
    {
        // The generated-C++ miss epilogue disables flush before lfd; its hit
        // epilogue does not. Both restore the saved FPR from live guest RAM.
        if (!hit)
            fp_services.DisableFlushMode();
        frame.f31_bits = ReadU64(memory, sp - 40u);
    }
    else
        frame.r28 = ReadU64(memory, sp - 40u);
    frame.r29 = ReadU64(memory, sp - 32u);
    frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
    return true;
}

} // namespace lo::semantic::gpu::manager_index_operations
