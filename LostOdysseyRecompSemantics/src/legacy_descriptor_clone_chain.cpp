#include "lo_semantics/legacy_descriptor_clone_chain.h"

#include "lo_semantics/legacy_descriptor_mutation_routes.h"
#include "lo_semantics/legacy_descriptor_record_routes.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::legacy_descriptor_clone_chain
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
using Integer = crt_stream_operations::Registers;
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
void Pool(GuestMemory& m, Services& services, Registers& s)
{ (void)legacy_descriptor_array_allocation::Apply(0x82fb36b0u,m,services,s); }
void CopyAttachment(GuestMemory& m, Services& services, Registers& s)
{
    Save(m,s,30u,112u);
    R(s,31)=R(s,4);R(s,30)=R(s,5);R(s,5)=26u;R(s,4)=20u;
    s.integer.lr=0x82fb6b50u;Pool(m,services,s);
    R(s,11)=5u;R(s,10)=R(s,3);s.integer.ctr=R(s,11);
    do
    {
        Load(m,s,11u,R(s,31));R(s,31)+=4u;
        Store(m,R(s,10),R(s,11));R(s,10)+=4u;
        --s.integer.ctr;
    } while(Address(s.integer.ctr)!=0u);
    R(s,11)=0u;Store(m,R(s,3)+12u,R(s,30));Store(m,R(s,3)+16u,R(s,11));
    Load(m,s,11u,R(s,30)+4u);Store(m,R(s,3)+8u,R(s,11));
    Store(m,R(s,30)+4u,R(s,3));Restore(m,s,30u,112u);
}
void MergeFlags(GuestMemory& m, Registers& s)
{
    auto& cr=s.integer.cr0;
    Load(m,s,9u,R(s,3));R(s,10)=R(s,4);R(s,11)=Address(R(s,9))&31u;
    R(s,8)=WordRotateMask(R(s,11),0,4u);Compare(s,R(s,8),0u,cr,true);
    if(!cr.eq)
    {
        R(s,8)=WordRotateMask(R(s,4),0,2u);Compare(s,R(s,8),0u,cr,true);
        if(!cr.eq)R(s,11)-=4u;
    }
    R(s,8)=R(s,11)&R(s,4);R(s,8)=WordRotateMask(R(s,8),0,4u);
    Compare(s,R(s,8),0u,cr,true);
    if(!cr.eq){R(s,11)-=4u;R(s,10)=R(s,4)-4u;}
    R(s,8)=Address(R(s,11))&1u;Compare(s,R(s,8),0u,cr,true);
    if(!cr.eq)
    {
        R(s,8)=WordRotateMask(R(s,10),0,2u);Compare(s,R(s,8),0u,cr,true);
        if(!cr.eq)R(s,10)-=2u;
    }
    R(s,11)|=R(s,10);
    R(s,11)=WordRotateMask(R(s,9),0,0xffffffe0u)|(R(s,11)&0xffffffff0000001full);
    Store(m,R(s,3),R(s,11));
}
void Attach(GuestMemory& m, Services& services, Registers& s)
{
    Save(m,s,31u,96u);Load(m,s,11u,R(s,4)+16u);R(s,31)=R(s,3);
    Compare(s,R(s,11),0u,s.integer.cr6);
    if(!s.integer.cr6.eq)
    {
        R(s,11)=WordRotateMask(R(s,4),0,0xfffff000u);
        Load(m,s,5u,R(s,4)+12u);Load(m,s,11u,R(s,11));
        Load(m,s,3u,R(s,11)+148u);s.integer.lr=0x82fbda54u;
        CopyAttachment(m,services,s);R(s,4)=R(s,3);
    }
    Store(m,R(s,4)+16u,R(s,31));R(s,3)=R(s,4);
    Load(m,s,11u,R(s,31));Store(m,R(s,4)+4u,R(s,11));
    Store(m,R(s,31),R(s,4));Restore(m,s,31u,96u);
}
void AllocateDescriptor(GuestMemory& m, Services& services, Registers& s)
{
    Save(m,s,25u,144u,0x83056b98u);
    R(s,29)=R(s,6);R(s,28)=R(s,7);R(s,27)=R(s,8);R(s,25)=R(s,4);
    R(s,31)=R(s,5);R(s,6)=R(s,27);R(s,5)=R(s,28);R(s,4)=R(s,29);
    R(s,30)=R(s,3);s.integer.lr=0x83056bc4u;
    (void)legacy_descriptor_record_routes::Apply(0x82fac238u,m,s.integer);
    R(s,4)=R(s,3);R(s,3)=R(s,30);R(s,5)=34u;
    s.integer.lr=0x83056bd4u;Pool(m,services,s);
    R(s,8)=R(s,27);R(s,7)=R(s,28);R(s,6)=R(s,29);R(s,5)=R(s,25);
    R(s,4)=R(s,30);R(s,26)=R(s,3);s.integer.lr=0x83056bf0u;
    (void)legacy_descriptor_mutation_routes::Apply(0x83056568u,m,s.integer);
    Compare(s,R(s,31),0u,s.integer.cr6);
    if(!s.integer.cr6.eq)
    {
        R(s,11)=WordRotateMask(R(s,26),0,0xfffffffeu);Load(m,s,9u,R(s,31));
        R(s,10)=R(s,31)-32u;R(s,11)+=32u;R(s,8)=R(s,10)|1u;
        R(s,7)=R(s,11)-32u;R(s,10)=R(s,11)+4u;
        Store(m,R(s,11),R(s,9));Load(m,s,9u,R(s,31));
        R(s,9)=WordRotateMask(R(s,9),0,0xfffffffeu);
        Store(m,R(s,9),R(s,7));Store(m,R(s,11)+4u,R(s,8));
        Store(m,R(s,31),R(s,10));
    }
    R(s,3)=R(s,26);Restore(m,s,25u,144u);
}
void Construct(GuestAddress entry, GuestMemory& m, Services& services, Registers& s)
{
    const std::uint32_t kind=entry==0x83057c38u?58u:entry==0x83057ce0u?59u:
        entry==0x83057d88u?60u:61u;
    Save(m,s,28u,128u,entry+8u);R(s,30)=R(s,5);R(s,8)=1u;R(s,7)=1u;
    R(s,6)=kind;R(s,5)=0u;R(s,29)=R(s,3);R(s,28)=R(s,4);
    s.integer.lr=entry+44u;AllocateDescriptor(m,services,s);
    Load(m,s,11u,R(s,30)+16u);R(s,31)=R(s,3);R(s,4)=R(s,30);
    Compare(s,R(s,11),0u,s.integer.cr6);
    if(!s.integer.cr6.eq)
    {
        R(s,3)=R(s,29);Load(m,s,5u,R(s,30)+12u);
        s.integer.lr=entry+76u;CopyAttachment(m,services,s);R(s,4)=R(s,3);
    }
    R(s,3)=R(s,31);s.integer.lr=entry+88u;Attach(m,services,s);
    R(s,9)=R(s,3);R(s,11)=R(s,28)+24u;
    R(s,10)=WordRotateMask(R(s,31),0,0xfffffffeu);R(s,7)=R(s,11)-32u;
    R(s,10)+=32u;Store(m,R(s,31)+40u,R(s,9));R(s,7)|=1u;
    Load(m,s,8u,R(s,11));R(s,6)=R(s,10)-32u;R(s,9)=R(s,10)+4u;
    R(s,3)=R(s,31);Store(m,R(s,10),R(s,8));Load(m,s,8u,R(s,11));
    R(s,8)=WordRotateMask(R(s,8),0,0xfffffffeu);Store(m,R(s,8),R(s,6));
    Store(m,R(s,10)+4u,R(s,7));Store(m,R(s,11),R(s,9));Restore(m,s,28u,128u);
}
void Clone(GuestMemory& m, Services& services, Registers& s)
{
    Save(m,s,29u,112u,0x83058ed0u);R(s,31)=R(s,3);R(s,30)=R(s,4);R(s,29)=0u;
    Load(m,s,11u,R(s,31)+8u);R(s,11)=WordRotateMask(R(s,11),25,0x7fu);
    auto& cr=s.integer.cr6;
    Compare(s,R(s,11),58u,cr);
    GuestAddress constructor=0x83057ce0u;bool selected=cr.eq!=0;
    if(!selected){Compare(s,R(s,11),59u,cr);constructor=0x83057c38u;selected=cr.eq!=0;}
    if(!selected){Compare(s,R(s,11),60u,cr);constructor=0x83057e30u;selected=cr.eq!=0;}
    if(!selected){Compare(s,R(s,11),61u,cr);constructor=0x83057d88u;selected=cr.eq!=0;}
    if(selected)
    {
        if(constructor==0x83057d88u||constructor==0x83057e30u)
        {
            Load(m,s,4u,R(s,31)+40u);R(s,3)=R(s,30);Load(m,s,5u,R(s,4)+12u);
            s.integer.lr=constructor==0x83057d88u?0x83058f18u:0x83058f48u;
            CopyAttachment(m,services,s);R(s,4)=4u;R(s,29)=R(s,3);
            s.integer.lr=constructor==0x83057d88u?0x83058f24u:0x83058f54u;
            MergeFlags(m,s);R(s,5)=R(s,29);
        }
        else Load(m,s,5u,R(s,31)+40u);
        R(s,3)=R(s,30);Load(m,s,4u,R(s,31)+24u);
        s.integer.lr=constructor==0x83057d88u?0x83058f34u:
            constructor==0x83057e30u?0x83058f64u:
            constructor==0x83057c38u?0x83058f78u:0x83058f8cu;
        Construct(constructor,m,services,s);R(s,29)=R(s,3);
    }
    Load(m,s,31u,R(s,31));
    for(;;)
    {
        Compare(s,R(s,31),0u,cr);if(cr.eq)break;
        Load(m,s,11u,R(s,31));R(s,11)=WordRotateMask(R(s,11),0,0x0e000000u);
        Compare(s,R(s,11),0u,s.integer.cr0,true);
        if(s.integer.cr0.eq)
        {R(s,4)=R(s,31);R(s,3)=R(s,29);s.integer.lr=0x83058fb4u;Attach(m,services,s);}
        Load(m,s,31u,R(s,31)+4u);
    }
    R(s,3)=R(s,29);Restore(m,s,29u,112u);
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Services& services, Registers& s)
{
    switch(entry)
    {
    case 0x83058ec8u:Clone(memory,services,s);return true;
    case 0x83057c38u:case 0x83057ce0u:case 0x83057d88u:case 0x83057e30u:
        Construct(entry,memory,services,s);return true;
    case 0x83056b90u:AllocateDescriptor(memory,services,s);return true;
    case 0x82fb6b28u:CopyAttachment(memory,services,s);return true;
    case 0x82fbd450u:MergeFlags(memory,s);return true;
    case 0x82fbda20u:Attach(memory,services,s);return true;
    default:return false;
    }
}
}
