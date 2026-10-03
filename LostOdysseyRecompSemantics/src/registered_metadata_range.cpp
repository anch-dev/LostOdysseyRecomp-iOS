#include "lo_semantics/registered_metadata_range.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::registered_metadata_range
{
namespace
{
struct Parent
{
    GuestAddress address;
    GuestAddress helper_return;
    std::uint32_t offset;
    std::uint32_t size;
    std::uint32_t prefix;
    std::uint32_t suffix_count;
    std::uint32_t suffix[2];
};

constexpr Parent kParents[] = {
    // BEGIN GENERATED METADATA RANGE PARAMETERS
    {0x82412E88, 0x82412ECC, 184, 40, 0x0001003C, 1, {0x00010020}},
    {0x825F45F0, 0x825F4618, 60, 60, 0x00000000, 1, {0x00010008}},
    {0x826B06E8, 0x826B072C, 128, 56, 0x0001003C, 2, {0x00010000, 0x0001001C}},
    {0x826F8810, 0x826F8838, 60, 100, 0x00000000, 2, {0x00010040, 0x00010044}},
    // END GENERATED METADATA RANGE PARAMETERS
};

struct Registers
{
    std::uint64_t lr;
    std::uint64_t r30;
    std::uint64_t r31;
};

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

GuestAddress EnterFrame(GuestMemory& memory, GuestAddress sp,
    const Registers& registers)
{
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(registers.lr));
    WriteU64(memory, sp - 24u, registers.r30);
    WriteU64(memory, sp - 16u, registers.r31);
    memory.WriteU32(sp - 112u, sp);
    return sp - 112u;
}

void LeaveFrame(GuestMemory& memory, GuestAddress sp, Registers& registers)
{
    registers.lr = memory.ReadU32(sp - 8u);
    registers.r30 = ReadU64(memory, sp - 24u);
    registers.r31 = ReadU64(memory, sp - 16u);
}

std::uint64_t AppendWord(GuestMemory& memory, ArrayResizeServices& services,
    GuestAddress array, GuestAddress outgoing, std::uint32_t word)
{
    memory.WriteU32(outgoing, word);
    return registered_metadata_words::AppendMetadataWord(
        memory, services, array, outgoing);
}

std::uint64_t AppendRange(GuestMemory& memory, ArrayResizeServices& services,
    std::uint64_t owner_register, std::uint64_t offset_register,
    std::uint64_t size_register, GuestAddress caller_sp, Registers& registers)
{
    const GuestAddress frame_sp = EnterFrame(memory, caller_sp, registers);
    const std::uint32_t first =
        0x00040000u | (static_cast<std::uint32_t>(offset_register) & 0xffffu);
    const std::uint64_t array_register = owner_register + 364u;
    registers.r31 = array_register;
    registers.r30 = size_register;
    const GuestAddress array = static_cast<GuestAddress>(array_register);
    const GuestAddress outgoing = frame_sp + 80u;
    (void)AppendWord(memory, services, array, outgoing, first);
    (void)AppendWord(memory, services, array, outgoing,
                     static_cast<std::uint32_t>(registers.r30));
    const std::uint64_t result = AppendWord(memory, services, array, outgoing,
                                            0xDEADBABEu);
    LeaveFrame(memory, caller_sp, registers);
    return result;
}

void BackfillRange(GuestMemory& memory, GuestAddress array,
    std::uint64_t sentinel_index)
{
    const std::uint32_t count = memory.ReadU32(array + 4u);
    const std::uint32_t sentinel_offset =
        static_cast<std::uint32_t>(sentinel_index) << 2u;
    const GuestAddress storage = memory.ReadU32(array);
    const GuestAddress last = storage + (count << 2u) - 4u;
    const std::uint32_t old_word = memory.ReadU32(last);
    memory.WriteU32(last, old_word + 0x01000000u);

    // The PPC body reloads the header after the last-word store, so an alias
    // there changes the second address and the patched sentinel location.
    const std::uint32_t live_count = memory.ReadU32(array + 4u);
    const GuestAddress live_storage = memory.ReadU32(array);
    const GuestAddress live_last = live_storage + (live_count << 2u) - 4u;
    const std::uint32_t first_byte = memory.ReadU8(live_last);
    const std::uint32_t distance =
        live_count - static_cast<std::uint32_t>(sentinel_index);
    const std::uint32_t patch =
        (distance & 0x00ffffffu) | ((first_byte - 1u) << 24u);
    memory.WriteU32(live_storage + sentinel_offset, patch);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, std::uint64_t object_register,
    std::uint64_t offset_register, std::uint64_t size_register,
    const EntryAbi& abi, Result& result)
{
    const Parent* parent = std::lower_bound(std::begin(kParents),
        std::end(kParents), address,
        [](const Parent& candidate, GuestAddress target)
        { return candidate.address < target; });
    if (address != 0x8249B130u &&
        (parent == std::end(kParents) || parent->address != address))
        return false;

    const GuestAddress caller_sp = static_cast<GuestAddress>(abi.caller_sp);
    Registers registers{abi.incoming_lr, abi.incoming_r30, abi.incoming_r31};
    if (address == 0x8249B130u)
    {
        const std::uint64_t index = AppendRange(memory, services,
            object_register, offset_register, size_register, caller_sp, registers);
        result = {index, registers.lr, registers.r30, registers.r31};
        return true;
    }

    const GuestAddress frame_sp = EnterFrame(memory, caller_sp, registers);
    const GuestAddress outgoing = frame_sp + 80u;
    const GuestAddress object = static_cast<GuestAddress>(object_register);
    std::uint64_t owner_register = memory.ReadU32(object + 52u);
    std::uint64_t array_register = owner_register;
    std::uint64_t sentinel_index = 0;
    std::uint64_t final_index = 0;
    if (parent->prefix != 0)
    {
        array_register += 364u;
        registers.r30 = owner_register;
        registers.r31 = array_register;
        final_index = AppendWord(memory, services,
            static_cast<GuestAddress>(array_register), outgoing, parent->prefix);
        owner_register = registers.r30;
    }
    else
        registers.r31 = owner_register;

    registers.lr = parent->helper_return;
    sentinel_index = AppendRange(memory, services, owner_register,
        parent->offset, parent->size, frame_sp, registers);
    registers.r30 = sentinel_index;
    if (parent->prefix == 0)
    {
        registers.r31 += 364u;
        array_register = registers.r31;
    }
    else
        array_register = registers.r31;
    const GuestAddress array = static_cast<GuestAddress>(array_register);
    for (std::uint32_t index = 0; index < parent->suffix_count; ++index)
        final_index = AppendWord(memory, services, array, outgoing,
                                 parent->suffix[index]);
    BackfillRange(memory, array, sentinel_index);
    LeaveFrame(memory, caller_sp, registers);
    result = {final_index, registers.lr, registers.r30, registers.r31};
    return true;
}

} // namespace lo::semantic::gpu::registered_metadata_range
