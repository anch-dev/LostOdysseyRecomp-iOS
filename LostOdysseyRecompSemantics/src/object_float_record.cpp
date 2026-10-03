#include "lo_semantics/object_float_record.h"

#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::object_float_record
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

void CompareWord(Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = Address(left), b = Address(right);
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

void CompareSignedZero(Registers& state, std::uint64_t value)
{
    const auto word = static_cast<std::int32_t>(Address(value));
    state.cr6 = {std::uint8_t(word < 0), std::uint8_t(word > 0),
        std::uint8_t(word == 0), state.xer_so};
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state)
{
    if (entry != 0x82384c08u)
        return false;

    const auto original_sp = state.sp;
    state.r12 = state.lr;
    memory.WriteU32(Address(original_sp - 8u), Address(state.r12));
    WriteU64(memory, Address(original_sp - 16u), state.r31);
    memory.WriteU32(Address(original_sp - 96u), Address(original_sp));
    state.sp -= 96u;
    state.r31 = state.r3;
    CompareWord(state, state.r31, 0);

    bool found = false;
    if (!state.cr6.eq)
    {
        // The direct branch's link address is visible to the nested PPC
        // frame. The accepted lower handles its own constructor semantics.
        state.lr = 0x82384c28u;
        std::uint64_t singleton = 0;
        (void)registered_constructor_family::Apply(0x8242d038u,
            memory, manager_services, registration_services, state.r3,
            Address(state.sp), singleton);
        state.r3 = singleton;
        state.r11 = memory.ReadU32(Address(state.r31 + 52u));
        CompareWord(state, state.r11, 0);
        while (!state.cr6.eq)
        {
            CompareWord(state, state.r11, state.r3);
            if (state.cr6.eq)
            {
                found = true;
                break;
            }
            state.r11 = memory.ReadU32(Address(state.r11 + 60u));
            CompareWord(state, state.r11, 0);
        }
        if (!found)
        {
            state.r11 = Address(state.r3) == 0 ? 1u : 0u;
            CompareSignedZero(state, state.r11);
            found = !state.cr6.eq;
        }
    }

    state.r3 = found ? state.r31 : 0;
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    state.r31 = ReadU64(memory, Address(state.sp - 16u));
    return true;
}

} // namespace lo::semantic::gpu::object_float_record
