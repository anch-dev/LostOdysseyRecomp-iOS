#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_grow_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Segment=0x50000u;
constexpr GuestAddress GrowthBlock=0x180000u,Reserved=0x200000u;
constexpr GuestAddress NewBlock=0x230000u,Stack=0x3f0000u;
constexpr std::uint32_t Requested=0x800u;
constexpr GuestAddress LargeHead=Heap+384u;

enum class Mode {Empty,TooSmall,CommitMiss,CommitSuccess,
    ReserveFailure,ReserveSuccess};

inline bool HasSegment(Mode mode)
{return mode==Mode::TooSmall || mode==Mode::CommitMiss ||
    mode==Mode::CommitSuccess;}

inline bool Reserve(Mode mode)
{return mode==Mode::ReserveFailure || mode==Mode::ReserveSuccess;}

inline void Seed(GuestMemory& memory,Mode mode)
{
    for(unsigned units=0u;units<128u;++units)
    {
        const auto head=Heap+(units+48u)*8u;
        memory.WriteU32(head,head);
        memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0u;word<4u;++word)
        memory.WriteU32(Heap+(88u+word)*4u,0u);
    for(unsigned index=0u;index<64u;++index)
        memory.WriteU32(Heap+(index+24u)*4u,0u);
    memory.WriteU32(Heap+20u,Reserve(mode)?2u:0u);
    memory.WriteU32(Heap+32u,Requested+0x10000u);
    memory.WriteU32(Heap+36u,0x10000u);
    memory.WriteU32(Heap+40u,0xffffu);
    memory.WriteU32(Heap+44u,0xffffffffu);
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+1412u,0u);
    if(!HasSegment(mode)) return;
    memory.WriteU32(Heap+96u,Segment);
    memory.WriteU32(Segment+24u,0u);
    memory.WriteU32(Segment+28u,0x10000u);
    memory.WriteU32(Segment+40u,0u);
    memory.WriteU32(Segment+44u,0x300000u);
    memory.WriteU32(Segment+48u,
        mode==Mode::TooSmall?0u:1u);
    memory.WriteU32(Segment+52u,0u);
    memory.WriteU32(Segment+56u,0u);
    memory.WriteU32(Segment+64u,0u);
    memory.WriteU8(Segment+4u,0u);
}

inline void SeedCommittedBlock(GuestMemory& memory)
{
    memory.WriteU16(GrowthBlock,4096u);
    memory.WriteU16(GrowthBlock+2u,0u);
    memory.WriteU8(GrowthBlock+4u,0u);
    memory.WriteU8(GrowthBlock+5u,0u);
    const auto next=GrowthBlock+0x10000u;
    memory.WriteU16(next,1u);
    memory.WriteU16(next+2u,4096u);
    memory.WriteU8(next+5u,1u);
}
} // namespace heap_grow_context_fixture
