#pragma once
#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <cstdint>
namespace lo::semantic::gpu::detail::ppc_integer_context
{
using std::int32_t;
using std::uint32_t;
using std::uint64_t;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
// FPR, FP control and CR1/CR7 remain live in the owning full context.
// Save accesses cover ordinary RAM, excluding fault width/MMIO/concurrency.
union PpcRegister
{
    uint64_t u64;
    std::int64_t s64;
    uint32_t u32;
    int32_t s32;
    std::int8_t s8;
    std::int16_t s16;
    std::uint8_t u8;
    std::uint16_t u16;
    PpcRegister() : u64(0) {}
};
struct Xer { std::uint8_t so = 0, ca = 0; };
struct Cr
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    template<class T> void compare(T a, T b, const Xer& xer)
    {
        lt = std::uint8_t(a < b); gt = std::uint8_t(a > b);
        eq = std::uint8_t(a == b); so = xer.so;
    }
};
struct Context
{
    std::array<PpcRegister, 32> r{};
    uint64_t lr = 0;
    PpcRegister ctr{};
    Xer xer{};
    Cr cr0{}, cr6{};
};
inline void FromFull(Context& ctx, const crt_async_status_transfer::Registers& full)
{
    for (unsigned i = 0; i != 32; ++i) ctx.r[i].u64 = full.r[i];
    ctx.lr = full.lr; ctx.ctr.u64 = full.ctr;
    ctx.xer = {full.xer_so, full.xer_ca};
    ctx.cr0 = {full.cr0.lt, full.cr0.gt, full.cr0.eq, full.cr0.so};
    ctx.cr6 = {full.cr6.lt, full.cr6.gt, full.cr6.eq, full.cr6.so};
}
inline void ToFull(crt_async_status_transfer::Registers& full, const Context& ctx)
{
    for (unsigned i = 0; i != 32; ++i) full.r[i] = ctx.r[i].u64;
    full.lr = ctx.lr; full.ctr = ctx.ctr.u64;
    full.xer_so = ctx.xer.so; full.xer_ca = ctx.xer.ca;
    full.cr0 = {ctx.cr0.lt, ctx.cr0.gt, ctx.cr0.eq, ctx.cr0.so};
    full.cr6 = {ctx.cr6.lt, ctx.cr6.gt, ctx.cr6.eq, ctx.cr6.so};
}
template<class Base>
void Save(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31; ++i)
        WriteU64(base.memory, Address(ctx.r[1].u64 - 16u - 8u * (31u - i)),
            ctx.r[i].u64);
    base.memory.WriteU32(Address(ctx.r[1].u64 - 8u), ctx.r[12].u32);
}
template<class Base>
void Restore(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31; ++i)
        ctx.r[i].u64 = ReadU64(base.memory,
            Address(ctx.r[1].u64 - 16u - 8u * (31u - i)));
    ctx.r[12].u64 = base.memory.ReadU32(Address(ctx.r[1].u64 - 8u));
    ctx.lr = ctx.r[12].u64;
}

} // namespace lo::semantic::gpu::detail::ppc_integer_context
