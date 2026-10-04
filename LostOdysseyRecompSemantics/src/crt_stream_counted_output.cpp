#include "lo_semantics/crt_stream_counted_output.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::crt_stream_counted_output
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Registers=crt_stream_operations::Registers;
void Compare(Registers& s,crt_stream_operations::Condition& c,
    std::uint64_t a,std::uint64_t b,bool signed_words)
{
    const auto x=Address(a),y=Address(b);
    if(signed_words)
    {
        const auto left=std::bit_cast<std::int32_t>(x);
        const auto right=std::bit_cast<std::int32_t>(y);
        c={std::uint8_t(left<right),std::uint8_t(left>right),
            std::uint8_t(left==right),s.xer_so};
    }
    else c={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,
    crt_stream_operations::Dependencies dependencies,Registers& s)
{
    if(entry!=0x82b82728u)return false;
    auto& r=s.r;
    r[12]=s.lr;
    memory.WriteU32(Address(s.sp-8u),Address(r[12]));
    WriteU64(memory,Address(s.sp-16u),r[31]);
    const auto caller_sp=s.sp;
    s.sp-=96u;
    memory.WriteU32(Address(s.sp),Address(caller_sp));
    r[10]=memory.ReadU32(Address(r[4]+12u));
    r[11]=r[3];r[31]=r[5];
    r[10]&=0x40u;
    Compare(s,s.cr0,r[10],0,true);
    bool count_only=false;
    if(!s.cr0.eq)
    {
        r[10]=memory.ReadU32(Address(r[4]+8u));
        Compare(s,s.cr6,r[10],0,false);
        count_only=s.cr6.eq;
    }
    if(!count_only)
    {
        r[10]=memory.ReadU32(Address(r[4]+4u));
        s.xer_ca=std::uint8_t(Address(r[10])>0u);
        --r[10];
        Compare(s,s.cr0,r[10],0,true);
        memory.WriteU32(Address(r[4]+4u),Address(r[10]));
        if(!s.cr0.lt)
        {
            r[10]=memory.ReadU32(Address(r[4]));
            r[3]=Address(r[11])&0xffu;
            memory.WriteU8(Address(r[10]),static_cast<std::uint8_t>(r[11]));
            r[11]=memory.ReadU32(Address(r[4]));
            ++r[11];memory.WriteU32(Address(r[4]),Address(r[11]));
        }
        else
        {
            r[3]=static_cast<std::uint64_t>(static_cast<std::int64_t>(
                std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(r[11]))));
            s.lr=0x82b8278cu;
            if(!crt_stream_operations::Apply(0x82b82558u,memory,dependencies,s))
                throw std::logic_error("selected stream output missing");
        }
        Compare(s,s.cr6,r[3],UINT64_MAX,true);
        if(s.cr6.eq)r[11]=UINT64_MAX;
        else count_only=true;
    }
    if(count_only)
    {r[11]=memory.ReadU32(Address(r[31]));++r[11];}
    memory.WriteU32(Address(r[31]),Address(r[11]));
    s.sp+=96u;r[12]=memory.ReadU32(Address(s.sp-8u));s.lr=r[12];
    r[31]=ReadU64(memory,Address(s.sp-16u));
    return true;
}
}
