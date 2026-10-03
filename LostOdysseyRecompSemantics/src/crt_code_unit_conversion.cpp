#include "lo_semantics/crt_code_unit_conversion.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_code_unit_conversion
{
namespace
{
using recovery_abi::Address;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void Compare(Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = static_cast<std::uint32_t>(left);
    const auto b = static_cast<std::uint32_t>(right);
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

void Enter(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
}

void Leave(GuestMemory& memory, Registers& state)
{
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

class ErrorAddressServices final : public AllocationFailureServices
{
public:
    ErrorAddressServices(GuestMemory& memory, CrtThreadDataServices& thread,
        Registers& state) : memory_(memory), thread_(thread), state_(state) {}

    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall call{R(state_, 13)};
        const auto record = GetCrtThreadData(memory_, thread_, call);
        R(state_, 13) = call.thread_environment;
        R(state_, 3) = record;
        return record;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::logic_error("unexpected CRT error output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::logic_error("unexpected CRT bug check"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::logic_error("unexpected CRT new handler"); }

private:
    GuestMemory& memory_;
    CrtThreadDataServices& thread_;
    Registers& state_;
};

void CallErrorAddress(GuestMemory& memory, Dependencies dependencies,
    Registers& state, std::uint32_t return_address)
{
    state.lr = return_address;
    Enter(memory, state); // 82B7FD78 has its own 96-byte frame.
    ErrorAddressServices services(memory, dependencies.thread, state);
    R(state, 3) = GetAllocationErrorAddress(services);
    Leave(memory, state);
}

void CallInvalid(GuestMemory& memory, Dependencies dependencies,
    Registers& state, std::uint32_t return_address)
{
    state.lr = return_address;
    InvalidParameterCall call{{{R(state, 3), R(state, 4), R(state, 5),
        R(state, 6), R(state, 7), R(state, 8), R(state, 9), R(state, 10)}},
        R(state, 13)};
    R(state, 3) = ReportInvalidParameter(memory, dependencies.invalid, call);
    for (unsigned index = 1; index < 8; ++index)
        R(state, index + 3u) = call.arguments[index];
    R(state, 13) = call.thread_environment;
}

void Convert(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    Enter(memory, state);
    R(state, 11) = R(state, 4);
    Compare(state, R(state, 11), 0);
    if (state.cr6.eq)
    {
        Compare(state, R(state, 5), 0);
        if (!state.cr6.eq)
        {
            Compare(state, R(state, 3), 0);
            if (state.cr6.eq)
            {
                R(state, 3) = 0;
                Leave(memory, state);
                return;
            }
            // A null output and nonzero extent write r4's low word (zero)
            // through the count slot. No buffer or errno path is entered.
            memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
            R(state, 3) = 0;
            Leave(memory, state);
            return;
        }
    }

    Compare(state, R(state, 3), 0);
    if (!state.cr6.eq)
    {
        R(state, 10) = ~std::uint64_t{0};
        memory.WriteU32(Address(R(state, 3)), Address(R(state, 10)));
    }
    R(state, 10) = 0x7fffffffu;
    Compare(state, R(state, 5), R(state, 10));
    if (state.cr6.gt)
    {
        CallErrorAddress(memory, dependencies, state, 0x82b86b08u);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22;
        for (unsigned index = 3; index <= 7; ++index)
            R(state, index) = 0;
        memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
        CallInvalid(memory, dependencies, state, 0x82b86b2cu);
        R(state, 3) = 22;
        Leave(memory, state);
        return;
    }

    R(state, 10) = static_cast<std::uint32_t>(R(state, 6)) & 0xffffu;
    Compare(state, R(state, 10), 255);
    if (state.cr6.gt)
    {
        Compare(state, R(state, 11), 0);
        if (!state.cr6.eq)
        {
            Compare(state, R(state, 5), 0);
            if (!state.cr6.eq)
            {
                R(state, 4) = 0;
                R(state, 3) = R(state, 11);
                state.lr = 0x82b86b5cu;
                R(state, 3) = FillGuestMemory(memory, Address(R(state, 3)),
                    static_cast<std::uint32_t>(R(state, 4)),
                    static_cast<std::uint32_t>(R(state, 5)));
            }
        }
        CallErrorAddress(memory, dependencies, state, 0x82b86b60u);
        R(state, 11) = 42;
        memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
        CallErrorAddress(memory, dependencies, state, 0x82b86b6cu);
        R(state, 3) = memory.ReadU32(Address(R(state, 3)));
        Leave(memory, state);
        return;
    }

    Compare(state, R(state, 11), 0);
    if (!state.cr6.eq)
    {
        Compare(state, R(state, 5), 0);
        if (state.cr6.eq)
        {
            CallErrorAddress(memory, dependencies, state, 0x82b86b88u);
            R(state, 11) = R(state, 3);
            R(state, 10) = 34;
            for (unsigned index = 3; index <= 7; ++index)
                R(state, index) = 0;
            memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
            CallInvalid(memory, dependencies, state, 0x82b86bacu);
            R(state, 3) = 34;
            Leave(memory, state);
            return;
        }
        memory.WriteU8(Address(R(state, 11)),
            static_cast<std::uint8_t>(R(state, 6)));
    }
    Compare(state, R(state, 3), 0);
    if (!state.cr6.eq)
    {
        R(state, 11) = 1;
        memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
    }
    R(state, 3) = 0;
    Leave(memory, state);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (address == 0x82b86be0u)
        R(registers, 7) = 0; // Tail branch: no wrapper frame or LR change.
    else if (address != 0x82b86ab8u)
        return false;
    Convert(memory, dependencies, registers);
    return true;
}
} // namespace lo::semantic::gpu::crt_code_unit_conversion
