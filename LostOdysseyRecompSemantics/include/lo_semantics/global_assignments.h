#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace lo::semantic::gpu
{

enum class GlobalAssignmentRegister : std::uint8_t
{
    R3, R4, R5, R6, R7, R8, R9, R10, R11
};

struct GlobalAssignmentRegisters
{
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0, r8 = 0, r9 = 0;
    std::uint64_t r10 = 0, r11 = 0;

    [[nodiscard]] std::uint64_t Get(GlobalAssignmentRegister reg) const;
    void Set(GlobalAssignmentRegister reg, std::uint64_t value);
};

struct ConstantRegisterValue
{
    GlobalAssignmentRegister reg;
    std::uint64_t value;
};

enum class AssignmentAddressKind : std::uint8_t { ObjectField, Global };
enum class AssignmentValueKind : std::uint8_t { InputRegister, Constant };
enum class AssignmentWidth : std::uint8_t { Byte = 1, Halfword = 2, Word = 4 };

struct AssignmentFieldWrite
{
    AssignmentAddressKind address_kind;
    GlobalAssignmentRegister base_register;
    std::uint32_t global_address;
    std::int32_t displacement;
    AssignmentWidth width;
    AssignmentValueKind value_kind;
    GlobalAssignmentRegister value_register;
    std::uint32_t constant_value;
    std::span<const ConstantRegisterValue> register_values_before;
};

// Initialize object/output and fixed global fields in the observed order.
// Register values before a write represent folded constant setup, including
// setup between writes; input registers are read at the moment of each write.
void WriteFieldAssignments(GuestMemory& memory, GlobalAssignmentRegisters& registers,
                           std::span<const AssignmentFieldWrite> writes,
                           std::span<const ConstantRegisterValue> register_values_after);

} // namespace lo::semantic::gpu

#include "lo_semantics/detail/global_assignments_impl.h"
