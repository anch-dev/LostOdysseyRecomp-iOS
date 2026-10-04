#include "lo_semantics/legacy_descriptor_numeric_match_routes.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::legacy_descriptor_numeric_match_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
using Condition=crt_stream_operations::Condition;
std::uint64_t& R(Registers& s,unsigned i)
{auto& g=s.numeric.classifier.integer;return i==1u?g.sp:g.r[i];}
void Load(GuestMemory& m,Registers& s,unsigned i,std::uint64_t a)
{R(s,i)=m.ReadU32(Address(a));}
void Compare(Registers& s,std::uint64_t a,std::uint64_t b,Condition& cr,bool signed_words=false)
{
    const auto x=Address(a),y=Address(b);const auto sx=std::bit_cast<std::int32_t>(x),sy=std::bit_cast<std::int32_t>(y);
    cr={std::uint8_t(signed_words?sx<sy:x<y),std::uint8_t(signed_words?sx>sy:x>y),std::uint8_t(x==y),s.numeric.classifier.integer.xer_so};
}
std::uint64_t Right(std::uint64_t x,std::uint64_t n)
{const auto shift=std::uint8_t(n);return shift&32u?0u:Address(x)>>(shift&63u);}
std::uint64_t Left(std::uint64_t x,std::uint64_t n)
{const auto shift=std::uint8_t(n);return shift&32u?0u:std::uint32_t(Address(x)<<(shift&63u));}
void Extract(GuestMemory& m,Services& services,Registers& s)
{
    auto& g=s.numeric.classifier.integer;
    R(s,12)=g.lr;m.WriteU32(Address(R(s,1)-8u),Address(R(s,12)));
    WriteU64(m,Address(R(s,1)-24u),R(s,30));WriteU64(m,Address(R(s,1)-16u),R(s,31));
    const auto old_sp=R(s,1);R(s,1)-=112u;m.WriteU32(Address(R(s,1)),Address(old_sp));
    R(s,11)=WordRotateMask(R(s,3),0,0xfffff000u);R(s,30)=R(s,5);
    Load(m,s,11u,R(s,11));Load(m,s,11u,R(s,11)+148u);Load(m,s,11u,R(s,11)+40u);
    R(s,11)=~R(s,11);R(s,11)=WordRotateMask(R(s,11),18,1u);
    Compare(s,R(s,11),0u,g.cr0,true);
    if(g.cr0.eq)R(s,31)=0u;
    else
    {
        Load(m,s,11u,R(s,3)+16u);R(s,10)=WordRotateMask(R(s,4),1,0xfffffffeu);
        R(s,11)=WordRotateMask(R(s,11),18,0xffu);R(s,11)=Right(R(s,11),R(s,10));
        R(s,31)=Address(R(s,11))&3u;
    }
    g.lr=0x83053ef4u;(void)legacy_descriptor_numeric_read::Apply(0x82ff9b60u,m,services,s);
    R(s,5)=R(s,30);R(s,4)=R(s,31);g.lr=0x83053f00u;
    (void)legacy_fp_flagged_routes::Apply(0x83053c78u,m,services,s);
    R(s,1)+=112u;Load(m,s,12u,R(s,1)-8u);g.lr=R(s,12);
    R(s,30)=ReadU64(m,Address(R(s,1)-24u));R(s,31)=ReadU64(m,Address(R(s,1)-16u));
}
void Match(GuestMemory& m,Registers& s)
{
    auto& g=s.numeric.classifier.integer;
    Load(m,s,11u,R(s,3)+12u);Load(m,s,10u,R(s,4)+12u);
    Compare(s,R(s,11),R(s,10),g.cr6);
    if(!g.cr6.eq)
    {
        Load(m,s,11u,R(s,11)+8u);R(s,11)=WordRotateMask(R(s,11),0,0x3f80u);
        Compare(s,R(s,11),15232u,g.cr6);if(!g.cr6.eq)goto mismatch;
        Load(m,s,11u,R(s,10)+8u);R(s,11)=WordRotateMask(R(s,11),0,0x3f80u);
        Compare(s,R(s,11),15232u,g.cr6);if(!g.cr6.eq)goto mismatch;
        Load(m,s,11u,R(s,3));Load(m,s,10u,R(s,4));
        R(s,9)=WordRotateMask(R(s,11),2,1u);R(s,8)=WordRotateMask(R(s,10),2,1u);
        Compare(s,R(s,9),R(s,8),g.cr6);if(!g.cr6.eq)goto mismatch;
        Compare(s,R(s,9),0u,g.cr6);
        if(!g.cr6.eq)
        {
            R(s,11)^=R(s,10);R(s,11)=WordRotateMask(R(s,11),0,0x01ffe000u);
            Compare(s,R(s,11),0u,g.cr0,true);if(!g.cr0.eq)goto mismatch;
        }
    }
    Load(m,s,11u,R(s,4));Load(m,s,10u,R(s,3));R(s,9)=R(s,10)^R(s,11);
    R(s,9)=Address(R(s,9))&31u;Compare(s,R(s,9),0u,g.cr0,true);if(!g.cr0.eq)goto mismatch;
    R(s,9)=WordRotateMask(R(s,11),7,7u);R(s,8)=WordRotateMask(R(s,10),7,7u);
    Compare(s,R(s,8),R(s,9),g.cr6);if(!g.cr6.eq)goto mismatch;
    R(s,7)=WordRotateMask(R(s,8),1,0xfffffffeu);R(s,8)=1u;
    R(s,9)=WordRotateMask(R(s,9),1,0xfffffffeu);
    R(s,5)=WordRotateMask(R(s,11),27,0x07ffffffu);R(s,6)=WordRotateMask(R(s,10),27,0x07ffffffu);
    R(s,10)=Left(R(s,8),R(s,7));R(s,11)=Left(R(s,8),R(s,9));
    R(s,10)-=1u;R(s,11)-=1u;R(s,10)&=R(s,6);R(s,11)&=R(s,5);
    R(s,11)=R(s,10)^R(s,11);R(s,11)=Address(R(s,11))&0xffu;
    Compare(s,R(s,11),0u,g.cr0,true);if(g.cr0.eq)goto done;
mismatch:
    R(s,8)=0u;
done:
    R(s,3)=Address(R(s,8))&0xffu;
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Services& services,Registers& s)
{
    switch(entry){case 0x83053ea0u:Extract(memory,services,s);return true;
    case 0x82fac128u:Match(memory,s);return true;default:return false;}
}
}
