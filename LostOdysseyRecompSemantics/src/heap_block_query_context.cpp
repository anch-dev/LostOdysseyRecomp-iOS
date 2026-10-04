#include "lo_semantics/heap_block_query_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::heap_block_query_context
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

void Query(GuestMemory& memory,NativeServices& native,Registers& state)
{
    const auto old_stack=state.r[1];
    state.r[12]=state.lr;
    memory.WriteU32(Address(state.r[1]-8u),Low32(state.r[12]));
    WriteU64(memory,Address(state.r[1]-24u),state.r[30]);
    WriteU64(memory,Address(state.r[1]-16u),state.r[31]);
    state.r[1]-=112u;
    memory.WriteU32(Address(state.r[1]),Low32(old_stack));
    state.r[30]=state.r[3];
    state.r[31]=state.r[5];
    state.r[11]=memory.ReadU32(Address(state.r[30]+20u));
    state.r[11]=Low32(state.r[11])&0x40000u;
    Compare<std::int32_t>(state.cr0,Signed32(state.r[11]),0,state.xer_so);
    if(state.cr0.eq) goto loc_827CC268;
    state.lr=0x827cc244u;
    native.CallNative(0x830da07cu,memory,state);
    state.r[11]=memory.ReadU8(Address(state.r[30]+379u));
    Compare<std::int32_t>(state.cr6,Signed32(state.r[11]),
        Signed32(state.r[3]),state.xer_so);
    if(state.cr6.eq) goto loc_827CC268;
    state.r[7]=state.r[31];
    state.r[5]=memory.ReadU32(Address(state.r[1]+104u));
    state.r[6]=5140u;
    state.r[4]=state.r[30];
    state.r[3]=244u;
    state.lr=0x827cc268u;
    native.CallNative(0x830da06cu,memory,state);
loc_827CC268:
    state.r[11]=memory.ReadU8(Address(state.r[31]-11u));
    state.r[10]=Low32(state.r[11])&1u;
    Compare<std::int32_t>(state.cr0,Signed32(state.r[10]),0,state.xer_so);
    if(!state.cr0.eq) goto loc_827CC27C;
    state.r[3]=~std::uint64_t{0};
    goto loc_827CC2A8;
loc_827CC27C:
    state.r[11]=Low32(state.r[11])&8u;
    Compare<std::int32_t>(state.cr0,Signed32(state.r[11]),0,state.xer_so);
    if(state.cr0.eq) goto loc_827CC298;
    state.r[11]=memory.ReadU16(Address(state.r[31]-16u));
    state.r[10]=memory.ReadU32(Address(state.r[31]-24u));
    state.r[11]=state.r[10]-state.r[11];
    state.r[3]=state.r[11]-48u;
    goto loc_827CC2A8;
loc_827CC298:
    state.r[11]=memory.ReadU16(Address(state.r[31]-16u));
    state.r[10]=memory.ReadU8(Address(state.r[31]-10u));
    state.r[11]=std::rotl(Low32(state.r[11]),4);
    state.r[3]=state.r[11]-state.r[10];
loc_827CC2A8:
    state.r[1]+=112u;
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
    state.r[30]=ReadU64(memory,Address(state.r[1]-24u));
    state.r[31]=ReadU64(memory,Address(state.r[1]-16u));
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    NativeServices& native,Registers& state)
{
    if(entry!=0x827cc218u) return false;
    Query(memory,native,state);
    return true;
}
} // namespace lo::semantic::gpu::heap_block_query_context
