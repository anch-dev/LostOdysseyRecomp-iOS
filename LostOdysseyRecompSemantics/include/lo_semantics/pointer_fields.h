#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>
#include <span>

namespace lo::semantic::gpu
{

enum class PointerFieldRegister : std::uint8_t { R3, R4, R5, R6, R9, R10, R11, R13 };
enum class PointerFieldWidth : std::uint8_t { Byte = 1, Halfword = 2, Word = 4 };

struct PointerFieldRegisters
{
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0;
    std::uint64_t r9 = 0, r10 = 0, r11 = 0, r13 = 0;

    [[nodiscard]] std::uint64_t Get(PointerFieldRegister reg) const;
    void Set(PointerFieldRegister reg, std::uint64_t value);
};

struct ConstantFieldAssignment
{
    PointerFieldRegister destination;
    std::uint64_t value;
};

struct ConstantFieldWrite
{
    std::int32_t displacement;
    PointerFieldWidth width;
    std::uint32_t value;
};

struct PointerFieldOffset
{
    std::int32_t displacement;
    PointerFieldWidth width;
};

// Initialize fields in the recorded store order. The base is the incoming
// register value; assignments never target the base in this recovered family.
void InitializeConstantFields(GuestMemory& memory, PointerFieldRegisters& registers,
                              PointerFieldRegister base_register,
                              std::span<const ConstantFieldAssignment> assignments,
                              std::span<const ConstantFieldWrite> writes);

// Follow word-sized links, then read the final unsigned field into r3.
void ReadPointerChainField(GuestMemory& memory, PointerFieldRegisters& registers,
                           PointerFieldRegister root_register,
                           std::span<const PointerFieldOffset> fields);

// Follow word-sized links and return the address of a member in r3.
void AddressOfPointerChainMember(GuestMemory& memory, PointerFieldRegisters& registers,
                                 PointerFieldRegister root_register,
                                 std::span<const PointerFieldOffset> links,
                                 std::int32_t member_displacement);

void ReadBaseByteIndexField(GuestMemory& memory, PointerFieldRegisters& registers,
                            std::int32_t displacement);
void ReadBiasedIndexField(GuestMemory& memory, PointerFieldRegisters& registers,
                          std::int32_t index_bias);
void ReadPointerArrayElement(GuestMemory& memory, PointerFieldRegisters& registers,
                             std::int32_t array_pointer_displacement);

} // namespace lo::semantic::gpu
