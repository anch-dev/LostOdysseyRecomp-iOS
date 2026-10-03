#include "lo_semantics/field_operations.h"

#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu
{
std::uint64_t FieldOperationRegisters::Get(FieldOperationRegister reg) const
{
    switch (reg)
    {
    case FieldOperationRegister::R3: return r3;
    case FieldOperationRegister::R4: return r4;
    case FieldOperationRegister::R5: return r5;
    case FieldOperationRegister::R6: return r6;
    case FieldOperationRegister::R7: return r7;
    case FieldOperationRegister::R8: return r8;
    case FieldOperationRegister::R9: return r9;
    case FieldOperationRegister::R10: return r10;
    case FieldOperationRegister::R11: return r11;
    }
    throw std::invalid_argument("unsupported field register");
}

void FieldOperationRegisters::Set(FieldOperationRegister reg, std::uint64_t value)
{
    switch (reg)
    {
    case FieldOperationRegister::R3: r3 = value; return;
    case FieldOperationRegister::R4: r4 = value; return;
    case FieldOperationRegister::R5: r5 = value; return;
    case FieldOperationRegister::R6: r6 = value; return;
    case FieldOperationRegister::R7: r7 = value; return;
    case FieldOperationRegister::R8: r8 = value; return;
    case FieldOperationRegister::R9: r9 = value; return;
    case FieldOperationRegister::R10: r10 = value; return;
    case FieldOperationRegister::R11: r11 = value; return;
    }
    throw std::invalid_argument("unsupported field register");
}

void CopyFixedFields(GuestMemory& memory, FieldOperationRegisters& registers,
                     std::span<const FieldCopyStep> steps)
{
    CopyFixedFieldsWith(memory, registers, steps);
}

void CopyFixedFieldsAndReturnConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                                      std::span<const FieldCopyStep> steps)
{
    CopyFixedFieldsAndReturnConstantWith(memory, registers, steps);
}

void ReadGlobalField(GuestMemory& memory, FieldOperationRegisters& registers,
                     std::uint32_t global_base, std::optional<std::int32_t> adjustment,
                     std::span<const GlobalFieldLoad> loads)
{
    ReadGlobalFieldWith(memory, registers, global_base, adjustment, loads);
}

void ReadGlobalPointerChainField(GuestMemory& memory, FieldOperationRegisters& registers,
                                 std::uint32_t global_base,
                                 std::span<const GlobalFieldLoad> loads)
{
    ReadGlobalPointerChainFieldWith(memory, registers, global_base, loads);
}

void FieldEqualsConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                         FieldOperationRegister base, std::int32_t displacement,
                         std::uint8_t width, std::uint32_t comparison)
{
    FieldEqualsConstantWith(memory, registers, base, displacement, width, comparison);
}

void FieldNotEqualsConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                            FieldOperationRegister base, std::int32_t displacement,
                            std::uint8_t width, std::uint32_t comparison)
{
    FieldNotEqualsConstantWith(memory, registers, base, displacement, width, comparison);
}

} // namespace lo::semantic::gpu
