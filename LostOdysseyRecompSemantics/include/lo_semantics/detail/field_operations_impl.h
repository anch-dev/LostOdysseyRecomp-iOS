#pragma once

#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu
{
namespace detail
{
inline std::uint32_t FieldOperationAddress(std::uint64_t base, std::int32_t displacement)
{
    return static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(displacement);
}

template <typename Memory>
std::uint32_t ReadOperationField(Memory& memory, std::uint32_t address, std::uint8_t width)
{
    switch (width)
    {
    case 8: return memory.ReadU8(address);
    case 16: return memory.ReadU16(address);
    case 32: return memory.ReadU32(address);
    }
    throw std::invalid_argument("unsupported field width");
}

template <typename Memory>
void WriteOperationField(Memory& memory, std::uint32_t address,
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

template <typename Memory>
void CopyOperationFields(Memory& memory, FieldOperationRegisters& registers,
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
                ReadOperationField(memory,
                    FieldOperationAddress(registers.Get(step.source), step.displacement), step.width));
            break;
        case FieldCopyKind::Write:
            WriteOperationField(memory,
                FieldOperationAddress(registers.Get(step.destination), step.displacement),
                step.width, registers.Get(step.source));
            break;
        case FieldCopyKind::ReturnConstant:
            registers.r3 = step.constant;
            break;
        }
    }
}

template <typename Memory>
void ReadOperationGlobal(Memory& memory, FieldOperationRegisters& registers,
                         std::uint32_t global_base, std::optional<std::int32_t> adjustment,
                         std::span<const GlobalFieldLoad> loads)
{
    registers.r11 = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(global_base)));
    if (adjustment)
        registers.r11 += static_cast<std::uint64_t>(static_cast<std::int64_t>(*adjustment));
    for (const auto& load : loads)
        registers.Set(load.destination,
            ReadOperationField(memory, FieldOperationAddress(registers.r11, load.displacement), load.width));
}

template <typename Memory>
void CompareOperationField(Memory& memory, FieldOperationRegisters& registers,
                           FieldOperationRegister base, std::int32_t displacement,
                           std::uint8_t width, std::uint32_t comparison, bool invert)
{
    const std::uint32_t value = ReadOperationField(memory,
        FieldOperationAddress(registers.Get(base), displacement), width);
    const std::uint32_t difference = value - comparison;
    const std::uint32_t leading_zeros = std::countl_zero(difference);
    const std::uint64_t equal = difference == 0 ? 1 : 0;
    registers.r11 = invert ? equal : leading_zeros;
    registers.r3 = invert ? (equal ^ 1u) : equal;
}
} // namespace detail

template <typename Memory>
void CopyFixedFieldsWith(Memory& memory, FieldOperationRegisters& registers,
                         std::span<const FieldCopyStep> steps)
{
    detail::CopyOperationFields(memory, registers, steps);
}

template <typename Memory>
void CopyFixedFieldsAndReturnConstantWith(Memory& memory, FieldOperationRegisters& registers,
                                          std::span<const FieldCopyStep> steps)
{
    detail::CopyOperationFields(memory, registers, steps);
}

template <typename Memory>
void ReadGlobalFieldWith(Memory& memory, FieldOperationRegisters& registers,
                         std::uint32_t global_base, std::optional<std::int32_t> adjustment,
                         std::span<const GlobalFieldLoad> loads)
{
    detail::ReadOperationGlobal(memory, registers, global_base, adjustment, loads);
}

template <typename Memory>
void ReadGlobalPointerChainFieldWith(Memory& memory, FieldOperationRegisters& registers,
                                     std::uint32_t global_base,
                                     std::span<const GlobalFieldLoad> loads)
{
    detail::ReadOperationGlobal(memory, registers, global_base, std::nullopt, loads);
}

template <typename Memory>
void FieldEqualsConstantWith(Memory& memory, FieldOperationRegisters& registers,
                             FieldOperationRegister base, std::int32_t displacement,
                             std::uint8_t width, std::uint32_t comparison)
{
    detail::CompareOperationField(memory, registers, base, displacement, width, comparison, false);
}

template <typename Memory>
void FieldNotEqualsConstantWith(Memory& memory, FieldOperationRegisters& registers,
                                FieldOperationRegister base, std::int32_t displacement,
                                std::uint8_t width, std::uint32_t comparison)
{
    detail::CompareOperationField(memory, registers, base, displacement, width, comparison, true);
}
} // namespace lo::semantic::gpu
