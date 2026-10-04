#pragma once

#include "heap_free_context_fixture.h"

namespace heap_coalesce_context_fixture
{
using heap_free_context_fixture::Heap;
using heap_free_context_fixture::Block;
using heap_free_context_fixture::Payload;
using heap_free_context_fixture::Stack;
using heap_free_context_fixture::Header;
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

enum class Mode {Previous,Following,Both,DirectBoth};

inline std::uint32_t ExpectedUnits(Mode mode)
{
    switch(mode)
    {
    case Mode::Previous:return 6u;
    case Mode::Following:return 7u;
    default:return 9u;
    }
}

inline GuestAddress ExpectedBlock(Mode mode)
{
    return mode==Mode::Following?Block:Block-32u;
}

inline void Link(GuestMemory& memory,GuestAddress block,
    std::uint32_t units)
{
    const auto head=Heap+(units+48u)*8u;
    const auto node=block+8u;
    memory.WriteU32(head,node);memory.WriteU32(head+4u,node);
    memory.WriteU32(node,head);memory.WriteU32(node+4u,head);
    const auto bitmap=Heap+((units>>5u)+88u)*4u;
    memory.WriteU32(bitmap,memory.ReadU32(bitmap)|(1u<<(units&31u)));
    memory.WriteU32(Heap+48u,memory.ReadU32(Heap+48u)+units);
}

inline void Seed(GuestMemory& memory,Mode mode)
{
    heap_free_context_fixture::Seed(memory,
        heap_free_context_fixture::Mode::Small);
    if(mode!=Mode::Following)
    {
        constexpr GuestAddress previous=Block-32u;
        Header(memory,previous,2u,0u,0u);
        memory.WriteU16(Block+2u,2u);
        Link(memory,previous,2u);
    }
    if(mode!=Mode::Previous)
    {
        constexpr GuestAddress following=Block+64u;
        Header(memory,following,3u,4u,0u);
        Header(memory,following+48u,1u,3u,1u);
        Link(memory,following,3u);
    }
}
} // namespace heap_coalesce_context_fixture
