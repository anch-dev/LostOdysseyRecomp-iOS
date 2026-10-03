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
    WriteFieldAssignmentsWith(memory, registers, writes, register_values_after);
}

} // namespace lo::semantic::gpu
