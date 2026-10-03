#include "lo_semantics/instance_manager_link_family.h"

#include "lo_semantics/loaded_single.h"

#include <cstdint>
#include <initializer_list>

namespace lo::semantic::gpu::instance_manager_link_family
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

void InitializeData(GuestMemory& memory, FpServices& fp_services,
    std::uint64_t full_data, Registers& registers)
{
    const GuestAddress data = static_cast<GuestAddress>(full_data);
    fp_services.DisableFlushMode();
    const LoadedSingle f0 = LoadedSingle::FromWord(memory.ReadU32(0x82000e50u));
    registers.f0 = f0.FprValue();
    memory.WriteU32(data + 24u, 8u);
    fp_services.DisableFlushMode();
    const LoadedSingle f13 = LoadedSingle::FromWord(memory.ReadU32(0x8218958cu));
    registers.f13 = f13.FprValue();
    registers.r10 = 8u;
    registers.r11 = 0u;
    for (GuestAddress offset : {8u, 12u, 16u, 20u, 28u, 32u, 36u, 40u})
        memory.WriteU32(data + offset, 0u);
    memory.WriteU32(data + 44u, 8u);
    for (GuestAddress offset : {48u, 52u, 56u, 60u})
        memory.WriteU32(data + offset, 0u);
    memory.WriteU32(data + 64u, 8u);
    for (GuestAddress offset : {80u, 84u, 88u})
        memory.WriteU32(data + offset, f0.StoreWord());
    memory.WriteU32(data + 92u, f13.StoreWord());
    for (GuestAddress offset : {96u, 100u, 104u})
        memory.WriteU32(data + offset, f0.StoreWord());
    memory.WriteU32(data + 108u, f13.StoreWord());
    for (GuestAddress offset : {112u, 116u, 120u})
        memory.WriteU32(data + offset, f0.StoreWord());
    memory.WriteU32(data + 124u, f13.StoreWord());
}

} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    FpServices& fp_services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    Registers& registers, std::uint64_t& result)
{
    if (address != 0x82700f28u && address != 0x82700fd8u)
        return false;
    if (address == 0x82700fd8u)
    {
        InitializeData(memory, fp_services, incoming_r3, registers);
        result = incoming_r3;
        return true;
    }

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(registers.lr));
    WriteU64(memory, sp - 24u, registers.r30);
    WriteU64(memory, sp - 16u, registers.r31);
    const GuestAddress frame = static_cast<GuestAddress>(caller_sp - 112u);
    memory.WriteU32(frame, sp);

    memory.WriteU32(object + 4u, 0x8207472cu);
    memory.WriteU32(object, 0x820014d0u);
    memory.WriteU32(object + 4u, 0x822096ccu);
    memory.WriteU32(object + 8u, static_cast<GuestAddress>(incoming_r4));
    memory.WriteU32(object + 12u, 0u);
    InitializeData(memory, fp_services, incoming_r3 + 16u, registers);
    memory.WriteU32(object + 144u, 0u);

    result = incoming_r3;
    registers.r9 = 0xffffffff822096ccull;
    registers.lr = memory.ReadU32(sp - 8u);
    registers.r30 = ReadU64(memory, sp - 24u);
    registers.r31 = ReadU64(memory, sp - 16u);
    return true;
}

} // namespace lo::semantic::gpu::instance_manager_link_family
