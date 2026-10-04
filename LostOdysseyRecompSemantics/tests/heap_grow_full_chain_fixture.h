#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_grow_full_chain_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Descriptor=0x70000u;
constexpr GuestAddress Reserved=0x200000u,Stack=0x3f0000u;
constexpr GuestAddress LargeHead=Heap+384u;
constexpr std::uint32_t Requested=0x800u;

enum class Mode {Empty,ReserveFailure,ReserveFull,ReservePartial};

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
    memory.WriteU32(Heap+20u,mode==Mode::Empty?0u:2u);
    memory.WriteU32(Heap+32u,Requested+0x10000u);
    memory.WriteU32(Heap+36u,0x10000u);
    memory.WriteU32(Heap+40u,0xffffu);
    memory.WriteU32(Heap+44u,0xffffffffu);
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+76u,Descriptor);
    memory.WriteU32(Heap+1412u,0u);
    memory.WriteU32(Descriptor,0u);
    memory.WriteU32(Descriptor+4u,0u);
    memory.WriteU32(Descriptor+8u,0u);
    memory.WriteU32(Reserved+28u,0u);
    memory.WriteU32(Reserved+48u,0u);
    memory.WriteU32(Reserved+52u,0u);
    memory.WriteU32(Reserved+56u,0u);
    memory.WriteU32(Reserved+64u,0u);
}
} // namespace heap_grow_full_chain_fixture
