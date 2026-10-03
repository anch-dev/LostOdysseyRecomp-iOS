#include "lo_semantics/instance_owned_state_family.h"

#include "lo_semantics/loaded_single.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>

namespace lo::semantic::gpu::instance_owned_state_family
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
    // BEGIN GENERATED INSTANCE OWNED STATE ENTRIES
    {0x82693a10u, 0x82693a10u, true},
    {0x82693c38u, 0x82693c38u, false},
    {0x82693dd8u, 0x82693e38u, true},
    {0x82693e38u, 0x82693e38u, false},
    {0x82713418u, 0x827134b8u, true},
    {0x827134b8u, 0x827134b8u, false},
    {0x82713538u, 0x82713538u, false},
    {0x8272fe20u, 0x8272fe20u, false},
    // END GENERATED INSTANCE OWNED STATE ENTRIES
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

std::uint64_t EnterFrame(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress address = static_cast<GuestAddress>(sp);
    memory.WriteU32(address - 8u, static_cast<std::uint32_t>(frame.lr));
    WriteU64(memory, address - 16u, frame.r31);
    memory.WriteU32(address - 96u, address);
    return sp - 96u;
}

void LeaveFrame(GuestMemory& memory, std::uint64_t sp, FrameRegisters& frame)
{
    const GuestAddress address = static_cast<GuestAddress>(sp);
    frame.lr = memory.ReadU32(address - 8u);
    frame.r31 = ReadU64(memory, address - 16u);
}

bool EnsureOwnedState(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    std::uint64_t owner_register, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    GuestAddress return_address, std::uint64_t& state_result)
{
    const GuestAddress owner = static_cast<GuestAddress>(owner_register);
    const bool ready = (ReadU64(memory, owner + 8u) & 0x200u) != 0;
    memory.WriteU32(owner, 0x820043e8u);
    if (!ready)
    {
        if (return_address != 0)
            frame.lr = return_address;
        (void)owned_state_initializer::Apply(0x823fa008u, memory,
            manager_services, state_services, owner_register, incoming_r4,
            caller_sp, frame, state_result);
    }
    return !ready;
}

void InitializeSimple(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const std::uint64_t sp = EnterFrame(memory, caller_sp, frame);
    frame.r31 = incoming_r3;
    std::uint64_t state_result = 0;
    (void)EnsureOwnedState(memory, manager_services, state_services,
        frame.r31, incoming_r4, sp, frame, 0x827134ecu, state_result);
    result = frame.r31;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object, 0x8220c098u);
    for (GuestAddress offset : {108u, 112u, 116u, 120u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 124u, 8);
    for (GuestAddress offset : {128u, 132u, 136u})
        memory.WriteU32(object + offset, 0);
    LeaveFrame(memory, caller_sp, frame);
}

void InitializeFloatFields(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& f0_bits, std::uint64_t& result)
{
    const std::uint64_t sp = EnterFrame(memory, caller_sp, frame);
    frame.r31 = incoming_r3;
    std::uint64_t state_result = 0;
    (void)EnsureOwnedState(memory, manager_services, state_services,
        frame.r31, incoming_r4, sp, frame, 0x82693e6cu, state_result);
    result = frame.r31;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    fp_services.DisableFlushMode();
    const LoadedSingle loaded = LoadedSingle::FromWord(
        memory.ReadU32(0x82000e50u));
    f0_bits = loaded.FprBits();
    memory.WriteU32(object + 72u, 0x82062cc0u);
    memory.WriteU32(object + 76u, 0x821f9778u);
    memory.WriteU32(object, 0x82004628u);
    memory.WriteU32(object + 72u, 0x82000fa0u);
    memory.WriteU32(object + 76u, 0x821fbfa8u);
    memory.WriteU32(object + 132u, 8);
    for (GuestAddress offset : {116u, 120u, 124u, 128u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 168u, 8);
    for (GuestAddress offset : {152u, 156u, 160u, 164u})
        memory.WriteU32(object + offset, 0);

    // The four float stores and reads/flag writes are interleaved in the PPC
    // body. Keep each live read after its preceding store for aliases.
    memory.WriteU32(object + 180u, loaded.StoreWord());
    const std::uint32_t first = memory.ReadU32(object + 184u);
    memory.WriteU32(object + 196u, loaded.StoreWord());
    const std::uint32_t second = memory.ReadU32(object + 200u);
    memory.WriteU32(object + 212u, loaded.StoreWord());
    const std::uint32_t third = memory.ReadU32(object + 216u);
    const std::uint32_t fourth = memory.ReadU32(object + 232u);
    memory.WriteU32(object + 228u, loaded.StoreWord());
    memory.WriteU32(object + 172u, 0);
    memory.WriteU32(object + 176u, 0);
    memory.WriteU32(object + 184u, first | 0x80000000u);
    memory.WriteU32(object + 188u, 0);
    memory.WriteU32(object + 192u, 0);
    memory.WriteU32(object + 200u, second | 0x80000000u);
    memory.WriteU32(object + 204u, 0);
    memory.WriteU32(object + 208u, 0);
    memory.WriteU32(object + 216u, third | 0x80000000u);
    memory.WriteU32(object + 220u, 0);
    memory.WriteU32(object + 224u, 0);
    memory.WriteU32(object + 232u, fourth | 0x80000000u);
    LeaveFrame(memory, caller_sp, frame);
}

std::uint64_t Run(GuestAddress callee, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& f0_bits);

void InitializeWrapper(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& f0_bits, std::uint64_t& result)
{
    const std::uint64_t sp = EnterFrame(memory, caller_sp, frame);
    frame.r31 = incoming_r3;
    if (static_cast<GuestAddress>(frame.r31) != 0)
    {
        frame.lr = address == 0x82713538u ? 0x82713558u : 0x8272fe40u;
        result = Run(0x827134b8u, memory, manager_services, state_services,
            fp_services, incoming_r3, incoming_r4, sp, frame, f0_bits);
        const GuestAddress object = static_cast<GuestAddress>(frame.r31);
        memory.WriteU32(object, address == 0x82713538u ?
            0x82004e90u : 0x82212028u);
    }
    else
        result = incoming_r3;
    LeaveFrame(memory, caller_sp, frame);
}

void InitializeConditionalWrapper(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    const std::uint64_t sp = EnterFrame(memory, caller_sp, frame);
    frame.r31 = incoming_r3;
    if (static_cast<GuestAddress>(frame.r31) != 0)
    {
        result = incoming_r3;
        (void)EnsureOwnedState(memory, manager_services, state_services,
            frame.r31, incoming_r4, sp, frame, 0x82693c74u, result);
        memory.WriteU32(static_cast<GuestAddress>(frame.r31), 0x82004508u);
    }
    else
        result = incoming_r3;
    LeaveFrame(memory, caller_sp, frame);
}

std::uint64_t Run(GuestAddress callee, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& f0_bits)
{
    std::uint64_t result = incoming_r3;
    switch (callee)
    {
    case 0x827134b8u:
        InitializeSimple(memory, manager_services, state_services,
            incoming_r3, incoming_r4, caller_sp, frame, result);
        break;
    case 0x82693e38u:
        InitializeFloatFields(memory, manager_services, state_services,
            fp_services, incoming_r3, incoming_r4, caller_sp, frame,
            f0_bits, result);
        break;
    case 0x82693a10u:
    {
        if (static_cast<GuestAddress>(incoming_r3) == 0)
            break;
        (void)EnsureOwnedState(memory, manager_services, state_services,
            incoming_r3, incoming_r4, caller_sp, frame, 0, result);
        break;
    }
    case 0x82693c38u:
        InitializeConditionalWrapper(memory, manager_services,
            state_services, incoming_r3, incoming_r4, caller_sp, frame, result);
        break;
    case 0x82713538u:
    case 0x8272fe20u:
        InitializeWrapper(callee, memory, manager_services, state_services,
            fp_services, incoming_r3, incoming_r4, caller_sp, frame,
            f0_bits, result);
        break;
    }
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& f0_bits, std::uint64_t& result)
{
    const Entry* entry = std::lower_bound(std::begin(kEntries),
        std::end(kEntries), address,
        [](const Entry& value, GuestAddress target)
        { return value.address < target; });
    if (entry == std::end(kEntries) || entry->address != address)
        return false;
    if (entry->null_guard && static_cast<GuestAddress>(incoming_r3) == 0)
        result = incoming_r3;
    else
        result = Run(entry->callee, memory, manager_services, state_services,
            fp_services, incoming_r3, incoming_r4,
            caller_sp, frame, f0_bits);
    return true;
}

} // namespace lo::semantic::gpu::instance_owned_state_family
