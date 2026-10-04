#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_decommit_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Segment=0x50000u;
constexpr GuestAddress Arena=0x60000u,Descriptor=0x70000u;
constexpr GuestAddress Block=0x100000u,Stack=0x3f0000u;
constexpr GuestAddress LargeHead=Heap+384u;

enum class Mode {GateFallback,NoWholePage,Decommit,
    NativeFailure,LeadingRemainder};

inline GuestAddress BlockAddress(Mode mode)
{return mode==Mode::LeadingRemainder?Block+16u:Block;}

inline std::uint32_t Units(Mode mode)
{return mode==Mode::GateFallback || mode==Mode::NoWholePage?
    4u:9000u;}

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
    memory.WriteU32(Heap+48u,0u);
    memory.WriteU32(Heap+96u,Segment);
    memory.WriteU32(Heap+1412u,mode==Mode::GateFallback?1u:0u);
    memory.WriteU32(Segment+24u,Arena);
    memory.WriteU32(Segment+28u,0u);
    memory.WriteU32(Segment+40u,0u);
    memory.WriteU32(Segment+48u,0u);
    memory.WriteU32(Segment+52u,0u);
    memory.WriteU32(Segment+56u,0u);
    memory.WriteU32(Segment+64u,0u);
    memory.WriteU8(Segment+4u,0u);
    memory.WriteU32(Arena+76u,Descriptor);
    memory.WriteU32(Descriptor,0u);
    memory.WriteU32(Descriptor+4u,0u);
    memory.WriteU32(Descriptor+8u,0u);
    const auto block=BlockAddress(mode);
    memory.WriteU16(block,0u);
    memory.WriteU16(block+2u,0u);
    memory.WriteU8(block+4u,0u);
    memory.WriteU8(block+5u,0u);
    memory.WriteU16(block+Units(mode)*16u+2u,0u);
    memory.WriteU32(Segment+44u,0x300000u);
}
} // namespace heap_decommit_context_fixture
