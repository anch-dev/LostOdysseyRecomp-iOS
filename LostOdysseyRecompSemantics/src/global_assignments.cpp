#include "lo_semantics/global_assignments.h"

#include <stdexcept>

namespace lo::semantic::gpu
{

std::uint64_t GlobalAssignmentRegisters::Get(GlobalAssignmentRegister reg) const
{
    switch (reg)
    {
    case GlobalAssignmentRegister::R3: return r3;
    case GlobalAssignmentRegister::R4: return r4;
    case GlobalAssignmentRegister::R5: return r5;
    case GlobalAssignmentRegister::R6: return r6;
    case GlobalAssignmentRegister::R7: return r7;
    case GlobalAssignmentRegister::R8: return r8;
    case GlobalAssignmentRegister::R9: return r9;
    case GlobalAssignmentRegister::R10: return r10;
    case GlobalAssignmentRegister::R11: return r11;
    }
    throw std::invalid_argument("unsupported field register");
}

void GlobalAssignmentRegisters::Set(GlobalAssignmentRegister reg, std::uint64_t value)
{
    switch (reg)
    {
    case GlobalAssignmentRegister::R3: r3 = value; return;
    case GlobalAssignmentRegister::R4: r4 = value; return;
    case GlobalAssignmentRegister::R5: r5 = value; return;
    case GlobalAssignmentRegister::R6: r6 = value; return;
    case GlobalAssignmentRegister::R7: r7 = value; return;
    case GlobalAssignmentRegister::R8: r8 = value; return;
    case GlobalAssignmentRegister::R9: r9 = value; return;
    case GlobalAssignmentRegister::R10: r10 = value; return;
    case GlobalAssignmentRegister::R11: r11 = value; return;
    }
    throw std::invalid_argument("unsupported field register");
}

void WriteFieldAssignments(GuestMemory& memory, GlobalAssignmentRegisters& registers,
                           std::span<const AssignmentFieldWrite> writes,
                           std::span<const ConstantRegisterValue> register_values_after)
{
    for (const auto& write : writes)
    {
        for (const auto& value : write.register_values_before)
            registers.Set(value.reg, value.value);

        const std::uint32_t address = write.address_kind == AssignmentAddressKind::Global
            ? write.global_address
            : static_cast<std::uint32_t>(registers.Get(write.base_register)) +
              static_cast<std::uint32_t>(write.displacement);
        const std::uint32_t value = write.value_kind == AssignmentValueKind::Constant
            ? write.constant_value
            : static_cast<std::uint32_t>(registers.Get(write.value_register));

        switch (write.width)
        {
        case AssignmentWidth::Byte:
            memory.WriteU8(address, static_cast<std::uint8_t>(value));
            break;
        case AssignmentWidth::Halfword:
            memory.WriteU16(address, static_cast<std::uint16_t>(value));
            break;
        case AssignmentWidth::Word:
            memory.WriteU32(address, value);
            break;
        default:
            throw std::invalid_argument("unsupported assignment width");
        }
    }
    for (const auto& value : register_values_after)
        registers.Set(value.reg, value.value);
}

} // namespace lo::semantic::gpu
