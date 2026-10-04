#include "lo_semantics/legacy_descriptor_rank_attachment.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::legacy_descriptor_rank_attachment
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
using Condition = crt_stream_operations::Condition;
std::uint64_t& R(Registers& s, unsigned i)
{ return i == 1u ? s.integer.sp : s.integer.r[i]; }
void Load(GuestMemory& m, Registers& s, unsigned i, std::uint64_t a)
{ R(s,i)=m.ReadU32(Address(a)); }
void Store(GuestMemory& m, std::uint64_t a, std::uint64_t value)
{ m.WriteU32(Address(a),Address(value)); }
void Compare(Registers& s, std::uint64_t a, std::uint64_t b,
    Condition& cr, bool signed_words=false)
{
    const auto x=Address(a),y=Address(b);
    const auto sx=std::bit_cast<std::int32_t>(x);
    const auto sy=std::bit_cast<std::int32_t>(y);
    cr={std::uint8_t(signed_words?sx<sy:x<y),
        std::uint8_t(signed_words?sx>sy:x>y),std::uint8_t(x==y),s.integer.xer_so};
}
void Save(GuestMemory& m, Registers& s, unsigned first, unsigned frame,
    std::uint32_t return_pc=0u)
{
    R(s,12)=s.integer.lr;
    if(return_pc) s.integer.lr=return_pc;
    const auto sp=R(s,1);
    for(unsigned i=first;i<=31u;++i)
        WriteU64(m,Address(sp-8u*(33u-i)),R(s,i));
    Store(m,sp-8u,R(s,12));
    R(s,1)-=frame; Store(m,R(s,1),sp);
}
void Restore(GuestMemory& m, Registers& s, unsigned first, unsigned frame)
{
    R(s,1)+=frame;
    Load(m,s,12u,R(s,1)-8u);s.integer.lr=R(s,12);
    for(unsigned i=first;i<=31u;++i)
        R(s,i)=ReadU64(m,Address(R(s,1)-8u*(33u-i)));
}
void Call(GuestAddress entry, GuestMemory& m, Services& services, Registers& s)
{ (void)legacy_descriptor_clone_chain::Apply(entry,m,services,s); }
void Construct(GuestMemory& m, Services& services, Registers& s)
{
    Save(m,s,27u,128u,0x82ff3738u);
    R(s,30)=R(s,5);R(s,28)=R(s,6);R(s,8)=4u;R(s,7)=2u;R(s,6)=3u;R(s,5)=0u;
    R(s,29)=R(s,3);R(s,27)=R(s,4);s.integer.lr=0x82ff3760u;Call(0x83056b90u,m,services,s);
    Load(m,s,11u,R(s,30)+16u);R(s,31)=R(s,3);R(s,4)=R(s,30);
    Compare(s,R(s,11),0u,s.integer.cr6);
    if(!s.integer.cr6.eq){R(s,3)=R(s,29);Load(m,s,5u,R(s,30)+12u);
        s.integer.lr=0x82ff3780u;Call(0x82fb6b28u,m,services,s);R(s,4)=R(s,3);}
    R(s,3)=R(s,31);s.integer.lr=0x82ff378cu;Call(0x82fbda20u,m,services,s);
    Store(m,R(s,31)+40u,R(s,3));Load(m,s,11u,R(s,28)+16u);R(s,4)=R(s,28);
    Compare(s,R(s,11),0u,s.integer.cr6);
    if(!s.integer.cr6.eq){R(s,3)=R(s,29);Load(m,s,5u,R(s,28)+12u);
        s.integer.lr=0x82ff37acu;Call(0x82fb6b28u,m,services,s);R(s,4)=R(s,3);}
    R(s,3)=R(s,31);s.integer.lr=0x82ff37b8u;Call(0x82fbda20u,m,services,s);
    R(s,9)=R(s,3);Load(m,s,8u,R(s,31)+40u);R(s,10)=R(s,27)+24u;
    Load(m,s,7u,R(s,31)+8u);R(s,11)=WordRotateMask(R(s,31),0,0xfffffffeu);
    R(s,6)=R(s,10)-32u;R(s,11)+=32u;Store(m,R(s,31)+44u,R(s,9));R(s,6)|=1u;
    Load(m,s,9u,R(s,8));R(s,5)=R(s,11)-32u;R(s,8)=R(s,11)+4u;
    R(s,9)=WordRotateMask(R(s,9),7,7u);R(s,3)=R(s,31);
    R(s,7)=WordRotateMask(R(s,9),14,0x1c000u)|(R(s,7)&0xfffffffffffe3fffull);
    Store(m,R(s,31)+8u,R(s,7));Load(m,s,9u,R(s,10));Store(m,R(s,11),R(s,9));
    Load(m,s,9u,R(s,10));R(s,9)=WordRotateMask(R(s,9),0,0xfffffffeu);
    Store(m,R(s,9),R(s,5));Store(m,R(s,11)+4u,R(s,6));Store(m,R(s,10),R(s,8));
    Restore(m,s,27u,128u);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Services& services,Registers& state)
{
    if(entry!=0x82ff3730u)return false;
    Construct(memory,services,state);return true;
}
}
