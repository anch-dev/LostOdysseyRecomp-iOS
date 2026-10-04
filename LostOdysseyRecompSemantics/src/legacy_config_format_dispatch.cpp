#include "lo_semantics/legacy_config_format_dispatch.h"

#include "lo_semantics/legacy_config_format_heap_context.h"
#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_config_format_dispatch
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{ return index == 1u ? state.sp : state.r[index]; }

void CompareSigned(Registers& state, std::uint64_t left,
    std::int32_t right)
{
    const auto word = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    state.cr6 = {std::uint8_t(word < right), std::uint8_t(word > right),
        std::uint8_t(word == right), state.xer_so};
}

void CompareUnsigned64(Registers& state, std::uint64_t left)
{
    state.cr6 = {0u, std::uint8_t(left != 0u),
        std::uint8_t(left == 0u), state.xer_so};
}

void Save27(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(memory, Address(state.sp - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
}

void Restore27(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 27u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

heap_reallocate::Registers ToHeap(const Registers& state)
{
    heap_reallocate::Registers lower{};
    lower.sp = state.sp; lower.lr = state.lr; lower.ctr = state.ctr;
    lower.r3 = state.r[3]; lower.r4 = state.r[4];
    lower.r5 = state.r[5]; lower.r6 = state.r[6];
    lower.r7 = state.r[7]; lower.r8 = state.r[8];
    lower.r9 = state.r[9]; lower.r10 = state.r[10];
    lower.r11 = state.r[11]; lower.r12 = state.r[12];
    lower.r13 = state.r[13]; lower.r19 = state.r[19];
    lower.r20 = state.r[20]; lower.r21 = state.r[21];
    lower.r22 = state.r[22]; lower.r23 = state.r[23];
    lower.r24 = state.r[24]; lower.r25 = state.r[25];
    lower.r26 = state.r[26]; lower.r27 = state.r[27];
    lower.r28 = state.r[28]; lower.r29 = state.r[29];
    lower.r30 = state.r[30]; lower.r31 = state.r[31];
    lower.xer_so = state.xer_so;
    lower.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq,
        state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq,
        state.cr6.so};
    return lower;
}

void FromHeap(Registers& state, const heap_reallocate::Registers& lower)
{
    state.sp = lower.sp; state.lr = lower.lr; state.ctr = lower.ctr;
    state.r[3] = lower.r3; state.r[4] = lower.r4;
    state.r[5] = lower.r5; state.r[6] = lower.r6;
    state.r[7] = lower.r7; state.r[8] = lower.r8;
    state.r[9] = lower.r9; state.r[10] = lower.r10;
    state.r[11] = lower.r11; state.r[12] = lower.r12;
    state.r[13] = lower.r13; state.r[19] = lower.r19;
    state.r[20] = lower.r20; state.r[21] = lower.r21;
    state.r[22] = lower.r22; state.r[23] = lower.r23;
    state.r[24] = lower.r24; state.r[25] = lower.r25;
    state.r[26] = lower.r26; state.r[27] = lower.r27;
    state.r[28] = lower.r28; state.r[29] = lower.r29;
    state.r[30] = lower.r30; state.r[31] = lower.r31;
    state.xer_so = lower.xer_so;
    state.cr0 = {lower.cr0.lt, lower.cr0.gt, lower.cr0.eq,
        lower.cr0.so};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq,
        lower.cr6.so};
}

raw_allocation_context::Registers ToRaw(const Registers& state)
{
    raw_allocation_context::Registers lower{};
    lower.r=state.r;
    lower.r[1]=state.sp;
    lower.lr=state.lr;lower.ctr=state.ctr;
    lower.xer_so=state.xer_so;lower.xer_ca=state.xer_ca;
    lower.cr0={state.cr0.lt,state.cr0.gt,state.cr0.eq,state.cr0.so};
    lower.cr6={state.cr6.lt,state.cr6.gt,state.cr6.eq,state.cr6.so};
    return lower;
}

void FromRaw(Registers& state,const raw_allocation_context::Registers& lower)
{
    state.r=lower.r;
    state.sp=lower.r[1];state.r[1]=0u;
    state.lr=lower.lr;state.ctr=lower.ctr;
    state.xer_so=lower.xer_so;state.xer_ca=lower.xer_ca;
    state.cr0={lower.cr0.lt,lower.cr0.gt,lower.cr0.eq,lower.cr0.un};
    state.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.un};
}

class RawHeapBoundary final : public raw_allocation_context::PpcBoundaryServices
{
public:
    explicit RawHeapBoundary(CrtAllocationServices& allocation)
        : allocation_(allocation) {}

    void CallDirect(GuestAddress entry,GuestMemory& memory,
        raw_allocation_context::Registers& state) override
    {
        if(entry!=0x823accb0u ||
            !legacy_config_format_heap_context::Apply(memory,allocation_,
                state))
            throw std::logic_error("unsupported selected heap branch");
    }
private:
    CrtAllocationServices& allocation_;
};

void CallReallocateNull(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    const auto caller_sp=Address(state.sp);
    Save27(memory,state);
    state.lr=0x823acae0u;
    memory.WriteU32(Address(state.sp-128u),caller_sp);
    state.sp-=128u;
    R(state,28)=R(state,3);
    R(state,31)=R(state,4);
    state.cr6={0u,0u,1u,state.xer_so};
    R(state,3)=R(state,31);
    state.lr=0x823acafcu;
    auto lower=ToRaw(state);
    RawHeapBoundary boundary(dependencies.allocation);
    if(!raw_allocation_context::Apply(0x823acbd0u,memory,boundary,lower))
        throw std::logic_error("raw selected context missing");
    FromRaw(state,lower);
    state.sp+=128u;
    Restore27(memory,state);
}

void CallReallocate(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    if (dependencies.reallocate != nullptr)
    {
        dependencies.reallocate->Call(memory, state);
        return;
    }
    if(Address(state.r[3])==0u)
    {
        const auto heap=memory.ReadU32(0x83245708u);
        if(legacy_config_format_heap_context::Supports(memory,heap,0u,
                Address(state.r[4])))
        {
            CallReallocateNull(memory,dependencies,state);
            return;
        }
    }
    auto lower = ToHeap(state);
    if (!crt_reallocate::Apply(0x823acad8u, memory, dependencies.heap,
            dependencies.allocation, lower))
        throw std::logic_error("accepted reallocate lower missing");
    FromHeap(state, lower);
}

void CallFormatter(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    if (!crt_formatter::Apply(0x82b7d158u, memory,
            dependencies.formatter, state))
        throw std::logic_error("accepted formatter lower missing");
}

void CallFree(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    if (!crt_free_context::Apply(0x823addc0u, memory,
            dependencies.free_lower, state))
        throw std::logic_error("free context lower missing");
}

void Format(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    Save27(memory, state);
    state.lr = 0x824790b0u;
    for (unsigned index = 5u; index <= 10u; ++index)
        WriteU64(memory, Address(state.sp + 32u + 8u * (index - 5u)),
            R(state, index));
    memory.WriteU32(Address(state.sp - 144u), Address(state.sp));
    state.sp -= 144u;

    R(state, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(-2094792704));
    R(state, 27) = R(state, 3);
    R(state, 28) = R(state, 4);
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + 25184u));
    CompareSigned(state, R(state, 11), 0);
    if (!state.cr6.eq)
    {
        R(state, 11) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(-2093547520));
        R(state, 12) = std::uint64_t{1} << 44;
        R(state, 11) = memory.ReadU32(Address(R(state, 11) - 28464u));
        R(state, 11) = memory.ReadU32(Address(R(state, 11) + 3040u));
        R(state, 11) = ReadU64(memory, Address(R(state, 11) + 4u));
        R(state, 11) &= R(state, 12);
        CompareUnsigned64(state, R(state, 11));
        if (!state.cr6.eq) goto done;
    }

    R(state, 30) = 1024u;
    R(state, 31) = 0u;
retry:
    R(state, 29) = std::rotl(static_cast<std::uint32_t>(R(state, 30)),
        1) & 0xfffffffeu;
    R(state, 3) = R(state, 31);
    R(state, 4) = R(state, 29);
    state.lr = 0x82479120u;
    CallReallocate(memory, dependencies, state);
    R(state, 11) = state.sp + 80u;
    R(state, 10) = state.sp + 176u;
    R(state, 5) = R(state, 28);
    R(state, 4) = R(state, 30) - 1u;
    R(state, 31) = R(state, 3);
    memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
    R(state, 6) = memory.ReadU32(Address(state.sp + 80u));
    state.lr = 0x82479140u;
    CallFormatter(memory, dependencies, state);
    R(state, 30) = R(state, 29);
    CompareSigned(state, R(state, 3), -1);
    if (state.cr6.eq) goto retry;
    R(state, 11) = std::rotl(static_cast<std::uint32_t>(R(state, 3)),
        1) & 0xfffffffeu;
    R(state, 10) = 0u;
    R(state, 5) = 760u;
    R(state, 4) = R(state, 31);
    R(state, 3) = R(state, 27);
    memory.WriteU16(Address(R(state, 11) + R(state, 31)),
        static_cast<std::uint16_t>(R(state, 10)));
    R(state, 11) = memory.ReadU32(Address(R(state, 27)));
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + 4u));
    state.ctr = R(state, 11);
    state.lr = 0x82479174u;
    dependencies.virtual_calls.Call(Address(state.ctr) & ~GuestAddress{3},
        memory, state);
    R(state, 3) = R(state, 31);
    state.lr = 0x8247917cu;
    CallFree(memory, dependencies, state);

done:
    state.sp += 144u;
    Restore27(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x824790a8u) return false;
    Format(memory, dependencies, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_config_format_dispatch
