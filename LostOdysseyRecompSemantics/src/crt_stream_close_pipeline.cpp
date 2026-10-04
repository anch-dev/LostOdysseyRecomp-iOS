#include "lo_semantics/crt_stream_close_pipeline.h"

#include "lo_semantics/recovery_abi.h"

#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_close_pipeline
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

constexpr GuestAddress kPipeline = 0x82b85cc8u;
constexpr GuestAddress kBufferRelease = 0x82b87c90u;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }

void CompareSigned(crt_stream_operations::Condition& condition,
    std::uint64_t left, std::uint8_t so)
{
    const auto word = static_cast<std::int32_t>(Address(left));
    condition = {std::uint8_t(word < 0), std::uint8_t(word > 0),
        std::uint8_t(word == 0), so};
}

void CompareUnsigned(crt_stream_operations::Condition& condition,
    std::uint64_t left, std::uint8_t so)
{
    const auto word = Address(left);
    condition = {0u, std::uint8_t(word != 0),
        std::uint8_t(word == 0), so};
}

void CallFree(GuestMemory& memory, Dependencies dependencies,
    Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    if (!crt_free_context::Apply(0x823addc0u, memory,
        dependencies.free_lower, state))
        throw std::logic_error("missing accepted CRT free context");
}

void BufferRelease(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(state.sp - 16u), R(state, 31));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    R(state, 31) = R(state, 3);
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 12u));
    R(state, 10) = R(state, 11) & 131u;
    CompareSigned(state.cr0, R(state, 10), state.xer_so);
    CompareSigned(state.cr0, R(state, 10), state.xer_so);
    if (!state.cr0.eq)
    {
        R(state, 11) = WordRotateMask(R(state, 11), 0, 0x8u);
        CompareSigned(state.cr0, R(state, 11), state.xer_so);
        if (!state.cr0.eq)
        {
            R(state, 3) = memory.ReadU32(Address(R(state, 31) + 8u));
            CallFree(memory, dependencies, state, 0x82b87cc4u);
            R(state, 10) = memory.ReadU32(Address(R(state, 31) + 12u));
            R(state, 11) = 0;
            R(state, 10) = WordRotateMask(R(state, 10), 0,
                0xfffffffffffffff7ull);
            R(state, 10) = WordRotateMask(R(state, 10), 0,
                0xfffffffffffffbffull);
            memory.WriteU32(Address(R(state, 31)), 0);
            memory.WriteU32(Address(R(state, 31) + 8u), 0);
            memory.WriteU32(Address(R(state, 31) + 4u), 0);
            memory.WriteU32(Address(R(state, 31) + 12u), Address(R(state, 10)));
        }
    }
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void EnterPipeline(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 29; index <= 31; ++index)
        WriteU64(memory, Address(state.sp - 16u - 8u * (31u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    state.lr = 0x82b85cd0u;
    memory.WriteU32(Address(state.sp - 112u), Address(state.sp));
    state.sp -= 112u;
}

void LeavePipeline(GuestMemory& memory, Registers& state)
{
    state.sp += 112u;
    for (unsigned index = 29; index <= 31; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 16u - 8u * (31u - index)));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void Accepted(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    if (!crt_stream_operations::ApplyAcceptedCallee(address, memory,
        dependencies.close.accepted, state))
        throw std::logic_error("missing accepted stream callee");
}

void Pipeline(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    EnterPipeline(memory, state);
    R(state, 31) = R(state, 3);
    R(state, 30) = UINT64_MAX;
    CompareUnsigned(state.cr6, R(state, 31), state.xer_so);
    if (state.cr6.eq)
    {
        Accepted(0x82b7fd78u, memory, dependencies, state, 0x82b85ce8u);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22;
        R(state, 7) = R(state, 6) = R(state, 5) =
            R(state, 4) = R(state, 3) = 0;
        memory.WriteU32(Address(R(state, 11)), 22u);
        Accepted(0x82b7fec0u, memory, dependencies, state, 0x82b85d0cu);
        R(state, 3) = UINT64_MAX;
        LeavePipeline(memory, state);
        return;
    }
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 12u));
    R(state, 29) = 0;
    R(state, 11) &= 131u;
    CompareSigned(state.cr0, R(state, 11), state.xer_so);
    CompareSigned(state.cr0, R(state, 11), state.xer_so);
    if (!state.cr0.eq)
    {
        R(state, 3) = R(state, 31);
        state.lr = 0x82b85d30u;
        if (!crt_format_stream::Apply(0x82b7b838u, memory,
            dependencies.format, state))
            throw std::logic_error("missing accepted flush callee");
        R(state, 30) = R(state, 3);
        R(state, 3) = R(state, 31);
        state.lr = 0x82b85d3cu;
        BufferRelease(memory, dependencies, state);
        R(state, 3) = R(state, 31);
        Accepted(0x82b81648u, memory, dependencies, state, 0x82b85d44u);
        state.lr = 0x82b85d48u;
        if (!crt_stream_close_caller::Apply(0x82b87b18u, memory,
            dependencies.close, state))
            throw std::logic_error("missing recovered close caller");
        CompareSigned(state.cr0, R(state, 3), state.xer_so);
        if (state.cr0.lt)
            R(state, 30) = UINT64_MAX;
        else
        {
            R(state, 3) = memory.ReadU32(Address(R(state, 31) + 28u));
            CompareUnsigned(state.cr0, R(state, 3), state.xer_so);
            if (!state.cr0.eq)
            {
                CallFree(memory, dependencies, state, 0x82b85d68u);
                memory.WriteU32(Address(R(state, 31) + 28u), Address(R(state, 29)));
            }
        }
    }
    R(state, 3) = R(state, 30);
    memory.WriteU32(Address(R(state, 31) + 12u), Address(R(state, 29)));
    LeavePipeline(memory, state);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (address)
    {
    case kPipeline: Pipeline(memory, dependencies, state); return true;
    case kBufferRelease: BufferRelease(memory, dependencies, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_close_pipeline
