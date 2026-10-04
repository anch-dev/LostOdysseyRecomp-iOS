#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_insert_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Block=0x100000u;
constexpr GuestAddress Existing=0x280000u,Existing2=0x290000u;
constexpr GuestAddress Segment=0x50000u,Stack=0x3f0000u;
constexpr GuestAddress LargeHead=Heap+384u;

enum class Mode {SmallEmpty,SmallOccupied,LargeOrdered,
    HugeSplit,SegmentEnd};

inline std::uint32_t Units(Mode mode)
{
    if(mode==Mode::SmallEmpty || mode==Mode::SmallOccupied)
        return 2u;
    if(mode==Mode::LargeOrdered) return 150u;
    return 0xf001u;
}

inline GuestAddress Head(std::uint32_t units)
{return units<128u?Heap+(units+48u)*8u:LargeHead;}

inline void Header(GuestMemory& memory,GuestAddress address,
    std::uint16_t size)
{
    memory.WriteU16(address,size);
    memory.WriteU16(address+2u,7u);
    memory.WriteU8(address+4u,0u);
    memory.WriteU8(address+5u,0u);
}

inline void Link(GuestMemory& memory,GuestAddress block,
    std::uint32_t units)
{
    const auto head=Head(units),node=block+8u;
    const auto previous=memory.ReadU32(head+4u);
    memory.WriteU32(node,head);memory.WriteU32(node+4u,previous);
    memory.WriteU32(previous,node);memory.WriteU32(head+4u,node);
    if(memory.ReadU32(head)==head) memory.WriteU32(head,node);
    if(units<128u)
    {
        const auto word=Heap+((units>>5u)+88u)*4u;
        memory.WriteU32(word,memory.ReadU32(word)|
            (1u<<(units&31u)));
    }
}

inline void Seed(GuestMemory& memory,Mode mode)
{
    for(unsigned units=0u;units<128u;++units)
    {
        const auto head=Head(units);
        memory.WriteU32(head,head);memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0u;word<4u;++word)
        memory.WriteU32(Heap+(88u+word)*4u,0u);
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+96u,Segment);
    memory.WriteU32(Segment+44u,mode==Mode::SegmentEnd?
        Block+0xeff0u*16u:0x300000u);
    Header(memory,Block,0u);
    if(mode==Mode::SmallOccupied)
    {
        Header(memory,Existing,2u);
        Link(memory,Existing,2u);
    }
    if(mode==Mode::LargeOrdered)
    {
        Header(memory,Existing,130u);
        Header(memory,Existing2,200u);
        Link(memory,Existing,130u);
        Link(memory,Existing2,200u);
    }
}
} // namespace heap_insert_context_fixture
