#include "lo_semantics/crt_stream_close_reopen_context.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_format_stream.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_close_reopen_context
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
void __savegprlr_28(Context& c, Base& b) { Save(28, c, b); }
void __savegprlr_29(Context& c, Base& b) { Save(29, c, b); }
void __restgprlr_28(Context& c, Base& b) { Restore(28, c, b); }
void __restgprlr_29(Context& c, Base& b) { Restore(29, c, b); }

void Body_82DF1EA8(Context&,Base&);
void Body_82DF1DD0(Context&,Base&);
void Body_82DF1F7C(Context&,Base&);

void Accepted(GuestAddress entry,Base& base)
{
    if(entry==0x82df1aa0u||entry==0x82df44a8u)
    {
        if(!crt_stream_close_file_lower::Apply(entry,base.memory,
                base.dependencies,base.full))
            throw std::logic_error("missing accepted close file body");
        return;
    }
    if(entry==0x82b7b708u)
    {
        auto lower=crt_context_adapter::ToStream(base.full);
        if(!crt_stream_bulk_close_routes::Apply(entry,base.memory,
                base.dependencies.close,lower))
            throw std::logic_error("missing accepted record lock body");
        crt_context_adapter::FromStream(base.full,lower);
        return;
    }
    if(entry==0x82b7b838u)
    {
        auto lower=crt_context_adapter::ToStream(base.full);
        if(!crt_format_stream::Apply(entry,base.memory,
                base.dependencies.close.pipeline.format,lower))
            throw std::logic_error("missing accepted record format body");
        crt_context_adapter::FromStream(base.full,lower);
        return;
    }
    if(entry==0x82b7fd78u||entry==0x82b7fec0u||entry==0x82b7b7c8u)
    {
        if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
                base.memory,base.dependencies,base.full))
            throw std::logic_error("missing accepted shared close lower");
        return;
    }
    if(entry==0x82b81648u)
    {
        auto lower=crt_context_adapter::ToStream(base.full);
        if(!crt_stream_operations::ApplyAcceptedCallee(entry,base.memory,
                base.dependencies.close.pipeline.close.accepted,lower))
            throw std::logic_error("missing accepted record state body");
        crt_context_adapter::FromStream(base.full,lower);
        return;
    }
    throw std::logic_error("unselected close reopen direct callee");
}
void Direct(GuestAddress entry,Context& ctx,Base& base)
{
    ToFull(base.full,ctx);
    switch(entry)
    {
    case 0x82df1ea8u:Body_82DF1EA8(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df1dd0u:Body_82DF1DD0(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df1f7cu:Body_82DF1F7C(ctx,base);ToFull(base.full,ctx);break;
    default:Accepted(entry,base);break;
    }
    FromFull(ctx,base.full);
}
void sub_82DF1DD0(Context& c,Base& b) {Direct(0x82df1dd0u,c,b);}
void sub_82DF1F7C(Context& c,Base& b) {Direct(0x82df1f7cu,c,b);}
void sub_82DF1AA0(Context& c,Base& b) {Direct(0x82df1aa0u,c,b);}
void sub_82DF44A8(Context& c,Base& b) {Direct(0x82df44a8u,c,b);}
void sub_82B7FD78(Context& c,Base& b) {Direct(0x82b7fd78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b) {Direct(0x82b7fec0u,c,b);}
void sub_82B7B708(Context& c,Base& b) {Direct(0x82b7b708u,c,b);}
void sub_82B7B838(Context& c,Base& b) {Direct(0x82b7b838u,c,b);}
void sub_82B7B7C8(Context& c,Base& b) {Direct(0x82b7b7c8u,c,b);}
void sub_82B81648(Context& c,Base& b) {Direct(0x82b81648u,c,b);}

uint64_t RotateLeft64(uint64_t v,int n) {return std::rotl(v,n);}
#define __builtin_rotateleft64(v,n) RotateLeft64((v),(n))
#define __builtin_clz(v) std::countl_zero(uint32_t(v))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),uint32_t(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),uint64_t(v))

void Body_82DF1EA8(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x82DF1EB0;
	__savegprlr_28(ctx, base);
	// addi r31,r1,-128
	ctx.r[31].s64 = ctx.r[1].s64 + -128;
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// stw r30,148(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 148, ctx.r[30].u32);
	// mr r28,r4
	ctx.r[28].u64 = ctx.r[4].u64;
	// mr r29,r5
	ctx.r[29].u64 = ctx.r[5].u64;
	// cntlzw r11,r30
	ctx.r[11].u64 = ctx.r[30].u32 == 0 ? 32 : __builtin_clz(ctx.r[30].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df1f0c
	if (!ctx.cr0.eq) goto loc_82DF1F0C;
loc_82DF1EDC:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF1EE0;
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
	ctx.lr = 0x82DF1F04;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df1f54
	goto loc_82DF1F54;
loc_82DF1F0C:
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// beq cr6,0x82df1f24
	if (ctx.cr6.eq) goto loc_82DF1F24;
	// cmpwi cr6,r29,1
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 1, ctx.xer);
	// beq cr6,0x82df1f24
	if (ctx.cr6.eq) goto loc_82DF1F24;
	// cmpwi cr6,r29,2
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 2, ctx.xer);
	// bne cr6,0x82df1edc
	if (!ctx.cr6.eq) goto loc_82DF1EDC;
loc_82DF1F24:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b708
	ctx.lr = 0x82DF1F2C;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// mr r5,r29
	ctx.r[5].u64 = ctx.r[29].u64;
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df1dd0
	ctx.lr = 0x82DF1F40;
	sub_82DF1DD0(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,128
	ctx.r[12].s64 = ctx.r[31].s64 + 128;
	// bl 0x82df1f7c
	ctx.lr = 0x82DF1F50;
	sub_82DF1F7C(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82DF1F54:
	// addi r1,r31,128
	ctx.r[1].s64 = ctx.r[31].s64 + 128;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}

void Body_82DF1DD0(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82DF1DD8;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r29,r4
	ctx.r[29].u64 = ctx.r[4].u64;
	// mr r30,r5
	ctx.r[30].u64 = ctx.r[5].u64;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// andi. r10,r11,131
	ctx.r[10].u64 = ctx.r[11].u64 & 131;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82df1e10
	if (!ctx.cr0.eq) goto loc_82DF1E10;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF1DFC;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,22
	ctx.r[10].s64 = 22;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// b 0x82df1e94
	goto loc_82DF1E94;
loc_82DF1E10:
	// rlwinm r11,r11,0,28,26
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFFFFFFFFEF;
	// cmpwi cr6,r30,1
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 1, ctx.xer);
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// bne cr6,0x82df1e30
	if (!ctx.cr6.eq) goto loc_82DF1E30;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82df1aa0
	ctx.lr = 0x82DF1E28;
	sub_82DF1AA0(ctx, base);
	// li r30,0
	ctx.r[30].s64 = 0;
	// add r29,r3,r29
	ctx.r[29].u64 = ctx.r[3].u64 + ctx.r[29].u64;
loc_82DF1E30:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b7b838
	ctx.lr = 0x82DF1E38;
	sub_82B7B838(ctx, base);
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r10,r11,0,24,24
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x80;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82df1e50
	if (ctx.cr0.eq) goto loc_82DF1E50;
	// rlwinm r11,r11,0,0,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFC;
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// b 0x82df1e70
	goto loc_82DF1E70;
loc_82DF1E50:
	// clrlwi. r10,r11,31
	ctx.r[10].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82df1e70
	if (ctx.cr0.eq) goto loc_82DF1E70;
	// rlwinm. r10,r11,0,28,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82df1e70
	if (ctx.cr0.eq) goto loc_82DF1E70;
	// rlwinm. r11,r11,0,21,21
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x400;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df1e70
	if (!ctx.cr0.eq) goto loc_82DF1E70;
	// li r11,512
	ctx.r[11].s64 = 512;
	// stw r11,24(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 24, ctx.r[11].u32);
loc_82DF1E70:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF1E78;
	sub_82B81648(ctx, base);
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x82df44a8
	ctx.lr = 0x82DF1E84;
	sub_82DF44A8(ctx, base);
	// addi r11,r3,1
	ctx.r[11].s64 = ctx.r[3].s64 + 1;
	// cntlzw r11,r11
	ctx.r[11].u64 = ctx.r[11].u32 == 0 ? 32 : __builtin_clz(ctx.r[11].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// neg r3,r11
	ctx.r[3].s64 = -ctx.r[11].s64;
loc_82DF1E94:
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}

void Body_82DF1F7C(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-128
	ctx.r[31].s64 = ctx.r[12].s64 + -128;
	// std r30,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[30].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b7c8
	ctx.lr = 0x82DF1F9C;
	sub_82B7B7C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r30,-16(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// lwz r12,-24(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -24);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}


#undef PPC_STORE_U64
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_LOAD_U32
#undef __builtin_clz
#undef __builtin_rotateleft64
} // namespace

bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& state)
{
    if(entry!=0x82df1ea8u&&entry!=0x82df1dd0u&&entry!=0x82df1f7cu)
        return false;
    Context ctx{};FromFull(ctx,state);
    Base base{memory,dependencies,state};
    Direct(entry,ctx,base);ToFull(state,ctx);
    return true;
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& state)
{
    switch(entry)
    {
    case 0x82df1aa0u:case 0x82df44a8u:case 0x82b7b708u:
    case 0x82b7b838u:case 0x82b7fd78u:case 0x82b7fec0u:
    case 0x82b7b7c8u:case 0x82b81648u:break;
    default:return false;
    }
    Context ctx{};FromFull(ctx,state);
    Base base{memory,dependencies,state};
    Direct(entry,ctx,base);ToFull(state,ctx);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_close_reopen_context
