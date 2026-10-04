#include "lo_semantics/crt_stream_shutdown_sweep.h"

#include "lo_semantics/crt_stream_index_unlock.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_shutdown_sweep
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;
std::uint64_t& R(Registers& state, unsigned index)
{return index==1u?state.sp:state.r[index];}
std::uint32_t W(std::uint64_t value) {return Address(value);}
std::int32_t S(std::uint64_t value)
{return std::bit_cast<std::int32_t>(W(value));}
void Compare(Condition& condition,Registers& state,
    std::uint64_t left,std::uint64_t right,bool sign)
{
    if(sign)
    {
        const auto a=S(left),b=S(right);
        condition={std::uint8_t(a<b),std::uint8_t(a>b),
            std::uint8_t(a==b),state.xer_so};
    }
    else
    {
        const auto a=W(left),b=W(right);
        condition={std::uint8_t(a<b),std::uint8_t(a>b),
            std::uint8_t(a==b),state.xer_so};
    }
}
void Save26(GuestMemory& memory,Registers& state)
{
    for(unsigned index=26u;index<=31u;++index)
        WriteU64(memory,Address(state.sp-16u-8u*(31u-index)),R(state,index));
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
}
void Restore26(GuestMemory& memory,Registers& state)
{
    for(unsigned index=26u;index<=31u;++index)
        R(state,index)=ReadU64(memory,
            Address(state.sp-16u-8u*(31u-index)));
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}
crt_stream_locks::Registers ToLock(const Registers& state)
{
    crt_stream_locks::Registers call{};
    call.sp=state.sp;call.lr=state.lr;call.ctr=state.ctr;
    call.r3=state.r[3];call.r4=state.r[4];call.r5=state.r[5];
    call.r6=state.r[6];call.r7=state.r[7];call.r8=state.r[8];
    call.r9=state.r[9];call.r10=state.r[10];call.r11=state.r[11];
    call.r12=state.r[12];call.r13=state.r[13];call.r28=state.r[28];
    call.r29=state.r[29];call.r30=state.r[30];call.r31=state.r[31];
    call.cr0={bool(state.cr0.lt),bool(state.cr0.gt),
        bool(state.cr0.eq),bool(state.cr0.so)};
    call.cr6={bool(state.cr6.lt),bool(state.cr6.gt),
        bool(state.cr6.eq),bool(state.cr6.so)};
    call.xer_ca=state.xer_ca;call.xer_so=state.xer_so;
    return call;
}
void FromLock(Registers& state,const crt_stream_locks::Registers& call)
{
    state.sp=call.sp;state.lr=call.lr;state.ctr=call.ctr;
    state.r[3]=call.r3;state.r[4]=call.r4;state.r[5]=call.r5;
    state.r[6]=call.r6;state.r[7]=call.r7;state.r[8]=call.r8;
    state.r[9]=call.r9;state.r[10]=call.r10;state.r[11]=call.r11;
    state.r[12]=call.r12;state.r[13]=call.r13;state.r[28]=call.r28;
    state.r[29]=call.r29;state.r[30]=call.r30;state.r[31]=call.r31;
    state.cr0={std::uint8_t(call.cr0.lt),std::uint8_t(call.cr0.gt),
        std::uint8_t(call.cr0.eq),std::uint8_t(call.cr0.so)};
    state.cr6={std::uint8_t(call.cr6.lt),std::uint8_t(call.cr6.gt),
        std::uint8_t(call.cr6.eq),std::uint8_t(call.cr6.so)};
    state.xer_ca=call.xer_ca;state.xer_so=call.xer_so;
}
void UnlockIndex(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    crt_stream_index_unlock::Registers call{state.sp,state.lr,R(state,3),
        R(state,10),R(state,11),R(state,12),R(state,29),R(state,31)};
    if(!crt_stream_index_unlock::Apply(0x82b819c8u,memory,
            dependencies.pipeline.close.accepted.locks.index_unlock,call))
        throw std::logic_error("missing accepted indexed unlock");
    state.sp=call.sp;state.lr=call.lr;R(state,3)=call.r3;
    R(state,10)=call.r10;R(state,11)=call.r11;R(state,12)=call.r12;
    R(state,29)=call.r29;R(state,31)=call.r31;
}
void UnlockFunclet(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    // Actual 82B818A8 body, including its own backchain and LR slot.
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
    const auto next=state.sp-96u;
    memory.WriteU32(Address(next),W(state.sp));
    state.sp=next;
    R(state,3)=1u;
    state.lr=0x82b818bcu;
    UnlockIndex(memory,dependencies,state);
    state.sp=memory.ReadU32(Address(state.sp));
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}
void Sweep(GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    // The generated 82B817E8 caller; all addresses below are guest words.
    R(state,12)=state.lr;
    Save26(memory,state);
    state.lr=0x82b817f0u;
    R(state,31)=state.sp-144u;
    const auto next=state.sp-144u;
    memory.WriteU32(Address(next),W(state.sp));
    state.sp=next;
    R(state,3)=1u;
    R(state,26)=0u;
    memory.WriteU32(Address(R(state,31)+80u),W(R(state,26)));
    state.lr=0x82b81808u;
    auto lock=ToLock(state);
    if(!crt_stream_locks::Apply(0x82b81b28u,memory,
            dependencies.pipeline.close.accepted.locks,lock))
        throw std::logic_error("missing accepted global lock");
    FromLock(state,lock);
    R(state,28)=3u;
    R(state,29)=static_cast<std::uint64_t>(-2093481984ll);
    R(state,27)=static_cast<std::uint64_t>(-2093481984ll);
    memory.WriteU32(Address(R(state,31)+84u),W(R(state,28)));
    R(state,11)=memory.ReadU32(Address(R(state,27)-29036u));
    for(;;)
    {
        Compare(state.cr6,state,R(state,28),R(state,11),true);
        if(!state.cr6.lt) break;
        R(state,30)=std::rotl(W(R(state,28))|
            (R(state,28)<<32),2)&0xfffffffcu;
        R(state,11)=memory.ReadU32(Address(R(state,29)-29040u));
        R(state,10)=memory.ReadU32(Address(W(R(state,30))+W(R(state,11))));
        Compare(state.cr6,state,R(state,10),0u,false);
        if(!state.cr6.eq)
        {
            R(state,3)=memory.ReadU32(Address(W(R(state,30))+W(R(state,11))));
            R(state,11)=memory.ReadU32(Address(R(state,3)+12u));
            R(state,11)&=131u;
            Compare(state.cr0,state,R(state,11),0u,true);
            Compare(state.cr0,state,R(state,11),0u,true);
            if(!state.cr0.eq)
            {
                state.lr=0x82b81854u;
                if(!crt_stream_bulk_close_routes::Apply(0x82b85d88u,memory,
                        dependencies,state))
                    throw std::logic_error("missing recovered bulk close");
                Compare(state.cr6,state,R(state,3),UINT64_MAX,true);
                if(!state.cr6.eq)
                {
                    R(state,26)=R(state,26)+1u;
                    memory.WriteU32(Address(R(state,31)+80u),W(R(state,26)));
                }
            }
            Compare(state.cr6,state,R(state,28),20u,true);
            if(!state.cr6.lt)
            {
                R(state,11)=memory.ReadU32(Address(R(state,29)-29040u));
                R(state,3)=memory.ReadU32(Address(W(R(state,30))+W(R(state,11))));
                state.lr=0x82b81878u;
                if(!crt_free_context::Apply(0x823addc0u,memory,
                        dependencies.pipeline.free_lower,state))
                    throw std::logic_error("missing accepted free context");
                R(state,11)=memory.ReadU32(Address(R(state,29)-29040u));
                R(state,10)=0u;
                memory.WriteU32(Address(W(R(state,30))+W(R(state,11))),0u);
            }
        }
        R(state,28)=R(state,28)+1u;
        memory.WriteU32(Address(R(state,31)+84u),W(R(state,28)));
        R(state,11)=memory.ReadU32(Address(R(state,27)-29036u));
    }
    R(state,12)=R(state,31)+144u;
    state.lr=0x82b8189cu;
    UnlockFunclet(memory,dependencies,state);
    R(state,3)=memory.ReadU32(Address(R(state,31)+80u));
    state.sp=R(state,31)+144u;
    Restore26(memory,state);
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& registers)
{
    switch(entry)
    {
    case 0x82b817e8u:Sweep(memory,dependencies,registers);return true;
    case 0x82b818a8u:UnlockFunclet(memory,dependencies,registers);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_shutdown_sweep
