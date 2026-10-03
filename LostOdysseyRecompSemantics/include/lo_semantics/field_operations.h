#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>
#include <optional>
#include <span>

namespace lo::semantic::gpu
{

enum class FieldOperationRegister : std::uint8_t
{
    R3, R4, R5, R6, R7, R8, R9, R10, R11
};

struct FieldOperationRegisters
{
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    std::uint64_t r8 = 0, r9 = 0, r10 = 0, r11 = 0;

    [[nodiscard]] std::uint64_t Get(FieldOperationRegister reg) const;
    void Set(FieldOperationRegister reg, std::uint64_t value);
};

enum class FieldCopyKind : std::uint8_t
{
    AddressBase, Read, Write, ReturnConstant
};

struct FieldCopyStep
{
    FieldCopyKind kind;
    FieldOperationRegister destination;
    FieldOperationRegister source;
    std::int32_t displacement = 0;
    std::uint8_t width = 0;
    std::uint64_t constant = 0;
};

// Retain the recorded read and write order. Adjacent guest fields may alias.
void CopyFixedFields(GuestMemory& memory, FieldOperationRegisters& registers,
                     std::span<const FieldCopyStep> steps);
void CopyFixedFieldsAndReturnConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                                      std::span<const FieldCopyStep> steps);

struct GlobalFieldLoad
{
    FieldOperationRegister destination;
    std::int32_t displacement;
    std::uint8_t width;
};

// The global base is a signed PPC lis result; adjustment models an optional addi.
void ReadGlobalField(GuestMemory& memory, FieldOperationRegisters& registers,
                     std::uint32_t global_base, std::optional<std::int32_t> adjustment,
                     std::span<const GlobalFieldLoad> loads);
void ReadGlobalPointerChainField(GuestMemory& memory, FieldOperationRegisters& registers,
                                 std::uint32_t global_base,
                                 std::span<const GlobalFieldLoad> loads);

void FieldEqualsConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                         FieldOperationRegister base, std::int32_t displacement,
                         std::uint8_t width, std::uint32_t comparison);
void FieldNotEqualsConstant(GuestMemory& memory, FieldOperationRegisters& registers,
                            FieldOperationRegister base, std::int32_t displacement,
                            std::uint8_t width, std::uint32_t comparison);

} // namespace lo::semantic::gpu
