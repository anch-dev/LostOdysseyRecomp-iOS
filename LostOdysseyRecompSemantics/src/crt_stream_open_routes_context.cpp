#include "lo_semantics/crt_stream_open_routes_context.h"

#include "lo_semantics/crt_status_error.h"
#include "lo_semantics/crt_stream_index_unlock.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_open_routes_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Dependencies=crt_stream_operations::Dependencies;
using Condition=crt_stream_operations::Condition;

std::uint64_t& R(Registers& s,unsigned index) {return s.r[index];}
std::uint32_t Word(std::uint64_t value) {return Address(value);}
std::int32_t Signed(std::uint64_t value)
{return std::bit_cast<std::int32_t>(Word(value));}
std::uint64_t SignWord(std::uint32_t value)
{return static_cast<std::uint64_t>(static_cast<std::int64_t>(
    std::bit_cast<std::int32_t>(value)));}
std::uint64_t ShiftRight(std::uint64_t value,unsigned count)
{
    const auto bits=Word(value);
    const auto fill=bits&0x80000000u ? ~(UINT32_MAX>>count) : 0u;
    return SignWord((bits>>count)|fill);
}
std::uint64_t RotateMask(std::uint64_t value,unsigned count,
    std::uint64_t mask)
{
    const auto composed=std::uint64_t{Word(value)}|(value<<32);
    return std::rotl(composed,static_cast<int>(count))&mask;
}
void Compare(Condition& cr,std::uint64_t a,std::uint64_t b,
    std::uint8_t so,bool signed_words)
{
    if(signed_words)
    {
        const auto left=Signed(a),right=Signed(b);
        cr={std::uint8_t(left<right),std::uint8_t(left>right),
            std::uint8_t(left==right),so};
    }
    else
    {
        const auto left=Word(a),right=Word(b);
        cr={std::uint8_t(left<right),std::uint8_t(left>right),
            std::uint8_t(left==right),so};
    }
}
void Push(GuestMemory& memory,Registers& s,unsigned bytes)
{
    memory.WriteU32(Address(s.sp-bytes),Address(s.sp));
    s.sp-=bytes;
}
void SimpleEnter(GuestMemory& memory,Registers& s,unsigned bytes)
{
    R(s,12)=s.lr;
    memory.WriteU32(Address(s.sp-8u),Address(R(s,12)));
    Push(memory,s,bytes);
}
void SimpleExit(GuestMemory& memory,Registers& s,unsigned bytes,
    bool reload_backchain=false)
{
    if(reload_backchain)s.sp=memory.ReadU32(Address(s.sp));
    else s.sp+=bytes;
    R(s,12)=memory.ReadU32(Address(s.sp-8u));
    s.lr=R(s,12);
}
void Save(GuestMemory& memory,Registers& s,unsigned first,
    GuestAddress return_address)
{
    R(s,12)=s.lr;s.lr=return_address;
    for(unsigned index=first;index<=31u;++index)
        WriteU64(memory,Address(s.sp-16u-8u*(31u-index)),R(s,index));
    memory.WriteU32(Address(s.sp-8u),Address(R(s,12)));
}
void Restore(GuestMemory& memory,Registers& s,unsigned first)
{
    for(unsigned index=first;index<=31u;++index)
        R(s,index)=ReadU64(memory,
            Address(s.sp-16u-8u*(31u-index)));
    R(s,12)=memory.ReadU32(Address(s.sp-8u));s.lr=R(s,12);
}
void Accepted(GuestAddress entry,GuestMemory& memory,Dependencies deps,
    Registers& s)
{
    if(!crt_stream_operations::ApplyAcceptedCallee(entry,memory,deps,s))
        throw std::logic_error("missing accepted CRT lower");
}
void LockLower(GuestAddress entry,GuestMemory& memory,Dependencies deps,
    Registers& s)
{
    crt_stream_locks::Registers lower{};
    lower.sp=s.sp;lower.lr=s.lr;lower.ctr=s.ctr;
    lower.r3=R(s,3);lower.r4=R(s,4);lower.r5=R(s,5);
    lower.r6=R(s,6);lower.r7=R(s,7);lower.r8=R(s,8);
    lower.r9=R(s,9);lower.r10=R(s,10);lower.r11=R(s,11);
    lower.r12=R(s,12);lower.r13=R(s,13);
    lower.r28=R(s,28);lower.r29=R(s,29);
    lower.r30=R(s,30);lower.r31=R(s,31);
    lower.xer_so=s.xer_so;lower.xer_ca=s.xer_ca;
    lower.cr0={s.cr0.lt!=0,s.cr0.gt!=0,s.cr0.eq!=0,s.cr0.so!=0};
    lower.cr6={s.cr6.lt!=0,s.cr6.gt!=0,s.cr6.eq!=0,s.cr6.so!=0};
    if(!crt_stream_locks::Apply(entry,memory,deps.locks,lower))
        throw std::logic_error("missing accepted CRT lock lower");
    s.sp=lower.sp;s.lr=lower.lr;s.ctr=lower.ctr;
    R(s,3)=lower.r3;R(s,4)=lower.r4;R(s,5)=lower.r5;
    R(s,6)=lower.r6;R(s,7)=lower.r7;R(s,8)=lower.r8;
    R(s,9)=lower.r9;R(s,10)=lower.r10;R(s,11)=lower.r11;
    R(s,12)=lower.r12;R(s,13)=lower.r13;
    R(s,28)=lower.r28;R(s,29)=lower.r29;
    R(s,30)=lower.r30;R(s,31)=lower.r31;
    s.xer_so=lower.xer_so;s.xer_ca=lower.xer_ca;
    s.cr0={std::uint8_t(lower.cr0.lt),std::uint8_t(lower.cr0.gt),
        std::uint8_t(lower.cr0.eq),std::uint8_t(lower.cr0.so)};
    s.cr6={std::uint8_t(lower.cr6.lt),std::uint8_t(lower.cr6.gt),
        std::uint8_t(lower.cr6.eq),std::uint8_t(lower.cr6.so)};
}
void StateLower(GuestMemory& memory,Dependencies deps,Registers& s)
{
    InvalidParameterCall call{{{R(s,3),R(s,4),R(s,5),R(s,6),
        R(s,7),R(s,8),R(s,9),R(s,10)}},R(s,13)};
    crt_stream_state::FrameRegisters frame{s.lr,R(s,31),s.sp};
    std::uint64_t result=R(s,3);
    if(!crt_stream_state::Apply(0x82b821b0u,memory,deps.thread,
        deps.invalid,deps.raw,deps.state,call,s.sp,frame,result))
        throw std::logic_error("missing accepted CRT state lower");
    s.sp=frame.sp;s.lr=frame.lr;R(s,31)=frame.r31;R(s,3)=result;
    for(unsigned index=1;index<8;++index)
        R(s,index+3u)=call.arguments[index];
    R(s,13)=call.thread_environment;
}
void IndexUnlock(GuestMemory& memory,Dependencies deps,Registers& s)
{
    crt_stream_index_unlock::Registers lower{};
    lower.sp=s.sp;lower.lr=s.lr;lower.r3=R(s,3);
    lower.r10=R(s,10);lower.r11=R(s,11);lower.r12=R(s,12);
    lower.r29=R(s,29);lower.r31=R(s,31);
    if(!crt_stream_index_unlock::Apply(0x82b819c8u,memory,
        deps.locks.index_unlock,lower))
        throw std::logic_error("missing accepted index unlock");
    s.sp=lower.sp;s.lr=lower.lr;R(s,3)=lower.r3;
    R(s,10)=lower.r10;R(s,11)=lower.r11;R(s,12)=lower.r12;
    R(s,29)=lower.r29;R(s,31)=lower.r31;
}
void Status(GuestMemory& memory,Dependencies deps,Registers& s)
{
    crt_status_error::Registers lower{};
    lower.sp=s.sp;lower.lr=s.lr;lower.r3=R(s,3);
    lower.r11=R(s,11);lower.r12=R(s,12);lower.r13=R(s,13);
    lower.xer_so=s.xer_so;
    lower.cr6={s.cr6.lt,s.cr6.gt,s.cr6.eq,s.cr6.so};
    if(!crt_status_error::Apply(0x827ca628u,memory,deps.io,lower))
        throw std::logic_error("missing accepted status lower");
    s.sp=lower.sp;s.lr=lower.lr;R(s,3)=lower.r3;
    R(s,11)=lower.r11;R(s,12)=lower.r12;R(s,13)=lower.r13;
    s.cr6={lower.cr6.lt,lower.cr6.gt,lower.cr6.eq,lower.cr6.so};
}
void StoreError(GuestMemory& memory,GuestServices& services,Registers& s)
{
    if(!crt_thread_error_routes::Apply(0x822ca180u,memory,services,s))
        throw std::logic_error("missing accepted thread-error lower");
}

void SetSlot(GuestMemory& memory,Dependencies deps,Registers& s)
{
    SimpleEnter(memory,s,96u);
    Compare(s.cr6,R(s,3),0,s.xer_so,true);
    bool valid=!s.cr6.lt;
    if(valid)
    {
        R(s,11)=0xffffffff83380000ull;
        R(s,11)=memory.ReadU32(0x83378d68u);
        Compare(s.cr6,R(s,3),R(s,11),s.xer_so,false);
        valid=s.cr6.lt;
    }
    if(valid)
    {
        const auto index=R(s,3);
        s.xer_ca=std::uint8_t(Signed(index)<0 && (Word(index)&31u)!=0);
        R(s,10)=ShiftRight(index,5u);
        R(s,11)=0xffffffff83380000ull;
        R(s,9)=RotateMask(R(s,10),2u,0xfffffffcu);
        R(s,11)-=29312u;
        R(s,10)=RotateMask(index,6u,0x7c0u);
        R(s,11)=memory.ReadU32(Address(R(s,9)+R(s,11)));
        R(s,9)=memory.ReadU32(Address(R(s,11)+R(s,10)));
        Compare(s.cr6,R(s,9),UINT32_MAX,s.xer_so,true);
        valid=s.cr6.eq;
    }
    if(valid)
    {
        R(s,3)=0;
        memory.WriteU32(Address(R(s,11)+R(s,10)),Address(R(s,4)));
    }
    else
    {
        s.lr=0x82b86160u;
        Accepted(0x82b7fd78u,memory,deps,s);
        R(s,11)=9;
        memory.WriteU32(Address(R(s,3)),9u);
        s.lr=0x82b8616cu;
        Accepted(0x82b7fdb0u,memory,deps,s);
        R(s,11)=R(s,3);R(s,10)=0;R(s,3)=UINT64_MAX;
        memory.WriteU32(Address(R(s,11)),0u);
    }
    SimpleExit(memory,s,96u);
}

void UnlockFrame(GuestMemory& memory,Dependencies deps,Registers& s,
    bool restore_loop)
{
    SimpleEnter(memory,s,96u);
    R(s,3)=restore_loop?10u:11u;
    s.lr=restore_loop?0x82b86668u:0x82b86644u;
    IndexUnlock(memory,deps,s);
    if(restore_loop)
    {
        R(s,11)=0xffffffff83380000ull;
        R(s,29)=R(s,11)-29312u;
        R(s,24)=UINT64_MAX;R(s,26)=1u;
        R(s,25)=memory.ReadU32(Address(R(s,31)+84u));
        R(s,28)=memory.ReadU32(Address(R(s,31)+92u));
        R(s,30)=memory.ReadU32(Address(R(s,31)+88u));
    }
    SimpleExit(memory,s,96u,true);
}

void OpenFile(GuestMemory& memory,Dependencies deps,GuestServices& services,
    Registers& s)
{
    Save(memory,s,26u,0x82be2be8u);
    Push(memory,s,192u);
    R(s,26)=R(s,7);R(s,28)=R(s,3);R(s,29)=R(s,4);
    R(s,27)=R(s,5);R(s,31)=R(s,8);
    Compare(s.cr6,R(s,26),1u,s.xer_so,false);
    if(s.cr6.eq)R(s,30)=2u;
    else
    {
        Compare(s.cr6,R(s,26),2u,s.xer_so,false);
        if(s.cr6.eq)R(s,30)=5u;
        else
        {
            Compare(s.cr6,R(s,26),3u,s.xer_so,false);
            if(s.cr6.eq)R(s,30)=1u;
            else
            {
                Compare(s.cr6,R(s,26),4u,s.xer_so,false);
                if(s.cr6.eq)R(s,30)=3u;
                else
                {
                    Compare(s.cr6,R(s,26),5u,s.xer_so,false);
                    if(!s.cr6.eq)goto invalid_mode;
                    R(s,11)=RotateMask(R(s,29),0u,0x40000000u);
                    Compare(s.cr0,R(s,11),0u,s.xer_so,true);
                    R(s,30)=4u;
                    if(s.cr0.eq)goto invalid_mode;
                }
            }
        }
    }
    R(s,4)=R(s,28);R(s,3)=s.sp+104u;
    s.lr=0x82be2c70u;
    services.InitAnsiString(memory,s);
    R(s,11)=memory.ReadU16(Address(s.sp+104u));
    Compare(s.cr6,R(s,11),1u,s.xer_so,false);
    if(s.cr6.gt)
    {
        R(s,11)+=R(s,28);R(s,28)=1u;
        R(s,11)=memory.ReadU8(Address(R(s,11)-1u));
        Compare(s.cr6,R(s,11),92u,s.xer_so,false);
        if(s.cr6.eq)goto slash_done;
    }
    R(s,28)=0u;
slash_done:
    R(s,10)=R(s,31);
    R(s,9)=RotateMask(R(s,31),0u,0x8000000u);
    R(s,10)=(RotateMask(R(s,31),28u,0x8000000u)|
        (R(s,10)&0xfffffffff7ffffffull));
    R(s,7)=UINT64_MAX-2u;
    R(s,10)=RotateMask(R(s,10),31u,0x1c000000u);
    R(s,8)=RotateMask(R(s,31),0u,0x10000000u);
    R(s,10)=RotateMask(R(s,10),0u,0xfffffffff7ffffffull);
    R(s,11)=RotateMask(R(s,31),0u,0x2000000u);
    R(s,10)|=R(s,9);
    memory.WriteU32(Address(s.sp+120u),Address(R(s,7)));
    R(s,7)=64u;
    R(s,10)=RotateMask(R(s,10),24u,0xffffffu);
    R(s,9)=~R(s,31);
    R(s,10)|=R(s,8);
    R(s,9)=RotateMask(R(s,9),7u,0x20u);
    R(s,10)=RotateMask(R(s,10),26u,0x3ffffffu);
    memory.WriteU32(Address(s.sp+128u),Address(R(s,7)));
    R(s,7)=s.sp+104u;
    R(s,10)|=R(s,11);
    R(s,10)=RotateMask(R(s,10),21u,0x1fffffu);
    R(s,8)=RotateMask(R(s,31),0u,0x4000000u);
    Compare(s.cr0,R(s,8),0u,s.xer_so,true);
    memory.WriteU32(Address(s.sp+124u),Address(R(s,7)));
    R(s,10)|=R(s,9);
    if(!s.cr0.eq)
    {R(s,10)|=4096u;R(s,29)|=65536u;}
    Compare(s.cr6,R(s,11),0u,s.xer_so,false);
    if(s.cr6.eq)R(s,10)|=64u;
    R(s,11)=0xffffffff831e0000ull;
    memory.WriteU32(Address(s.sp+84u),Address(R(s,10)));
    R(s,4)=R(s,29)|1048576u;
    R(s,10)=R(s,30);R(s,9)=R(s,27);
    R(s,8)=R(s,31)&32679u;
    Compare(s.cr0,R(s,8),0u,s.xer_so,true);
    R(s,11)=memory.ReadU32(Address(R(s,11)+32244u));
    R(s,7)=0u;R(s,6)=s.sp+112u;R(s,5)=s.sp+120u;
    R(s,4)|=128u;R(s,3)=s.sp+96u;
    R(s,11)=memory.ReadU32(Address(R(s,11)+12u));
    s.ctr=R(s,11);s.lr=0x82be2d44u;
    services.CallOpenFile(Address(s.ctr)&~3u,memory,s);
    R(s,31)=R(s,3);
    Compare(s.cr0,R(s,31),0u,s.xer_so,true);
    if(s.cr0.lt)
    {
        R(s,3)=R(s,31);s.lr=0x82be2d54u;
        Status(memory,deps,s);
        R(s,11)=0xffffffffc0000035ull;
        Compare(s.cr6,R(s,31),R(s,11),s.xer_so,true);
        if(s.cr6.eq)R(s,3)=80u;
        else
        {
            R(s,11)=0xffffffffc00000baull;
            Compare(s.cr6,R(s,31),R(s,11),s.xer_so,true);
            if(!s.cr6.eq)goto fail;
            Compare(s.cr6,R(s,28),0u,s.xer_so,true);
            R(s,3)=s.cr6.eq?5u:3u;
        }
        s.lr=0x82be2d90u;
        StoreError(memory,services,s);
        goto fail;
    }
    R(s,11)=memory.ReadU32(Address(s.sp+116u));
    Compare(s.cr6,R(s,26),2u,s.xer_so,false);
    if(s.cr6.eq)
    {
        Compare(s.cr6,R(s,11),3u,s.xer_so,false);
        if(s.cr6.eq)goto exists;
    }
    Compare(s.cr6,R(s,26),4u,s.xer_so,false);
    if(s.cr6.eq)
    {
        Compare(s.cr6,R(s,11),1u,s.xer_so,false);
        if(s.cr6.eq)goto exists;
    }
    R(s,3)=0u;
    goto store_success;
exists:
    R(s,3)=183u;
store_success:
    s.lr=0x82be2dc8u;
    StoreError(memory,services,s);
    R(s,3)=memory.ReadU32(Address(s.sp+96u));
    goto done;
invalid_mode:
    R(s,3)=0xffffffffc000000dull;
    s.lr=0x82be2c40u;
    Status(memory,deps,s);
fail:
    R(s,3)=UINT64_MAX;
done:
    s.sp+=192u;
    Restore(memory,s,26u);
}

void InitializeStream(GuestMemory& memory,Dependencies deps,
    GuestServices& services,Registers& s)
{
    Save(memory,s,24u,0x82b86428u);
    R(s,31)=s.sp-176u;
    Push(memory,s,176u);
    R(s,3)=11u;R(s,24)=UINT64_MAX;
    memory.WriteU32(Address(R(s,31)+80u),UINT32_MAX);
    R(s,25)=0u;
    memory.WriteU32(Address(R(s,31)+84u),0u);
    s.lr=0x82b86448u;
    LockLower(0x82b819e8u,memory,deps,s);
    Compare(s.cr0,R(s,3),0u,s.xer_so,true);
    if(s.cr0.eq) {R(s,3)=UINT64_MAX;goto done;}
    R(s,3)=11u;s.lr=0x82b86460u;
    LockLower(0x82b81b28u,memory,deps,s);
    R(s,28)=0u;R(s,11)=0xffffffff83380000ull;
    R(s,29)=R(s,11)-29312u;R(s,26)=1u;
outer:
    memory.WriteU32(Address(R(s,31)+92u),Address(R(s,28)));
    Compare(s.cr6,R(s,28),64u,s.xer_so,true);
    if(!s.cr6.lt)goto finish;
    R(s,11)=RotateMask(R(s,28),2u,0xfffffffcu);
    R(s,30)=memory.ReadU32(Address(R(s,11)+R(s,29)));
    Compare(s.cr0,R(s,30),0u,s.xer_so,false);
    if(s.cr0.eq)goto allocate_block;
inner:
    memory.WriteU32(Address(R(s,31)+88u),Address(R(s,30)));
    R(s,11)=RotateMask(R(s,28),2u,0xfffffffcu);
    R(s,11)=memory.ReadU32(Address(R(s,11)+R(s,29)));
    R(s,11)+=2048u;
    Compare(s.cr6,R(s,30),R(s,11),s.xer_so,false);
    if(!s.cr6.lt)goto after_inner;
    R(s,11)=memory.ReadU8(Address(R(s,30)+4u));
    R(s,11)&=1u;
    Compare(s.cr0,R(s,11),0u,s.xer_so,true);
    if(!s.cr0.eq)goto next_record;
    R(s,11)=memory.ReadU32(Address(R(s,30)+8u));
    Compare(s.cr6,R(s,11),0u,s.xer_so,true);
    if(!s.cr6.eq)goto after_initialization;
    R(s,3)=10u;s.lr=0x82b864c8u;
    LockLower(0x82b81b28u,memory,deps,s);
    R(s,11)=memory.ReadU32(Address(R(s,30)+8u));
    Compare(s.cr6,R(s,11),0u,s.xer_so,true);
    if(s.cr6.eq)
    {
        R(s,4)=4000u;R(s,3)=R(s,30)+12u;
        s.lr=0x82b864e4u;
        StateLower(memory,deps,s);
        Compare(s.cr0,R(s,3),0u,s.xer_so,true);
        if(!s.cr0.eq)
        {
            R(s,11)=memory.ReadU32(Address(R(s,30)+8u));
            R(s,11)+=1u;
            memory.WriteU32(Address(R(s,30)+8u),Address(R(s,11)));
        }
        else memory.WriteU32(Address(R(s,31)+84u),Address(R(s,26)));
    }
    R(s,12)=R(s,31)+176u;s.lr=0x82b8650cu;
    UnlockFrame(memory,deps,s,true);
after_initialization:
    Compare(s.cr6,R(s,25),0u,s.xer_so,true);
    if(!s.cr6.eq)goto next_record;
    R(s,27)=R(s,30)+12u;R(s,3)=R(s,27);
    s.lr=0x82b86520u;
    services.EnterCriticalSection(memory,s);
    R(s,11)=memory.ReadU8(Address(R(s,30)+4u));
    R(s,11)&=1u;
    Compare(s.cr0,R(s,11),0u,s.xer_so,true);
    if(!s.cr0.eq)
    {
        R(s,3)=R(s,27);s.lr=0x82b86534u;
        services.LeaveCriticalSection(memory,s);
        goto next_record;
    }
    Compare(s.cr6,R(s,25),0u,s.xer_so,true);
    if(!s.cr6.eq)goto next_record;
    memory.WriteU8(Address(R(s,30)+4u),1u);
    memory.WriteU32(Address(R(s,30)),UINT32_MAX);
    R(s,11)=RotateMask(R(s,28),2u,0xfffffffcu);
    R(s,10)=RotateMask(R(s,28),5u,0xffffffe0u);
    R(s,11)=memory.ReadU32(Address(R(s,11)+R(s,29)));
    R(s,11)=R(s,30)-R(s,11);
    s.xer_ca=std::uint8_t(Signed(R(s,11))<0 &&
        (Word(R(s,11))&63u)!=0);
    R(s,11)=ShiftRight(R(s,11),6u);
    R(s,11)+=R(s,10);
    memory.WriteU32(Address(R(s,31)+80u),Address(R(s,11)));
    goto after_inner;
next_record:
    R(s,30)+=64u;
    goto inner;
after_inner:
    R(s,11)=memory.ReadU32(Address(R(s,31)+80u));
    Compare(s.cr6,R(s,11),UINT32_MAX,s.xer_so,true);
    if(!s.cr6.eq)goto finish;
    R(s,28)+=1u;
    goto outer;
allocate_block:
    R(s,4)=64u;R(s,3)=32u;s.lr=0x82b86594u;
    services.AllocateCrtRecord(memory,s);
    Compare(s.cr0,R(s,3),0u,s.xer_so,false);
    if(s.cr0.eq)goto finish;
    R(s,9)=RotateMask(R(s,28),2u,0xfffffffcu);
    memory.WriteU32(Address(R(s,9)+R(s,29)),Address(R(s,3)));
    R(s,11)=0xffffffff83380000ull;
    R(s,10)=memory.ReadU32(0x83378d68u);
    R(s,10)+=32u;
    memory.WriteU32(0x83378d68u,Address(R(s,10)));
    R(s,10)=0u;
block_loop:
    R(s,11)=memory.ReadU32(Address(R(s,9)+R(s,29)));
    R(s,11)+=2048u;
    Compare(s.cr6,R(s,3),R(s,11),s.xer_so,false);
    if(!s.cr6.lt)goto block_initialized;
    R(s,11)=10u;
    memory.WriteU8(Address(R(s,3)+4u),0u);
    memory.WriteU32(Address(R(s,3)),UINT32_MAX);
    memory.WriteU8(Address(R(s,3)+5u),10u);
    memory.WriteU32(Address(R(s,3)+8u),0u);
    R(s,3)+=64u;
    goto block_loop;
block_initialized:
    R(s,3)=RotateMask(R(s,28),5u,0xffffffe0u);
    memory.WriteU32(Address(R(s,31)+80u),Address(R(s,3)));
    s.xer_ca=std::uint8_t(Signed(R(s,3))<0 && (Word(R(s,3))&31u)!=0);
    R(s,11)=ShiftRight(R(s,3),5u);
    R(s,11)=RotateMask(R(s,11),2u,0xfffffffcu);
    R(s,10)=RotateMask(R(s,3),6u,0x7c0u);
    R(s,11)=memory.ReadU32(Address(R(s,11)+R(s,29)));
    R(s,11)+=R(s,10);
    memory.WriteU8(Address(R(s,11)+4u),1u);
    s.lr=0x82b8660cu;
    Accepted(0x82b862f8u,memory,deps,s);
    Compare(s.cr0,R(s,3),0u,s.xer_so,true);
    if(s.cr0.eq)
        memory.WriteU32(Address(R(s,31)+80u),UINT32_MAX);
finish:
    R(s,12)=R(s,31)+176u;s.lr=0x82b86624u;
    UnlockFrame(memory,deps,s,false);
    R(s,3)=memory.ReadU32(Address(R(s,31)+80u));
done:
    s.sp=R(s,31)+176u;
    Restore(memory,s,24u);
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies accepted,
    GuestServices& services,Registers& state)
{
    switch(entry)
    {
    case 0x82b86108u:SetSlot(memory,accepted,state);return true;
    case 0x82b86630u:UnlockFrame(memory,accepted,state,false);return true;
    case 0x82b86654u:UnlockFrame(memory,accepted,state,true);return true;
    case 0x82be2be0u:OpenFile(memory,accepted,services,state);return true;
    case 0x82b86420u:InitializeStream(memory,accepted,services,state);
        return true;
    default:return false;
    }
}

bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,
    Dependencies accepted,GuestServices& services,Registers& state)
{
    switch(entry)
    {
    case 0x82b819e8u:case 0x82b81b28u:case 0x82b862f8u:
        LockLower(entry,memory,accepted,state);return true;
    case 0x82b821b0u:StateLower(memory,accepted,state);return true;
    case 0x82b819c8u:IndexUnlock(memory,accepted,state);return true;
    case 0x827ca628u:Status(memory,accepted,state);return true;
    case 0x822ca180u:StoreError(memory,services,state);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_open_routes_context
