#pragma once

#include "cpu/semantic_accessor.h"
#include "lo_semantics/field_arithmetic.h"
#include "lo_semantics/field_bits.h"

namespace lo::runtime::semantic_fields
{

inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_FIELDS_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

inline void DispatchBits(std::uint32_t address, PPCContext& ctx,
                         std::uint8_t* base, PPCFunc* original)
{
    if (!Enabled())
    {
        original(ctx, base);
        return;
    }
    semantic_accessor::NativeAccessorMemory memory(base);
    lo::semantic::field_bits::Registers registers{
        ctx.r3.u64, ctx.r4.u64, ctx.r11.u64};
    if (!lo::semantic::field_bits::ApplyWith(address, registers, memory))
    {
        original(ctx, base);
        return;
    }
    ctx.r3.u64 = registers.r3;
    ctx.r11.u64 = registers.r11;
}

inline void DispatchArithmetic(std::uint32_t address, PPCContext& ctx,
                               std::uint8_t* base, PPCFunc* original)
{
    if (!Enabled())
    {
        original(ctx, base);
        return;
    }
    semantic_accessor::NativeAccessorMemory memory(base);
    lo::semantic::field_arithmetic::Registers registers{
        ctx.r3.u64, ctx.r4.u64, ctx.r5.u64, ctx.r8.u64,
        ctx.r9.u64, ctx.r10.u64, ctx.r11.u64};
    if (!lo::semantic::field_arithmetic::ApplyWith(address, registers, memory))
    {
        original(ctx, base);
        return;
    }
    ctx.r3.u64 = registers.r3;
    ctx.r8.u64 = registers.r8;
    ctx.r9.u64 = registers.r9;
    ctx.r10.u64 = registers.r10;
    ctx.r11.u64 = registers.r11;
}

} // namespace lo::runtime::semantic_fields
