#include "lo_semantics/pending_record_cleanup.h"

#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::pending_record_cleanup
{
namespace
{
using recovery_abi::Address;

std::uint64_t& R(Registers& state,unsigned index) {return state.r[index];}

void ClearPendingRecord(GuestMemory& memory,NativeServices& native,
    Registers& state)
{
    R(state,11)=memory.ReadU32(Address(R(state,3)+4u));
    const auto pending=Address(R(state,11));
    state.cr6={0,std::uint8_t(pending!=0),
        std::uint8_t(pending==0),state.xer_so};
    if (state.cr6.eq)
        return;

    native.LightweightSync();
    R(state,11)=memory.ReadU32(Address(R(state,3)));
    R(state,10)=memory.ReadU32(Address(R(state,3)+8u));
    R(state,8)=0;
    R(state,9)=memory.ReadU32(Address(R(state,11)+8u));
    R(state,10)+=R(state,9);
    memory.WriteU32(Address(R(state,11)+8u),Address(R(state,10)));
    R(state,11)=memory.ReadU32(Address(R(state,3)));
    memory.WriteU32(Address(R(state,11)+16u),Address(R(state,8)));
    memory.WriteU32(Address(R(state,3)+4u),Address(R(state,8)));
}

void ClearPendingFunclet(GuestMemory& memory,NativeServices& native,
    Registers& state)
{
    R(state,31)=R(state,12)-512u;
    R(state,12)=state.lr;
    memory.WriteU32(Address(state.sp-8u),Address(R(state,12)));
    memory.WriteU32(Address(state.sp-96u),Address(state.sp));
    state.sp-=96u;
    R(state,3)=R(state,31)+104u;
    state.lr=0x82290658u;
    ClearPendingRecord(memory,native,state);
    state.sp+=96u;
    R(state,12)=memory.ReadU32(Address(state.sp-8u));
    state.lr=R(state,12);
}
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    NativeServices& native,Registers& registers)
{
    switch (entry)
    {
    case 0x82373158u:ClearPendingRecord(memory,native,registers);return true;
    case 0x82290640u:ClearPendingFunclet(memory,native,registers);return true;
    default:return false;
    }
}
} // namespace lo::semantic::gpu::pending_record_cleanup
