#include "lo_semantics/legacy_config_format_heap_context.h"

#include "lo_semantics/heap_allocate.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::legacy_config_format_heap_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

void Save22(GuestMemory& memory,raw_allocation_context::Registers& state)
{
    const auto caller_sp=Address(state.r[1]);
    state.r[12]=state.lr;
    state.lr=0x823accb8u;
    for(unsigned i=22u;i<=31u;++i)
        WriteU64(memory,caller_sp-8u*(33u-i),state.r[i]);
    memory.WriteU32(caller_sp-8u,Address(state.r[12]));
    state.r[31]=state.r[1]-320u;
    memory.WriteU32(Address(state.r[31]),caller_sp);
    state.r[1]=state.r[31];
}

void LeaveLockFrame(GuestMemory& memory,
    raw_allocation_context::Registers& state,GuestAddress heap)
{
    // 823ACCB0: addi r12,r31,320; bl 823AD544. The supported path has
    // r22=0, so 823AD544 skips its native RtlLeaveCriticalSection call.
    const auto heap_frame=Address(state.r[31]);
    state.r[12]=state.r[31]+320u;
    state.lr=0x823ad510u;
    WriteU64(memory,heap_frame-8u,state.r[31]);
    WriteU64(memory,heap_frame-16u,heap);
    WriteU64(memory,heap_frame-24u,0u);
    state.r[12]=state.lr;
    memory.WriteU32(heap_frame-32u,Address(state.r[12]));
    memory.WriteU32(heap_frame-112u,heap_frame);
    state.r[1]=state.r[31]-112u;
    state.cr6={0u,0u,1u,state.xer_so};
    state.r[1]=memory.ReadU32(Address(state.r[1]));
    state.r[31]=ReadU64(memory,heap_frame-8u);
    state.r[27]=ReadU64(memory,heap_frame-16u);
    state.r[22]=ReadU64(memory,heap_frame-24u);
    state.r[12]=memory.ReadU32(heap_frame-32u);
    state.lr=state.r[12];
}

void Restore22(GuestMemory& memory,raw_allocation_context::Registers& state)
{
    state.r[1]=state.r[31]+320u;
    for(unsigned i=22u;i<=31u;++i)
        state.r[i]=ReadU64(memory,Address(state.r[1]-8u*(33u-i)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));
    state.lr=state.r[12];
}
} // namespace

bool Supports(GuestMemory& memory,GuestAddress heap,std::uint32_t flags,
    std::uint32_t bytes)
{
    if(heap==0 || bytes==0 || bytes>0xfffff000u ||
        (memory.ReadU32(heap+20u)&0x40000u)!=0u)
        return false;
    const auto combined=memory.ReadU32(heap+24u)|flags;
    if((combined&1u)==0u || (combined&12u)!=0u) return false;
    const auto rounded=(bytes+31u)&0xfffffff0u;
    const auto units=rounded>>4u;
    if(units<128u || units>memory.ReadU32(heap+28u)) return false;
    const auto head=heap+384u;
    const auto link=memory.ReadU32(head+4u);
    if(link==head || link<8u || memory.ReadU32(head)!=link)
        return false;
    const auto block=link-8u;
    return memory.ReadU16(block)==units &&
        memory.ReadU32(block+8u)==head &&
        memory.ReadU32(block+12u)==head;
}

bool Apply(GuestMemory& memory,CrtAllocationServices& services,
    raw_allocation_context::Registers& state)
{
    const auto heap=Address(state.r[3]);
    const auto flags=Address(state.r[4]);
    const auto bytes=Address(state.r[5]);
    if(!Supports(memory,heap,flags,bytes)) return false;
    Save22(memory,state);
    const auto frame=Address(state.r[31]);
    (void)AllocateHeapBlock(memory,services,heap,flags,bytes,frame);
    state.r[27]=heap;
    state.r[22]=0u;
    LeaveLockFrame(memory,state,heap);
    state.r[3]=memory.ReadU32(frame+100u);
    Restore22(memory,state);
    return true;
}
} // namespace lo::semantic::gpu::legacy_config_format_heap_context
