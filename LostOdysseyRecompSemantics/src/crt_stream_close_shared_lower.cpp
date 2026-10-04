#include "lo_semantics/crt_stream_close_shared_lower.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_free_context.h"
#include "lo_semantics/crt_format_stream.h"
#include "lo_semantics/crt_stream_index_unlock.h"
#include "lo_semantics/crt_stream_open_routes_context.h"
#include "lo_semantics/heap_allocation_context.h"
#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_close_shared_lower
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using std::int32_t;
using std::uint32_t;
using std::uint64_t;

// The authoritative generated bodies below retain their register operations
// and labels. This local context bridges those operations to the selected
// full register contract at every real direct or native call.
union PpcRegister
{
    uint64_t u64;
    std::int64_t s64;
    uint32_t u32;
    int32_t s32;
    std::int8_t s8;
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
    uint64_t lr = 0, ctr = 0;
    Xer xer{};
    Cr cr0{}, cr6{};
};
struct Base
{
    GuestMemory& memory;
    Dependencies dependencies;
    Registers& full;
};
void FromFull(Context& ctx, const Registers& full)
{
    for (unsigned i = 0; i != 32; ++i) ctx.r[i].u64 = full.r[i];
    ctx.lr = full.lr; ctx.ctr = full.ctr;
    ctx.xer = {full.xer_so, full.xer_ca};
    ctx.cr0 = {full.cr0.lt, full.cr0.gt, full.cr0.eq, full.cr0.so};
    ctx.cr6 = {full.cr6.lt, full.cr6.gt, full.cr6.eq, full.cr6.so};
}
void ToFull(Registers& full, const Context& ctx)
{
    for (unsigned i = 0; i != 32; ++i) full.r[i] = ctx.r[i].u64;
    full.lr = ctx.lr; full.ctr = ctx.ctr;
    full.xer_so = ctx.xer.so; full.xer_ca = ctx.xer.ca;
    full.cr0 = {ctx.cr0.lt, ctx.cr0.gt, ctx.cr0.eq, ctx.cr0.so};
    full.cr6 = {ctx.cr6.lt, ctx.cr6.gt, ctx.cr6.eq, ctx.cr6.so};
}
void Save(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31; ++i)
        WriteU64(base.memory, Address(ctx.r[1].u64 - 16u - 8u * (31u - i)),
            ctx.r[i].u64);
    base.memory.WriteU32(Address(ctx.r[1].u64 - 8u), ctx.r[12].u32);
}
void Restore(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31; ++i)
        ctx.r[i].u64 = ReadU64(base.memory,
            Address(ctx.r[1].u64 - 16u - 8u * (31u - i)));
    ctx.r[12].u64 = base.memory.ReadU32(Address(ctx.r[1].u64 - 8u));
    ctx.lr = ctx.r[12].u64;
}
void __savegprlr_25(Context& c, Base& b) { Save(25, c, b); }
void __savegprlr_26(Context& c, Base& b) { Save(26, c, b); }
void __savegprlr_29(Context& c, Base& b) { Save(29, c, b); }
void __restgprlr_25(Context& c, Base& b) { Restore(25, c, b); }
void __restgprlr_26(Context& c, Base& b) { Restore(26, c, b); }
void __restgprlr_29(Context& c, Base& b) { Restore(29, c, b); }

raw_allocation_context::Registers ToRaw(const Registers& s)
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
void FromRaw(Registers& s, const raw_allocation_context::Registers& x)
{
    s.r = x.r; s.lr = x.lr; s.ctr = x.ctr;
    s.fpr_bits[0] = x.f0_bits; s.fpr_bits[1] = x.f1_bits;
    s.fpr_bits[13] = x.f13_bits; s.fpr_bits[30] = x.f30_bits;
    s.fpr_bits[31] = x.f31_bits; s.cached_fp_control = x.cached_fp_control;
    s.xer_so = x.xer_so; s.xer_ca = x.xer_ca;
    s.cr0 = {x.cr0.lt, x.cr0.gt, x.cr0.eq, x.cr0.un};
    s.cr6 = {x.cr6.lt, x.cr6.gt, x.cr6.eq, x.cr6.un};
}
class RawHeap final : public raw_allocation_context::PpcBoundaryServices
{
public:
    explicit RawHeap(heap_allocation_context::BoundaryServices& boundary)
        : boundary_(boundary) {}
    void CallDirect(GuestAddress entry, GuestMemory& memory,
        raw_allocation_context::Registers& state) override
    {
        if (!heap_allocation_context::Apply(entry, memory, boundary_, state))
            throw std::logic_error("unselected raw allocation callee");
    }
private:
    heap_allocation_context::BoundaryServices& boundary_;
};

void Body_82DF2150(Context&, Base&);
void Body_82DF1FC0(Context&, Base&);
void Body_82DF4898(Context&, Base&);
void Body_82DF3030(Context&, Base&);
void Body_82DF4638(Context&, Base&);
void Body_82DF2118(Context&, Base&);
void Body_82DF4A30(Context&, Base&);
void Body_82DF6908(Context&, Base&);
void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    switch (entry)
    {
    case 0x82df2150u: Body_82DF2150(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df1fc0u: Body_82DF1FC0(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df4898u: Body_82DF4898(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df3030u: Body_82DF3030(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df4638u: Body_82DF4638(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df2118u: Body_82DF2118(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df4a30u: Body_82DF4A30(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df6908u: Body_82DF6908(ctx, base); ToFull(base.full, ctx); break;
    case 0x82df6760u:
        if (!crt_stream_open_wrapper::Apply(entry, base.memory,
                base.dependencies.open, base.full))
            throw std::logic_error("missing accepted open wrapper");
        break;
    default:
    {
        auto lower = crt_context_adapter::ToStream(base.full);
        if (entry == 0x823acbd0u)
        {
            auto raw = ToRaw(base.full);
            RawHeap heap(base.dependencies.open.open.read.heap);
            if (!raw_allocation_context::Apply(entry, base.memory, heap, raw))
                throw std::logic_error("missing accepted allocation body");
            FromRaw(base.full, raw);
        }
        else if (entry == 0x823addc0u)
        {
            if (!crt_free_context::Apply(entry, base.memory,
                    base.dependencies.open.open.read.free_lower, lower))
                throw std::logic_error("missing accepted free body");
            crt_context_adapter::FromStream(base.full, lower);
        }
        else if (entry == 0x82b7b778u || entry == 0x82b7b810u)
        {
            if (!crt_format_stream::Apply(entry, base.memory,
                    base.dependencies.close.pipeline.format, lower))
                throw std::logic_error("missing accepted format body");
            crt_context_adapter::FromStream(base.full, lower);
        }
        else if (entry == 0x82b7b7c8u)
        {
            if (!crt_stream_bulk_close_routes::Apply(entry, base.memory,
                    base.dependencies.close, lower))
                throw std::logic_error("missing accepted bulk close body");
            crt_context_adapter::FromStream(base.full, lower);
        }
        else if (entry == 0x82b819e8u || entry == 0x82b81b28u ||
                 entry == 0x82b821b0u)
        {
            if (!crt_stream_open_routes_context::ApplyAcceptedLower(entry,
                    base.memory, base.dependencies.open.open.stream,
                    base.dependencies.open.open.open, lower))
                throw std::logic_error("missing accepted lock/state body");
            crt_context_adapter::FromStream(base.full, lower);
        }
        else if (entry == 0x82b819c8u)
        {
            crt_stream_index_unlock::Registers x{lower.sp, lower.lr,
                lower.r[3], lower.r[10], lower.r[11], lower.r[12],
                lower.r[29], lower.r[31]};
            if (!crt_stream_index_unlock::Apply(entry, base.memory,
                    base.dependencies.close.pipeline.close.accepted.locks.index_unlock,
                    x))
                throw std::logic_error("missing accepted index unlock");
            lower.sp = x.sp; lower.lr = x.lr; lower.r[3] = x.r3;
            lower.r[10] = x.r10; lower.r[11] = x.r11;
            lower.r[12] = x.r12; lower.r[29] = x.r29;
            lower.r[31] = x.r31;
            crt_context_adapter::FromStream(base.full, lower);
        }
        else
        {
            if (!crt_stream_operations::ApplyAcceptedCallee(entry,
                    base.memory,
                    base.dependencies.close.pipeline.close.accepted, lower))
                throw std::logic_error("unselected direct close lower");
            crt_context_adapter::FromStream(base.full, lower);
        }
        break;
    }
    }
    FromFull(ctx, base.full);
}
void __imp__RtlEnterCriticalSection(Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    base.dependencies.native.EnterCriticalSection(base.memory, base.full);
    FromFull(ctx, base.full);
}
void __imp__RtlUnwind(Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    base.dependencies.native.RtlUnwind(base.memory, base.full);
    FromFull(ctx, base.full);
}
void sub_82DF1FC0(Context& c, Base& b) { Direct(0x82DF1FC0u, c, b); }
void sub_82DF4898(Context& c, Base& b) { Direct(0x82DF4898u, c, b); }
void sub_82DF3030(Context& c, Base& b) { Direct(0x82DF3030u, c, b); }
void sub_82DF4638(Context& c, Base& b) { Direct(0x82DF4638u, c, b); }
void sub_82DF2118(Context& c, Base& b) { Direct(0x82DF2118u, c, b); }
void sub_82DF4A30(Context& c, Base& b) { Direct(0x82DF4A30u, c, b); }
void sub_82DF6908(Context& c, Base& b) { Direct(0x82DF6908u, c, b); }
void sub_82DF6760(Context& c, Base& b) { Direct(0x82DF6760u, c, b); }
void sub_82B7FD78(Context& c, Base& b) { Direct(0x82B7FD78u, c, b); }
void sub_82B7FEC0(Context& c, Base& b) { Direct(0x82B7FEC0u, c, b); }
void sub_82B81B28(Context& c, Base& b) { Direct(0x82B81B28u, c, b); }
void sub_82B819E8(Context& c, Base& b) { Direct(0x82B819E8u, c, b); }
void sub_82B7B778(Context& c, Base& b) { Direct(0x82B7B778u, c, b); }
void sub_82B7B810(Context& c, Base& b) { Direct(0x82B7B810u, c, b); }
void sub_823ACBD0(Context& c, Base& b) { Direct(0x823ACBD0u, c, b); }
void sub_82B821B0(Context& c, Base& b) { Direct(0x82B821B0u, c, b); }
void sub_823ADDC0(Context& c, Base& b) { Direct(0x823ADDC0u, c, b); }
void sub_82B819C8(Context& c, Base& b) { Direct(0x82B819C8u, c, b); }
void sub_82B7B7C8(Context& c, Base& b) { Direct(0x82B7B7C8u, c, b); }

uint64_t RotateLeft64(uint64_t v, int n) { return std::rotl(v, n); }
uint32_t RotateLeft32(uint32_t v, int n) { return std::rotl(v, n); }
#define __builtin_rotateleft64(v,n) RotateLeft64((v),(n))
#define __builtin_rotateleft32(v,n) RotateLeft32((v),(n))
#define __builtin_clz(v) std::countl_zero(uint32_t(v))
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory, Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),uint32_t(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),uint64_t(v))

// Authoritative translated PPC bodies are inserted below without changing
// their instruction order or branch labels.
void Body_82DF2150(Context& ctx, Base& base)
{
	// li r5,64
	ctx.r[5].s64 = 64;
	// b 0x82df1fc0
	sub_82DF1FC0(ctx, base);
	return;
}

void Body_82DF1FC0(Context& ctx, Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x82DF1FC8;
	__savegprlr_26(ctx, base);
	// addi r31,r1,-160
	ctx.r[31].s64 = ctx.r[1].s64 + -160;
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// mr r27,r5
	ctx.r[27].u64 = ctx.r[5].u64;
	// cntlzw r11,r29
	ctx.r[11].u64 = ctx.r[29].u32 == 0 ? 32 : __builtin_clz(ctx.r[29].u32);
	// li r26,0
	ctx.r[26].s64 = 0;
	// stw r26,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[26].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df2028
	if (!ctx.cr0.eq) goto loc_82DF2028;
loc_82DF1FF8:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF1FFC;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,22
	ctx.r[10].s64 = 22;
	// li r7,0
	ctx.r[7].s64 = 0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,0
	ctx.r[5].s64 = 0;
	// li r4,0
	ctx.r[4].s64 = 0;
	// li r3,0
	ctx.r[3].s64 = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// bl 0x82b7fec0
	ctx.lr = 0x82DF2020;
	sub_82B7FEC0(ctx, base);
	// li r3,0
	ctx.r[3].s64 = 0;
	// b 0x82df20f0
	goto loc_82DF20F0;
loc_82DF2028:
	// cntlzw r11,r30
	ctx.r[11].u64 = ctx.r[30].u32 == 0 ? 32 : __builtin_clz(ctx.r[30].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df1ff8
	if (ctx.cr0.eq) goto loc_82DF1FF8;
	// lbz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[30].u32 + 0);
	// extsb r11,r11
	ctx.r[11].s64 = ctx.r[11].s8;
	// cntlzw r11,r11
	ctx.r[11].u64 = ctx.r[11].u32 == 0 ? 32 : __builtin_clz(ctx.r[11].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df1ff8
	if (ctx.cr0.eq) goto loc_82DF1FF8;
	// bl 0x82df4898
	ctx.lr = 0x82DF205C;
	sub_82DF4898(ctx, base);
	// mr. r28,r3
	ctx.r[28].u64 = ctx.r[3].u64;
	ctx.cr0.compare<int32_t>(ctx.r[28].s32, 0, ctx.xer);
	// stw r28,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[28].u32);
	// bne 0x82df2080
	if (!ctx.cr0.eq) goto loc_82DF2080;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF206C;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,24
	ctx.r[10].s64 = 24;
	// li r3,0
	ctx.r[3].s64 = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// b 0x82df20f0
	goto loc_82DF20F0;
loc_82DF2080:
	// nop
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lbz r11,0(r29)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82df20c0
	if (!ctx.cr6.eq) goto loc_82DF20C0;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF2098;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r9,22
	ctx.r[9].s64 = 22;
	// lis r10,-32033
	ctx.r[10].s64 = -2099314688;
	// addi r3,r31,160
	ctx.r[3].s64 = ctx.r[31].s64 + 160;
	// addi r4,r10,8428
	ctx.r[4].s64 = ctx.r[10].s64 + 8428;
	// stw r9,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[9].u32);
	// stw r26,88(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 88, ctx.r[26].u32);
	// bl 0x82df3030
	ctx.lr = 0x82DF20B8;
	sub_82DF3030(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// b 0x82df20ec
	goto loc_82DF20EC;
loc_82DF20C0:
	// mr r6,r28
	ctx.r[6].u64 = ctx.r[28].u64;
	// mr r5,r27
	ctx.r[5].u64 = ctx.r[27].u64;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82df4638
	ctx.lr = 0x82DF20D4;
	sub_82DF4638(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,160
	ctx.r[12].s64 = ctx.r[31].s64 + 160;
	// bl 0x82df2118
	ctx.lr = 0x82DF20E4;
	sub_82DF2118(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// b 0x82df20f0
	goto loc_82DF20F0;
loc_82DF20EC:
	// lwz r3,88(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 88);
loc_82DF20F0:
	// addi r1,r31,160
	ctx.r[1].s64 = ctx.r[31].s64 + 160;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
}

void Body_82DF4898(Context& ctx, Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x82DF48A0;
	__savegprlr_25(ctx, base);
	// addi r31,r1,-160
	ctx.r[31].s64 = ctx.r[1].s64 + -160;
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r3,1
	ctx.r[3].s64 = 1;
	// li r25,0
	ctx.r[25].s64 = 0;
	// mr r26,r25
	ctx.r[26].u64 = ctx.r[25].u64;
	// stw r26,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[26].u32);
	// bl 0x82b81b28
	ctx.lr = 0x82DF48BC;
	sub_82B81B28(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// mr r30,r25
	ctx.r[30].u64 = ctx.r[25].u64;
	// stw r30,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[30].u32);
	// lis r29,-31944
	ctx.r[29].s64 = -2093481984;
	// lis r27,-31944
	ctx.r[27].s64 = -2093481984;
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
loc_82DF48D4:
	// lwz r10,-29036(r27)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[27].u32 + -29036);
	// cmpw cr6,r30,r10
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, ctx.r[10].s32, ctx.xer);
	// bge cr6,0x82df49ec
	if (!ctx.cr6.lt) goto loc_82DF49EC;
	// rlwinm r28,r30,2,0,29
	ctx.r[28].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r10,r28,r11
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[11].u32);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82df4980
	if (ctx.cr6.eq) goto loc_82DF4980;
	// rotlwi r10,r10,0
	ctx.r[10].u64 = __builtin_rotateleft32(ctx.r[10].u32, 0);
	// lwz r10,12(r10)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 12);
	// andi. r9,r10,131
	ctx.r[9].u64 = ctx.r[10].u64 & 131;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// cmpwi r9,0
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// bne 0x82df490c
	if (!ctx.cr0.eq) goto loc_82DF490C;
	// rlwinm. r10,r10,0,16,16
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x8000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82df4914
	if (ctx.cr0.eq) goto loc_82DF4914;
loc_82DF490C:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// b 0x82df4968
	goto loc_82DF4968;
loc_82DF4914:
	// addi r10,r30,-3
	ctx.r[10].s64 = ctx.r[30].s64 + -3;
	// cmplwi cr6,r10,16
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 16, ctx.xer);
	// bgt cr6,0x82df4934
	if (ctx.cr6.gt) goto loc_82DF4934;
	// addi r3,r30,16
	ctx.r[3].s64 = ctx.r[30].s64 + 16;
	// bl 0x82b819e8
	ctx.lr = 0x82DF4928;
	sub_82B819E8(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df49ec
	if (ctx.cr0.eq) goto loc_82DF49EC;
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
loc_82DF4934:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// lwzx r4,r28,r11
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[11].u32);
	// bl 0x82b7b778
	ctx.lr = 0x82DF4940;
	sub_82B7B778(ctx, base);
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
	// lwzx r4,r28,r11
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[11].u32);
	// lwz r10,12(r4)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 12);
	// andi. r10,r10,131
	ctx.r[10].u64 = ctx.r[10].u64 & 131;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82df4974
	if (ctx.cr0.eq) goto loc_82DF4974;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b810
	ctx.lr = 0x82DF4960;
	sub_82B7B810(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
loc_82DF4968:
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// stw r30,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[30].u32);
	// b 0x82df48d4
	goto loc_82DF48D4;
loc_82DF4974:
	// rlwinm r10,r30,2,0,29
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r26,r10,r11
	ctx.r[26].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[11].u32);
	// b 0x82df49e8
	goto loc_82DF49E8;
loc_82DF4980:
	// li r3,60
	ctx.r[3].s64 = 60;
	// rlwinm r30,r30,2,0,29
	ctx.r[30].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x823acbd0
	ctx.lr = 0x82DF498C;
	sub_823ACBD0(ctx, base);
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
	// stwx r3,r30,r11
	PPC_STORE_U32(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[3].u32);
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
	// lwzx r10,r30,r11
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[11].u32);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82df49ec
	if (ctx.cr6.eq) goto loc_82DF49EC;
	// rotlwi r11,r10,0
	ctx.r[11].u64 = __builtin_rotateleft32(ctx.r[10].u32, 0);
	// li r4,4000
	ctx.r[4].s64 = 4000;
	// addi r3,r11,32
	ctx.r[3].s64 = ctx.r[11].s64 + 32;
	// bl 0x82b821b0
	ctx.lr = 0x82DF49B4;
	sub_82B821B0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
	// bne 0x82df49d4
	if (!ctx.cr0.eq) goto loc_82DF49D4;
	// lwzx r3,r30,r11
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[11].u32);
	// bl 0x823addc0
	ctx.lr = 0x82DF49C8;
	sub_823ADDC0(ctx, base);
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
	// stwx r25,r30,r11
	PPC_STORE_U32(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[25].u32);
	// b 0x82df49ec
	goto loc_82DF49EC;
loc_82DF49D4:
	// lwzx r11,r30,r11
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[11].u32);
	// addi r3,r11,32
	ctx.r[3].s64 = ctx.r[11].s64 + 32;
	// bl 0x830d9c6c
	ctx.lr = 0x82DF49E0;
	__imp__RtlEnterCriticalSection(ctx, base);
	// lwz r11,-29040(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + -29040);
	// lwzx r26,r30,r11
	ctx.r[26].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[11].u32);
loc_82DF49E8:
	// stw r26,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[26].u32);
loc_82DF49EC:
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82df4a18
	if (ctx.cr6.eq) goto loc_82DF4A18;
	// lwz r11,12(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 12);
	// rlwinm r11,r11,0,16,16
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x8000;
	// li r10,-1
	ctx.r[10].s64 = -1;
	// stw r11,12(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 12, ctx.r[11].u32);
	// stw r25,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[25].u32);
	// stw r25,8(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 8, ctx.r[25].u32);
	// stw r25,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[25].u32);
	// stw r25,28(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 28, ctx.r[25].u32);
	// stw r10,16(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 16, ctx.r[10].u32);
loc_82DF4A18:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,160
	ctx.r[12].s64 = ctx.r[31].s64 + 160;
	// bl 0x82df4a30
	ctx.lr = 0x82DF4A24;
	sub_82DF4A30(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// addi r1,r31,160
	ctx.r[1].s64 = ctx.r[31].s64 + 160;
	// b 0x82b7a72c
	__restgprlr_25(ctx, base);
	return;
}

void Body_82DF3030(Context& ctx, Base& base)
{
	PpcRegister temp{};
	// std r31,-72(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -72, ctx.r[31].u64);
	// mflr r31
	ctx.r[31].u64 = ctx.lr;
	// stwu r1,-80(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-80);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r5,0
	ctx.r[5].s64 = 0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// bl 0x830da28c
	ctx.lr = 0x82DF3048;
	__imp__RtlUnwind(ctx, base);
	// mtlr r31
	ctx.lr = ctx.r[31].u64;
	// ld r31,8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 8);
	// addi r1,r1,80
	ctx.r[1].s64 = ctx.r[1].s64 + 80;
	// blr
	return;
}

void Body_82DF4638(Context& ctx, Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82DF4640;
	__savegprlr_29(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lis r11,-31953
	ctx.r[11].s64 = -2094071808;
	// li r29,0
	ctx.r[29].s64 = 0;
	// mr r30,r6
	ctx.r[30].u64 = ctx.r[6].u64;
	// mr r7,r29
	ctx.r[7].u64 = ctx.r[29].u64;
	// mr r6,r29
	ctx.r[6].u64 = ctx.r[29].u64;
	// lwz r11,-13028(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -13028);
	// b 0x82df4664
	goto loc_82DF4664;
loc_82DF4660:
	// addi r4,r4,1
	ctx.r[4].s64 = ctx.r[4].s64 + 1;
loc_82DF4664:
	// lbz r10,0(r4)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[4].u32 + 0);
	// cmplwi cr6,r10,32
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 32, ctx.xer);
	// beq cr6,0x82df4660
	if (ctx.cr6.eq) goto loc_82DF4660;
	// extsb r10,r10
	ctx.r[10].s64 = ctx.r[10].s8;
	// cmpwi cr6,r10,97
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 97, ctx.xer);
	// beq cr6,0x82df46d0
	if (ctx.cr6.eq) goto loc_82DF46D0;
	// cmpwi cr6,r10,114
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 114, ctx.xer);
	// beq cr6,0x82df46c4
	if (ctx.cr6.eq) goto loc_82DF46C4;
	// cmpwi cr6,r10,119
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 119, ctx.xer);
	// beq cr6,0x82df46bc
	if (ctx.cr6.eq) goto loc_82DF46BC;
loc_82DF468C:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF4690;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,22
	ctx.r[10].s64 = 22;
	// li r7,0
	ctx.r[7].s64 = 0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,0
	ctx.r[5].s64 = 0;
	// li r4,0
	ctx.r[4].s64 = 0;
	// li r3,0
	ctx.r[3].s64 = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// bl 0x82b7fec0
	ctx.lr = 0x82DF46B4;
	sub_82B7FEC0(ctx, base);
loc_82DF46B4:
	// li r3,0
	ctx.r[3].s64 = 0;
	// b 0x82df4888
	goto loc_82DF4888;
loc_82DF46BC:
	// li r10,769
	ctx.r[10].s64 = 769;
	// b 0x82df46d4
	goto loc_82DF46D4;
loc_82DF46C4:
	// mr r10,r29
	ctx.r[10].u64 = ctx.r[29].u64;
	// ori r31,r11,1
	ctx.r[31].u64 = ctx.r[11].u64 | 1;
	// b 0x82df46d8
	goto loc_82DF46D8;
loc_82DF46D0:
	// li r10,265
	ctx.r[10].s64 = 265;
loc_82DF46D4:
	// ori r31,r11,2
	ctx.r[31].u64 = ctx.r[11].u64 | 2;
loc_82DF46D8:
	// addi r8,r4,1
	ctx.r[8].s64 = ctx.r[4].s64 + 1;
	// li r9,1
	ctx.r[9].s64 = 1;
	// b 0x82df480c
	goto loc_82DF480C;
loc_82DF46E4:
	// cmpwi cr6,r9,0
	ctx.cr6.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq cr6,0x82df4820
	if (ctx.cr6.eq) goto loc_82DF4820;
	// cmpwi cr6,r11,83
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 83, ctx.xer);
	// bgt cr6,0x82df4784
	if (ctx.cr6.gt) goto loc_82DF4784;
	// beq cr6,0x82df4770
	if (ctx.cr6.eq) goto loc_82DF4770;
	// cmpwi cr6,r11,32
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 32, ctx.xer);
	// beq cr6,0x82df4808
	if (ctx.cr6.eq) goto loc_82DF4808;
	// cmpwi cr6,r11,43
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 43, ctx.xer);
	// beq cr6,0x82df4754
	if (ctx.cr6.eq) goto loc_82DF4754;
	// cmpwi cr6,r11,44
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 44, ctx.xer);
	// beq cr6,0x82df47fc
	if (ctx.cr6.eq) goto loc_82DF47FC;
	// cmpwi cr6,r11,68
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 68, ctx.xer);
	// beq cr6,0x82df4744
	if (ctx.cr6.eq) goto loc_82DF4744;
	// cmpwi cr6,r11,78
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 78, ctx.xer);
	// beq cr6,0x82df473c
	if (ctx.cr6.eq) goto loc_82DF473C;
	// cmpwi cr6,r11,82
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 82, ctx.xer);
	// bne cr6,0x82df468c
	if (!ctx.cr6.eq) goto loc_82DF468C;
	// cmpwi cr6,r6,0
	ctx.cr6.compare<int32_t>(ctx.r[6].s32, 0, ctx.xer);
	// bne cr6,0x82df47fc
	if (!ctx.cr6.eq) goto loc_82DF47FC;
	// li r6,1
	ctx.r[6].s64 = 1;
	// ori r10,r10,16
	ctx.r[10].u64 = ctx.r[10].u64 | 16;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF473C:
	// ori r10,r10,128
	ctx.r[10].u64 = ctx.r[10].u64 | 128;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF4744:
	// rlwinm. r11,r10,0,25,25
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df47fc
	if (!ctx.cr0.eq) goto loc_82DF47FC;
	// ori r10,r10,64
	ctx.r[10].u64 = ctx.r[10].u64 | 64;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF4754:
	// rlwinm. r11,r10,0,30,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df47fc
	if (!ctx.cr0.eq) goto loc_82DF47FC;
	// rlwinm r11,r10,0,0,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFE;
	// rlwinm r4,r31,0,0,29
	ctx.r[4].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 0) & 0xFFFFFFFC;
	// ori r10,r11,2
	ctx.r[10].u64 = ctx.r[11].u64 | 2;
	// ori r31,r4,128
	ctx.r[31].u64 = ctx.r[4].u64 | 128;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF4770:
	// cmpwi cr6,r6,0
	ctx.cr6.compare<int32_t>(ctx.r[6].s32, 0, ctx.xer);
	// bne cr6,0x82df47fc
	if (!ctx.cr6.eq) goto loc_82DF47FC;
	// li r6,1
	ctx.r[6].s64 = 1;
	// ori r10,r10,32
	ctx.r[10].u64 = ctx.r[10].u64 | 32;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF4784:
	// cmpwi cr6,r11,84
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 84, ctx.xer);
	// beq cr6,0x82df47f4
	if (ctx.cr6.eq) goto loc_82DF47F4;
	// cmpwi cr6,r11,98
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 98, ctx.xer);
	// beq cr6,0x82df47e4
	if (ctx.cr6.eq) goto loc_82DF47E4;
	// cmpwi cr6,r11,99
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 99, ctx.xer);
	// beq cr6,0x82df47d0
	if (ctx.cr6.eq) goto loc_82DF47D0;
	// cmpwi cr6,r11,110
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 110, ctx.xer);
	// beq cr6,0x82df47bc
	if (ctx.cr6.eq) goto loc_82DF47BC;
	// cmpwi cr6,r11,116
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 116, ctx.xer);
	// bne cr6,0x82df468c
	if (!ctx.cr6.eq) goto loc_82DF468C;
	// rlwinm. r11,r10,0,16,17
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xC000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df47fc
	if (!ctx.cr0.eq) goto loc_82DF47FC;
	// ori r10,r10,16384
	ctx.r[10].u64 = ctx.r[10].u64 | 16384;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF47BC:
	// cmpwi cr6,r7,0
	ctx.cr6.compare<int32_t>(ctx.r[7].s32, 0, ctx.xer);
	// bne cr6,0x82df47fc
	if (!ctx.cr6.eq) goto loc_82DF47FC;
	// li r7,1
	ctx.r[7].s64 = 1;
	// rlwinm r31,r31,0,18,16
	ctx.r[31].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 0) & 0xFFFFFFFFFFFFBFFF;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF47D0:
	// cmpwi cr6,r7,0
	ctx.cr6.compare<int32_t>(ctx.r[7].s32, 0, ctx.xer);
	// bne cr6,0x82df47fc
	if (!ctx.cr6.eq) goto loc_82DF47FC;
	// li r7,1
	ctx.r[7].s64 = 1;
	// ori r31,r31,16384
	ctx.r[31].u64 = ctx.r[31].u64 | 16384;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF47E4:
	// rlwinm. r11,r10,0,16,17
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xC000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df47fc
	if (!ctx.cr0.eq) goto loc_82DF47FC;
	// ori r10,r10,32768
	ctx.r[10].u64 = ctx.r[10].u64 | 32768;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF47F4:
	// rlwinm. r11,r10,0,19,19
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x1000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df4804
	if (ctx.cr0.eq) goto loc_82DF4804;
loc_82DF47FC:
	// mr r9,r29
	ctx.r[9].u64 = ctx.r[29].u64;
	// b 0x82df4808
	goto loc_82DF4808;
loc_82DF4804:
	// ori r10,r10,4096
	ctx.r[10].u64 = ctx.r[10].u64 | 4096;
loc_82DF4808:
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
loc_82DF480C:
	// lbz r11,0(r8)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[8].u32 + 0);
	// extsb. r11,r11
	ctx.r[11].s64 = ctx.r[11].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df46e4
	if (!ctx.cr0.eq) goto loc_82DF46E4;
	// b 0x82df4820
	goto loc_82DF4820;
loc_82DF481C:
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
loc_82DF4820:
	// lbz r11,0(r8)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[8].u32 + 0);
	// cmplwi cr6,r11,32
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 32, ctx.xer);
	// beq cr6,0x82df481c
	if (ctx.cr6.eq) goto loc_82DF481C;
	// clrlwi r11,r11,24
	ctx.r[11].u64 = ctx.r[11].u32 & 0xFF;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82df468c
	if (!ctx.cr6.eq) goto loc_82DF468C;
	// mr r6,r5
	ctx.r[6].u64 = ctx.r[5].u64;
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// li r7,384
	ctx.r[7].s64 = 384;
	// mr r5,r10
	ctx.r[5].u64 = ctx.r[10].u64;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// bl 0x82df6908
	ctx.lr = 0x82DF4850;
	sub_82DF6908(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df46b4
	if (!ctx.cr0.eq) goto loc_82DF46B4;
	// lis r11,-31955
	ctx.r[11].s64 = -2094202880;
	// lwz r9,80(r1)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// lwz r10,15032(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 15032);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// stw r10,15032(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 15032, ctx.r[10].u32);
	// stw r31,12(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 12, ctx.r[31].u32);
	// stw r29,4(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 4, ctx.r[29].u32);
	// stw r29,0(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 0, ctx.r[29].u32);
	// stw r29,8(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 8, ctx.r[29].u32);
	// stw r29,28(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 28, ctx.r[29].u32);
	// stw r9,16(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 16, ctx.r[9].u32);
loc_82DF4888:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}

void Body_82DF2118(Context& ctx, Base& base)
{
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-160
	ctx.r[31].s64 = ctx.r[12].s64 + -160;
	// std r28,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[28].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// bl 0x82b7b7c8
	ctx.lr = 0x82DF2138;
	sub_82B7B7C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r28,-16(r1)
	ctx.r[28].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// lwz r12,-24(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -24);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82DF4A30(Context& ctx, Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r3,1
	ctx.r[3].s64 = 1;
	// bl 0x82b819c8
	ctx.lr = 0x82DF4A44;
	sub_82B819C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82DF6908(Context& ctx, Base& base)
{
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// mr r3,r4
	ctx.r[3].u64 = ctx.r[4].u64;
	// mr r4,r5
	ctx.r[4].u64 = ctx.r[5].u64;
	// mr r5,r6
	ctx.r[5].u64 = ctx.r[6].u64;
	// mr r6,r7
	ctx.r[6].u64 = ctx.r[7].u64;
	// li r8,1
	ctx.r[8].s64 = 1;
	// mr r7,r11
	ctx.r[7].u64 = ctx.r[11].u64;
	// b 0x82df6760
	sub_82DF6760(ctx, base);
	return;
}


#undef PPC_STORE_U64
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_LOAD_U32
#undef PPC_LOAD_U8
#undef __builtin_clz
#undef __builtin_rotateleft32
#undef __builtin_rotateleft64
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82df2150u: case 0x82df1fc0u: case 0x82df4898u:
    case 0x82df3030u: case 0x82df4638u: case 0x82df2118u:
    case 0x82df4a30u: case 0x82df6908u: break;
    default: return false;
    }
    Context ctx{};
    FromFull(ctx, state);
    Base base{memory, dependencies, state};
    Direct(entry, ctx, base);
    ToFull(state, ctx);
    return true;
}

bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82df6760u: case 0x82b7fd78u: case 0x82b7fec0u:
    case 0x82b81b28u: case 0x82b819e8u: case 0x82b7b778u:
    case 0x82b7b810u: case 0x823acbd0u: case 0x82b821b0u:
    case 0x823addc0u: case 0x82b819c8u: case 0x82b7b7c8u: break;
    default: return false;
    }
    Context ctx{};
    FromFull(ctx, state);
    Base base{memory, dependencies, state};
    Direct(entry, ctx, base);
    ToFull(state, ctx);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_close_shared_lower
