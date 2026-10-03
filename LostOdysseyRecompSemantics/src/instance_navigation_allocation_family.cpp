#include "lo_semantics/instance_navigation_allocation_family.h"

#include "lo_semantics/loaded_single.h"

#include <algorithm>
#include <array>

namespace lo::semantic::gpu::instance_navigation_allocation_family
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

void EnterFrame31(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame, GuestAddress size)
{
    const GuestAddress low = static_cast<GuestAddress>(sp);
    memory.WriteU32(low - 8u, static_cast<GuestAddress>(frame.lr));
    WriteU64(memory, low - 16u, frame.r31);
    memory.WriteU32(low - size, low);
}

void LeaveFrame31(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress low = static_cast<GuestAddress>(sp);
    frame.lr = memory.ReadU32(low - 8u);
    frame.r31 = ReadU64(memory, low - 16u);
}

void EnterFrame30(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress low = static_cast<GuestAddress>(sp);
    memory.WriteU32(low - 8u, static_cast<GuestAddress>(frame.lr));
    WriteU64(memory, low - 24u, frame.r30);
    WriteU64(memory, low - 16u, frame.r31);
    memory.WriteU32(low - 112u, low);
}

void LeaveFrame30(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    LeaveFrame31(memory, sp, frame);
    frame.r30 = ReadU64(memory, static_cast<GuestAddress>(sp) - 24u);
}

void EnterFrame27(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress low = static_cast<GuestAddress>(sp);
    WriteU64(memory, low - 48u, frame.r27);
    WriteU64(memory, low - 40u, frame.r28);
    WriteU64(memory, low - 32u, frame.r29);
    WriteU64(memory, low - 24u, frame.r30);
    WriteU64(memory, low - 16u, frame.r31);
    memory.WriteU32(low - 8u, static_cast<GuestAddress>(frame.lr));
    frame.lr = 0x825af050u;
    memory.WriteU32(low - 128u, low);
}

void LeaveFrame27(GuestMemory& memory, std::uint64_t sp,
    FrameRegisters& frame)
{
    const GuestAddress low = static_cast<GuestAddress>(sp);
    frame.r27 = ReadU64(memory, low - 48u);
    frame.r28 = ReadU64(memory, low - 40u);
    frame.r29 = ReadU64(memory, low - 32u);
    frame.r30 = ReadU64(memory, low - 24u);
    frame.r31 = ReadU64(memory, low - 16u);
    frame.lr = memory.ReadU32(low - 8u);
}

std::uint64_t LinkNavigationNode(GuestMemory& memory,
    VirtualServices& virtual_services, std::uint64_t incoming_r3,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    EnterFrame31(memory, caller_sp, frame, 96u);
    frame.r31 = incoming_r3;
    std::uint64_t result = incoming_r3;
    GuestAddress object = static_cast<GuestAddress>(frame.r31);
    if ((memory.ReadU32(object + 16u) & 0x80000000u) == 0)
    {
        memory.WriteU32(object + 8u, 0);
        memory.WriteU32(object + 4u, object);
        memory.WriteU32(object + 12u, 0);
        constexpr GuestAddress kHead = 0x8336ce04u;
        GuestAddress previous = memory.ReadU32(kHead);
        if (previous != 0)
        {
            memory.WriteU32(previous + 8u, object + 8u);
            previous = memory.ReadU32(kHead);
        }
        memory.WriteU32(object + 12u, kHead);
        memory.WriteU32(object + 8u, previous);
        memory.WriteU32(kHead, object + 4u);

        if (memory.ReadU32(0x83318070u) != 0)
        {
            object = static_cast<GuestAddress>(frame.r31);
            GuestAddress vtable = memory.ReadU32(object);
            GuestAddress method = memory.ReadU32(vtable) & ~3u;
            frame.lr = 0x823b87b0u;
            result = virtual_services.Call(method, memory, frame.r31,
                caller_sp - 96u, frame);
            object = static_cast<GuestAddress>(frame.r31);
            vtable = memory.ReadU32(object);
            method = memory.ReadU32(vtable + 8u) & ~3u;
            frame.lr = 0x823b87c4u;
            result = virtual_services.Call(method, memory, frame.r31,
                caller_sp - 96u, frame);
        }
        object = static_cast<GuestAddress>(frame.r31);
        memory.WriteU32(object + 16u,
            memory.ReadU32(object + 16u) | 0x80000000u);
    }
    LeaveFrame31(memory, caller_sp, frame);
    return result;
}

std::uint64_t InitializeNavigationNode(GuestMemory& memory,
    VirtualServices& virtual_services, std::uint64_t incoming_r3,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    EnterFrame31(memory, caller_sp, frame, 96u);
    frame.r31 = incoming_r3;
    const GuestAddress object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 8u, 0);
    memory.WriteU32(object + 12u, 0);
    const std::uint32_t flags = memory.ReadU32(object + 16u);
    memory.WriteU32(object, 0x821da7a4u);
    memory.WriteU32(object + 16u, flags & 0x7fffffffu);
    for (GuestAddress offset : {20u, 24u, 28u, 32u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 36u, 8);
    for (GuestAddress offset : {40u, 44u, 48u, 52u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object + 56u, 8);
    frame.lr = 0x825aefa0u;
    (void)LinkNavigationNode(memory, virtual_services, frame.r31,
        caller_sp - 96u, frame);
    const std::uint64_t result = frame.r31;
    LeaveFrame31(memory, caller_sp, frame);
    return result;
}

// Matrix defaults are stored in the same increasing-offset order as stfs.
constexpr std::array<GuestAddress, 16> kDiagonal = {
    144u, 164u, 184u, 204u, 208u, 228u, 248u, 268u,
    272u, 292u, 312u, 332u, 336u, 356u, 376u, 396u,
};

std::uint64_t InitializeNavigationAllocation(GuestMemory& memory,
    VirtualServices& virtual_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& f0_bits,
    std::uint64_t& f13_bits)
{
    EnterFrame27(memory, caller_sp, frame);
    frame.r31 = incoming_r3;
    frame.r30 = 0;
    GuestAddress object = static_cast<GuestAddress>(frame.r31);
    frame.r29 = frame.r31 + 16u;
    memory.WriteU32(object + 4u, 0);
    frame.r28 = 1;
    memory.WriteU32(object + 8u, 0);
    memory.WriteU32(object + 12u, 0x8207472cu);
    memory.WriteU32(object, 0x821da7c0u);
    memory.WriteU32(object + 12u, 0x821da7c8u);
    frame.r27 = frame.r29;
    do
    {
        frame.lr = 0x825af09cu;
        (void)InitializeNavigationNode(memory, virtual_services,
            frame.r27, caller_sp - 128u, frame);
        frame.r28 -= 1u;
        frame.r27 += 60u;
    } while (static_cast<std::int32_t>(frame.r28) >= 0);

    object = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(object + 412u, static_cast<GuestAddress>(frame.r30));
    const GuestAddress second_node = static_cast<GuestAddress>(frame.r31 + 76u);
    memory.WriteU32(object + 416u, static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 420u, static_cast<GuestAddress>(frame.r30));
    fp_services.DisableFlushMode();
    const LoadedSingle zero = LoadedSingle::FromWord(memory.ReadU32(0x82000e50u));
    const LoadedSingle one = LoadedSingle::FromWord(memory.ReadU32(0x8218958cu));
    f0_bits = zero.FprBits();
    f13_bits = one.FprBits();
    memory.WriteU32(object + 424u, 8);
    for (GuestAddress offset : {428u, 432u, 436u})
        memory.WriteU32(object + offset,
            static_cast<GuestAddress>(frame.r30));
    memory.WriteU32(object + 440u, 8);
    memory.WriteU32(object + 400u, zero.StoreWord());
    memory.WriteU32(object + 404u, one.StoreWord());
    memory.WriteU32(object + 136u, static_cast<GuestAddress>(frame.r29));
    memory.WriteU32(object + 140u, second_node);
    for (GuestAddress offset = 144u; offset <= 396u; offset += 4u)
    {
        const bool diagonal = std::binary_search(kDiagonal.begin(),
            kDiagonal.end(), offset);
        memory.WriteU32(object + offset,
            diagonal ? one.StoreWord() : zero.StoreWord());
    }
    const std::uint64_t result = frame.r31;
    memory.WriteU32(object + 408u, static_cast<GuestAddress>(frame.r30));
    LeaveFrame27(memory, caller_sp, frame);
    return result;
}

std::uint64_t InitializeNavigationOwner(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    VirtualServices& virtual_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& f0_bits,
    std::uint64_t& f13_bits)
{
    EnterFrame30(memory, caller_sp, frame);
    frame.r31 = incoming_r3;
    frame.r30 = 0;
    const GuestAddress owner = static_cast<GuestAddress>(frame.r31);
    memory.WriteU32(owner + 60u, 0x82062cc0u);
    memory.WriteU32(owner, 0x821de740u);
    memory.WriteU32(owner + 60u, 0x82000fa4u);
    memory.WriteU32(owner + 128u, 0);
    memory.WriteU32(owner + 132u, 0);

    std::uint64_t cursor = frame.r31;
    bool active = false;
    while (true)
    {
        if ((ReadU64(memory, static_cast<GuestAddress>(cursor) + 8u) & 0x600u) != 0)
        {
            active = true;
            break;
        }
        const GuestAddress next = memory.ReadU32(
            static_cast<GuestAddress>(cursor) + 40u);
        if (next == 0)
            break;
        cursor = next;
    }
    if (!active)
    {
        frame.lr = 0x825e1310u;
        std::uint64_t allocation = AllocateManagerBuffer(memory,
            manager_services, 448, static_cast<GuestAddress>(caller_sp - 112u));
        if (static_cast<GuestAddress>(allocation) != 0)
        {
            frame.lr = 0x825e131cu;
            allocation = InitializeNavigationAllocation(memory,
                virtual_services, fp_services, allocation, caller_sp - 112u,
                frame, f0_bits, f13_bits);
        }
        else
            allocation = 0;
        const GuestAddress live_owner = static_cast<GuestAddress>(frame.r31);
        const GuestAddress previous = memory.ReadU32(live_owner + 120u);
        memory.WriteU32(live_owner + 124u,
            static_cast<GuestAddress>(allocation));
        if (previous == 0)
        {
            const GuestAddress source = memory.ReadU32(0x83315fb4u);
            memory.WriteU32(live_owner + 120u, memory.ReadU32(source + 560u));
        }
        const std::uint64_t receiver = memory.ReadU32(0x83318090u);
        const GuestAddress vtable = memory.ReadU32(
            static_cast<GuestAddress>(receiver));
        const GuestAddress method = memory.ReadU32(vtable) & ~3u;
        frame.lr = 0x825e135cu;
        const std::uint64_t callback_result = virtual_services.Call(method,
            memory, receiver, caller_sp - 112u, frame);
        memory.WriteU32(static_cast<GuestAddress>(frame.r31) + 132u,
            static_cast<GuestAddress>(callback_result));
    }
    const std::uint64_t result = frame.r31;
    LeaveFrame30(memory, caller_sp, frame);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    VirtualServices& virtual_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& f0_bits,
    std::uint64_t& f13_bits, std::uint64_t& result)
{
    switch (address)
    {
    case 0x823b8728u:
        result = LinkNavigationNode(memory, virtual_services,
            incoming_r3, caller_sp, frame);
        return true;
    case 0x825aef38u:
        result = InitializeNavigationNode(memory, virtual_services,
            incoming_r3, caller_sp, frame);
        return true;
    case 0x825af048u:
        result = InitializeNavigationAllocation(memory, virtual_services,
            fp_services, incoming_r3, caller_sp, frame, f0_bits, f13_bits);
        return true;
    case 0x825e12a0u:
        result = InitializeNavigationOwner(memory, manager_services,
            virtual_services, fp_services, incoming_r3, caller_sp,
            frame, f0_bits, f13_bits);
        return true;
    case 0x825e2b08u:
        result = static_cast<GuestAddress>(incoming_r3) == 0 ? incoming_r3 :
            InitializeNavigationOwner(memory, manager_services,
                virtual_services, fp_services, incoming_r3, caller_sp,
                frame, f0_bits, f13_bits);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::instance_navigation_allocation_family
