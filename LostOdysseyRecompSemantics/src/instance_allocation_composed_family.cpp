#include "lo_semantics/instance_allocation_composed_family.h"

#include "lo_semantics/memory_fill.h"

#include <bit>
#include <initializer_list>

namespace lo::semantic::gpu::instance_allocation_composed_family
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

void EnterManualFrame(GuestMemory& memory, std::uint64_t sp,
    const FrameRegisters& frame)
{
    const GuestAddress low_sp = static_cast<GuestAddress>(sp);
    memory.WriteU32(low_sp - 8u, static_cast<GuestAddress>(frame.lr));
    WriteU64(memory, low_sp - 24u, frame.r30);
    WriteU64(memory, low_sp - 16u, frame.r31);
    memory.WriteU32(low_sp - 112u, low_sp);
}

void LeaveManualFrame(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress low_sp = static_cast<GuestAddress>(sp);
    frame.lr = memory.ReadU32(low_sp - 8u);
    frame.r30 = ReadU64(memory, low_sp - 24u);
    frame.r31 = ReadU64(memory, low_sp - 16u);
}

void EnterResizeFrame(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress low_sp = static_cast<GuestAddress>(sp);
    // Actual __savegprlr_28 order: GPRs first, then the incoming LR.
    WriteU64(memory, low_sp - 40u, frame.r28);
    WriteU64(memory, low_sp - 32u, frame.r29);
    WriteU64(memory, low_sp - 24u, frame.r30);
    WriteU64(memory, low_sp - 16u, frame.r31);
    memory.WriteU32(low_sp - 8u, static_cast<GuestAddress>(frame.lr));
    frame.lr = 0x823058F8u;
    memory.WriteU32(low_sp - 128u, low_sp);
}

void LeaveResizeFrame(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress low_sp = static_cast<GuestAddress>(sp);
    frame.r28 = ReadU64(memory, low_sp - 40u);
    frame.r29 = ReadU64(memory, low_sp - 32u);
    frame.r30 = ReadU64(memory, low_sp - 24u);
    frame.r31 = ReadU64(memory, low_sp - 16u);
    frame.lr = memory.ReadU32(low_sp - 8u);
}

// addi wraps at the word consumed by srawi; addze makes negative division
// truncate toward zero. rlwinm then retains only the low 32-bit byte count.
std::int64_t BitWordCount(std::uint32_t bits)
{
    return std::bit_cast<std::int32_t>(bits + 31u) / std::int64_t{32};
}

std::uint32_t BitStorageBytes(std::uint32_t bits)
{
    return static_cast<std::uint32_t>(BitWordCount(bits)) << 2u;
}

std::uint64_t ResizeBitWords(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    BitWordResizeServices& bit_resize_services,
    std::uint64_t incoming_r3, std::uint64_t sp, FrameRegisters& frame)
{
    EnterResizeFrame(memory, sp, frame);
    const std::uint64_t nested_sp = sp - 128u;
    frame.r31 = incoming_r3;
    frame.r28 = memory.ReadU32(static_cast<GuestAddress>(frame.r31));
    std::uint64_t result = incoming_r3;
    if (frame.r28 != 0 ||
        memory.ReadU32(static_cast<GuestAddress>(frame.r31) + 8u) != 0)
    {
        const std::uint32_t bits =
            memory.ReadU32(static_cast<GuestAddress>(frame.r31) + 8u);
        frame.r30 = 0xFFFFFFFF83310000ull;
        frame.r29 = static_cast<std::uint64_t>(BitWordCount(bits));
        std::uint64_t manager = memory.ReadU32(
            static_cast<GuestAddress>(frame.r30) - 18936u);
        if (manager == 0)
        {
            frame.lr = 0x8230593Cu;
            EnterManualFrame(memory, nested_sp, frame);
            frame.r31 = nested_sp - 112u;
            (void)InitializeManager(memory, manager_services,
                static_cast<GuestAddress>(frame.r31));
            LeaveManualFrame(memory, nested_sp, frame);
            manager = memory.ReadU32(
                static_cast<GuestAddress>(frame.r30) - 18936u);
        }
        const GuestAddress vtable = memory.ReadU32(
            static_cast<GuestAddress>(manager));
        const GuestAddress method = memory.ReadU32(vtable + 8u) & ~3u;
        frame.lr = 0x8230595Cu;
        result = bit_resize_services.Resize(method, memory, manager,
            frame.r28, static_cast<std::uint32_t>(frame.r29) << 2u,
            8, nested_sp, frame);
        // Use live r31 after the callback, including callback mutations.
        memory.WriteU32(static_cast<GuestAddress>(frame.r31),
            static_cast<GuestAddress>(result));
    }
    LeaveResizeFrame(memory, sp, frame);
    return result;
}

std::uint64_t InitializeHolder(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    BitWordResizeServices& bit_resize_services,
    std::uint64_t incoming_r3, std::uint64_t sp, FrameRegisters& frame)
{
    EnterManualFrame(memory, sp, frame);
    const std::uint64_t nested_sp = sp - 112u;
    frame.r31 = incoming_r3;
    frame.r30 = frame.r31 + 24u;
    for (GuestAddress offset = 0; offset <= 20; offset += 4)
        memory.WriteU32(static_cast<GuestAddress>(frame.r31) + offset, 0);
    for (GuestAddress offset : {0u, 4u, 8u})
        memory.WriteU32(static_cast<GuestAddress>(frame.r30) + offset, 0);
    frame.lr = 0x825BA66Cu;
    (void)ResizeBitWords(memory, manager_services, bit_resize_services,
        frame.r30, nested_sp, frame);
    const GuestAddress array = static_cast<GuestAddress>(frame.r30);
    const std::uint32_t live_count = memory.ReadU32(array + 4u);
    if (live_count != 0)
    {
        const GuestAddress storage = memory.ReadU32(array);
        frame.lr = 0x825BA694u;
        (void)FillGuestMemory(memory, storage, 0, BitStorageBytes(live_count));
    }
    const std::uint64_t result = frame.r31;
    LeaveManualFrame(memory, sp, frame);
    return result;
}

std::uint64_t InitializeWorld(GuestMemory& memory,
    ArrayResizeServices& string_resize_services,
    ManagerFacadeServices& manager_services,
    BitWordResizeServices& bit_resize_services,
    std::uint64_t incoming_r3, std::uint64_t sp, FrameRegisters& frame)
{
    EnterManualFrame(memory, sp, frame);
    const std::uint64_t nested_sp = sp - 112u;
    frame.r31 = incoming_r3;
    std::uint64_t result = incoming_r3;
    if (static_cast<GuestAddress>(frame.r31) != 0)
    {
        const GuestAddress object = static_cast<GuestAddress>(frame.r31);
        frame.r30 = 0;
        memory.WriteU32(object + 60u, 0x82202A80u);
        memory.WriteU32(object, 0x821DAF10u);
        memory.WriteU32(object + 60u, 0x821DB01Cu);
        for (GuestAddress offset : {68u, 72u, 76u})
            memory.WriteU32(object + offset, 0);
        frame.lr = 0x825BA8B8u;
        (void)string_property_initializer::Apply(0x82496948u, memory,
            string_resize_services, manager_services, frame.r31 + 212u,
            0, nested_sp, frame, result);
        for (GuestAddress offset : {292u, 296u, 300u, 372u, 376u, 380u})
            memory.WriteU32(static_cast<GuestAddress>(frame.r31) + offset,
                static_cast<GuestAddress>(frame.r30));
        frame.lr = 0x825BA8D8u;
        (void)InitializeHolder(memory, manager_services, bit_resize_services,
            frame.r31 + 388u, nested_sp, frame);
        frame.lr = 0x825BA8E0u;
        result = InitializeHolder(memory, manager_services, bit_resize_services,
            frame.r31 + 424u, nested_sp, frame);
    }
    // No final mr r3,r31: the second holder's full return register survives.
    LeaveManualFrame(memory, sp, frame);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& string_resize_services,
    ManagerFacadeServices& manager_services,
    BitWordResizeServices& bit_resize_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case 0x823058F0u:
        result = ResizeBitWords(memory, manager_services, bit_resize_services,
            incoming_r3, caller_sp, frame);
        return true;
    case 0x825BA620u:
        result = InitializeHolder(memory, manager_services, bit_resize_services,
            incoming_r3, caller_sp, frame);
        return true;
    case 0x825BA858u:
        result = InitializeWorld(memory, string_resize_services, manager_services,
            bit_resize_services, incoming_r3, caller_sp, frame);
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::instance_allocation_composed_family
