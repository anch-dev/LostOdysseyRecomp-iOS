#pragma once

#include "lo_semantics/crt_stream_operations.h"

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
} // namespace lo::semantic::gpu::crt_context_adapter
