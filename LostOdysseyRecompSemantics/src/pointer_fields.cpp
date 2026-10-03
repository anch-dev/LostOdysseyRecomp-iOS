#include "lo_semantics/pointer_fields.h"

#include <stdexcept>

namespace lo::semantic::gpu
{
std::uint64_t PointerFieldRegisters::Get(PointerFieldRegister reg) const
{
    switch (reg)
    {
    case PointerFieldRegister::R3: return r3;
    case PointerFieldRegister::R4: return r4;
    case PointerFieldRegister::R5: return r5;
    case PointerFieldRegister::R6: return r6;
    case PointerFieldRegister::R9: return r9;
    case PointerFieldRegister::R10: return r10;
    case PointerFieldRegister::R11: return r11;
    case PointerFieldRegister::R13: return r13;
    }
    throw std::invalid_argument("unsupported pointer field register");
}

void PointerFieldRegisters::Set(PointerFieldRegister reg, std::uint64_t value)
{
    switch (reg)
    {
    case PointerFieldRegister::R3: r3 = value; return;
    case PointerFieldRegister::R4: r4 = value; return;
    case PointerFieldRegister::R5: r5 = value; return;
    case PointerFieldRegister::R6: r6 = value; return;
    case PointerFieldRegister::R9: r9 = value; return;
    case PointerFieldRegister::R10: r10 = value; return;
    case PointerFieldRegister::R11: r11 = value; return;
    case PointerFieldRegister::R13: r13 = value; return;
    }
    throw std::invalid_argument("unsupported pointer field register");
}

void InitializeConstantFields(GuestMemory& memory, PointerFieldRegisters& registers,
                              PointerFieldRegister base_register,
                              std::span<const ConstantFieldAssignment> assignments,
                              std::span<const ConstantFieldWrite> writes)
{
    InitializeConstantFieldsWith(memory, registers, base_register, assignments, writes);
}

void ReadPointerChainField(GuestMemory& memory, PointerFieldRegisters& registers,
                           PointerFieldRegister root_register,
                           std::span<const PointerFieldOffset> fields)
{
    ReadPointerChainFieldWith(memory, registers, root_register, fields);
}

void AddressOfPointerChainMember(GuestMemory& memory, PointerFieldRegisters& registers,
                                 PointerFieldRegister root_register,
                                 std::span<const PointerFieldOffset> links,
                                 std::int32_t member_displacement)
{
    AddressOfPointerChainMemberWith(memory, registers, root_register, links, member_displacement);
}

void ReadBaseByteIndexField(GuestMemory& memory, PointerFieldRegisters& registers,
                            std::int32_t displacement)
{
    ReadBaseByteIndexFieldWith(memory, registers, displacement);
}

void ReadBiasedIndexField(GuestMemory& memory, PointerFieldRegisters& registers,
                          std::int32_t index_bias)
{
    ReadBiasedIndexFieldWith(memory, registers, index_bias);
}

void ReadPointerArrayElement(GuestMemory& memory, PointerFieldRegisters& registers,
                             std::int32_t array_pointer_displacement)
{
    ReadPointerArrayElementWith(memory, registers, array_pointer_displacement);
}

} // namespace lo::semantic::gpu
