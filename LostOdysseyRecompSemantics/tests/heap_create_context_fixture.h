#pragma once

#include "lo_semantics/guest_memory.h"

namespace heap_create_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap=0x200000u,Stack=0x3f0000u;
// The caller's 1452-byte header plus 128-byte table rounds to 16 bytes.
constexpr GuestAddress FirstSegment=Heap+0x630u;
constexpr GuestAddress DescriptorPool=0x700000u;
constexpr GuestAddress Defaults=0x831e7df8u,GlobalFlags=0x83374868u;
constexpr std::uint32_t ReserveBytes=0x100000u,CommitBytes=0x20000u;
constexpr std::uint32_t DescriptorReserveBytes=0x100000u,
    DescriptorCommitBytes=0x10000u;

enum class Mode {ReserveFailure,CommitFailure,FreshSuccess,ProvidedSuccess};

inline void Seed(GuestMemory& memory)
{
    // Exact image_disc1.bin words at the PPC-read VAs.
    memory.WriteU32(Defaults,ReserveBytes);
    memory.WriteU32(Defaults+4u,CommitBytes);
    memory.WriteU32(Defaults+8u,0x10000u);
    memory.WriteU32(Defaults+12u,0x10000u);
    memory.WriteU32(GlobalFlags,0u);
}
} // namespace heap_create_context_fixture
