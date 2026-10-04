#include "lo_semantics/object_registration_post.h"

#include "lo_semantics/object_startup.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::object_registration_post
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void CompareWord(Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = Address(left), b = Address(right);
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

void CompareSignedZero(Registers& state, std::uint64_t value)
{
    const auto word = std::bit_cast<std::int32_t>(Address(value));
    state.cr6 = {std::uint8_t(word < 0), std::uint8_t(word > 0),
        std::uint8_t(word == 0), state.xer_so};
}

void Restore(GuestMemory& memory, Registers& state)
{
    R(state, 1) += 112u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 29) = ReadU64(memory, Address(R(state, 1) - 32u));
    R(state, 30) = ReadU64(memory, Address(R(state, 1) - 24u));
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 16u));
}

void GetAssociated(GuestMemory& memory, Dependencies dependencies,
    Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    std::uint64_t result = 0;
    (void)registered_constructor_family::Apply(0x8242cf78u, memory,
        dependencies.manager, dependencies.constructor_registration,
        R(state, 3), Address(R(state, 1)), result);
    R(state, 3) = result;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (entry != 0x82627230u)
        return false;

    constexpr GuestAddress Singleton = 0x833189ecu;
    constexpr GuestAddress Shared = 0x83315f60u;
    constexpr GuestAddress Primary = 0x83315f9cu;
    constexpr GuestAddress Ready = 0x83315ed8u;

    const auto caller_sp = R(state, 1);
    R(state, 12) = state.lr;
    memory.WriteU32(Address(caller_sp - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(caller_sp - 32u), R(state, 29));
    WriteU64(memory, Address(caller_sp - 24u), R(state, 30));
    WriteU64(memory, Address(caller_sp - 16u), R(state, 31));
    memory.WriteU32(Address(caller_sp - 112u), Address(caller_sp));
    R(state, 1) -= 112u;

    GetAssociated(memory, dependencies, state, 0x82627240u);
    R(state, 29) = 0xffffffff83320000ull;
    R(state, 11) = memory.ReadU32(Singleton);
    CompareWord(state, R(state, 3), R(state, 11));
    if (!state.cr6.eq)
    {
        GetAssociated(memory, dependencies, state, 0x82627254u);
        R(state, 11) = memory.ReadU32(Singleton);
        memory.WriteU32(Address(R(state, 11) + 60u), Address(R(state, 3)));
    }
    else
    {
        R(state, 10) = 0;
        memory.WriteU32(Address(R(state, 11) + 60u), 0);
    }

    R(state, 31) = 0xffffffff83310000ull;
    R(state, 30) = 0xffffffff8218c210ull;
    R(state, 10) = memory.ReadU32(Shared);
    CompareWord(state, R(state, 10), 0);
    if (state.cr6.eq)
    {
        R(state, 3) = R(state, 30);
        state.lr = 0x82627288u;
        std::uint64_t constructed = 0;
        (void)registered_constructor_family::Apply(0x82403148u, memory,
            dependencies.manager, dependencies.constructor_registration,
            R(state, 3), Address(R(state, 1)), constructed);
        R(state, 3) = constructed;
        memory.WriteU32(Shared, Address(R(state, 3)));
        state.lr = 0x82627290u;
        R(state, 3) = registered_callback_family::RegisterSharedMetadataObject(
            memory, dependencies.manager, dependencies.callback,
            R(state, 3), Address(R(state, 1)));
        R(state, 11) = memory.ReadU32(Singleton);
        R(state, 10) = memory.ReadU32(Shared);
    }
    R(state, 31) = 0xffffffff83310000ull;
    memory.WriteU32(Address(R(state, 11) + 196u), Address(R(state, 10)));

    R(state, 10) = memory.ReadU32(Primary);
    CompareWord(state, R(state, 10), 0);
    if (state.cr6.eq)
    {
        R(state, 3) = R(state, 30);
        state.lr = 0x826272b4u;
        R(state, 3) = ConstructRegisteredObject(memory,
            dependencies.manager, R(state, 3), Address(R(state, 1)));
        memory.WriteU32(Primary, Address(R(state, 3)));
        state.lr = 0x826272bcu;
        R(state, 3) = RegisterObjectGraph(memory, dependencies.graph);
        R(state, 11) = memory.ReadU32(Singleton);
        R(state, 10) = memory.ReadU32(Primary);
    }

    R(state, 9) = 0xffffffff83310000ull;
    memory.WriteU32(Address(R(state, 11) + 52u), Address(R(state, 10)));
    R(state, 9) = memory.ReadU32(Ready);
    CompareSignedZero(state, R(state, 9));
    if (!state.cr6.eq)
    {
        R(state, 31) = Address(R(state, 10));
        state.lr = 0x826272e0u;
        R(state, 3) = GetPrimaryRegisteredObject(memory,
            dependencies.manager, dependencies.graph, Address(R(state, 1)));
        CompareWord(state, R(state, 31), R(state, 3));
        if (state.cr6.eq)
        {
            R(state, 3) = memory.ReadU32(Singleton);
            R(state, 11) = memory.ReadU32(Address(R(state, 3)));
            R(state, 11) = memory.ReadU32(Address(R(state, 11) + 124u));
            state.ctr = R(state, 11);
            state.lr = 0x826272fcu;
            dependencies.dynamic.Call(Address(state.ctr) & ~GuestAddress{3},
                memory, state);
        }
    }

    Restore(memory, state);
    return true;
}

} // namespace lo::semantic::gpu::object_registration_post
