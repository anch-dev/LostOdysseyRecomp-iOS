#include "lo_semantics/manager_ring_node.h"

namespace lo::semantic::gpu::manager_ring_node
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
    return (std::uint64_t{memory.ReadU32(address)} << 32) | memory.ReadU32(address + 4u);
}

void EnterFrame(GuestMemory& memory, std::uint64_t caller_sp, Registers& registers)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    WriteU64(memory, sp - 40u, registers.r28);
    WriteU64(memory, sp - 32u, registers.r29);
    WriteU64(memory, sp - 24u, registers.r30);
    WriteU64(memory, sp - 16u, registers.r31);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(registers.lr));
    registers.lr = 0x82326588u;
    memory.WriteU32(sp - 128u, sp);
}

void LeaveFrame(GuestMemory& memory, std::uint64_t caller_sp, Registers& registers)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    registers.r28 = ReadU64(memory, sp - 40u);
    registers.r29 = ReadU64(memory, sp - 32u);
    registers.r30 = ReadU64(memory, sp - 24u);
    registers.r31 = ReadU64(memory, sp - 16u);
    registers.lr = memory.ReadU32(sp - 8u);
}

void InitializeData(GuestMemory& memory,
    instance_manager_link_family::FpServices& fp_services,
    std::uint64_t nested_sp, Registers& registers)
{
    instance_manager_link_family::Registers data_registers{};
    data_registers.lr = registers.lr;
    data_registers.f0 = registers.f0;
    data_registers.f13 = registers.f13;
    std::uint64_t ignored = 0;
    (void)instance_manager_link_family::Apply(0x82700FD8u, memory, fp_services,
        registers.r31 + 16u, 0, nested_sp, data_registers, ignored);
    registers.f0 = data_registers.f0;
    registers.f13 = data_registers.f13;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    ring_reservation::SynchronizationServices& synchronization,
    instance_manager_link_family::FpServices& fp_services,
    VirtualServices& virtual_services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    Registers& registers, std::uint64_t& result)
{
    if (address != 0x82326580u)
        return false;
    EnterFrame(memory, caller_sp, registers);
    const std::uint64_t nested_sp = caller_sp - 128u;
    registers.r28 = incoming_r3;
    registers.r29 = incoming_r4;
    if (memory.ReadU32(0x83318040u) != 0)
    {
        registers.lr = 0x823265ACu;
        registers.r30 = AllocateManagerBuffer(memory, manager_services, 20,
            static_cast<GuestAddress>(nested_sp));
        if (static_cast<GuestAddress>(registers.r30) != 0)
        {
            memory.WriteU32(static_cast<GuestAddress>(registers.r30), 0x820010C4u);
            registers.lr = 0x823265D8u;
            ring_reservation::Registers ring_registers{};
            std::uint64_t ignored = 0;
            (void)ring_reservation::Apply(0x82290AB8u, memory, synchronization,
                registers.r30 + 8u, 0xFFFFFFFF8336A7A4ull, 144, ring_registers, ignored);
            registers.r31 = memory.ReadU32(static_cast<GuestAddress>(registers.r30) + 12u);
            if (registers.r31 != 0)
            {
                memory.WriteU32(static_cast<GuestAddress>(registers.r31) + 4u,
                    static_cast<GuestAddress>(registers.r29));
                memory.WriteU32(static_cast<GuestAddress>(registers.r31), 0x820011D4u);
                registers.lr = 0x823265FCu;
                InitializeData(memory, fp_services, nested_sp, registers);
            }
            memory.WriteU32(static_cast<GuestAddress>(registers.r30) + 4u,
                static_cast<GuestAddress>(registers.r31));
            memory.WriteU32(static_cast<GuestAddress>(registers.r28) + 4u,
                static_cast<GuestAddress>(registers.r30));
        }
        else
            memory.WriteU32(static_cast<GuestAddress>(registers.r28) + 4u, 0);
    }
    else
    {
        registers.lr = 0x82326634u;
        registers.r31 = AllocateManagerBuffer(memory, manager_services, 144,
            static_cast<GuestAddress>(nested_sp));
        if (static_cast<GuestAddress>(registers.r31) != 0)
        {
            memory.WriteU32(static_cast<GuestAddress>(registers.r31) + 4u,
                static_cast<GuestAddress>(registers.r29));
            memory.WriteU32(static_cast<GuestAddress>(registers.r31), 0x822096D0u);
            registers.lr = 0x82326658u;
            InitializeData(memory, fp_services, nested_sp, registers);
        }
        else
            registers.r31 = 0;
        memory.WriteU32(static_cast<GuestAddress>(registers.r28) + 4u,
            static_cast<GuestAddress>(registers.r31));
    }
    const std::uint64_t receiver = memory.ReadU32(
        static_cast<GuestAddress>(registers.r28) + 4u);
    const GuestAddress vtable = memory.ReadU32(static_cast<GuestAddress>(receiver));
    const GuestAddress method = memory.ReadU32(vtable + 4u) & ~3u;
    registers.lr = 0x82326678u;
    const std::uint64_t callback_result = virtual_services.Call(method, memory,
        receiver, nested_sp, registers);
    result = registers.r28; // mr r3,r28 precedes the potentially aliased store.
    memory.WriteU32(static_cast<GuestAddress>(registers.r28),
        static_cast<GuestAddress>(callback_result));
    LeaveFrame(memory, caller_sp, registers);
    return true;
}
} // namespace lo::semantic::gpu::manager_ring_node
