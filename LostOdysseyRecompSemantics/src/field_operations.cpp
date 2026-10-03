#include "lo_semantics/field_operations.h"

#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu
{
namespace
{

std::uint32_t Address(std::uint64_t base, std::int32_t displacement)
{
    return static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(displacement);
}

std::uint32_t ReadField(GuestMemory& memory, std::uint32_t address, std::uint8_t width)
{
    switch (width)
    {
    case 8: return memory.ReadU8(address);
    case 16: return memory.ReadU16(address);
    case 32: return memory.ReadU32(address);
    }
    throw std::invalid_argument("unsupported field width");
}

void WriteField(GuestMemory& memory, std::uint32_t address,
                std::uint8_t width, std::uint64_t value)
{
    switch (width)
    {
    case 8: memory.WriteU8(address, static_cast<std::uint8_t>(value)); return;
    case 16: memory.WriteU16(address, static_cast<std::uint16_t>(value)); return;
    case 32: memory.WriteU32(address, static_cast<std::uint32_t>(value)); return;
    }
    throw std::invalid_argument("unsupported field width");
}

void CopyFields(GuestMemory& memory, FieldOperationRegisters& registers,
                std::span<const FieldCopyStep> steps)
{
    for (const auto& step : steps)
    {
        switch (step.kind)
        {
        case FieldCopyKind::AddressBase:
            registers.Set(step.destination, registers.Get(step.source) +
                static_cast<std::uint64_t>(static_cast<std::int64_t>(step.displacement)));
            break;
        case FieldCopyKind::Read:
            registers.Set(step.destination,
                ReadField(memory, Address(registers.Get(step.source), step.displacement), step.width));
            break;
        case FieldCopyKind::Write:
            WriteField(memory, Address(registers.Get(step.destination), step.displacement),
                       step.width, registers.Get(step.source));
            break;
        case FieldCopyKind::ReturnConstant:
            registers.r3 = step.constant;
            break;
        }
    }
}

void ReadGlobal(GuestMemory& memory, FieldOperationRegisters& registers,
                std::uint32_t global_base, std::optional<std::int32_t> adjustment,
                std::span<const GlobalFieldLoad> loads)
{
    registers.r11 = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(global_base)));
    if (adjustment)
        registers.r11 += static_cast<std::uint64_t>(static_cast<std::int64_t>(*adjustment));
    for (const auto& load : loads)
        registers.Set(load.destination,
            ReadField(memory, Address(registers.r11, load.displacement), load.width));
}

void CompareField(GuestMemory& memory, FieldOperationRegisters& registers,
                  FieldOperationRegister base, std::int32_t displacement,
                  std::uint8_t width, std::uint32_t comparison, bool invert)
{
    const std::uint32_t value = ReadField(memory, Address(registers.Get(base), displacement), width);
    // The original addi acts on the full register, then cntlzw inspects only its low word.
    const std::uint32_t difference = value - comparison;
    const std::uint32_t leading_zeros = std::countl_zero(difference);
    const std::uint64_t equal = difference == 0 ? 1 : 0;
    registers.r11 = invert ? equal : leading_zeros;
    registers.r3 = invert ? (equal ^ 1u) : equal;
}

} // namespace

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
    CopyFields(memory, registers, steps);
}

void CopyFixedFieldsAndReturnConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                                      std::span<const FieldCopyStep> steps)
{
    CopyFields(memory, registers, steps);
}

void ReadGlobalField(GuestMemory& memory, FieldOperationRegisters& registers,
                     std::uint32_t global_base, std::optional<std::int32_t> adjustment,
                     std::span<const GlobalFieldLoad> loads)
{
    ReadGlobal(memory, registers, global_base, adjustment, loads);
}

void ReadGlobalPointerChainField(GuestMemory& memory, FieldOperationRegisters& registers,
                                 std::uint32_t global_base,
                                 std::span<const GlobalFieldLoad> loads)
{
    ReadGlobal(memory, registers, global_base, std::nullopt, loads);
}

void FieldEqualsConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                         FieldOperationRegister base, std::int32_t displacement,
                         std::uint8_t width, std::uint32_t comparison)
{
    CompareField(memory, registers, base, displacement, width, comparison, false);
}

void FieldNotEqualsConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                            FieldOperationRegister base, std::int32_t displacement,
                            std::uint8_t width, std::uint32_t comparison)
{
    CompareField(memory, registers, base, displacement, width, comparison, true);
}

} // namespace lo::semantic::gpu
