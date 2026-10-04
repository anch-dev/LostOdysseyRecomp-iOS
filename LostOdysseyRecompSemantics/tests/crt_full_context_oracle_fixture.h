#pragma once

#include "crt_stream_oracle_fixture.h"
#include "lo_semantics/crt_async_status_transfer.h"

#include <array>
#include <cstdint>

namespace crt_full_oracle
{
using Registers = lo::semantic::gpu::crt_async_status_transfer::Registers;

inline std::array<PPCRegister*, 32> Gprs(PPCContext& context)
{
    return {&context.r0, &context.r1, &context.r2, &context.r3,
        &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11,
        &context.r12, &context.r13, &context.r14, &context.r15,
        &context.r16, &context.r17, &context.r18, &context.r19,
        &context.r20, &context.r21, &context.r22, &context.r23,
        &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
}

inline std::array<PPCRegister*, 32> Fprs(PPCContext& context)
{
    return {&context.f0, &context.f1, &context.f2, &context.f3,
        &context.f4, &context.f5, &context.f6, &context.f7,
        &context.f8, &context.f9, &context.f10, &context.f11,
        &context.f12, &context.f13, &context.f14, &context.f15,
        &context.f16, &context.f17, &context.f18, &context.f19,
        &context.f20, &context.f21, &context.f22, &context.f23,
        &context.f24, &context.f25, &context.f26, &context.f27,
        &context.f28, &context.f29, &context.f30, &context.f31};
}

inline Registers FromPpc(PPCContext& context)
{
    Registers state{};
    const auto gprs = Gprs(context), fprs = Fprs(context);
    for (unsigned index = 0; index < 32u; ++index)
    {
        state.r[index] = gprs[index]->u64;
        state.fpr_bits[index] = fprs[index]->u64;
    }
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.cached_fp_control = context.fpscr.csr;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    state.cr0 = {context.cr0.lt, context.cr0.gt,
        context.cr0.eq, context.cr0.so};
    state.cr6 = {context.cr6.lt, context.cr6.gt,
        context.cr6.eq, context.cr6.so};
    return state;
}

inline void ToPpc(PPCContext& context, const Registers& state)
{
    const auto gprs = Gprs(context), fprs = Fprs(context);
    for (unsigned index = 0; index < 32u; ++index)
    {
        gprs[index]->u64 = state.r[index];
        fprs[index]->u64 = state.fpr_bits[index];
    }
    context.lr = state.lr;
    context.ctr.u64 = state.ctr;
    context.fpscr.csr = state.cached_fp_control;
    context.xer.so = state.xer_so;
    context.xer.ca = state.xer_ca;
    context.cr0 = {state.cr0.lt, state.cr0.gt,
        state.cr0.eq, {state.cr0.so}};
    context.cr6 = {state.cr6.lt, state.cr6.gt,
        state.cr6.eq, {state.cr6.so}};
}

inline std::array<std::uint64_t, 70> Snapshot(const Registers& state)
{
    std::array<std::uint64_t, 70> result{};
    for (unsigned index = 0; index < 32u; ++index)
    {
        result[index] = state.r[index];
        result[index + 32u] = state.fpr_bits[index];
    }
    result[64] = state.lr;
    result[65] = state.ctr;
    result[66] = state.cached_fp_control;
    result[67] = state.xer_so | (std::uint64_t(state.xer_ca) << 8u);
    const auto pack = [](lo::semantic::gpu::crt_async_status_transfer::Condition cr)
    {
        return std::uint64_t(cr.lt) | (std::uint64_t(cr.gt) << 8u) |
            (std::uint64_t(cr.eq) << 16u) | (std::uint64_t(cr.so) << 24u);
    };
    result[68] = pack(state.cr0);
    result[69] = pack(state.cr6);
    return result;
}
} // namespace crt_full_oracle
