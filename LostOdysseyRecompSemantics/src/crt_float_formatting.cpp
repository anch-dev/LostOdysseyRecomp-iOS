#include "lo_semantics/crt_float_formatting.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/crt_float_text_helpers.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_float_formatting
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
using Condition=crt_stream_operations::Condition;

std::uint64_t& R(Registers& state,unsigned index) { return state.r[index]; }
std::uint32_t W(std::uint64_t value) { return Address(value); }
std::int32_t S(std::uint64_t value)
{return static_cast<std::int32_t>(value);}
std::int64_t Signed(std::uint64_t value)
{return static_cast<std::int64_t>(value);}

void CompareU(Condition& cr,std::uint64_t value,std::uint32_t other,
    std::uint8_t so)
{
    const auto word=W(value);
    cr={std::uint8_t(word<other),std::uint8_t(word>other),
        std::uint8_t(word==other),so};
}
void CompareS(Condition& cr,std::uint64_t value,std::int32_t other,
    std::uint8_t so)
{
    const auto word=S(value);
    cr={std::uint8_t(word<other),std::uint8_t(word>other),
        std::uint8_t(word==other),so};
}
void CompareSWords(Condition& cr,std::uint64_t left,
    std::uint64_t right,std::uint8_t so)
{CompareS(cr,left,S(right),so);}
void CompareU64(Condition& cr,std::uint64_t left,
    std::uint64_t right,std::uint8_t so)
{
    cr={std::uint8_t(left<right),std::uint8_t(left>right),
        std::uint8_t(left==right),so};
}
void CompareS64(Condition& cr,std::uint64_t left,
    std::int64_t right,std::uint8_t so)
{
    const auto value=Signed(left);
    cr={std::uint8_t(value<right),std::uint8_t(value>right),
        std::uint8_t(value==right),so};
}

void SaveFrame(GuestMemory& memory,Registers& state,unsigned first,
    unsigned size,GuestAddress return_address)
{
    R(state,12)=state.lr;
    state.lr=return_address;
    for(unsigned index=first;index<=31u;++index)
        WriteU64(memory,Address(state.sp-16u-8u*(31u-index)),R(state,index));
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
    memory.WriteU32(Address(state.sp-size),Address(state.sp));
    state.sp-=size;
}
void RestoreFrame(GuestMemory& memory,Registers& state,unsigned first,
    unsigned size)
{
    state.sp+=size;
    for(unsigned index=first;index<=31u;++index)
        R(state,index)=ReadU64(memory,
            Address(state.sp-16u-8u*(31u-index)));
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}

class ErrorAddressServices final:public AllocationFailureServices
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
    Registers& state,GuestAddress return_address)
{
    state.lr=return_address;
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),W(R(state,12)));
    memory.WriteU32(Address(state.sp-96u),Address(state.sp));
    state.sp-=96u;
    ErrorAddressServices service(memory,dependencies.helpers.thread,state);
    R(state,3)=GetAllocationErrorAddress(service);
    state.sp+=96u;
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}
void CallInvalid(GuestMemory& memory,Dependencies dependencies,
    Registers& state,GuestAddress return_address)
{
    state.lr=return_address;
    InvalidParameterCall call{{{R(state,3),R(state,4),R(state,5),
        R(state,6),R(state,7),R(state,8),R(state,9),R(state,10)}},
        R(state,13)};
    R(state,3)=ReportInvalidParameter(memory,dependencies.helpers.invalid,call);
    for(unsigned index=1;index<8u;++index)
        R(state,index+3u)=call.arguments[index];
    R(state,13)=call.thread_environment;
}
void ClearInvalidArguments(Registers& state)
{for(unsigned index=3;index<=7u;++index) R(state,index)=0;}
void InvalidArgument(GuestMemory& memory,Dependencies dependencies,
    Registers& state,GuestAddress getter_return,GuestAddress invalid_return,
    bool preserve_r31,std::uint32_t code)
{
    CallErrorAddress(memory,dependencies,state,getter_return);
    if(preserve_r31)
    {
        R(state,31)=code;
        memory.WriteU32(Address(R(state,3)),code);
        ClearInvalidArguments(state);
    }
    else
    {
        R(state,11)=R(state,3);
        R(state,10)=code;
        ClearInvalidArguments(state);
        memory.WriteU32(Address(R(state,11)),code);
    }
    CallInvalid(memory,dependencies,state,invalid_return);
    R(state,3)=code;
}

void CallConvert(GuestMemory& memory,Dependencies dependencies,
    Registers& state,GuestAddress return_address)
{
    state.lr=return_address;
    if(!crt_float_conversion::Apply(0x8231a2f0u,memory,
            dependencies,state))
        throw std::logic_error("missing accepted binary64 conversion");
}
void CallText(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& state,GuestAddress return_address)
{
    state.lr=return_address;
    if(!crt_float_text_helpers::Apply(entry,memory,
            {dependencies.helpers,dependencies.environment},state))
        throw std::logic_error("missing accepted float text helper");
}
void CallSearch(GuestMemory& memory,Dependencies dependencies,
    Registers& state,GuestAddress return_address)
{
    state.lr=return_address;
    if(!crt_float_environment::Apply(0x82b7e580u,memory,
            dependencies.environment,state))
        throw std::logic_error("missing accepted last-byte search");
}

void Scientific(GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    SaveFrame(memory,state,27u,176u,0x82b7f2d0u);
    R(state,31)=R(state,4);
    R(state,3)=ReadU64(memory,Address(R(state,3)));
    R(state,29)=R(state,5);
    R(state,30)=R(state,6);
    R(state,6)=22u;
    R(state,5)=state.sp+96u;
    R(state,4)=state.sp+80u;
    R(state,28)=R(state,7);
    R(state,27)=R(state,8);
    CallConvert(memory,dependencies,state,0x82b7f2fcu);
    CompareU(state.cr6,R(state,31),0,state.xer_so);
    if(state.cr6.eq)
    {
        InvalidArgument(memory,dependencies,state,0x82b7f308u,
            0x82b7f32cu,false,22u);
        RestoreFrame(memory,state,27u,176u);
        return;
    }
    CompareU(state.cr6,R(state,29),0,state.xer_so);
    if(state.cr6.eq)
    {
        InvalidArgument(memory,dependencies,state,0x82b7f308u,
            0x82b7f32cu,false,22u);
        RestoreFrame(memory,state,27u,176u);
        return;
    }
    R(state,9)=memory.ReadU32(Address(state.sp+80u));
    CompareS(state.cr6,R(state,29),-1,state.xer_so);
    if(state.cr6.eq) R(state,4)=~std::uint64_t{0};
    else
    {
        CompareS(state.cr6,R(state,30),0,state.xer_so);
        R(state,11)=state.cr6.gt?1u:0u;
        R(state,10)=R(state,9)-45u;
        R(state,10)=W(R(state,10))==0u?1u:0u;
        R(state,10)=R(state,29)-R(state,10);
        R(state,4)=R(state,10)-R(state,11);
    }
    CompareS(state.cr6,R(state,30),0,state.xer_so);
    R(state,10)=state.cr6.gt?1u:0u;
    R(state,11)=R(state,9)-45u;
    R(state,6)=state.sp+80u;
    R(state,11)=W(R(state,11))==0u?1u:0u;
    R(state,5)=R(state,30)+1u;
    R(state,11)+=R(state,10);
    R(state,3)=R(state,11)+R(state,31);
    CallText(0x8231b1b8u,memory,dependencies,state,0x82b7f3a4u);
    CompareS(state.cr0,R(state,3),0,state.xer_so);
    if(!state.cr0.eq)
    {
        R(state,11)=0;
        memory.WriteU8(Address(R(state,31)),0);
        RestoreFrame(memory,state,27u,176u);
        return;
    }
    R(state,9)=R(state,27);
    R(state,8)=0;
    R(state,7)=state.sp+80u;
    R(state,6)=R(state,28);
    R(state,5)=R(state,30);
    R(state,4)=R(state,29);
    R(state,3)=R(state,31);
    CallText(0x82b7f048u,memory,dependencies,state,0x82b7f3d8u);
    RestoreFrame(memory,state,27u,176u);
}

void Fixed(GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    SaveFrame(memory,state,28u,160u,0x82b7fa08u);
    R(state,31)=R(state,4);
    R(state,3)=ReadU64(memory,Address(R(state,3)));
    R(state,29)=R(state,5);
    R(state,30)=R(state,6);
    R(state,6)=22u;
    R(state,5)=state.sp+96u;
    R(state,4)=state.sp+80u;
    R(state,28)=R(state,7);
    CallConvert(memory,dependencies,state,0x82b7fa30u);
    CompareU(state.cr6,R(state,31),0,state.xer_so);
    if(!state.cr6.eq)
        CompareU(state.cr6,R(state,29),0,state.xer_so);
    if(state.cr6.eq)
    {
        InvalidArgument(memory,dependencies,state,0x82b7fa3cu,
            0x82b7fa60u,false,22u);
        RestoreFrame(memory,state,28u,160u);
        return;
    }
    R(state,11)=memory.ReadU32(Address(state.sp+80u));
    CompareS(state.cr6,R(state,29),-1,state.xer_so);
    if(state.cr6.eq) R(state,4)=~std::uint64_t{0};
    else
    {
        R(state,10)=R(state,11)-45u;
        R(state,10)=W(R(state,10))==0u?1u:0u;
        R(state,4)=R(state,29)-R(state,10);
    }
    R(state,11)-=45u;
    R(state,6)=state.sp+80u;
    R(state,10)=W(R(state,11))==0u?1u:0u;
    R(state,11)=memory.ReadU32(Address(state.sp+84u));
    R(state,5)=R(state,11)+R(state,30);
    R(state,11)=R(state,10);
    R(state,3)=R(state,11)+R(state,31);
    CallText(0x8231b1b8u,memory,dependencies,state,0x82b7fab4u);
    CompareS(state.cr0,R(state,3),0,state.xer_so);
    if(!state.cr0.eq)
    {
        R(state,11)=0;
        memory.WriteU8(Address(R(state,31)),0);
        RestoreFrame(memory,state,28u,160u);
        return;
    }
    R(state,8)=R(state,28);
    R(state,7)=0;
    R(state,6)=state.sp+80u;
    R(state,5)=R(state,30);
    R(state,4)=R(state,29);
    R(state,3)=R(state,31);
    CallText(0x82b7f828u,memory,dependencies,state,0x82b7fae4u);
    RestoreFrame(memory,state,28u,160u);
}

void General(GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    SaveFrame(memory,state,25u,192u,0x82b7faf8u);
    R(state,31)=R(state,4);
    R(state,3)=ReadU64(memory,Address(R(state,3)));
    R(state,30)=R(state,5);
    R(state,27)=R(state,6);
    R(state,6)=22u;
    R(state,5)=state.sp+96u;
    R(state,4)=state.sp+80u;
    R(state,26)=R(state,7);
    R(state,25)=R(state,8);
    CallConvert(memory,dependencies,state,0x82b7fb24u);
    CompareU(state.cr6,R(state,31),0,state.xer_so);
    if(!state.cr6.eq)
        CompareU(state.cr6,R(state,30),0,state.xer_so);
    if(state.cr6.eq)
    {
        InvalidArgument(memory,dependencies,state,0x82b7fb30u,
            0x82b7fb54u,false,22u);
        RestoreFrame(memory,state,25u,192u);
        return;
    }
    R(state,11)=memory.ReadU32(Address(state.sp+80u));
    CompareS(state.cr6,R(state,30),-1,state.xer_so);
    R(state,10)=memory.ReadU32(Address(state.sp+84u));
    R(state,4)=~std::uint64_t{0};
    R(state,11)-=45u;
    R(state,29)=R(state,10)-1u;
    R(state,11)=W(R(state,11))==0u?1u:0u;
    R(state,28)=R(state,11)+R(state,31);
    if(!state.cr6.eq) R(state,4)=R(state,30)-R(state,11);
    R(state,6)=state.sp+80u;
    R(state,5)=R(state,27);
    R(state,3)=R(state,28);
    CallText(0x8231b1b8u,memory,dependencies,state,0x82b7fba0u);
    CompareS(state.cr0,R(state,3),0,state.xer_so);
    if(!state.cr0.eq)
    {
        R(state,11)=0;
        memory.WriteU8(Address(R(state,31)),0);
        RestoreFrame(memory,state,25u,192u);
        return;
    }
    R(state,11)=memory.ReadU32(Address(state.sp+84u));
    R(state,10)=1;
    R(state,11)-=1u;
    CompareSWords(state.cr6,R(state,29),R(state,11),state.xer_so);
    if(!state.cr6.lt) R(state,10)=0;
    CompareS(state.cr6,R(state,11),-4,state.xer_so);
    bool fixed_path=!state.cr6.lt;
    if(fixed_path)
    {
        CompareSWords(state.cr6,R(state,11),R(state,27),state.xer_so);
        fixed_path=state.cr6.lt;
    }
    if(fixed_path)
    {
        R(state,11)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
            static_cast<std::int8_t>(R(state,10))));
        CompareS(state.cr0,R(state,11),0,state.xer_so);
        if(!state.cr0.eq)
        {
            do
            {
                R(state,11)=memory.ReadU8(Address(R(state,28)));
                R(state,28)+=1u;
                CompareU(state.cr6,R(state,11),0,state.xer_so);
            } while(!state.cr6.eq);
            memory.WriteU8(Address(R(state,28)-2u),
                static_cast<std::uint8_t>(R(state,11)));
        }
        R(state,8)=R(state,25);
        R(state,7)=1;
        R(state,6)=state.sp+80u;
        R(state,5)=R(state,27);
        R(state,4)=R(state,30);
        R(state,3)=R(state,31);
        CallText(0x82b7f828u,memory,dependencies,state,0x82b7fc14u);
    }
    else
    {
        R(state,9)=R(state,25);
        R(state,8)=1;
        R(state,7)=state.sp+80u;
        R(state,6)=R(state,26);
        R(state,5)=R(state,27);
        R(state,4)=R(state,30);
        R(state,3)=R(state,31);
        CallText(0x82b7f048u,memory,dependencies,state,0x82b7fc38u);
    }
    RestoreFrame(memory,state,25u,192u);
}

// PPC subfic/subfe on a word condition produces an all-zero or all-one word.
// Its source GPR still receives the full-width subtraction before subfe.
void SelectAlphabet(Registers& state,unsigned scratch,unsigned output,
    unsigned flag,std::uint64_t base)
{
    state.xer_ca=std::uint8_t(W(R(state,flag))==0u);
    R(state,scratch)=0u-R(state,flag);
    R(state,output)=state.xer_ca?0u:~std::uint64_t{0};
    R(state,output)=WordRotateMask(R(state,output),0,0xffffffe0u);
    R(state,output)+=base;
}

std::uint64_t MantissaWindow(std::uint64_t value)
{return std::rotl(value,52)&0x000fffffffffffffull;}
std::uint64_t ShiftWord(std::uint64_t value,std::uint64_t shift)
{return (W(shift)&0x40u)?0u:(value>>(W(shift)&0x7fu));}

void Hex(GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    SaveFrame(memory,state,26u,144u,0x82b7f3e8u);
    R(state,28)=48u;
    R(state,29)=R(state,7);
    R(state,31)=R(state,4);
    R(state,30)=1023u;
    R(state,7)=R(state,28);
    R(state,26)=0;
    CompareS(state.cr6,R(state,6),0,state.xer_so);
    if(state.cr6.lt) R(state,6)=R(state,26);
    CompareU(state.cr6,R(state,31),0,state.xer_so);
    if(state.cr6.eq)
    {
        InvalidArgument(memory,dependencies,state,0x82b7f41cu,
            0x82b7f440u,false,22u);
        RestoreFrame(memory,state,26u,144u);
        return;
    }
    CompareU(state.cr6,R(state,5),0,state.xer_so);
    if(state.cr6.eq)
    {
        InvalidArgument(memory,dependencies,state,0x82b7f454u,
            0x82b7f474u,true,22u);
        RestoreFrame(memory,state,26u,144u);
        return;
    }
    R(state,11)=R(state,6)+11u;
    memory.WriteU8(Address(R(state,31)),0);
    CompareU(state.cr6,R(state,5),W(R(state,11)),state.xer_so);
    if(!state.cr6.gt)
    {
        InvalidArgument(memory,dependencies,state,0x82b7f490u,
            0x82b7f474u,true,34u);
        RestoreFrame(memory,state,26u,144u);
        return;
    }
    R(state,11)=ReadU64(memory,Address(R(state,3)));
    R(state,10)=WordRotateMask(R(state,11),0,0xffeu);
    CompareU64(state.cr6,R(state,10),4094u,state.xer_so);
    if(state.cr6.eq)
    {
        CompareS(state.cr6,R(state,5),-1,state.xer_so);
        if(state.cr6.eq) R(state,5)=~std::uint64_t{0};
        else R(state,5)-=2u;
        R(state,30)=R(state,31)+2u;
        R(state,8)=0;
        R(state,7)=0;
        R(state,4)=R(state,30);
        state.lr=0x82b7f4d0u;
        Scientific(memory,dependencies,state);
        CompareS(state.cr0,R(state,3),0,state.xer_so);
        if(!state.cr0.eq)
        {
            memory.WriteU8(Address(R(state,31)),0);
            RestoreFrame(memory,state,26u,144u);
            return;
        }
        R(state,11)=memory.ReadU8(Address(R(state,30)));
        CompareU(state.cr6,R(state,11),45u,state.xer_so);
        if(state.cr6.eq)
        {
            memory.WriteU8(Address(R(state,31)),
                static_cast<std::uint8_t>(R(state,11)));
            R(state,31)+=1u;
        }
        state.xer_ca=std::uint8_t(W(R(state,29))==0u);
        R(state,11)=0u-R(state,29);
        memory.WriteU8(Address(R(state,31)),48u);
        R(state,4)=101u;
        R(state,10)=state.xer_ca?0u:~std::uint64_t{0};
        R(state,11)=R(state,31)+1u;
        R(state,10)=WordRotateMask(R(state,10),0,0xffffffe0u);
        R(state,3)=R(state,11)+1u;
        R(state,10)+=120u;
        memory.WriteU8(Address(R(state,11)),
            static_cast<std::uint8_t>(R(state,10)));
        CallSearch(memory,dependencies,state,0x82b7f51cu);
        CompareU(state.cr0,R(state,3),0,state.xer_so);
        if(!state.cr0.eq)
        {
            SelectAlphabet(state,11,11,29,112u);
            memory.WriteU8(Address(R(state,3)),
                static_cast<std::uint8_t>(R(state,11)));
            memory.WriteU8(Address(R(state,3)+3u),0);
        }
        R(state,3)=0; // The successful special-value branch joins at F81C.
        RestoreFrame(memory,state,26u,144u);
        return;
    }

    R(state,11)&=1u;
    R(state,27)=45u;
    CompareU64(state.cr6,R(state,11),0u,state.xer_so);
    if(!state.cr6.eq)
    {
        memory.WriteU8(Address(R(state,31)),45u);
        R(state,31)+=1u;
    }
    SelectAlphabet(state,11,10,29,120u);
    memory.WriteU8(Address(R(state,31)),48u);
    state.xer_ca=std::uint8_t(W(R(state,29))==0u);
    R(state,9)=0u-R(state,29);
    R(state,9)=state.xer_ca?0u:~std::uint64_t{0};
    R(state,8)=R(state,10);
    R(state,11)=R(state,31)+1u;
    R(state,10)=WordRotateMask(R(state,9),0,0xffffffe0u);
    R(state,10)+=97u;
    R(state,5)=R(state,10)-58u;
    memory.WriteU8(Address(R(state,11)),
        static_cast<std::uint8_t>(R(state,8)));
    R(state,10)=ReadU64(memory,Address(R(state,3)));
    R(state,11)+=1u;
    R(state,10)=WordRotateMask(R(state,10),0,0xffeu);
    CompareU64(state.cr6,R(state,10),0u,state.xer_so);
    if(state.cr6.eq)
    {
        memory.WriteU8(Address(R(state,11)),48u);
        R(state,11)+=1u;
        R(state,10)=ReadU64(memory,Address(R(state,3)));
        R(state,10)&=0xfffffffffffff000ull;
        CompareU64(state.cr6,R(state,10),0u,state.xer_so);
        R(state,30)=state.cr6.eq?R(state,26):1022u;
    }
    else
    {
        R(state,10)=49u;
        memory.WriteU8(Address(R(state,11)),49u);
        R(state,11)+=1u;
    }
    R(state,4)=R(state,11);
    R(state,8)=R(state,11)+1u;
    CompareS(state.cr6,R(state,6),0,state.xer_so);
    if(state.cr6.eq) memory.WriteU8(Address(R(state,4)),0);
    else
    {
        R(state,11)=0xffffffff83210000ull;
        R(state,11)=memory.ReadU32(Address(R(state,11)+22088u));
        R(state,11)=memory.ReadU32(Address(R(state,11)));
        R(state,11)=memory.ReadU8(Address(R(state,11)));
        memory.WriteU8(Address(R(state,4)),
            static_cast<std::uint8_t>(R(state,11)));
    }
    R(state,11)=ReadU64(memory,Address(R(state,3)));
    R(state,11)&=0xfffffffffffff000ull;
    CompareU64(state.cr6,R(state,11),0u,state.xer_so);
    if(state.cr6.gt)
    {
        R(state,10)=0x000f000000000000ull;
        do
        {
            CompareS(state.cr6,R(state,6),0,state.xer_so);
            if(!state.cr6.gt) break;
            R(state,11)=ReadU64(memory,Address(R(state,3)));
            R(state,9)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
                static_cast<std::int16_t>(R(state,7))));
            R(state,11)=MantissaWindow(R(state,11));
            R(state,11)&=R(state,10);
            R(state,11)=ShiftWord(R(state,11),R(state,9));
            R(state,11)=W(R(state,11))&0xffffu;
            R(state,11)+=48u;
            R(state,9)=W(R(state,11))&0xffffu;
            CompareU(state.cr6,R(state,9),57u,state.xer_so);
            if(state.cr6.gt)
            {
                R(state,11)=W(R(state,5))&0xffffu;
                R(state,11)+=R(state,9);
                R(state,11)=W(R(state,11))&0xffffu;
            }
            R(state,9)=R(state,7)-4u;
            memory.WriteU8(Address(R(state,8)),
                static_cast<std::uint8_t>(R(state,11)));
            R(state,10)=std::rotl(R(state,10),60)&0x0fffffffffffffffull;
            R(state,6)-=1u;
            R(state,8)+=1u;
            R(state,7)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
                static_cast<std::int16_t>(R(state,9))));
            CompareS(state.cr0,R(state,7),0,state.xer_so);
        } while(!state.cr0.lt);
        R(state,11)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
            static_cast<std::int16_t>(R(state,7))));
        CompareS(state.cr0,R(state,11),0,state.xer_so);
        if(!state.cr0.lt)
        {
            R(state,11)=ReadU64(memory,Address(R(state,3)));
            R(state,9)=static_cast<std::uint64_t>(static_cast<std::int64_t>(
                static_cast<std::int16_t>(R(state,7))));
            R(state,11)=MantissaWindow(R(state,11));
            R(state,11)&=R(state,10);
            R(state,11)=ShiftWord(R(state,11),R(state,9));
            R(state,11)=W(R(state,11))&0xffffu;
            CompareU(state.cr6,R(state,11),8u,state.xer_so);
            if(state.cr6.gt)
            {
                R(state,11)=R(state,8)-1u;
                for(;;)
                {
                    R(state,10)=memory.ReadU8(Address(R(state,11)));
                    R(state,10)=static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            static_cast<std::int8_t>(R(state,10))));
                    CompareS(state.cr6,R(state,10),102,state.xer_so);
                    if(!state.cr6.eq)
                    {
                        CompareS(state.cr6,R(state,10),70,state.xer_so);
                        if(!state.cr6.eq) break;
                    }
                    memory.WriteU8(Address(R(state,11)),48u);
                    R(state,11)-=1u;
                }
                CompareU(state.cr6,R(state,11),W(R(state,4)),state.xer_so);
                if(state.cr6.eq)
                {
                    R(state,10)=memory.ReadU8(Address(R(state,11)-1u));
                    R(state,10)+=1u;
                    memory.WriteU8(Address(R(state,11)-1u),
                        static_cast<std::uint8_t>(R(state,10)));
                }
                else
                {
                    R(state,10)=memory.ReadU8(Address(R(state,11)));
                    R(state,10)=static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(
                            static_cast<std::int8_t>(R(state,10))));
                    CompareS(state.cr6,R(state,10),57,state.xer_so);
                    R(state,10)=state.cr6.eq?R(state,5)+58u:R(state,10)+1u;
                    memory.WriteU8(Address(R(state,11)),
                        static_cast<std::uint8_t>(R(state,10)));
                }
            }
        }
    }
    CompareS(state.cr6,R(state,6),0,state.xer_so);
    if(state.cr6.gt)
    {
        R(state,11)=R(state,8);
        R(state,10)=R(state,28);
        CompareU(state.cr0,R(state,6),0,state.xer_so);
        if(!state.cr0.eq)
        {
            state.ctr=R(state,6);
            do
            {
                memory.WriteU8(Address(R(state,11)),
                    static_cast<std::uint8_t>(R(state,10)));
                R(state,11)+=1u;
                --state.ctr;
            } while(W(state.ctr)!=0u);
        }
        R(state,8)+=R(state,6);
    }
    R(state,11)=memory.ReadU8(Address(R(state,4)));
    CompareU(state.cr6,R(state,11),0,state.xer_so);
    if(state.cr6.eq) R(state,8)=R(state,4);
    SelectAlphabet(state,11,11,29,112u);
    R(state,10)=R(state,8)+1u;
    memory.WriteU8(Address(R(state,8)),
        static_cast<std::uint8_t>(R(state,11)));
    R(state,11)=ReadU64(memory,Address(R(state,3)));
    R(state,11)=std::rotr(R(state,11),1)&0x7ffu;
    R(state,11)-=R(state,30);
    CompareS64(state.cr6,R(state,11),0,state.xer_so);
    if(state.cr6.lt)
    {
        R(state,11)=0u-R(state,11);
        memory.WriteU8(Address(R(state,10)),45u);
    }
    else
    {
        R(state,9)=43u;
        memory.WriteU8(Address(R(state,10)),43u);
    }
    R(state,10)+=1u;
    CompareS64(state.cr6,R(state,11),1000,state.xer_so);
    R(state,8)=R(state,10);
    memory.WriteU8(Address(R(state,10)),48u);
    const bool thousands=!state.cr6.lt;
    if(thousands)
    {
        R(state,9)=1000u;
        R(state,7)=static_cast<std::uint64_t>(Signed(R(state,11))/1000);
        R(state,6)=R(state,7);
        R(state,9)=R(state,7);
        R(state,7)*=1000u;
        R(state,9)+=48u;
        R(state,11)-=R(state,7);
        memory.WriteU8(Address(R(state,10)),
            static_cast<std::uint8_t>(R(state,9)));
        R(state,10)+=1u;
        CompareU(state.cr6,R(state,10),W(R(state,8)),state.xer_so);
    }
    else CompareS64(state.cr6,R(state,11),100,state.xer_so);
    if(thousands || !state.cr6.lt)
    {
        R(state,9)=100u;
        R(state,7)=static_cast<std::uint64_t>(Signed(R(state,11))/100);
        R(state,6)=R(state,7);
        R(state,9)=R(state,7);
        R(state,7)*=100u;
        R(state,9)+=48u;
        R(state,11)-=R(state,7);
        memory.WriteU8(Address(R(state,10)),
            static_cast<std::uint8_t>(R(state,9)));
        R(state,10)+=1u;
    }
    CompareU(state.cr6,R(state,10),W(R(state,8)),state.xer_so);
    if(!state.cr6.eq)
    {
        R(state,9)=10u;
        R(state,8)=static_cast<std::uint64_t>(Signed(R(state,11))/10);
        R(state,7)=R(state,8);
        R(state,9)=R(state,8);
        R(state,8)*=10u;
        R(state,9)+=48u;
        R(state,11)-=R(state,8);
        memory.WriteU8(Address(R(state,10)),
            static_cast<std::uint8_t>(R(state,9)));
        R(state,10)+=1u;
    }
    else
    {
        CompareS64(state.cr6,R(state,11),10,state.xer_so);
        if(!state.cr6.lt)
        {
            R(state,9)=10u;
            R(state,8)=static_cast<std::uint64_t>(Signed(R(state,11))/10);
            R(state,7)=R(state,8);
            R(state,9)=R(state,8);
            R(state,8)*=10u;
            R(state,9)+=48u;
            R(state,11)-=R(state,8);
            memory.WriteU8(Address(R(state,10)),
                static_cast<std::uint8_t>(R(state,9)));
            R(state,10)+=1u;
        }
    }
    R(state,11)+=48u;
    memory.WriteU8(Address(R(state,10)),
        static_cast<std::uint8_t>(R(state,11)));
    memory.WriteU8(Address(R(state,10)+1u),0);
    R(state,3)=0;
    RestoreFrame(memory,state,26u,144u);
}

void Dispatch(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& state)
{
    if(entry==0x8231a2a0u)
    {
        R(state,11)=R(state,6);
        R(state,6)=R(state,7);
        R(state,7)=R(state,8);
        R(state,8)=R(state,9);
    }
    else
    {
        R(state,11)=R(state,6);
        R(state,6)=R(state,7);
        R(state,7)=R(state,8);
    }
    CompareS(state.cr6,R(state,11),101,state.xer_so);
    if(state.cr6.eq)
    {
        if(entry==0x82b7fc40u) R(state,8)=0;
        Scientific(memory,dependencies,state);return;
    }
    CompareS(state.cr6,R(state,11),69,state.xer_so);
    if(state.cr6.eq)
    {
        if(entry==0x82b7fc40u) R(state,8)=0;
        Scientific(memory,dependencies,state);return;
    }
    CompareS(state.cr6,R(state,11),102,state.xer_so);
    if(state.cr6.eq)
    {
        if(entry==0x8231a2a0u) R(state,7)=R(state,8);
        else R(state,7)=0;
        Fixed(memory,dependencies,state);
        return;
    }
    CompareS(state.cr6,R(state,11),97,state.xer_so);
    if(state.cr6.eq)
    {
        if(entry==0x82b7fc40u) R(state,8)=0;
        Hex(memory,dependencies,state);
        return;
    }
    CompareS(state.cr6,R(state,11),65,state.xer_so);
    if(state.cr6.eq)
    {
        if(entry==0x82b7fc40u) R(state,8)=0;
        Hex(memory,dependencies,state);
        return;
    }
    if(entry==0x82b7fc40u) R(state,8)=0;
    General(memory,dependencies,state);
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,
    Registers& registers)
{
    switch(entry)
    {
    case 0x8231a2a0u:
    case 0x82b7fc40u:Dispatch(entry,memory,dependencies,registers);return true;
    case 0x82b7f2c8u:Scientific(memory,dependencies,registers);return true;
    case 0x82b7f3e0u:Hex(memory,dependencies,registers);return true;
    case 0x82b7fa00u:Fixed(memory,dependencies,registers);return true;
    case 0x82b7faf0u:General(memory,dependencies,registers);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::crt_float_formatting
