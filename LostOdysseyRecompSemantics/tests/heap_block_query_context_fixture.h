#pragma once

#include "lo_semantics/guest_memory.h"

namespace heap_block_query_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x10000u,Payload=0x30010u;
constexpr GuestAddress Stack=0x1f0000u;

enum class Mode {Inactive,Small,Large,GuardSmall};

inline void Seed(GuestMemory& memory,Mode mode)
{
    memory.WriteU32(Heap+20u,mode==Mode::GuardSmall?0x40000u:0u);
    memory.WriteU8(Heap+379u,3u);
    memory.WriteU32(Payload-24u,5000u);
    memory.WriteU16(Payload-16u,mode==Mode::Large?100u:8u);
    memory.WriteU8(Payload-11u,
        mode==Mode::Inactive?0u:(mode==Mode::Large?9u:1u));
    memory.WriteU8(Payload-10u,24u);
}
} // namespace heap_block_query_context_fixture
