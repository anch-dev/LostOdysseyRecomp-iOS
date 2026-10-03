#include "lo_semantics/pointer_fields.h"

#include <stdexcept>

namespace lo::semantic::gpu
{
namespace
{

std::uint32_t Address(std::uint64_t base, std::int32_t displacement)
{
    return static_cast<std::uint32_t>(base) + static_cast<std::uint32_t>(displacement);
}

std::uint32_t ReadUnsignedField(GuestMemory& memory, std::uint32_t address,
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

void WriteConstantField(GuestMemory& memory, std::uint32_t address,
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

std::uint32_t ScaleWordIndex(std::uint64_t index)
{
    return static_cast<std::uint32_t>(index) << 2;
}

} // namespace

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
    const std::uint32_t base = static_cast<std::uint32_t>(registers.Get(base_register));
    for (const auto& assignment : assignments)
        registers.Set(assignment.destination, assignment.value);
    for (const auto& write : writes)
        WriteConstantField(memory, Address(base, write.displacement), write.width, write.value);
}

void ReadPointerChainField(GuestMemory& memory, PointerFieldRegisters& registers,
                           PointerFieldRegister root_register,
                           std::span<const PointerFieldOffset> fields)
{
    if (fields.size() < 2) throw std::invalid_argument("pointer chain requires a link and field");
    std::uint64_t current = registers.Get(root_register);
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        current = ReadUnsignedField(memory, Address(current, fields[i].displacement),
                                    fields[i].width);
        if (i + 1 == fields.size()) registers.r3 = current;
        else registers.r11 = current;
    }
}

void AddressOfPointerChainMember(GuestMemory& memory, PointerFieldRegisters& registers,
                                 PointerFieldRegister root_register,
                                 std::span<const PointerFieldOffset> links,
                                 std::int32_t member_displacement)
{
    if (links.empty()) throw std::invalid_argument("pointer member requires a link");
    std::uint64_t current = registers.Get(root_register);
    for (const auto& link : links)
    {
        current = ReadUnsignedField(memory, Address(current, link.displacement), link.width);
        registers.r11 = current;
    }
    registers.r3 = current + static_cast<std::uint64_t>(static_cast<std::int64_t>(member_displacement));
}

void ReadBaseByteIndexField(GuestMemory& memory, PointerFieldRegisters& registers,
                            std::int32_t displacement)
{
    registers.r11 = registers.r3 + registers.r4;
    registers.r3 = memory.ReadU8(Address(registers.r11, displacement));
}

void ReadBiasedIndexField(GuestMemory& memory, PointerFieldRegisters& registers,
                          std::int32_t index_bias)
{
    const std::uint64_t biased = registers.r4 + static_cast<std::uint64_t>(
        static_cast<std::int64_t>(index_bias));
    registers.r11 = ScaleWordIndex(biased);
    const auto address = static_cast<std::uint32_t>(registers.r3) +
                         static_cast<std::uint32_t>(registers.r11);
    registers.r3 = memory.ReadU32(address);
}

void ReadPointerArrayElement(GuestMemory& memory, PointerFieldRegisters& registers,
                             std::int32_t array_pointer_displacement)
{
    registers.r11 = memory.ReadU32(Address(registers.r3, array_pointer_displacement));
    registers.r10 = ScaleWordIndex(registers.r4);
    const auto address = static_cast<std::uint32_t>(registers.r11) +
                         static_cast<std::uint32_t>(registers.r10);
    registers.r3 = memory.ReadU32(address);
}

} // namespace lo::semantic::gpu
