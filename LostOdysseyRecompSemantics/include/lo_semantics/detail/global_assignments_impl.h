#pragma once

#include <stdexcept>

namespace lo::semantic::gpu
{
template <typename Memory>
void WriteFieldAssignmentsWith(Memory& memory, GlobalAssignmentRegisters& registers,
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
