#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace a8_heap_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Heap = 0x10000u;
constexpr GuestAddress Block = 0x20000u;
constexpr GuestAddress Payload = Block + 16u;
constexpr GuestAddress Segment = 0x30000u;
constexpr GuestAddress Sentinel = 0x35000u;
constexpr GuestAddress HeapGlobal = 0x83245708u;
constexpr GuestAddress GateGlobal = 0x83246260u;
constexpr GuestAddress RetryGlobal = 0x832d3aecu;
constexpr std::uint32_t RequestedBytes = 2048u;
constexpr std::uint32_t RoundedBytes = 2064u;
constexpr std::uint32_t BlockUnits = RoundedBytes / 16u; // 129
constexpr GuestAddress LargeHead = Heap + 384u;

constexpr GuestAddress ExpectedAllocation() { return Payload; }

inline void WriteU16(GuestMemory& memory, GuestAddress address,
    std::uint16_t value)
{
    memory.WriteU8(address, static_cast<std::uint8_t>(value >> 8));
    memory.WriteU8(address + 1u, static_cast<std::uint8_t>(value));
}

// A8 gate reads 0x83246260; 823ACBD0 reads process-heap 0x83245708 and
// retry flag 0x832D3AEC. Required committed regions include low RAM through
// the caller stack, {0x83245000,0x2000}, and {0x832D3000,0x1000}.
//
// A8 calls 823ACAD8(r3=0,r4=2048), then actual 823ACBD0 and 823ACCB0.
// The heap rounds 2048 to 2064 bytes/129 units. This exact-size large-list
// block yields Block+16 without grow/VM callbacks, remainder splitting, or
// zero-fill. Heap flag 1 avoids native critical-section calls on allocation
// and eventual 823ADDC0 release; high decommit thresholds retain the block.
// The formatter and 823ADE28 free lower retain their separate native/service
// boundaries; this fixture does not claim their runtime behavior.
inline void Seed(GuestMemory& memory)
{
    memory.WriteU32(GateGlobal, 0u);
    memory.WriteU32(HeapGlobal, Heap);
    memory.WriteU32(RetryGlobal, 0u);

    memory.WriteU32(Heap + 20u, 0u); // process guard disabled
    memory.WriteU32(Heap + 24u, 1u); // no heap lock for this fixture
    memory.WriteU32(Heap + 28u, 0xffffu); // VM cutoff > 129 units
    memory.WriteU32(Heap + 40u, 0xffffu); // no decommit on release
    memory.WriteU32(Heap + 44u, 0xffffffffu);
    memory.WriteU32(Heap + 48u, BlockUnits);
    memory.WriteU32(Heap + 96u, Segment);
    memory.WriteU32(Heap + 1408u, Sentinel);
    memory.WriteU8(Heap + 379u, 1u);

    // Large free list: sentinel <-> sole block link (Block+8).
    memory.WriteU32(LargeHead, Block + 8u);
    memory.WriteU32(LargeHead + 4u, Block + 8u);
    memory.WriteU32(Block + 8u, LargeHead);
    memory.WriteU32(Block + 12u, LargeHead);
    WriteU16(memory, Block, static_cast<std::uint16_t>(BlockUnits));
    WriteU16(memory, Block + 2u, 0u);
    memory.WriteU8(Block + 4u, 0u);
    memory.WriteU8(Block + 5u, 0u);
    memory.WriteU8(Block + 6u, 0u);
    memory.WriteU8(Block + 7u, 0u);

    // Adjacent busy block prevents coalescing during the eventual free.
    const GuestAddress next = Block + RoundedBytes;
    WriteU16(memory, next, 1u);
    WriteU16(memory, next + 2u, static_cast<std::uint16_t>(BlockUnits));
    memory.WriteU8(next + 4u, 0u);
    memory.WriteU8(next + 5u, 1u);
}
} // namespace a8_heap_fixture
