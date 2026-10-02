#pragma once

#include "ppc_context.h"
#include "lo_semantics/detail/memory_fill_impl.h"

namespace lo::semantic::gpu::ppc
{

// This adapter is exercised by the differential harness. No runtime symbol or
// function-table entry is replaced by including this header or building the library.
inline void FillGuestMemory(PPCContext& ctx, std::uint8_t* base)
{
    struct Stores
    {
        std::uint8_t* base;
        void WriteU8(std::uint32_t address, std::uint8_t value)
        {
            PPC_STORE_U8(address, value);
        }
        void WriteU32(std::uint32_t address, std::uint32_t value)
        {
            PPC_STORE_U32(address, value);
        }
    } stores{base};

    const auto destination = ctx.r3.u32;
    const auto bytes = ctx.r5.u32;
    const auto padding = (0u - destination) & 3u;
    const auto prefix = bytes < padding ? bytes : padding;
    const auto remaining = bytes - prefix;
    detail::FillMemory(stores, destination, ctx.r4.u32, bytes);

    // Preserve even the original leaf's volatile outputs. In particular r3 is
    // untouched, including its high word; the final byte stores do not advance r6.
    ctx.r6.u64 = ctx.r3.u64 + prefix + (remaining & ~3u);
    ctx.r5.u64 -= prefix;
    ctx.r4.u64 = (ctx.r4.u64 & 0xffffffff00000000ull) |
                 (std::uint32_t{ctx.r4.u8} * 0x01010101u);
    const auto tail = remaining & 3u;
    ctx.r0.u64 = tail;
    ctx.cr0.compare<std::int32_t>(static_cast<std::int32_t>(tail), 0, ctx.xer);
    ctx.ctr.u64 = tail == 3 ? 1 : 0;
}

} // namespace lo::semantic::gpu::ppc
