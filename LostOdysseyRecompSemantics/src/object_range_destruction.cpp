#include "lo_semantics/object_range_destruction.h"

#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::object_range_destruction
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void CompareUnsigned(Registers& state, std::uint64_t left,
    std::uint64_t right)
{
    const auto a=Address(left),b=Address(right);
    state.cr6={std::uint8_t(a<b),std::uint8_t(a>b),
        std::uint8_t(a==b),state.xer_so};
}

void DestroyObjectRange(GuestMemory& memory,DynamicServices& dynamic,
    Registers& state,std::uint32_t stride,GuestAddress return_address)
{
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),Address(R(state,12)));
    WriteU64(memory,Address(state.sp-24u),R(state,30));
    WriteU64(memory,Address(state.sp-16u),R(state,31));
    memory.WriteU32(Address(state.sp-112u),Address(state.sp));
    state.sp-=112u;
    R(state,31)=R(state,3);
    R(state,30)=R(state,4);
    CompareUnsigned(state,R(state,31),R(state,30));
    while (!state.cr6.eq)
    {
        CompareUnsigned(state,R(state,31),0);
        if (!state.cr6.eq)
        {
            R(state,11)=memory.ReadU32(Address(R(state,31)));
            R(state,4)=0;
            R(state,3)=R(state,31);
            R(state,11)=memory.ReadU32(Address(R(state,11)+8u));
            state.ctr=R(state,11);
            state.lr=return_address;
            dynamic.CallGuestDestructor(Address(state.ctr)&~GuestAddress{3},
                memory,state);
        }
        R(state,31)+=stride;
        CompareUnsigned(state,R(state,31),R(state,30));
    }
    state.sp+=112u;
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
    R(state,30)=ReadU64(memory,Address(state.sp-24u));
    R(state,31)=ReadU64(memory,Address(state.sp-16u));
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    DynamicServices& dynamic,Registers& registers)
{
    switch (entry)
    {
    case 0x82b8bde0u:
        DestroyObjectRange(memory,dynamic,registers,36u,0x82b8be20u);
        return true;
    case 0x82b8be48u:
        DestroyObjectRange(memory,dynamic,registers,88u,0x82b8be88u);
        return true;
    case 0x82b8dbb0u:
        DestroyObjectRange(memory,dynamic,registers,40u,0x82b8dbf0u);
        return true;
    case 0x82b8dc18u:
        DestroyObjectRange(memory,dynamic,registers,32u,0x82b8dc58u);
        return true;
    case 0x82b90340u:
        DestroyObjectRange(memory,dynamic,registers,20u,0x82b90380u);
        return true;
    case 0x82b92068u:
        DestroyObjectRange(memory,dynamic,registers,48u,0x82b920a8u);
        return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::object_range_destruction
