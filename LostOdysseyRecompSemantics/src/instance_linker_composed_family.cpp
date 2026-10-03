#include "lo_semantics/instance_linker_composed_family.h"

#include "lo_semantics/instance_tail_initializer_family.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>

namespace lo::semantic::gpu::instance_linker_composed_family
{
namespace
{
struct Wrapper
{
    GuestAddress address;
    GuestAddress after_linker;
    GuestAddress after_archive;
    GuestAddress object_vtable;
    GuestAddress archive_vtable;
    GuestAddress clear_end;
};

constexpr Wrapper kWrappers[] = {
    // BEGIN GENERATED INSTANCE LINKER WRAPPERS
    {0x824190f8u, 0x82419118u, 0x8241912cu, 0x82191ec8u, 0x82191fd0u, 376u},
    {0x82419230u, 0x82419250u, 0x82419264u, 0x82191db8u, 0x82002d30u, 1392u},
    // END GENERATED INSTANCE LINKER WRAPPERS
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
    FrameRegisters& frame, GuestAddress size, bool save_r30)
{
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    if (save_r30)
        WriteU64(memory, sp - 24u, frame.r30);
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - size, sp);
    return sp - size;
}

void LeaveFrame(GuestMemory& memory, GuestAddress sp,
    FrameRegisters& frame, bool restore_r30)
{
    frame.lr = memory.ReadU32(sp - 8u);
    if (restore_r30)
        frame.r30 = ReadU64(memory, sp - 24u);
    frame.r31 = ReadU64(memory, sp - 16u);
}

void InitializeArchiveFields(GuestMemory& memory, GuestAddress object)
{
    // Each load is live: object writes may alias the following global.
    const std::uint32_t first = memory.ReadU32(0x83235aa4u);
    memory.WriteU32(object + 4u, first);
    const std::uint32_t second = memory.ReadU32(0x83235aa0u);
    memory.WriteU32(object + 8u, second);
    const std::uint32_t third = memory.ReadU32(0x83235aacu);
    for (GuestAddress offset : {16u, 20u, 24u, 28u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 12u, third);
    memory.WriteU32(object + 44u, 0);
    memory.WriteU32(object + 48u, 0);
    for (GuestAddress offset : {32u, 36u, 40u})
        memory.WriteU32(object + offset, 1);
    for (GuestAddress offset = 52u; offset <= 76u; offset += 4u)
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 80u, 1);
    for (GuestAddress offset : {84u, 88u, 92u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 96u, 0xffffffffu);
    for (GuestAddress offset : {100u, 104u})
        memory.WriteU32(object + offset, 0);
}

void InitializeWrapper(const Wrapper& wrapper, GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const GuestAddress outer_sp = EnterFrame(memory, caller_sp, frame, 96u, false);
    frame.r31 = incoming_r3;
    if (static_cast<GuestAddress>(incoming_r3) != 0)
    {
        // 82419178 has a 112-byte frame. Its existing semantic Apply covers
        // the object/fill work; these live saves reproduce its ABI boundary.
        frame.lr = wrapper.after_linker;
        const GuestAddress inner_sp = EnterFrame(memory, outer_sp, frame, 112u, true);
        frame.r30 = incoming_r3;
        frame.r31 = 0;
        std::uint64_t linked = 0;
        (void)instance_tail_initializer_family::Apply(0x82419178u, memory,
            resize_services, incoming_r3, inner_sp, linked);
        LeaveFrame(memory, outer_sp, frame, true);

        // The restored *live* r31, rather than the incoming pointer, selects
        // the subobject after aliases with the Linker write/fill region.
        result = frame.r31 + 240u;
        const GuestAddress subobject = static_cast<GuestAddress>(result);
        memory.WriteU32(subobject, 0x82189968u);
        frame.lr = wrapper.after_archive;
        InitializeArchiveFields(memory, subobject);

        const GuestAddress object = static_cast<GuestAddress>(frame.r31);
        memory.WriteU32(object, wrapper.object_vtable);
        memory.WriteU32(object + 240u, wrapper.archive_vtable);
        for (GuestAddress offset = wrapper.clear_end == 376u ? 356u : 1384u;
             offset <= wrapper.clear_end; offset += 4u)
            memory.WriteU32(object + offset, 0);
    }
    else
        result = incoming_r3;
    LeaveFrame(memory, caller_sp, frame, false);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    if (address == 0x823f34b8u)
    {
        InitializeArchiveFields(memory, static_cast<GuestAddress>(incoming_r3));
        result = incoming_r3;
        return true;
    }
    const Wrapper* wrapper = std::lower_bound(std::begin(kWrappers),
        std::end(kWrappers), address,
        [](const Wrapper& entry, GuestAddress target)
        { return entry.address < target; });
    if (wrapper == std::end(kWrappers) || wrapper->address != address)
        return false;
    InitializeWrapper(*wrapper, memory, resize_services, incoming_r3,
        caller_sp, frame, result);
    return true;
}

} // namespace lo::semantic::gpu::instance_linker_composed_family
