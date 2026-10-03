#pragma once

#include <stdexcept>

namespace lo::semantic::gpu
{
namespace detail
{
inline std::uint32_t PointerFieldAddress(std::uint64_t base, std::int32_t displacement)
{
    return static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(displacement);
}

template <typename Memory>
std::uint32_t ReadUnsignedPointerField(Memory& memory, std::uint32_t address,
                                       PointerFieldWidth width)
{
    switch (width)
    {
    case PointerFieldWidth::Byte: return memory.ReadU8(address);
    case PointerFieldWidth::Halfword: return memory.ReadU16(address);
    case PointerFieldWidth::Word: return memory.ReadU32(address);
    }
    throw std::invalid_argument("unsupported pointer field width");
}

template <typename Memory>
void WriteConstantPointerField(Memory& memory, std::uint32_t address,
                               PointerFieldWidth width, std::uint32_t value)
{
    switch (width)
    {
    case PointerFieldWidth::Byte: memory.WriteU8(address, static_cast<std::uint8_t>(value)); return;
    case PointerFieldWidth::Halfword: memory.WriteU16(address, static_cast<std::uint16_t>(value)); return;
    case PointerFieldWidth::Word: memory.WriteU32(address, value); return;
    }
    throw std::invalid_argument("unsupported pointer field width");
}

inline std::uint32_t ScalePointerWordIndex(std::uint64_t index)
{
    return static_cast<std::uint32_t>(index) << 2;
}
} // namespace detail

template <typename Memory>
void InitializeConstantFieldsWith(Memory& memory, PointerFieldRegisters& registers,
                                  PointerFieldRegister base_register,
                                  std::span<const ConstantFieldAssignment> assignments,
                                  std::span<const ConstantFieldWrite> writes)
{
    const std::uint32_t base = static_cast<std::uint32_t>(registers.Get(base_register));
    for (const auto& assignment : assignments)
        registers.Set(assignment.destination, assignment.value);
    for (const auto& write : writes)
        detail::WriteConstantPointerField(memory,
            detail::PointerFieldAddress(base, write.displacement), write.width, write.value);
}

template <typename Memory>
void ReadPointerChainFieldWith(Memory& memory, PointerFieldRegisters& registers,
                               PointerFieldRegister root_register,
                               std::span<const PointerFieldOffset> fields)
{
    if (fields.size() < 2) throw std::invalid_argument("pointer chain requires a link and field");
    std::uint64_t current = registers.Get(root_register);
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        current = detail::ReadUnsignedPointerField(memory,
            detail::PointerFieldAddress(current, fields[i].displacement), fields[i].width);
        if (i + 1 == fields.size()) registers.r3 = current;
        else registers.r11 = current;
    }
}

template <typename Memory>
void AddressOfPointerChainMemberWith(Memory& memory, PointerFieldRegisters& registers,
                                     PointerFieldRegister root_register,
                                     std::span<const PointerFieldOffset> links,
                                     std::int32_t member_displacement)
{
    if (links.empty()) throw std::invalid_argument("pointer member requires a link");
    std::uint64_t current = registers.Get(root_register);
    for (const auto& link : links)
    {
        current = detail::ReadUnsignedPointerField(memory,
            detail::PointerFieldAddress(current, link.displacement), link.width);
        registers.r11 = current;
    }
    registers.r3 = current + static_cast<std::uint64_t>(static_cast<std::int64_t>(member_displacement));
}

template <typename Memory>
void ReadBaseByteIndexFieldWith(Memory& memory, PointerFieldRegisters& registers,
                                std::int32_t displacement)
{
    registers.r11 = registers.r3 + registers.r4;
    registers.r3 = memory.ReadU8(detail::PointerFieldAddress(registers.r11, displacement));
}

template <typename Memory>
void ReadBiasedIndexFieldWith(Memory& memory, PointerFieldRegisters& registers,
                              std::int32_t index_bias)
{
    const std::uint64_t biased = registers.r4 + static_cast<std::uint64_t>(
        static_cast<std::int64_t>(index_bias));
    registers.r11 = detail::ScalePointerWordIndex(biased);
    const auto address = static_cast<std::uint32_t>(registers.r3) +
                         static_cast<std::uint32_t>(registers.r11);
    registers.r3 = memory.ReadU32(address);
}

template <typename Memory>
void ReadPointerArrayElementWith(Memory& memory, PointerFieldRegisters& registers,
                                 std::int32_t array_pointer_displacement)
{
    registers.r11 = memory.ReadU32(detail::PointerFieldAddress(registers.r3, array_pointer_displacement));
    registers.r10 = detail::ScalePointerWordIndex(registers.r4);
    const auto address = static_cast<std::uint32_t>(registers.r11) +
                         static_cast<std::uint32_t>(registers.r10);
    registers.r3 = memory.ReadU32(address);
}
} // namespace lo::semantic::gpu
