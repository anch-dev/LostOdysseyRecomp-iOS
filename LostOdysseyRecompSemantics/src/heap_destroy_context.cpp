#include "lo_semantics/heap_destroy_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_destroy_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint32_t Low32(std::uint64_t value)
{return static_cast<std::uint32_t>(value);}
std::int32_t Signed32(std::uint64_t value)
{return std::bit_cast<std::int32_t>(Low32(value));}

template<typename T>
void Compare(object_child_float::Condition& condition,T left,T right,
    std::uint8_t so)
{
    condition={std::uint8_t(left<right),std::uint8_t(left>right),
        std::uint8_t(left==right),so};
}

void Save28(GuestMemory& memory,Registers& state)
{
    for(unsigned index=28u;index<=31u;++index)
        WriteU64(memory,Address(state.r[1]-8u*(33u-index)),state.r[index]);
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
}
void Restore28(GuestMemory& memory,Registers& state)
{
    for(unsigned index=28u;index<=31u;++index)
        state.r[index]=ReadU64(memory,Address(state.r[1]-8u*(33u-index)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}

void PageRelease(GuestMemory& memory,NativeServices& native,Registers& state)
{
    const auto old_stack=state.r[1];
    state.r[12]=state.lr;                                 // mflr r12
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
    state.r[1]-=96u;                                      // stwu r1,-96(r1)
    memory.WriteU32(Address(state.r[1]),Low32(old_stack));
    state.r[11]=memory.ReadU32(Address(state.r[3]+20u));
    state.r[11]=Low32(state.r[11])&1u;                    // clrlwi.
    Compare<std::int32_t>(state.cr0,Signed32(state.r[11]),0,state.xer_so);
    if(!state.cr0.eq) goto loc_827CBA4C;
    state.r[11]=memory.ReadU32(Address(state.r[3]+32u));
    state.r[5]=0u;
    state.r[6]=0u;
    state.r[5]|=32768u;
    state.r[4]=state.r[1]+84u;
    state.r[3]=state.r[1]+80u;
    memory.WriteU32(Address(state.r[1]+80u),Low32(state.r[11]));
    state.r[11]=0u;
    memory.WriteU32(Address(state.r[1]+84u),Low32(state.r[11]));
    state.lr=0x827cba48u;
    native.CallNative(0x830d9d3cu,memory,state);
    goto loc_827CBA50;
loc_827CBA4C:
    state.r[3]=0u;
loc_827CBA50:
    state.r[1]+=96u;
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}

void Destroy(GuestMemory& memory,NativeServices& native,Registers& state)
{
    const auto old_stack=state.r[1];
    state.r[12]=state.lr;
    state.lr=0x827cbba0u;
    Save28(memory,state);
    state.r[1]-=128u;
    memory.WriteU32(Address(state.r[1]),Low32(old_stack));
    state.r[29]=state.r[3];
    Compare<std::uint32_t>(state.cr6,Low32(state.r[29]),0u,state.xer_so);
    if(state.cr6.eq) goto loc_827CBC98;
    state.r[11]=memory.ReadU32(Address(state.r[29]+20u));
    state.r[11]=Low32(state.r[11])&0x40000u;
    Compare<std::int32_t>(state.cr0,Signed32(state.r[11]),0,state.xer_so);
    if(state.cr0.eq) goto loc_827CBBE4;
    state.lr=0x827cbbc0u;
    native.CallNative(0x830da07cu,memory,state);
    state.r[11]=memory.ReadU8(Address(state.r[29]+379u));
    Compare<std::int32_t>(state.cr6,Signed32(state.r[11]),
        Signed32(state.r[3]),state.xer_so);
    if(state.cr6.eq) goto loc_827CBBE4;
    state.r[7]=0u;
    state.r[5]=memory.ReadU32(Address(state.r[1]+120u));
    state.r[6]=1264u;
    state.r[4]=state.r[29];
    state.r[3]=244u;
    state.lr=0x827cbbe4u;
    native.CallNative(0x830da06cu,memory,state);
loc_827CBBE4:
    state.r[30]=state.r[29]+88u;
    state.r[28]=0u;
    state.r[31]=memory.ReadU32(Address(state.r[30]));
    goto loc_827CBC18;
loc_827CBBF4:
    state.r[5]=0u;
    memory.WriteU32(Address(state.r[1]+80u),Low32(state.r[31]));
    state.r[6]=0u;
    state.r[31]=memory.ReadU32(Address(state.r[31]));
    state.r[5]|=32768u;
    memory.WriteU32(Address(state.r[1]+84u),Low32(state.r[28]));
    state.r[4]=state.r[1]+84u;
    state.r[3]=state.r[1]+80u;
    state.lr=0x827cbc18u;
    native.CallNative(0x830d9d3cu,memory,state);
loc_827CBC18:
    Compare<std::uint32_t>(state.cr6,Low32(state.r[30]),
        Low32(state.r[31]),state.xer_so);
    if(!state.cr6.eq) goto loc_827CBBF4;
    state.r[11]=memory.ReadU32(Address(state.r[29]+20u));
    state.r[11]=Low32(state.r[11])&1u;
    Compare<std::int32_t>(state.cr0,Signed32(state.r[11]),0,state.xer_so);
    if(!state.cr0.eq) goto loc_827CBC30;
    memory.WriteU32(Address(state.r[29]+1408u),Low32(state.r[28]));
loc_827CBC30:
    state.r[31]=memory.ReadU32(Address(state.r[29]+72u));
    memory.WriteU32(Address(state.r[29]+72u),Low32(state.r[28]));
    Compare<std::uint32_t>(state.cr0,Low32(state.r[31]),0u,state.xer_so);
    if(state.cr0.eq) goto loc_827CBC6C;
loc_827CBC40:
    state.r[5]=0u;
    memory.WriteU32(Address(state.r[1]+80u),Low32(state.r[31]));
    state.r[6]=0u;
    state.r[31]=memory.ReadU32(Address(state.r[31]));
    state.r[5]|=32768u;
    memory.WriteU32(Address(state.r[1]+84u),Low32(state.r[28]));
    state.r[4]=state.r[1]+84u;
    state.r[3]=state.r[1]+80u;
    state.lr=0x827cbc64u;
    native.CallNative(0x830d9d3cu,memory,state);
    Compare<std::uint32_t>(state.cr6,Low32(state.r[31]),0u,state.xer_so);
    if(!state.cr6.eq) goto loc_827CBC40;
loc_827CBC6C:
    state.r[31]=64u;
loc_827CBC70:
    state.r[11]=(state.r[31]+255u)&0xffu;
    state.r[31]=state.r[11];
    state.r[11]=(Low32(state.r[31]+24u)<<2u)&0xfffffffcu;
    state.r[3]=memory.ReadU32(Address(state.r[29]+state.r[11]));
    Compare<std::uint32_t>(state.cr0,Low32(state.r[3]),0u,state.xer_so);
    if(state.cr0.eq) goto loc_827CBC90;
    state.lr=0x827cbc90u;
    PageRelease(memory,native,state);
loc_827CBC90:
    Compare<std::uint32_t>(state.cr6,Low32(state.r[31]),0u,state.xer_so);
    if(!state.cr6.eq) goto loc_827CBC70;
loc_827CBC98:
    state.r[3]=0u;
    state.r[1]+=128u;
    Restore28(memory,state);
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    NativeServices& native,Registers& state)
{
    switch(entry)
    {
    case 0x827cba08u:PageRelease(memory,native,state);return true;
    case 0x827cbb98u:Destroy(memory,native,state);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::heap_destroy_context
