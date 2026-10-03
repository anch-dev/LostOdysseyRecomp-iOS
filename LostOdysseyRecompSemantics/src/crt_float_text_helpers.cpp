#include "lo_semantics/crt_float_text_helpers.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/memory_move.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_float_text_helpers
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void CompareUnsigned(Condition& condition, std::uint64_t left,
    std::uint32_t right, std::uint8_t so)
{
    const auto word = static_cast<std::uint32_t>(left);
    condition = {std::uint8_t(word < right),std::uint8_t(word > right),
        std::uint8_t(word == right),so};
}

void CompareSigned(Condition& condition, std::uint64_t left,
    std::int32_t right, std::uint8_t so)
{
    const auto word = static_cast<std::int32_t>(left);
    condition = {std::uint8_t(word < right),std::uint8_t(word > right),
        std::uint8_t(word == right),so};
}

void SaveFrame(GuestMemory& memory, Registers& state,
    unsigned first, unsigned frame_bytes, std::uint32_t save_return)
{
    R(state, 12) = state.lr;
    state.lr = save_return;
    for (unsigned index=first;index<=31u;++index)
        WriteU64(memory,Address(state.sp-16u-8u*(31u-index)),R(state,index));
    memory.WriteU32(Address(state.sp-8u),Address(R(state,12)));
    memory.WriteU32(Address(state.sp-frame_bytes),Address(state.sp));
    state.sp-=frame_bytes;
}

void RestoreFrame(GuestMemory& memory, Registers& state,
    unsigned first, unsigned frame_bytes)
{
    state.sp+=frame_bytes;
    for (unsigned index=first;index<=31u;++index)
        R(state,index)=ReadU64(memory,
            Address(state.sp-16u-8u*(31u-index)));
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}

void EnterRoundingFrame(GuestMemory& memory,Registers& state)
{
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),Address(R(state,12)));
    WriteU64(memory,Address(state.sp-16u),R(state,31));
    memory.WriteU32(Address(state.sp-96u),Address(state.sp));
    state.sp-=96u;
}

void LeaveRoundingFrame(GuestMemory& memory,Registers& state)
{
    state.sp+=96u;
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
    R(state,31)=ReadU64(memory,Address(state.sp-16u));
}

class ErrorAddressServices final : public AllocationFailureServices
{
public:
    ErrorAddressServices(GuestMemory& memory,CrtThreadDataServices& thread,
        Registers& state):memory_(memory),thread_(thread),state_(state){}
    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall call{R(state_,13)};
        const auto record=GetCrtThreadData(memory_,thread_,call);
        R(state_,13)=call.thread_environment;
        R(state_,3)=record;
        return record;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    {throw std::logic_error("unexpected CRT error output");}
    std::uint64_t BugCheck(std::uint32_t) override
    {throw std::logic_error("unexpected CRT bug check");}
    std::uint64_t CallNewHandler(GuestAddress,std::uint64_t) override
    {throw std::logic_error("unexpected CRT new handler");}
private:
    GuestMemory& memory_;
    CrtThreadDataServices& thread_;
    Registers& state_;
};

void CallErrorAddress(GuestMemory& memory,Dependencies dependencies,
    Registers& state,std::uint32_t return_address)
{
    state.lr=return_address;
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),Address(R(state,12)));
    memory.WriteU32(Address(state.sp-96u),Address(state.sp));
    state.sp-=96u;
    ErrorAddressServices services(memory,dependencies.helpers.thread,state);
    R(state,3)=GetAllocationErrorAddress(services);
    state.sp+=96u;
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}

void CallInvalid(GuestMemory& memory,Dependencies dependencies,
    Registers& state,std::uint32_t return_address)
{
    state.lr=return_address;
    InvalidParameterCall call{{{R(state,3),R(state,4),R(state,5),
        R(state,6),R(state,7),R(state,8),R(state,9),R(state,10)}},R(state,13)};
    R(state,3)=ReportInvalidParameter(memory,dependencies.helpers.invalid,call);
    for (unsigned index=1;index<8u;++index)
        R(state,index+3u)=call.arguments[index];
    R(state,13)=call.thread_environment;
}

void CallMove(GuestMemory& memory,Registers& state,
    std::uint32_t return_address)
{
    state.lr=return_address;
    R(state,3)=MoveGuestMemory(memory,R(state,3),Address(R(state,4)),
        R(state,5),Address(state.sp));
}

void CallFill(GuestMemory& memory,Registers& state,
    std::uint32_t return_address)
{
    state.lr=return_address;
    R(state,3)=FillGuestMemory(memory,Address(R(state,3)),
        Address(R(state,4)),Address(R(state,5)));
}

void CallBoundedCopy(GuestMemory& memory,Dependencies dependencies,
    Registers& state,std::uint32_t return_address)
{
    state.lr=return_address;
    if (!crt_float_core_helpers::Apply(0x8231b0d0u,memory,
            dependencies.helpers,state))
        throw std::logic_error("missing accepted bounded-copy helper");
}

void CallFatal(GuestMemory& memory,Dependencies dependencies,
    Registers& state,std::uint32_t return_address)
{
    state.lr=return_address;
    if (!crt_float_environment::Apply(0x82b7ff08u,memory,
            dependencies.environment,state))
        throw std::logic_error("missing accepted fatal reporter");
}

void ClearInvalidArguments(Registers& state)
{
    for (unsigned index=3;index<=7u;++index) R(state,index)=0;
}

void ReportInvalid(GuestMemory& memory,Dependencies dependencies,
    Registers& state,std::uint32_t code,std::uint32_t getter_return,
    std::uint32_t invalid_return,bool preserve_r31)
{
    CallErrorAddress(memory,dependencies,state,getter_return);
    if (preserve_r31)
    {
        R(state,31)=code;
        memory.WriteU32(Address(R(state,3)),Address(R(state,31)));
    }
    else
    {
        R(state,11)=R(state,3);
        R(state,10)=code;
        ClearInvalidArguments(state);
        memory.WriteU32(Address(R(state,11)),Address(R(state,10)));
    }
    if (preserve_r31) ClearInvalidArguments(state);
    CallInvalid(memory,dependencies,state,invalid_return);
    R(state,3)=preserve_r31?R(state,31):code;
}

std::uint64_t LocaleSeparator(GuestMemory& memory,Registers& state)
{
    R(state,11)=0xffffffff83210000ull;
    R(state,11)=memory.ReadU32(Address(R(state,11)+22088u));
    R(state,11)=memory.ReadU32(Address(R(state,11)));
    return memory.ReadU8(Address(R(state,11)));
}

void RoundDecimalDigits(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    EnterRoundingFrame(memory,state);
    R(state,8)=memory.ReadU32(Address(R(state,6)+12u));
    CompareUnsigned(state.cr6,R(state,3),0,state.xer_so);
    if (state.cr6.eq)
    {
        ReportInvalid(memory,dependencies,state,22u,0x8231b1d8u,
            0x8231b1fcu,false);
        LeaveRoundingFrame(memory,state);
        return;
    }
    CompareUnsigned(state.cr6,R(state,4),0,state.xer_so);
    if (state.cr6.eq)
    {
        ReportInvalid(memory,dependencies,state,22u,0x8231b210u,
            0x8231b230u,true);
        LeaveRoundingFrame(memory,state);
        return;
    }
    R(state,7)=0;
    CompareSigned(state.cr6,R(state,5),0,state.xer_so);
    R(state,11)=R(state,5);
    memory.WriteU8(Address(R(state,3)),0);
    if (!state.cr6.gt) R(state,11)=R(state,7);
    R(state,11)+=1u;
    CompareUnsigned(state.cr6,R(state,4),Address(R(state,11)),state.xer_so);
    if (!state.cr6.gt)
    {
        ReportInvalid(memory,dependencies,state,34u,0x8231b260u,
            0x8231b230u,true);
        LeaveRoundingFrame(memory,state);
        return;
    }

    R(state,9)=48;
    R(state,4)=R(state,3)+1u;
    CompareSigned(state.cr6,R(state,5),0,state.xer_so);
    R(state,11)=R(state,4);
    memory.WriteU8(Address(R(state,3)),48);
    if (state.cr6.gt)
    {
        do
        {
            R(state,10)=memory.ReadU8(Address(R(state,8)));
            CompareUnsigned(state.cr0,R(state,10),0,state.xer_so);
            if (!state.cr0.eq) R(state,8)+=1u;
            else R(state,10)=R(state,9);
            memory.WriteU8(Address(R(state,11)),
                static_cast<std::uint8_t>(R(state,10)));
            state.xer_ca=std::uint8_t(Address(R(state,5))>0u);
            R(state,5)-=1u;
            CompareSigned(state.cr0,R(state,5),0,state.xer_so);
            R(state,11)+=1u;
        } while (state.cr0.gt);
    }
    CompareSigned(state.cr6,R(state,5),0,state.xer_so);
    memory.WriteU8(Address(R(state,11)),0);
    if (!state.cr6.lt)
    {
        R(state,10)=memory.ReadU8(Address(R(state,8)));
        R(state,10)=static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int8_t>(R(state,10))));
        CompareSigned(state.cr6,R(state,10),53,state.xer_so);
        if (!state.cr6.lt)
        {
            for (;;)
            {
                R(state,11)-=1u;
                R(state,10)=memory.ReadU8(Address(R(state,11)));
                CompareUnsigned(state.cr6,R(state,10),57,state.xer_so);
                if (!state.cr6.eq)
                {
                    R(state,10)=Address(R(state,10)) & 0xffu;
                    R(state,10)+=1u;
                    memory.WriteU8(Address(R(state,11)),
                        static_cast<std::uint8_t>(R(state,10)));
                    break;
                }
                memory.WriteU8(Address(R(state,11)),48);
            }
        }
    }
    R(state,11)=memory.ReadU8(Address(R(state,3)));
    CompareUnsigned(state.cr6,R(state,11),49,state.xer_so);
    if (state.cr6.eq)
    {
        R(state,11)=memory.ReadU32(Address(R(state,6)+4u));
        R(state,11)+=1u;
        memory.WriteU32(Address(R(state,6)+4u),Address(R(state,11)));
    }
    else
    {
        R(state,11)=R(state,4);
        R(state,10)=R(state,11);
        do
        {
            R(state,9)=memory.ReadU8(Address(R(state,11)));
            R(state,11)+=1u;
            CompareUnsigned(state.cr6,R(state,9),0,state.xer_so);
        } while (!state.cr6.eq);
        R(state,11)-=R(state,10);
        R(state,11)-=1u;
        R(state,11)=Address(R(state,11));
        R(state,5)=R(state,11)+1u;
        CallMove(memory,state,0x8231b330u);
    }
    R(state,3)=0;
    LeaveRoundingFrame(memory,state);
}

void FormatScientific(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    SaveFrame(memory,state,25u,144u,0x82b7f050u);
    R(state,30)=R(state,3);
    R(state,29)=R(state,4);
    R(state,31)=R(state,5);
    R(state,25)=R(state,6);
    R(state,27)=R(state,7);
    CompareUnsigned(state.cr6,R(state,30),0,state.xer_so);
    if (state.cr6.eq)
    {
        ReportInvalid(memory,dependencies,state,22u,0x82b7f074u,
            0x82b7f098u,false);
        RestoreFrame(memory,state,25u,144u);
        return;
    }
    CompareUnsigned(state.cr6,R(state,29),0,state.xer_so);
    if (state.cr6.eq)
    {
        ReportInvalid(memory,dependencies,state,22u,0x82b7f0acu,
            0x82b7f0ccu,true);
        RestoreFrame(memory,state,25u,144u);
        return;
    }
    CompareSigned(state.cr6,R(state,31),0,state.xer_so);
    R(state,11)=R(state,31);
    if (!state.cr6.gt) R(state,11)=0;
    R(state,11)+=9u;
    CompareUnsigned(state.cr6,R(state,29),Address(R(state,11)),
        state.xer_so);
    if (!state.cr6.gt)
    {
        ReportInvalid(memory,dependencies,state,34u,0x82b7f0f4u,
            0x82b7f0ccu,true);
        RestoreFrame(memory,state,25u,144u);
        return;
    }

    R(state,28)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
        static_cast<std::int8_t>(R(state,8))));
    CompareSigned(state.cr0,R(state,28),0,state.xer_so);
    if (!state.cr0.eq)
    {
        R(state,11)=memory.ReadU32(Address(R(state,27)));
        CompareSigned(state.cr6,R(state,31),0,state.xer_so);
        R(state,11)-=45u;
        R(state,11)=Address(R(state,11))==0u?1u:0u;
        R(state,4)=R(state,11)+R(state,30);
        if (state.cr6.gt)
        {
            R(state,11)=R(state,4);
            R(state,10)=R(state,11);
            do
            {
                R(state,9)=memory.ReadU8(Address(R(state,11)));
                R(state,11)+=1u;
                CompareUnsigned(state.cr6,R(state,9),0,state.xer_so);
            } while (!state.cr6.eq);
            R(state,11)-=R(state,10);
            R(state,3)=R(state,4)+1u;
            R(state,11)-=1u;
            R(state,11)=Address(R(state,11));
            R(state,5)=R(state,11)+1u;
            CallMove(memory,state,0x82b7f150u);
        }
    }

    R(state,10)=memory.ReadU32(Address(R(state,27)));
    R(state,11)=R(state,30);
    R(state,26)=45u;
    CompareSigned(state.cr6,R(state,10),45,state.xer_so);
    if (state.cr6.eq)
    {
        R(state,11)=R(state,30)+1u;
        memory.WriteU8(Address(R(state,30)),45u);
    }
    CompareSigned(state.cr6,R(state,31),0,state.xer_so);
    if (state.cr6.gt)
    {
        R(state,9)=memory.ReadU8(Address(R(state,11)+1u));
        R(state,10)=R(state,11)+1u;
        memory.WriteU8(Address(R(state,11)),
            static_cast<std::uint8_t>(R(state,9)));
        R(state,9)=LocaleSeparator(memory,state);
        R(state,11)=R(state,10);
        memory.WriteU8(Address(R(state,11)),
            static_cast<std::uint8_t>(R(state,9)));
    }
    R(state,10)=Address(R(state,28))==0u?1u:0u;
    CompareSigned(state.cr6,R(state,29),-1,state.xer_so);
    R(state,11)+=R(state,10);
    R(state,31)=R(state,11)+R(state,31);
    if (state.cr6.eq) R(state,4)=~std::uint64_t{0};
    else
    {
        R(state,11)=R(state,30)-R(state,31);
        R(state,4)=R(state,11)+R(state,29);
    }
    R(state,11)=0xffffffff820d0000ull;
    R(state,3)=R(state,31);
    R(state,5)=R(state,11)+12584u;
    CallBoundedCopy(memory,dependencies,state,0x82b7f1d0u);
    CompareSigned(state.cr0,R(state,3),0,state.xer_so);
    if (!state.cr0.eq)
    {
        ClearInvalidArguments(state);
        CallFatal(memory,dependencies,state,0x82b7f1f0u);
    }
    R(state,3)=R(state,31)+2u;
    CompareSigned(state.cr6,R(state,25),0,state.xer_so);
    if (!state.cr6.eq)
    {
        R(state,11)=69u;
        memory.WriteU8(Address(R(state,31)),69u);
    }
    R(state,11)=memory.ReadU32(Address(R(state,27)+12u));
    R(state,10)=R(state,31)+1u;
    R(state,11)=memory.ReadU8(Address(R(state,11)));
    CompareUnsigned(state.cr6,R(state,11),48u,state.xer_so);
    if (!state.cr6.eq)
    {
        R(state,11)=memory.ReadU32(Address(R(state,27)+4u));
        state.xer_ca=std::uint8_t(Address(R(state,11))>0u);
        R(state,11)-=1u;
        CompareSigned(state.cr0,R(state,11),0,state.xer_so);
        if (state.cr0.lt)
        {
            R(state,11)=0u-R(state,11);
            memory.WriteU8(Address(R(state,10)),45u);
        }
        R(state,10)+=1u;
        CompareSigned(state.cr6,R(state,11),100,state.xer_so);
        if (!state.cr6.lt)
        {
            R(state,7)=100u;
            R(state,8)=memory.ReadU8(Address(R(state,10)));
            const auto quotient=static_cast<std::int32_t>(R(state,11))/100;
            R(state,9)=(R(state,9)&0xffffffff00000000ull)|
                static_cast<std::uint32_t>(quotient);
            R(state,9)+=R(state,8);
            R(state,8)=(R(state,8)&0xffffffff00000000ull)|
                static_cast<std::uint32_t>(quotient);
            R(state,8)*=100u;
            memory.WriteU8(Address(R(state,10)),
                static_cast<std::uint8_t>(R(state,9)));
            R(state,11)-=R(state,8);
        }
        R(state,10)+=1u;
        CompareSigned(state.cr6,R(state,11),10,state.xer_so);
        if (!state.cr6.lt)
        {
            R(state,7)=10u;
            R(state,8)=memory.ReadU8(Address(R(state,10)));
            const auto quotient=static_cast<std::int32_t>(R(state,11))/10;
            R(state,9)=(R(state,9)&0xffffffff00000000ull)|
                static_cast<std::uint32_t>(quotient);
            R(state,9)+=R(state,8);
            R(state,8)=(R(state,8)&0xffffffff00000000ull)|
                static_cast<std::uint32_t>(quotient);
            R(state,8)*=10u;
            memory.WriteU8(Address(R(state,10)),
                static_cast<std::uint8_t>(R(state,9)));
            R(state,11)-=R(state,8);
        }
        R(state,9)=memory.ReadU8(Address(R(state,10)+1u));
        R(state,11)=R(state,9)+R(state,11);
        memory.WriteU8(Address(R(state,10)+1u),
            static_cast<std::uint8_t>(R(state,11)));
    }
    R(state,11)=0xffffffff832d0000ull;
    R(state,11)=memory.ReadU32(Address(R(state,11)+15544u));
    R(state,11)=Address(R(state,11))&1u;
    CompareSigned(state.cr0,R(state,11),0,state.xer_so);
    if (!state.cr0.eq)
    {
        R(state,11)=memory.ReadU8(Address(R(state,3)));
        CompareUnsigned(state.cr6,R(state,11),48u,state.xer_so);
        if (state.cr6.eq)
        {
            R(state,5)=3u;
            R(state,4)=R(state,3)+1u;
            CallMove(memory,state,0x82b7f2b8u);
        }
    }
    R(state,3)=0;
    RestoreFrame(memory,state,25u,144u);
}

void FormatFixed(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    SaveFrame(memory,state,26u,144u,0x82b7f830u);
    R(state,29)=R(state,6);
    R(state,27)=R(state,5);
    CompareUnsigned(state.cr6,R(state,3),0,state.xer_so);
    R(state,11)=memory.ReadU32(Address(R(state,29)+4u));
    R(state,10)=R(state,11)-1u;
    if (state.cr6.eq)
    {
        ReportInvalid(memory,dependencies,state,22u,0x82b7f850u,
            0x82b7f874u,false);
        RestoreFrame(memory,state,26u,144u);
        return;
    }
    CompareUnsigned(state.cr6,R(state,4),0,state.xer_so);
    if (state.cr6.eq)
    {
        ReportInvalid(memory,dependencies,state,22u,0x82b7f850u,
            0x82b7f874u,false);
        RestoreFrame(memory,state,26u,144u);
        return;
    }
    R(state,26)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
        static_cast<std::int8_t>(R(state,7))));
    CompareSigned(state.cr0,R(state,26),0,state.xer_so);
    R(state,28)=48u;
    if (!state.cr0.eq)
    {
        CompareSigned(state.cr6,R(state,10),
            static_cast<std::int32_t>(R(state,27)),state.xer_so);
        if (state.cr6.eq)
        {
            R(state,11)=memory.ReadU32(Address(R(state,29)));
            R(state,9)=0;
            R(state,11)-=45u;
            R(state,11)=Address(R(state,11))==0u?1u:0u;
            R(state,11)+=R(state,10);
            R(state,11)+=R(state,3);
            memory.WriteU8(Address(R(state,11)),48u);
            memory.WriteU8(Address(R(state,11)+1u),0);
        }
    }
    R(state,11)=memory.ReadU32(Address(R(state,29)));
    R(state,30)=R(state,3);
    CompareSigned(state.cr6,R(state,11),45,state.xer_so);
    if (state.cr6.eq)
    {
        R(state,30)=R(state,3)+1u;
        memory.WriteU8(Address(R(state,3)),
            static_cast<std::uint8_t>(R(state,11)));
    }
    R(state,11)=memory.ReadU32(Address(R(state,29)+4u));
    CompareSigned(state.cr0,R(state,11),0,state.xer_so);
    if (!state.cr0.gt)
    {
        R(state,11)=R(state,30);
        R(state,10)=R(state,11);
        do
        {
            R(state,9)=memory.ReadU8(Address(R(state,11)));
            R(state,11)+=1u;
            CompareUnsigned(state.cr6,R(state,9),0,state.xer_so);
        } while (!state.cr6.eq);
        R(state,11)-=R(state,10);
        R(state,31)=R(state,30)+1u;
        R(state,11)-=1u;
        R(state,4)=R(state,30);
        R(state,11)=Address(R(state,11));
        R(state,3)=R(state,31);
        R(state,5)=R(state,11)+1u;
        CallMove(memory,state,0x82b7f918u);
        memory.WriteU8(Address(R(state,30)),48u);
    }
    else R(state,31)=R(state,11)+R(state,30);
    CompareSigned(state.cr6,R(state,27),0,state.xer_so);
    if (state.cr6.gt)
    {
        R(state,11)=R(state,31);
        R(state,10)=R(state,11);
        do
        {
            R(state,9)=memory.ReadU8(Address(R(state,11)));
            R(state,11)+=1u;
            CompareUnsigned(state.cr6,R(state,9),0,state.xer_so);
        } while (!state.cr6.eq);
        R(state,11)-=R(state,10);
        R(state,30)=R(state,31)+1u;
        R(state,11)-=1u;
        R(state,4)=R(state,31);
        R(state,11)=Address(R(state,11));
        R(state,3)=R(state,30);
        R(state,5)=R(state,11)+1u;
        CallMove(memory,state,0x82b7f964u);
        R(state,11)=LocaleSeparator(memory,state);
        memory.WriteU8(Address(R(state,31)),
            static_cast<std::uint8_t>(R(state,11)));
        R(state,11)=memory.ReadU32(Address(R(state,29)+4u));
        CompareSigned(state.cr0,R(state,11),0,state.xer_so);
        if (state.cr0.lt)
        {
            CompareSigned(state.cr6,R(state,26),0,state.xer_so);
            if (!state.cr6.eq) R(state,27)=0u-R(state,11);
            else
            {
                R(state,11)=0u-R(state,11);
                CompareSigned(state.cr6,R(state,27),
                    static_cast<std::int32_t>(R(state,11)),state.xer_so);
                if (!state.cr6.lt) R(state,27)=R(state,11);
            }
            CompareSigned(state.cr6,R(state,27),0,state.xer_so);
            if (!state.cr6.eq)
            {
                R(state,11)=R(state,30);
                R(state,10)=R(state,11);
                do
                {
                    R(state,9)=memory.ReadU8(Address(R(state,11)));
                    R(state,11)+=1u;
                    CompareUnsigned(state.cr6,R(state,9),0,state.xer_so);
                } while (!state.cr6.eq);
                R(state,11)-=R(state,10);
                R(state,4)=R(state,30);
                R(state,11)-=1u;
                R(state,3)=R(state,30)+R(state,27);
                R(state,11)=Address(R(state,11));
                R(state,5)=R(state,11)+1u;
                CallMove(memory,state,0x82b7f9e0u);
            }
            R(state,5)=R(state,27);
            R(state,4)=48u;
            R(state,3)=R(state,30);
            CallFill(memory,state,0x82b7f9f0u);
        }
    }
    R(state,3)=0;
    RestoreFrame(memory,state,26u,144u);
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,
    Registers& registers)
{
    switch(entry)
    {
    case 0x8231b1b8u:RoundDecimalDigits(memory,dependencies,registers);return true;
    case 0x82b7f048u:FormatScientific(memory,dependencies,registers);return true;
    case 0x82b7f828u:FormatFixed(memory,dependencies,registers);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::crt_float_text_helpers
