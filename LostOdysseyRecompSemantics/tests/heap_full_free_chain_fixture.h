#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_full_free_chain_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Segment=0x50000u;
constexpr GuestAddress Arena=0x60000u,Descriptor=0x70000u;
constexpr GuestAddress Block=0x100000u,Payload=Block+16u;
constexpr GuestAddress Stack=0x3f0000u,LargeHead=Heap+384u;

enum class Mode {Small,LargeInsert,Decommit,NativeFailure};

inline std::uint32_t Units(Mode mode)
{
    if(mode==Mode::Small) return 4u;
    if(mode==Mode::LargeInsert) return 0xf001u;
    return 9000u;
}

inline void Seed(GuestMemory& memory,Mode mode)
{
    const auto units=Units(mode);
    for(unsigned index=0u;index<128u;++index)
    {
        const auto head=Heap+(index+48u)*8u;
        memory.WriteU32(head,head);
        memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0u;word<4u;++word)
        memory.WriteU32(Heap+(88u+word)*4u,0u);
    memory.WriteU32(Heap+20u,0u);
    memory.WriteU32(Heap+24u,1u);
    memory.WriteU32(Heap+40u,
        mode==Mode::Decommit || mode==Mode::NativeFailure?128u:0xffffu);
    memory.WriteU32(Heap+44u,
        mode==Mode::Decommit || mode==Mode::NativeFailure?1u:0xffffffffu);
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+96u,Segment);
    memory.WriteU32(Heap+1408u,0x320000u);
    memory.WriteU32(Heap+1412u,0u);
    memory.WriteU8(Heap+379u,1u);
    memory.WriteU16(Block,static_cast<std::uint16_t>(units));
    memory.WriteU16(Block+2u,0u);
    memory.WriteU8(Block+4u,0u);
    memory.WriteU8(Block+5u,0u);
    const auto next=Block+units*16u;
    memory.WriteU16(next,1u);
    memory.WriteU16(next+2u,static_cast<std::uint16_t>(units));
    memory.WriteU8(next+5u,1u);
    memory.WriteU32(Segment+24u,Arena);
    memory.WriteU32(Segment+28u,0u);
    memory.WriteU32(Segment+40u,0u);
    memory.WriteU32(Segment+44u,0x300000u);
    memory.WriteU32(Segment+48u,0u);
    memory.WriteU32(Segment+52u,0u);
    memory.WriteU32(Segment+56u,0u);
    memory.WriteU32(Segment+64u,0u);
    memory.WriteU8(Segment+4u,0u);
    memory.WriteU32(Arena+72u,0u);
    memory.WriteU32(Arena+76u,Descriptor);
    memory.WriteU32(Descriptor,0u);
    memory.WriteU32(Descriptor+4u,0u);
    memory.WriteU32(Descriptor+8u,0u);
}
} // namespace heap_full_free_chain_fixture
