#include "lo_semantics/crt_format_stream.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_format_stream
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t W(std::uint64_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t SW(std::uint64_t value) { return static_cast<std::int32_t>(value); }

void CompareSigned(Registers& state, Condition& cr,
    std::uint64_t left, std::int32_t right)
{
    const auto value = SW(left);
    cr = {std::uint8_t(value < right), std::uint8_t(value > right),
        std::uint8_t(value == right), state.xer_so};
}
void CompareUnsigned(Registers& state, Condition& cr,
    std::uint64_t left, std::uint64_t right)
{
    const auto a = W(left), b = W(right);
    cr = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

void SaveNonvolatiles(GuestMemory& memory, Registers& state,
    unsigned first)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    for (unsigned index = first; index <= 31u; ++index)
        WriteU64(memory, Address(state.sp - 16u - 8u * (31u - index)),
            R(state, index));
}
void RestoreNonvolatiles(GuestMemory& memory, Registers& state,
    unsigned first)
{
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    for (unsigned index = first; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 16u - 8u * (31u - index)));
}
void EnterFrame(GuestMemory& memory, Registers& state, std::uint32_t size)
{
    memory.WriteU32(Address(state.sp - size), Address(state.sp));
    state.sp -= size;
}

crt_stream_locks::Registers ToLock(const Registers& state)
{
    crt_stream_locks::Registers call{};
    call.sp=state.sp;call.lr=state.lr;call.ctr=state.ctr;
    call.r3=state.r[3];call.r4=state.r[4];call.r5=state.r[5];
    call.r6=state.r[6];call.r7=state.r[7];call.r8=state.r[8];
    call.r9=state.r[9];call.r10=state.r[10];call.r11=state.r[11];
    call.r12=state.r[12];call.r13=state.r[13];
    call.r28=state.r[28];call.r29=state.r[29];
    call.r30=state.r[30];call.r31=state.r[31];
    call.cr0={bool(state.cr0.lt),bool(state.cr0.gt),
        bool(state.cr0.eq),bool(state.cr0.so)};
    call.cr6={bool(state.cr6.lt),bool(state.cr6.gt),
        bool(state.cr6.eq),bool(state.cr6.so)};
    call.xer_ca=state.xer_ca;call.xer_so=state.xer_so;
    return call;
}
void FromLock(Registers& state, const crt_stream_locks::Registers& call)
{
    state.sp=call.sp;state.lr=call.lr;state.ctr=call.ctr;
    state.r[3]=call.r3;state.r[4]=call.r4;state.r[5]=call.r5;
    state.r[6]=call.r6;state.r[7]=call.r7;state.r[8]=call.r8;
    state.r[9]=call.r9;state.r[10]=call.r10;state.r[11]=call.r11;
    state.r[12]=call.r12;state.r[13]=call.r13;
    state.r[28]=call.r28;state.r[29]=call.r29;
    state.r[30]=call.r30;state.r[31]=call.r31;
    state.cr0={std::uint8_t(call.cr0.lt),std::uint8_t(call.cr0.gt),
        std::uint8_t(call.cr0.eq),std::uint8_t(call.cr0.so)};
    state.cr6={std::uint8_t(call.cr6.lt),std::uint8_t(call.cr6.gt),
        std::uint8_t(call.cr6.eq),std::uint8_t(call.cr6.so)};
    state.xer_ca=call.xer_ca;state.xer_so=call.xer_so;
}
crt_stream_index_unlock::Registers ToIndex(const Registers& state)
{
    return {state.sp,state.lr,state.r[3],state.r[10],state.r[11],
        state.r[12],state.r[29],state.r[31]};
}
void FromIndex(Registers& state,
    const crt_stream_index_unlock::Registers& call)
{
    state.sp=call.sp;state.lr=call.lr;
    state.r[3]=call.r3;state.r[10]=call.r10;state.r[11]=call.r11;
    state.r[12]=call.r12;state.r[29]=call.r29;state.r[31]=call.r31;
}

void CallLower(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state, GuestAddress return_address)
{
    state.lr=return_address;
    if (crt_wide_stream_output::ApplyAcceptedCallee(address, memory,
            dependencies.formatter.output, state))
        return;
    if (address==0x82b81b28u)
    {
        auto call=ToLock(state);
        if (!crt_stream_locks::Apply(address,memory,
                dependencies.formatter.output.streams.locks,call))
            throw std::logic_error("missing stream indexed lock");
        FromLock(state,call);
        return;
    }
    if (address==0x82b819c8u)
    {
        auto call=ToIndex(state);
        if (!crt_stream_index_unlock::Apply(address,memory,
                dependencies.formatter.output.streams.locks.index_unlock,
                call))
            throw std::logic_error("missing stream indexed unlock");
        FromIndex(state,call);
        return;
    }
    throw std::logic_error("unknown fixed CRT format-stream callee");
}
void CallNativeEnter(GuestMemory& memory, Dependencies dependencies,
    Registers& state, GuestAddress return_address)
{
    state.lr=return_address;
    auto call=ToLock(state);
    dependencies.formatter.output.streams.locks.native
        .EnterCriticalSection(memory,call);
    FromLock(state,call);
}
void TailNativeLeave(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    auto call=ToIndex(state);
    dependencies.formatter.output.streams.locks.index_unlock
        .LeaveCriticalSection(memory,call);
    FromIndex(state,call);
}
void CallFormatter(GuestMemory& memory, Dependencies dependencies,
    Registers& state, GuestAddress return_address)
{
    state.lr=return_address;
    if (!crt_formatter::Apply(0x82319330u,memory,
            dependencies.formatter,state))
        throw std::logic_error("missing recovered main CRT formatter");
}

void UnlockStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void FlushStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void RestoreTemporaryBuffer(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void PrepareTemporaryBuffer(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void LockStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void CleanupStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state);

void LockStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveNonvolatiles(memory,state,31u);
    EnterFrame(memory,state,96u);
    R(state,31)=R(state,4);
    CompareSigned(state,state.cr6,R(state,3),20);
    if (state.cr6.lt)
    {
        R(state,3)+=16u;
        CallLower(0x82b81b28u,memory,dependencies,state,0x82b7b79cu);
        R(state,11)=memory.ReadU32(Address(R(state,31)+12u));
        R(state,11)|=32768u;
        memory.WriteU32(Address(R(state,31)+12u),W(R(state,11)));
    }
    else
    {
        R(state,3)=R(state,31)+32u;
        CallNativeEnter(memory,dependencies,state,0x82b7b7b4u);
    }
    state.sp+=96u;
    RestoreNonvolatiles(memory,state,31u);
}

void UnlockStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    CompareSigned(state,state.cr6,R(state,3),20);
    if (state.cr6.lt)
    {
        R(state,11)=memory.ReadU32(Address(R(state,4)+12u));
        R(state,3)+=16u;
        R(state,11)=WordRotateMask(R(state,11),0,
            0xffffffffffff7fffull);
        memory.WriteU32(Address(R(state,4)+12u),W(R(state,11)));
        // The PPC branch is a tail transfer, retaining the caller's LR/SP.
        auto call=ToIndex(state);
        if (!crt_stream_index_unlock::Apply(0x82b819c8u,memory,
                dependencies.formatter.output.streams.locks.index_unlock,
                call))
            throw std::logic_error("missing indexed unlock tail");
        FromIndex(state,call);
    }
    else
    {
        R(state,3)=R(state,4)+32u;
        TailNativeLeave(memory,dependencies,state);
    }
}

void CleanupStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),Address(R(state,12)));
    EnterFrame(memory,state,96u);
    CallLower(0x822a03c8u,memory,dependencies,state,0x82b7b2d0u);
    R(state,11)=R(state,3);
    R(state,3)=1;
    R(state,4)=R(state,11)+32u;
    state.lr=0x82b7b2e0u;
    UnlockStream(memory,dependencies,state);
    state.sp=memory.ReadU32(Address(state.sp));
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}

void FlushStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveNonvolatiles(memory,state,28u);
    EnterFrame(memory,state,128u);
    R(state,31)=R(state,3);
    R(state,28)=0;
    R(state,11)=memory.ReadU32(Address(R(state,31)+12u));
    R(state,10)=W(R(state,11))&3u;
    CompareSigned(state,state.cr6,R(state,10),2);
    if (state.cr6.eq)
    {
        R(state,11)&=264u;
        CompareSigned(state,state.cr0,R(state,11),0);
        CompareSigned(state,state.cr0,R(state,11),0);
        if (!state.cr0.eq)
        {
            R(state,29)=memory.ReadU32(Address(R(state,31)+8u));
            R(state,11)=memory.ReadU32(Address(R(state,31)));
            R(state,30)=R(state,11)-R(state,29);
            const auto difference=static_cast<std::int64_t>(R(state,30));
            state.cr0={std::uint8_t(difference<0),
                std::uint8_t(difference>0),std::uint8_t(difference==0),
                state.xer_so};
            if (state.cr0.gt)
            {
                R(state,3)=R(state,31);
                CallLower(0x82b81648u,memory,dependencies,state,
                    0x82b7b87cu);
                R(state,4)=R(state,29);
                R(state,5)=R(state,30);
                CallLower(0x82b81de0u,memory,dependencies,state,
                    0x82b7b888u);
                R(state,11)=memory.ReadU32(Address(R(state,31)+12u));
                const auto result=SW(R(state,3));
                const auto length=SW(R(state,30));
                state.cr6={std::uint8_t(result<length),
                    std::uint8_t(result>length),
                    std::uint8_t(result==length),state.xer_so};
                if (!state.cr6.eq)
                {
                    R(state,28)=~std::uint64_t{0};
                    R(state,11)|=32u;
                    memory.WriteU32(Address(R(state,31)+12u),W(R(state,11)));
                }
                else
                {
                    R(state,10)=WordRotateMask(R(state,11),0,0x80u);
                    CompareSigned(state,state.cr0,R(state,10),0);
                    if (!state.cr0.eq)
                    {
                        R(state,11)=WordRotateMask(R(state,11),0,
                            0xfffffffffffffffdull);
                        memory.WriteU32(Address(R(state,31)+12u),
                            W(R(state,11)));
                    }
                }
            }
        }
    }
    R(state,11)=memory.ReadU32(Address(R(state,31)+8u));
    R(state,3)=R(state,28);
    memory.WriteU32(Address(R(state,31)),W(R(state,11)));
    R(state,11)=0;
    memory.WriteU32(Address(R(state,31)+4u),0);
    state.sp+=128u;
    RestoreNonvolatiles(memory,state,28u);
}

void PrepareTemporaryBuffer(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveNonvolatiles(memory,state,29u);
    EnterFrame(memory,state,112u);
    R(state,31)=R(state,3);
    CallLower(0x82b81648u,memory,dependencies,state,0x82b8128cu);
    CallLower(0x829664e8u,memory,dependencies,state,0x82b81290u);
    CompareSigned(state,state.cr0,R(state,3),0);
    bool recognized=false;
    if (!state.cr0.eq)
    {
        CallLower(0x822a03c8u,memory,dependencies,state,0x82b8129cu);
        R(state,11)=R(state,3)+32u;
        CompareUnsigned(state,state.cr6,R(state,31),R(state,11));
        if (state.cr6.eq)
        {
            R(state,9)=0;
            recognized=true;
        }
        else
        {
            CallLower(0x822a03c8u,memory,dependencies,state,
                0x82b812b4u);
            R(state,11)=R(state,3)+64u;
            CompareUnsigned(state,state.cr6,R(state,31),R(state,11));
            if (state.cr6.eq)
            {
                R(state,9)=1;
                recognized=true;
            }
        }
    }
    if (!recognized)
        R(state,3)=0;
    else
    {
        R(state,11)=0xffffffff832d0000ull;
        R(state,10)=memory.ReadU32(0x832d3ab8u);
        R(state,10)+=1u;
        memory.WriteU32(0x832d3ab8u,W(R(state,10)));
        R(state,11)=memory.ReadU32(Address(R(state,31)+12u));
        R(state,11)&=268u;
        CompareSigned(state,state.cr0,R(state,11),0);
        CompareSigned(state,state.cr0,R(state,11),0);
        if (!state.cr0.eq)
            R(state,3)=0;
        else
        {
            R(state,11)=0xffffffff832d0000ull;
            R(state,29)=WordRotateMask(R(state,9),2,0xfffffffcu);
            R(state,30)=R(state,11)+15128u;
            R(state,3)=memory.ReadU32(Address(R(state,30)+R(state,29)));
            CompareUnsigned(state,state.cr0,R(state,3),0);
            if (state.cr0.eq)
            {
                R(state,3)=4096;
                state.lr=0x82b81300u;
                R(state,3)=AllocateRawMemory(memory,
                    dependencies.formatter.output.streams.raw,R(state,3));
                CompareUnsigned(state,state.cr0,R(state,3),0);
                memory.WriteU32(Address(R(state,30)+R(state,29)),
                    W(R(state,3)));
            }
            if (state.cr0.eq)
            {
                R(state,11)=R(state,31)+20u;
                R(state,10)=2;
                memory.WriteU32(Address(R(state,31)+8u),W(R(state,11)));
                memory.WriteU32(Address(R(state,31)+24u),W(R(state,10)));
                memory.WriteU32(Address(R(state,31)+4u),W(R(state,10)));
                memory.WriteU32(Address(R(state,31)),W(R(state,11)));
            }
            else
            {
                R(state,11)=4096;
                memory.WriteU32(Address(R(state,31)+8u),W(R(state,3)));
                memory.WriteU32(Address(R(state,31)),W(R(state,3)));
                memory.WriteU32(Address(R(state,31)+24u),W(R(state,11)));
                memory.WriteU32(Address(R(state,31)+4u),W(R(state,11)));
            }
            R(state,11)=memory.ReadU32(Address(R(state,31)+12u));
            R(state,3)=1;
            R(state,11)|=4354u;
            memory.WriteU32(Address(R(state,31)+12u),W(R(state,11)));
        }
    }
    state.sp+=112u;
    RestoreNonvolatiles(memory,state,29u);
}

void RestoreTemporaryBuffer(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveNonvolatiles(memory,state,31u);
    EnterFrame(memory,state,96u);
    R(state,31)=R(state,4);
    CompareSigned(state,state.cr6,R(state,3),0);
    if (!state.cr6.eq)
    {
        R(state,11)=memory.ReadU32(Address(R(state,31)+12u));
        R(state,11)=WordRotateMask(R(state,11),0,0x1000u);
        CompareSigned(state,state.cr0,R(state,11),0);
        if (!state.cr0.eq)
        {
            R(state,3)=R(state,31);
            state.lr=0x82319ea8u;
            FlushStream(memory,dependencies,state);
            R(state,10)=memory.ReadU32(Address(R(state,31)+12u));
            R(state,11)=0;
            R(state,10)=WordRotateMask(R(state,10),0,
                0xfffffffffffffeffull);
            R(state,10)=WordRotateMask(R(state,10),0,
                0xffffffffffffefffull);
            memory.WriteU32(Address(R(state,31)+24u),0);
            memory.WriteU32(Address(R(state,31)),0);
            memory.WriteU32(Address(R(state,31)+8u),0);
            memory.WriteU32(Address(R(state,31)+12u),W(R(state,10)));
        }
    }
    state.sp+=96u;
    RestoreNonvolatiles(memory,state,31u);
}

void FormatWithStreamLock(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveNonvolatiles(memory,state,29u);
    const auto caller_sp=state.sp;
    for (unsigned index=4u;index<=10u;++index)
        WriteU64(memory,Address(caller_sp+24u+8u*(index-4u)),
            R(state,index));
    R(state,31)=caller_sp-128u;
    EnterFrame(memory,state,128u);
    R(state,29)=R(state,3);
    R(state,11)=std::countl_zero(W(R(state,29)));
    R(state,11)=WordRotateMask(R(state,11),27,1u);
    R(state,11)^=1u;
    CompareSigned(state,state.cr0,R(state,11),0);
    if (state.cr0.eq)
    {
        CallLower(0x82b7fd78u,memory,dependencies,state,0x82b7b218u);
        R(state,11)=R(state,3);
        R(state,10)=22;
        for (unsigned index=3u;index<=7u;++index)
            R(state,index)=0;
        memory.WriteU32(Address(R(state,11)),22u);
        CallLower(0x82b7fec0u,memory,dependencies,state,0x82b7b23cu);
        R(state,3)=~std::uint64_t{0};
    }
    else
    {
        R(state,11)=R(state,31)+80u;
        R(state,10)=R(state,31)+152u;
        memory.WriteU32(Address(R(state,11)),W(R(state,10)));
        CallLower(0x822a03c8u,memory,dependencies,state,0x82b7b254u);
        R(state,11)=R(state,3);
        R(state,3)=1;
        R(state,4)=R(state,11)+32u;
        state.lr=0x82b7b264u;
        LockStream(memory,dependencies,state);
        CallLower(0x822a03c8u,memory,dependencies,state,0x82b7b26cu);
        R(state,3)+=32u;
        state.lr=0x82b7b274u;
        PrepareTemporaryBuffer(memory,dependencies,state);
        R(state,30)=R(state,3);
        CallLower(0x822a03c8u,memory,dependencies,state,0x82b7b27cu);
        R(state,4)=R(state,29);
        R(state,3)+=32u;
        R(state,5)=0;
        R(state,6)=memory.ReadU32(Address(R(state,31)+80u));
        CallFormatter(memory,dependencies,state,0x82b7b290u);
        memory.WriteU32(Address(R(state,31)+84u),W(R(state,3)));
        CallLower(0x822a03c8u,memory,dependencies,state,0x82b7b298u);
        R(state,11)=R(state,3);
        R(state,3)=R(state,30);
        R(state,4)=R(state,11)+32u;
        state.lr=0x82b7b2a8u;
        RestoreTemporaryBuffer(memory,dependencies,state);
        R(state,12)=R(state,31)+128u;
        state.lr=0x82b7b2b4u;
        CleanupStream(memory,dependencies,state);
        R(state,3)=memory.ReadU32(Address(R(state,31)+84u));
    }
    state.sp=R(state,31)+128u;
    RestoreNonvolatiles(memory,state,29u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (entry)
    {
    case 0x82b7b1d0u:
        FormatWithStreamLock(memory,dependencies,registers);return true;
    case 0x82b7b2c0u:
        CleanupStream(memory,dependencies,registers);return true;
    case 0x82b7b778u:
        LockStream(memory,dependencies,registers);return true;
    case 0x82b7b810u:
        UnlockStream(memory,dependencies,registers);return true;
    case 0x82b7b838u:
        FlushStream(memory,dependencies,registers);return true;
    case 0x82b81278u:
        PrepareTemporaryBuffer(memory,dependencies,registers);return true;
    case 0x82319e78u:
        RestoreTemporaryBuffer(memory,dependencies,registers);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::crt_format_stream
