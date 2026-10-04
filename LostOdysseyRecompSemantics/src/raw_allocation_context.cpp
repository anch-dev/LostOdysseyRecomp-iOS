#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::raw_allocation_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(object_child_float::Condition& cr, std::uint64_t a,
    std::uint64_t b, bool sign, std::uint8_t so)
{
    const auto x=Address(a), y=Address(b);
    const auto sx=std::bit_cast<std::int32_t>(x), sy=std::bit_cast<std::int32_t>(y);
    cr={std::uint8_t(sign?sx<sy:x<y),std::uint8_t(sign?sx>sy:x>y),
        std::uint8_t(x==y),so};
}
void Heap(GuestMemory& m, Registers& s)
{
    s.r[11]=static_cast<std::uint64_t>(std::int64_t{-2094792704});
    s.r[3]=m.ReadU32(Address(s.r[11]+22280u));
}
void Direct(GuestAddress target, GuestAddress continuation,
    GuestMemory& m, PpcBoundaryServices& b, Registers& s)
{ s.lr=continuation; b.CallDirect(target,m,s); }
void Allocate(GuestMemory& m, PpcBoundaryServices& b, Registers& s)
{
    s.r[12]=s.lr;s.lr=0x823acbd8u;
    for(unsigned i=28;i<=31;++i)WriteU64(m,Address(s.r[1]-8u*(33u-i)),s.r[i]);
    m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
    const auto old=Address(s.r[1]);s.r[1]-=128u;m.WriteU32(Address(s.r[1]),old);
    s.r[30]=s.r[3];s.r[11]=std::uint64_t(std::int64_t{-4096});
    Compare(s.cr6,s.r[30],s.r[11],false,s.xer_so);
    if(s.cr6.gt)
    {
        s.r[3]=s.r[30];Direct(0x82b7fe68u,0x823acc7cu,m,b,s);
        Direct(0x82b7fd78u,0x823acc80u,m,b,s);
        s.r[11]=s.r[3];s.r[10]=12;s.r[3]=0;
        m.WriteU32(Address(s.r[11]),Address(s.r[10]));
    }
    else
    {
        s.r[28]=std::uint64_t(std::int64_t{-2094202880});
        for(;;)
        {
            s.lr=0x823acbf4u;Heap(m,s);
            Compare(s.cr0,s.r[3],0,false,s.xer_so);
            if(s.cr0.eq)
            {
                Direct(0x82b7fce0u,0x823acc00u,m,b,s);
                s.r[3]=30;Direct(0x82b7fc98u,0x823acc08u,m,b,s);
                s.r[3]=255;Direct(0x82b7bf20u,0x823acc10u,m,b,s);
            }
            Compare(s.cr6,s.r[30],0,false,s.xer_so);s.r[31]=s.r[30];
            if(s.cr6.eq)s.r[31]=1;
            s.lr=0x823acc24u;Heap(m,s);s.r[4]=0;s.r[5]=s.r[31];
            Direct(0x823accb0u,0x823acc30u,m,b,s);
            s.r[29]=s.r[3];Compare(s.cr0,s.r[29],0,true,s.xer_so);
            if(!s.cr0.eq){s.r[3]=s.r[29];break;}
            s.r[11]=m.ReadU32(Address(s.r[28]+15084u));s.r[31]=12;
            Compare(s.cr6,s.r[11],0,true,s.xer_so);
            if(!s.cr6.eq)
            {
                s.r[3]=s.r[30];Direct(0x82b7fe68u,0x823acc50u,m,b,s);
                Compare(s.cr0,s.r[3],0,true,s.xer_so);
                if(!s.cr0.eq)continue;
            }
            else
            {
                Direct(0x82b7fd78u,0x823acc60u,m,b,s);
                m.WriteU32(Address(s.r[3]),Address(s.r[31]));
            }
            Direct(0x82b7fd78u,0x823acc68u,m,b,s);
            m.WriteU32(Address(s.r[3]),Address(s.r[31]));
            s.r[3]=s.r[29];break;
        }
    }
    s.r[1]+=128u;
    for(unsigned i=28;i<=31;++i)s.r[i]=ReadU64(m,Address(s.r[1]-8u*(33u-i)));
    s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, PpcBoundaryServices& services,
    Registers& state)
{
    if(entry==0x823acbd0u){Allocate(memory,services,state);return true;}
    if(entry==0x823acc98u){Heap(memory,state);return true;}
    return false;
}
}
