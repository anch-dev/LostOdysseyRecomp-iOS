#include "lo_semantics/instance_property_chain_family.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>

namespace lo::semantic::gpu::instance_property_chain_family
{
namespace
{
struct Entry
{
    GuestAddress address;
    GuestAddress callee;
    bool null_guard;
};

constexpr Entry kEntries[] = {
    // BEGIN GENERATED INSTANCE PROPERTY CHAIN ENTRIES
    {0x8245d2a8u, 0x825d5398u, true},
    {0x825aeb20u, 0x825aeb30u, true},
    {0x825aeb30u, 0x825aeb30u, false},
    {0x825aec68u, 0x825aeec0u, true},
    {0x825aeec0u, 0x825aeec0u, false},
    {0x825d5398u, 0x825d5398u, false},
    {0x826d6f18u, 0x826d6f28u, true},
    {0x826d6f28u, 0x826d6f28u, false},
    {0x826d70a0u, 0x826d70a0u, false},
    // END GENERATED INSTANCE PROPERTY CHAIN ENTRIES
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

void InitializeSlot(GuestMemory& memory, ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const GuestAddress sp = EnterFrame(memory, caller_sp, frame, 96u, false);
    frame.r31 = incoming_r3;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 60u, 0x82062cc0u);
    for (GuestAddress offset : {696u, 700u, 704u, 712u, 716u, 720u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object, 0x8205f34cu);
    memory.WriteU32(object + 60u, 0x8205f344u);
    frame.lr = 0x825d53fcu;
    std::uint64_t child_result = 0;
    (void)string_property_initializer::Apply(0x82496948u, memory,
        resize_services, manager_services, frame.r31 + 928u,
        0xffffffff821a83d0ull, sp, frame, child_result);
    result = frame.r31;
    LeaveFrame(memory, caller_sp, frame, false);
}

void InitializeArrayProperty(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const GuestAddress sp = EnterFrame(memory, caller_sp, frame, 112u, true);
    frame.r31 = incoming_r3;
    frame.r30 = frame.r31 + 60u;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    const GuestAddress array = static_cast<GuestAddress>(frame.r30);
    memory.WriteU32(object, 0x821da3c0u);
    memory.WriteU32(array, 0);
    memory.WriteU32(array + 4u, 0);
    memory.WriteU32(array + 8u, 0);
    frame.lr = 0x825aef08u;
    ResizeArray(memory, resize_services, array, 4, 8);
    memory.WriteU32(array + 12u, static_cast<GuestAddress>(frame.r31));
    frame.lr = 0x825aef18u;
    std::uint64_t child_result = 0;
    (void)string_property_initializer::Apply(0x82496948u, memory,
        resize_services, manager_services, frame.r31 + 76u, 0,
        sp, frame, child_result);
    result = frame.r31;
    LeaveFrame(memory, caller_sp, frame, true);
}

std::uint64_t Run(GuestAddress callee, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame);

void InitializeExtended(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const GuestAddress sp = EnterFrame(memory, caller_sp, frame, 128u, false);
    frame.r31 = incoming_r3;
    frame.lr = 0x825aeb48u;
    (void)Run(0x825aeec0u, memory, resize_services, manager_services,
              incoming_r3, sp, frame);
    result = frame.r31;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object, 0x821da4c8u);
    for (GuestAddress offset = 84u; offset <= 104u; offset += 4u)
        memory.WriteU32(sp + offset, 0);
    memory.WriteU32(object + 148u, 0);
    memory.WriteU32(sp + 108u, 0);
    memory.WriteU32(sp + 80u, 0);
    memory.WriteU32(object + 152u, 0);
    for (GuestAddress offset = 84u; offset <= 96u; offset += 4u)
        memory.WriteU32(object + 156u + offset - 84u,
                        memory.ReadU32(sp + offset));
    const std::uint32_t staged100 = memory.ReadU32(sp + 100u);
    memory.WriteU32(object + 188u, 8);
    memory.WriteU32(object + 172u, staged100);
    memory.WriteU32(object + 176u, memory.ReadU32(sp + 104u));
    memory.WriteU32(object + 180u, memory.ReadU32(sp + 108u));
    for (GuestAddress offset : {184u, 192u, 196u, 200u, 204u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 208u, 8);
    for (GuestAddress offset : {240u, 244u, 248u, 256u, 260u, 264u, 268u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 272u, 8);
    for (GuestAddress offset = 280u; offset <= 304u; offset += 4u)
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 308u, 8);
    for (GuestAddress offset : {312u, 316u, 320u, 340u, 344u, 348u,
                                360u, 364u, 368u, 372u, 376u, 380u})
        memory.WriteU32(object + offset, 0);
    LeaveFrame(memory, caller_sp, frame, false);
}

void InitializeNetwork(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    (void)EnterFrame(memory, caller_sp, frame, 96u, false);
    frame.r31 = incoming_r3;
    frame.lr = 0x826d6f40u;
    (void)Run(0x825aeec0u, memory, resize_services, manager_services,
              incoming_r3, caller_sp - 96u, frame);
    result = frame.r31;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 144u, 0x82202a80u);
    memory.WriteU32(object, 0x82202968u);
    memory.WriteU32(object + 144u, 0x82202a80u);
    for (GuestAddress offset : {148u, 164u, 168u, 172u})
        memory.WriteU32(object + offset, 0);
    LeaveFrame(memory, caller_sp, frame, false);
}

void InitializeNetworkWrapper(GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const GuestAddress sp = EnterFrame(memory, caller_sp, frame, 96u, false);
    frame.r31 = incoming_r3;
    if (static_cast<GuestAddress>(frame.r31) != 0)
    {
        frame.lr = 0x826d70c0u;
        result = Run(0x826d6f28u, memory, resize_services, manager_services,
                     incoming_r3, sp, frame);
        const GuestAddress object = static_cast<GuestAddress>(frame.r31);
        memory.WriteU32(object, 0x82202aa0u);
        memory.WriteU32(object + 144u, 0x82202bb8u);
    }
    else
        result = incoming_r3;
    LeaveFrame(memory, caller_sp, frame, false);
}

std::uint64_t Run(GuestAddress callee, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame)
{
    std::uint64_t result = incoming_r3;
    switch (callee)
    {
    case 0x825d5398u:
        InitializeSlot(memory, resize_services, manager_services, incoming_r3,
                       caller_sp, frame, result);
        break;
    case 0x825aeec0u:
        InitializeArrayProperty(memory, resize_services, manager_services,
                                incoming_r3, caller_sp, frame, result);
        break;
    case 0x825aeb30u:
        InitializeExtended(memory, resize_services, manager_services,
                           incoming_r3, caller_sp, frame, result);
        break;
    case 0x826d6f28u:
        InitializeNetwork(memory, resize_services, manager_services,
                          incoming_r3, caller_sp, frame, result);
        break;
    case 0x826d70a0u:
        InitializeNetworkWrapper(memory, resize_services, manager_services,
                                 incoming_r3, caller_sp, frame, result);
        break;
    }
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, ManagerFacadeServices& manager_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    (void)incoming_r4; // Every admitted callee sets r4 before the foundation.
    const Entry* entry = std::lower_bound(std::begin(kEntries),
        std::end(kEntries), address,
        [](const Entry& value, GuestAddress target)
        { return value.address < target; });
    if (entry == std::end(kEntries) || entry->address != address)
        return false;
    if (entry->null_guard && static_cast<GuestAddress>(incoming_r3) == 0)
        result = incoming_r3;
    else
        result = Run(entry->callee, memory, resize_services, manager_services,
                     incoming_r3, static_cast<GuestAddress>(caller_sp), frame);
    return true;
}

} // namespace lo::semantic::gpu::instance_property_chain_family
