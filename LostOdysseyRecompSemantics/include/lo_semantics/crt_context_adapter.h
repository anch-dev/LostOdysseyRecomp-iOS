#pragma once

#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/raw_allocation_context.h"

namespace lo::semantic::gpu::crt_context_adapter
{
// Integer CRT bodies store SP separately. Preserve the caller's floating state
// while copying every selected integer register across an actual guest call.
template<class FullRegisters>
crt_stream_operations::Registers ToStream(const FullRegisters& state)
{
    crt_stream_operations::Registers lower{};
    lower.r = state.r;
    lower.r[1] = 0;
    lower.sp = state.r[1]; lower.lr = state.lr; lower.ctr = state.ctr;
    lower.xer_so = state.xer_so; lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    return lower;
}

template<class FullRegisters>
void FromStream(FullRegisters& state,
    const crt_stream_operations::Registers& lower)
{
    state.r = lower.r; state.r[1] = lower.sp;
    state.lr = lower.lr; state.ctr = lower.ctr;
    state.xer_so = lower.xer_so; state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt, lower.cr0.gt, lower.cr0.eq, lower.cr0.so};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq, lower.cr6.so};
}
// Preserve Full fields outside the allocator selected ABI.
template<class FullRegisters>
raw_allocation_context::Registers ToRaw(const FullRegisters& s)
{
    raw_allocation_context::Registers x{};
    x.r = s.r; x.lr = s.lr; x.ctr = s.ctr;
    x.f0_bits = s.fpr_bits[0]; x.f1_bits = s.fpr_bits[1];
    x.f13_bits = s.fpr_bits[13]; x.f30_bits = s.fpr_bits[30];
    x.f31_bits = s.fpr_bits[31]; x.cached_fp_control = s.cached_fp_control;
    x.xer_so = s.xer_so; x.xer_ca = s.xer_ca;
    x.cr0 = {s.cr0.lt, s.cr0.gt, s.cr0.eq, s.cr0.so};
    x.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, s.cr6.so};
    return x;
}
template<class FullRegisters>
void FromRaw(FullRegisters& s, const raw_allocation_context::Registers& x)
{
    s.r = x.r; s.lr = x.lr; s.ctr = x.ctr;
    s.fpr_bits[0] = x.f0_bits; s.fpr_bits[1] = x.f1_bits;
    s.fpr_bits[13] = x.f13_bits; s.fpr_bits[30] = x.f30_bits;
    s.fpr_bits[31] = x.f31_bits; s.cached_fp_control = x.cached_fp_control;
    s.xer_so = x.xer_so; s.xer_ca = x.xer_ca;
    s.cr0 = {x.cr0.lt, x.cr0.gt, x.cr0.eq, x.cr0.un};
    s.cr6 = {x.cr6.lt, x.cr6.gt, x.cr6.eq, x.cr6.un};
}
} // namespace lo::semantic::gpu::crt_context_adapter
