#include "lo_semantics/crt_stream_lifecycle_recursive.h"

#include "lo_semantics/crt_stream_index_unlock.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_lifecycle_recursive
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition=crt_stream_operations::Condition;
std::uint64_t& R(Registers& state,unsigned index)
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
void Save27(GuestMemory& memory,Registers& state)
{
    for(unsigned index=27u;index<=31u;++index)
        WriteU64(memory,Address(state.sp-16u-8u*(31u-index)),R(state,index));
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
}
void Restore27(GuestMemory& memory,Registers& state)
{
    for(unsigned index=27u;index<=31u;++index)
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
void IndexUnlock(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    crt_stream_index_unlock::Registers call{state.sp,state.lr,R(state,3),
        R(state,10),R(state,11),R(state,12),R(state,29),R(state,31)};
    if(!crt_stream_index_unlock::Apply(0x82b819c8u,memory,
            dependencies.shutdown.pipeline.close.accepted.locks.index_unlock,
            call))
        throw std::logic_error("missing accepted indexed unlock");
    state.sp=call.sp;state.lr=call.lr;R(state,3)=call.r3;
    R(state,10)=call.r10;R(state,11)=call.r11;R(state,12)=call.r12;
    R(state,29)=call.r29;R(state,31)=call.r31;
}
void Format(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& state)
{
    if(!crt_format_stream::Apply(entry,memory,
            dependencies.shutdown.pipeline.format,state))
        throw std::logic_error("missing accepted stream format lower");
}
void Accepted(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& state)
{
    if(!crt_stream_operations::ApplyAcceptedCallee(entry,memory,
            dependencies.shutdown.pipeline.close.accepted,state))
        throw std::logic_error("missing accepted stream lower");
}
void Direct(GuestAddress entry,GuestMemory&,Dependencies,Registers&);
void UnlockGlobal(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    // 82B7BA78: its own 96-byte frame and caller LR slot.
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
    const auto next=state.sp-96u;
    memory.WriteU32(Address(next),W(state.sp));
    state.sp=next;
    R(state,3)=1u;
    state.lr=0x82b7ba8cu;
    IndexUnlock(memory,dependencies,state);
    state.sp=memory.ReadU32(Address(state.sp));
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}
void UnlockRecord(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    // 82B7BAC8: r12 names the B950 caller frame before this nested frame.
    WriteU64(memory,Address(state.sp-8u),R(state,31));
    R(state,31)=R(state,12)-144u;
    WriteU64(memory,Address(state.sp-16u),R(state,30));
    WriteU64(memory,Address(state.sp-24u),R(state,28));
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-32u),W(R(state,12)));
    const auto next=state.sp-112u;
    memory.WriteU32(Address(next),W(state.sp));
    state.sp=next;
    R(state,11)=std::rotl(W(R(state,28))|
        (R(state,28)<<32),2)&0xfffffffcu;
    R(state,3)=R(state,28);
    R(state,10)=memory.ReadU32(Address(R(state,30)));
    R(state,4)=memory.ReadU32(Address(W(R(state,11))+W(R(state,10))));
    state.lr=0x82b7baf8u;
    Format(0x82b7b810u,memory,dependencies,state);
    R(state,11)=static_cast<std::uint64_t>(-2093481984ll);
    R(state,30)=R(state,11)-29040u;
    R(state,11)=static_cast<std::uint64_t>(-2093481984ll);
    R(state,10)=R(state,11)-29036u;
    R(state,27)=memory.ReadU32(Address(R(state,31)+164u));
    R(state,28)=memory.ReadU32(Address(R(state,31)+80u));
    R(state,11)=memory.ReadU32(Address(R(state,30)));
    state.sp=memory.ReadU32(Address(state.sp));
    R(state,31)=ReadU64(memory,Address(state.sp-8u));
    R(state,30)=ReadU64(memory,Address(state.sp-16u));
    R(state,28)=ReadU64(memory,Address(state.sp-24u));
    R(state,12)=memory.ReadU32(Address(state.sp-32u));
    state.lr=R(state,12);
}
void CloseOne(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    // 82B7B8D0: zero r3 recursively enters 82B7B950 in mode zero.
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
    WriteU64(memory,Address(state.sp-16u),R(state,31));
    const auto next=state.sp-96u;
    memory.WriteU32(Address(next),W(state.sp));
    state.sp=next;
    R(state,31)=R(state,3);
    Compare(state.cr6,state,R(state,31),0u,false);
    if(state.cr6.eq)
    {
        state.lr=0x82b7b8f0u;
        Direct(0x82b7b950u,memory,dependencies,state);
    }
    else
    {
        R(state,3)=R(state,31);
        state.lr=0x82b7b8fcu;
        Format(0x82b7b838u,memory,dependencies,state);
        Compare(state.cr0,state,R(state,3),0u,true);
        if(!state.cr0.eq) R(state,3)=UINT64_MAX;
        else
        {
            R(state,11)=memory.ReadU32(Address(R(state,31)+12u));
            R(state,11)=std::rotl(W(R(state,11))|
                (R(state,11)<<32),0)&0x4000u;
            Compare(state.cr0,state,R(state,11),0u,true);
            if(state.cr0.eq) R(state,3)=0u;
            else
            {
                R(state,3)=R(state,31);
                state.lr=0x82b7b920u;
                Accepted(0x82b81648u,memory,dependencies,state);
                state.lr=0x82b7b924u;
                if(!crt_stream_flush_context::Apply(0x82b81f78u,memory,
                        dependencies.shutdown.pipeline.close.accepted,
                        dependencies.flush,state))
                    throw std::logic_error("missing accepted flush context");
                state.xer_ca=(W(R(state,3))==0u);
                R(state,11)=0u-R(state,3);
                const auto word=W(R(state,11));
                const std::uint32_t inverted=~word;
                const std::uint32_t first=inverted+word;
                const auto carry=state.xer_ca;
                const auto full=~R(state,11)+R(state,11)+carry;
                state.xer_ca=std::uint8_t(first<inverted||
                    std::uint32_t(first+carry)<carry);
                R(state,3)=full;
            }
        }
    }
    state.sp+=96u;
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
    R(state,31)=ReadU64(memory,Address(state.sp-16u));
}
void Enumerate(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    // 82B7B950: mode one counts completed records; mode zero records failure.
    R(state,12)=state.lr;
    state.lr=0x82b7b958u;
    Save27(memory,state);
    R(state,31)=state.sp-144u;
    const auto next=state.sp-144u;
    memory.WriteU32(Address(next),W(state.sp));
    state.sp=next;
    R(state,27)=R(state,3);
    memory.WriteU32(Address(R(state,31)+164u),W(R(state,27)));
    R(state,3)=1u;
    R(state,28)=0u;
    memory.WriteU32(Address(R(state,31)+84u),0u);
    memory.WriteU32(Address(R(state,31)+88u),0u);
    state.lr=0x82b7b97cu;
    auto lock=ToLock(state);
    if(!crt_stream_locks::Apply(0x82b81b28u,memory,
            dependencies.shutdown.pipeline.close.accepted.locks,lock))
        throw std::logic_error("missing accepted global lock");
    FromLock(state,lock);
    memory.WriteU32(Address(R(state,31)+80u),W(R(state,28)));
    R(state,11)=static_cast<std::uint64_t>(-2093481984ll);
    R(state,30)=R(state,11)-29040u;
    R(state,11)=static_cast<std::uint64_t>(-2093481984ll);
    R(state,10)=R(state,11)-29036u;
    R(state,11)=memory.ReadU32(Address(R(state,30)));
    for(;;)
    {
        R(state,9)=memory.ReadU32(Address(R(state,10)));
        Compare(state.cr6,state,R(state,28),R(state,9),true);
        if(!state.cr6.lt) break;
        R(state,29)=std::rotl(W(R(state,28))|
            (R(state,28)<<32),2)&0xfffffffcu;
        R(state,9)=memory.ReadU32(Address(W(R(state,29))+W(R(state,11))));
        Compare(state.cr6,state,R(state,9),0u,false);
        if(!state.cr6.eq)
        {
            R(state,4)=W(R(state,9));
            R(state,9)=memory.ReadU32(Address(R(state,4)+12u));
            R(state,9)&=131u;
            Compare(state.cr0,state,R(state,9),0u,true);
            Compare(state.cr0,state,R(state,9),0u,true);
            if(!state.cr0.eq)
            {
                R(state,3)=R(state,28);
                state.lr=0x82b7b9d0u;
                Format(0x82b7b778u,memory,dependencies,state);
                R(state,11)=memory.ReadU32(Address(R(state,30)));
                R(state,3)=memory.ReadU32(Address(W(R(state,29))+W(R(state,11))));
                R(state,11)=memory.ReadU32(Address(R(state,3)+12u));
                R(state,10)=R(state,11)&131u;
                Compare(state.cr0,state,R(state,10),0u,true);
                Compare(state.cr0,state,R(state,10),0u,true);
                if(!state.cr0.eq)
                {
                    Compare(state.cr6,state,R(state,27),1u,true);
                    if(state.cr6.eq)
                    {
                        state.lr=0x82b7b9f8u;
                        Direct(0x82b7b8d0u,memory,dependencies,state);
                        Compare(state.cr6,state,R(state,3),UINT64_MAX,true);
                        if(!state.cr6.eq)
                        {
                            R(state,11)=memory.ReadU32(Address(R(state,31)+84u));
                            R(state,11)+=1u;
                            memory.WriteU32(Address(R(state,31)+84u),W(R(state,11)));
                        }
                    }
                    else
                    {
                        Compare(state.cr6,state,R(state,27),0u,true);
                        if(state.cr6.eq)
                        {
                            R(state,11)=std::rotl(W(R(state,11))|
                                (R(state,11)<<32),0)&0x2u;
                            Compare(state.cr0,state,R(state,11),0u,true);
                            if(!state.cr0.eq)
                            {
                                state.lr=0x82b7ba24u;
                                Direct(0x82b7b8d0u,memory,dependencies,state);
                                Compare(state.cr6,state,R(state,3),UINT64_MAX,true);
                                if(state.cr6.eq)
                                {
                                    R(state,11)=UINT64_MAX;
                                    memory.WriteU32(Address(R(state,31)+88u),
                                        0xffffffffu);
                                }
                            }
                        }
                    }
                }
                R(state,12)=R(state,31)+144u;
                state.lr=0x82b7ba40u;
                Direct(0x82b7bac8u,memory,dependencies,state);
            }
        }
        R(state,28)+=1u;
        memory.WriteU32(Address(R(state,31)+80u),W(R(state,28)));
    }
    R(state,12)=R(state,31)+144u;
    state.lr=0x82b7ba5cu;
    Direct(0x82b7ba78u,memory,dependencies,state);
    R(state,11)=memory.ReadU32(Address(R(state,31)+164u));
    Compare(state.cr6,state,R(state,11),1u,true);
    R(state,3)=memory.ReadU32(Address(R(state,31)+84u));
    if(!state.cr6.eq)
        R(state,3)=memory.ReadU32(Address(R(state,31)+88u));
    state.sp=R(state,31)+144u;
    Restore27(memory,state);
}
void Shutdown(GuestMemory& memory,Dependencies dependencies,
    Registers& state)
{
    // 82B7B6C8: tail-entering BC30/B950 precedes optional shutdown sweep.
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
    const auto next=state.sp-96u;
    memory.WriteU32(Address(next),W(state.sp));
    state.sp=next;
    state.lr=0x82b7b6d8u;
    Direct(0x82b7bc30u,memory,dependencies,state);
    R(state,11)=static_cast<std::uint64_t>(-2094202880ll);
    R(state,11)=memory.ReadU8(Address(R(state,11)+15040u));
    Compare(state.cr0,state,R(state,11),0u,false);
    if(!state.cr0.eq)
    {
        state.lr=0x82b7b6ecu;
        if(!crt_stream_shutdown_sweep::Apply(0x82b817e8u,memory,
                dependencies.shutdown,state))
            throw std::logic_error("missing accepted shutdown sweep");
    }
    R(state,11)=static_cast<std::uint64_t>(-2093481984ll);
    R(state,3)=memory.ReadU32(Address(R(state,11)-29040u));
    state.lr=0x82b7b6f8u;
    if(!crt_free_context::Apply(0x823addc0u,memory,
            dependencies.shutdown.pipeline.free_lower,state))
        throw std::logic_error("missing accepted free context");
    state.sp+=96u;
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}
void Direct(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& state)
{
    switch(entry)
    {
    case 0x82b7b6c8u:Shutdown(memory,dependencies,state);return;
    case 0x82b7bc30u:
        R(state,3)=1u;
        Enumerate(memory,dependencies,state);return; // actual tail branch
    case 0x82b7b950u:Enumerate(memory,dependencies,state);return;
    case 0x82b7b8d0u:CloseOne(memory,dependencies,state);return;
    case 0x82b7ba78u:UnlockGlobal(memory,dependencies,state);return;
    case 0x82b7bac8u:UnlockRecord(memory,dependencies,state);return;
    default:throw std::logic_error("unselected lifecycle direct entry");
    }
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& registers)
{
    switch(entry)
    {
    case 0x82b7b6c8u:case 0x82b7bc30u:case 0x82b7b950u:
    case 0x82b7b8d0u:case 0x82b7ba78u:case 0x82b7bac8u:
        Direct(entry,memory,dependencies,registers);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_lifecycle_recursive
