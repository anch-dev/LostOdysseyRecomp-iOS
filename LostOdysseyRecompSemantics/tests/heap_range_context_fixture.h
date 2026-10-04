#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace heap_range_context_fixture
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;

constexpr GuestAddress Segment=0x50000u,Arena=0x60000u;
constexpr GuestAddress First=0x70000u,Second=0x70020u;
constexpr GuestAddress Pool=0x100000u,Stack=0x3f0000u;
constexpr GuestAddress Range=0x120000u;
constexpr std::uint32_t RangeBytes=0x10000u;

enum class Mode {AcquireFree,AcquireGrow,AcquireReserveFail,
    RangeEmpty,RangeBefore,RangeAfter,RangeSeparate};

inline bool IsRange(Mode mode)
{return mode>=Mode::RangeEmpty;}

inline GuestAddress NewRange(Mode mode)
{return mode==Mode::RangeBefore?Range+RangeBytes:Range;}

inline void Seed(GuestMemory& memory,Mode mode)
{
    memory.WriteU32(Segment+24u,Arena);
    memory.WriteU32(Segment+28u,0u);
    memory.WriteU32(Segment+52u,0u);
    memory.WriteU32(Segment+56u,0u);
    memory.WriteU32(Arena+72u,0u);
    memory.WriteU32(Arena+76u,0u);
    memory.WriteU32(First,0u);memory.WriteU32(First+4u,0u);
    memory.WriteU32(First+8u,0u);
    memory.WriteU32(Second,0u);memory.WriteU32(Second+4u,0u);
    memory.WriteU32(Second+8u,0u);
    if(mode==Mode::AcquireFree)
    {
        memory.WriteU32(Arena+76u,First);
        memory.WriteU32(First,Second);
        return;
    }
    if(!IsRange(mode)) return;
    memory.WriteU32(Arena+76u,Second);
    if(mode==Mode::RangeEmpty)
    {
        memory.WriteU32(Second,First);
        return;
    }
    const auto existing=mode==Mode::RangeAfter?
        Range+RangeBytes:mode==Mode::RangeSeparate?
        Range+3u*RangeBytes:Range;
    memory.WriteU32(Segment+56u,First);
    memory.WriteU32(Segment+28u,RangeBytes);
    memory.WriteU32(Segment+52u,1u);
    memory.WriteU32(First+4u,existing);
    memory.WriteU32(First+8u,RangeBytes);
}
} // namespace heap_range_context_fixture
