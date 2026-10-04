#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_growth_lower_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Segment=0x50000u;
constexpr GuestAddress Arena=0x60000u,Descriptor=0x70000u;
constexpr GuestAddress Base=0x200000u,Stack=0x3f0000u;
constexpr GuestAddress UnitsSlot=Stack-256u,LargeHead=Heap+384u;

enum class Mode {CommitEmpty,CommitNativeFailure,
    CreateRejected,CreateAll,CreatePartial};

inline bool IsCommit(Mode mode)
{return mode==Mode::CommitEmpty ||
    mode==Mode::CommitNativeFailure;}

inline void Seed(GuestMemory& memory,Mode mode)
{
    for(unsigned index=0u;index<128u;++index)
    {
        const auto head=Heap+(index+48u)*8u;
        memory.WriteU32(head,head);
        memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0u;word<4u;++word)
        memory.WriteU32(Heap+(88u+word)*4u,0u);
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+76u,Descriptor);
    memory.WriteU32(Heap+96u,Segment);
    memory.WriteU32(Heap+1412u,0u);
    memory.WriteU32(Descriptor,0u);
    memory.WriteU32(Descriptor+4u,0u);
    memory.WriteU32(Descriptor+8u,0u);
    memory.WriteU32(Arena+76u,Descriptor);
    memory.WriteU32(Segment+24u,Arena);
    memory.WriteU32(Segment+28u,0x10000u);
    memory.WriteU32(Segment+40u,0x120000u);
    memory.WriteU32(Segment+44u,0x130000u);
    memory.WriteU32(Segment+48u,1u);
    memory.WriteU32(Segment+52u,1u);
    memory.WriteU32(Segment+56u,
        mode==Mode::CommitNativeFailure?Descriptor:0u);
    memory.WriteU32(Segment+64u,0u);
    memory.WriteU8(Segment+4u,0u);
    memory.WriteU32(UnitsSlot,0x10000u);
    if(mode==Mode::CommitNativeFailure)
    {
        memory.WriteU32(Descriptor+4u,0x120000u);
        memory.WriteU32(Descriptor+8u,0x10000u);
    }
    if(!IsCommit(mode))
    {
        memory.WriteU32(Base+24u,Heap);
        memory.WriteU32(Base+28u,0u);
        memory.WriteU32(Base+40u,0u);
        memory.WriteU32(Base+48u,0u);
        memory.WriteU32(Base+52u,0u);
        memory.WriteU32(Base+56u,0u);
        memory.WriteU32(Base+64u,0u);
        memory.WriteU8(Base+4u,0u);
    }
}
} // namespace heap_growth_lower_context_fixture
