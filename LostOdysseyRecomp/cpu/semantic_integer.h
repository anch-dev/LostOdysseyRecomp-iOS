#pragma once

#include "lo_semantics/integer_leaf.h"

#include <cstdlib>
#include <cstring>

namespace lo::runtime::semantic_integer
{

inline bool Enabled() noexcept
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_SEMANTIC_INTEGER_RUNTIME");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

// PPCContext and PPCFunc come from ppc_context.h in the generated source.
inline void Dispatch(std::uint32_t address, PPCContext& ctx, std::uint8_t* base,
                     PPCFunc* original)
{
    if (!Enabled())
    {
        original(ctx, base);
        return;
    }

    lo::semantic::integer_leaf::Registers registers{
        .r3 = ctx.r3.u64, .r4 = ctx.r4.u64, .r5 = ctx.r5.u64,
        .r6 = ctx.r6.u64, .r7 = ctx.r7.u64, .r10 = ctx.r10.u64,
        .r11 = ctx.r11.u64,
    };
    if (!lo::semantic::integer_leaf::Apply(address, registers))
    {
        original(ctx, base);
        return;
    }
    ctx.r3.u64 = registers.r3;
    ctx.r10.u64 = registers.r10;
    ctx.r11.u64 = registers.r11;
}

} // namespace lo::runtime::semantic_integer
