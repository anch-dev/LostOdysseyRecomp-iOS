#include "lo_semantics/crt_stream_flush_context.h"

#include "lo_semantics/crt_status_error.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_flush_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Condition=crt_stream_operations::Condition;
using Dependencies=crt_stream_operations::Dependencies;

std::uint64_t& R(Registers& s,unsigned index) {return s.r[index];}
std::uint32_t Word(std::uint64_t value) {return Address(value);}
std::int32_t Signed(std::uint64_t value)
{return std::bit_cast<std::int32_t>(Word(value));}
std::uint64_t ShiftRight5(std::uint64_t value)
{
    const auto word=Word(value);
    const auto shifted=(word>>5)|(word&0x80000000u?0xf8000000u:0u);
    return static_cast<std::uint64_t>(
        static_cast<std::int64_t>(std::bit_cast<std::int32_t>(shifted)));
}

void Compare(Condition& cr,std::uint64_t left,std::uint64_t right,
    std::uint8_t so,bool signed_words)
{
    if(signed_words)
    {
        const auto a=Signed(left),b=Signed(right);
        cr={std::uint8_t(a<b),std::uint8_t(a>b),
            std::uint8_t(a==b),so};
    }
    else
    {
        const auto a=Word(left),b=Word(right);
        cr={std::uint8_t(a<b),std::uint8_t(a>b),
            std::uint8_t(a==b),so};
    }
}

void Accepted(GuestAddress entry,GuestMemory& memory,Dependencies deps,
    Registers& state)
{
    if(!crt_stream_operations::ApplyAcceptedCallee(entry,memory,deps,state))
        throw std::logic_error("missing accepted CRT stream callee");
}

void Save27(GuestMemory& memory,Registers& s)
{
    R(s,12)=s.lr;
    s.lr=0x82b81f80u;
    for(unsigned index=27u;index<=31u;++index)
        WriteU64(memory,Address(s.sp-16u-8u*(31u-index)),R(s,index));
    memory.WriteU32(Address(s.sp-8u),Address(R(s,12)));
}

void Restore27(GuestMemory& memory,Registers& s)
{
    for(unsigned index=27u;index<=31u;++index)
        R(s,index)=ReadU64(memory,
            Address(s.sp-16u-8u*(31u-index)));
    R(s,12)=memory.ReadU32(Address(s.sp-8u));
    s.lr=R(s,12);
}

void Status(GuestMemory& memory,Dependencies deps,Registers& s)
{
    crt_status_error::Registers lower{};
    lower.sp=s.sp;lower.lr=s.lr;lower.r3=R(s,3);
    lower.r11=R(s,11);lower.r12=R(s,12);lower.r13=R(s,13);
    lower.xer_so=s.xer_so;
    lower.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so};
    if(!crt_status_error::Apply(0x827ca628u,memory,deps.io,lower))
        throw std::logic_error("missing accepted status callee");
    s.sp=lower.sp;s.lr=lower.lr;R(s,3)=lower.r3;
    R(s,11)=lower.r11;R(s,12)=lower.r12;R(s,13)=lower.r13;
    s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}

void PointerUnlock(GuestMemory& memory,Dependencies deps,Registers& s)
{
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=s.sp;lower.lr=s.lr;lower.r3=R(s,3);
    lower.r9=R(s,9);lower.r10=R(s,10);lower.r11=R(s,11);
    lower.r12=R(s,12);lower.r30=R(s,30);lower.r31=R(s,31);
    lower.xer_ca=s.xer_ca;
    if(!crt_stream_pointer_unlock::Apply(0x82b863f0u,memory,
        deps.unlock,lower))
        throw std::logic_error("missing accepted pointer unlock");
    s.sp=lower.sp;s.lr=lower.lr;R(s,3)=lower.r3;
    R(s,9)=lower.r9;R(s,10)=lower.r10;R(s,11)=lower.r11;
    R(s,12)=lower.r12;R(s,30)=lower.r30;R(s,31)=lower.r31;
    s.xer_ca=lower.xer_ca;
}

void FlushNative(GuestMemory& memory,Dependencies deps,
    NativeServices& native,Registers& s)
{
    R(s,12)=s.lr;
    memory.WriteU32(Address(s.sp-8u),Address(R(s,12)));
    memory.WriteU32(Address(s.sp-96u),Address(s.sp));
    s.sp-=96u;
    R(s,4)=s.sp+80u;
    s.lr=0x82be48acu;
    native.NtFlushBuffersFile(memory,s);
    Compare(s.cr0,R(s,3),0,s.xer_so,true);
    if(s.cr0.lt)
    {
        s.lr=0x82be48c0u;
        Status(memory,deps,s);
        R(s,3)=0;
    }
    else R(s,3)=1;
    s.sp+=96u;
    R(s,12)=memory.ReadU32(Address(s.sp-8u));
    s.lr=R(s,12);
}

void UnlockWrapper(GuestMemory& memory,Dependencies deps,Registers& s)
{
    WriteU64(memory,Address(s.sp-8u),R(s,31));
    R(s,31)=R(s,12)-144u;
    WriteU64(memory,Address(s.sp-16u),R(s,27));
    R(s,12)=s.lr;
    memory.WriteU32(Address(s.sp-24u),Address(R(s,12)));
    memory.WriteU32(Address(s.sp-112u),Address(s.sp));
    s.sp-=112u;
    R(s,3)=R(s,27);
    s.lr=0x82b820e4u;
    PointerUnlock(memory,deps,s);
    s.sp=memory.ReadU32(Address(s.sp));
    R(s,31)=ReadU64(memory,Address(s.sp-8u));
    R(s,27)=ReadU64(memory,Address(s.sp-16u));
    R(s,12)=memory.ReadU32(Address(s.sp-24u));
    s.lr=R(s,12);
}

void ErrorAddress(GuestAddress return_address,GuestMemory& memory,
    Dependencies deps,Registers& s)
{
    s.lr=return_address;
    Accepted(0x82b7fd78u,memory,deps,s);
}

void InvalidIndex(GuestMemory& memory,Dependencies deps,Registers& s)
{
    ErrorAddress(0x82b81fccu,memory,deps,s);
    R(s,11)=R(s,3);R(s,10)=9;
    for(unsigned index=3u;index<=7u;++index)R(s,index)=0;
    memory.WriteU32(Address(R(s,11)),Address(R(s,10)));
    s.lr=0x82b81ff0u;
    Accepted(0x82b7fec0u,memory,deps,s);
    R(s,3)=UINT64_MAX;
}

void Flush(GuestMemory& memory,Dependencies deps,NativeServices& native,
    Registers& s)
{
    Save27(memory,s);
    R(s,31)=s.sp-144u;
    memory.WriteU32(Address(R(s,31)),Address(s.sp));
    s.sp=R(s,31);
    R(s,27)=R(s,3);
    memory.WriteU32(Address(R(s,31)+164u),Address(R(s,27)));
    Compare(s.cr6,R(s,27),UINT32_MAX-1u,s.xer_so,true);
    if(s.cr6.eq)
    {
        ErrorAddress(0x82b81f9cu,memory,deps,s);
        R(s,11)=R(s,3);R(s,10)=9;R(s,3)=UINT64_MAX;
        memory.WriteU32(Address(R(s,11)),Address(R(s,10)));
        goto done;
    }
    Compare(s.cr6,R(s,27),0,s.xer_so,true);
    if(s.cr6.lt) {InvalidIndex(memory,deps,s);goto done;}
    R(s,11)=0xffffffff83380000ull;
    R(s,11)=memory.ReadU32(0x83378d68u);
    Compare(s.cr6,R(s,27),R(s,11),s.xer_so,false);
    if(!s.cr6.lt) {InvalidIndex(memory,deps,s);goto done;}
    R(s,11)=0xffffffff83380000ull;
    R(s,30)=R(s,11)-29312u;
    s.xer_ca=std::uint8_t(Signed(R(s,27))<0 &&
        (Word(R(s,27))&31u)!=0);
    R(s,11)=ShiftRight5(R(s,27));
    R(s,28)=WordRotateMask(R(s,11),2,0xfffffffcu);
    R(s,29)=WordRotateMask(R(s,27),6,0x7c0u);
    R(s,11)=memory.ReadU32(Address(R(s,28)+R(s,30)));
    R(s,11)+=R(s,29);
    R(s,11)=memory.ReadU8(Address(R(s,11)+4u));
    R(s,11)&=1u;
    Compare(s.cr0,R(s,11),0,s.xer_so,true);
    if(s.cr0.eq) {InvalidIndex(memory,deps,s);goto done;}
    R(s,3)=R(s,27);
    s.lr=0x82b82028u;
    Accepted(0x82b862f8u,memory,deps,s);
    R(s,11)=memory.ReadU32(Address(R(s,28)+R(s,30)));
    R(s,11)+=R(s,29);
    R(s,11)=memory.ReadU8(Address(R(s,11)+4u));
    R(s,11)&=1u;
    Compare(s.cr0,R(s,11),0,s.xer_so,true);
    if(s.cr0.eq) goto mark_invalid;
    R(s,3)=R(s,27);
    s.lr=0x82b82048u;
    Accepted(0x82b86228u,memory,deps,s);
    s.lr=0x82b8204cu;
    FlushNative(memory,deps,native,s);
    Compare(s.cr0,R(s,3),0,s.xer_so,true);
    if(s.cr0.eq)
    {
        s.lr=0x82b82058u;
        Accepted(0x822ca100u,memory,deps,s);
        R(s,30)=R(s,3);
    }
    else R(s,30)=0;
    memory.WriteU32(Address(R(s,31)+80u),Address(R(s,30)));
    Compare(s.cr6,R(s,30),0,s.xer_so,true);
    if(!s.cr6.eq)
    {
        s.lr=0x82b82074u;
        Accepted(0x82b7fdb0u,memory,deps,s);
        memory.WriteU32(Address(R(s,3)),Address(R(s,30)));
    }
    else goto unlock;
mark_invalid:
    ErrorAddress(0x82b8207cu,memory,deps,s);
    R(s,11)=9;
    memory.WriteU32(Address(R(s,3)),Address(R(s,11)));
    R(s,11)=UINT64_MAX;
    memory.WriteU32(Address(R(s,31)+80u),Address(R(s,11)));
unlock:
    R(s,12)=R(s,31)+144u;
    s.lr=0x82b82098u;
    UnlockWrapper(memory,deps,s);
    R(s,3)=memory.ReadU32(Address(R(s,31)+80u));
done:
    s.sp=R(s,31)+144u;
    Restore27(memory,s);
}
} // namespace

bool Apply(GuestAddress address,GuestMemory& memory,
    crt_stream_operations::Dependencies accepted,NativeServices& native,
    Registers& registers)
{
    switch(address)
    {
    case 0x82b81f78u: Flush(memory,accepted,native,registers);return true;
    case 0x82be4898u: FlushNative(memory,accepted,native,registers);
        return true;
    case 0x82b820c4u: UnlockWrapper(memory,accepted,registers);return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_flush_context
