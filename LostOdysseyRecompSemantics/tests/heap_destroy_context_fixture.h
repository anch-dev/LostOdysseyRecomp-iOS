#pragma once

#include "lo_semantics/guest_memory.h"

namespace heap_destroy_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Segment=0x20000u;
constexpr GuestAddress Descriptor=0x24000u,Page=0x30000u;
constexpr GuestAddress Reservation=0x50000u,Stack=0x1f0000u;
constexpr GuestAddress SegmentHead=Heap+88u;

enum class Mode {DirectFlagged,DirectVirtual,Null,Empty,Mixed};

inline void Seed(GuestMemory& memory,Mode mode)
{
    memory.WriteU32(SegmentHead,SegmentHead);
    memory.WriteU32(Heap+72u,0u);
    memory.WriteU32(Heap+1408u,0xdeadbeefu);
    for(unsigned index=0u;index<64u;++index)
        memory.WriteU32(Heap+(index+24u)*4u,0u);
    memory.WriteU32(Heap+20u,0u);
    memory.WriteU8(Heap+379u,3u);
    memory.WriteU32(Page+20u,mode==Mode::DirectFlagged?1u:0u);
    memory.WriteU32(Page+32u,Reservation);
    if(mode==Mode::Mixed)
    {
        memory.WriteU32(SegmentHead,Segment);
        memory.WriteU32(Segment,SegmentHead);
        memory.WriteU32(Heap+72u,Descriptor);
        memory.WriteU32(Descriptor,0u);
        memory.WriteU32(Heap+96u,Page);
    }
}
} // namespace heap_destroy_context_fixture
