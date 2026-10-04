#include "lo_semantics/crt_stream_close_file_lower.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_stream_open_pipeline.h"
#include "lo_semantics/crt_stream_pointer_unlock.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_close_file_lower
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
void __savegprlr_24(Context& c, Base& b) { Save(24, c, b); }
void __savegprlr_25(Context& c, Base& b) { Save(25, c, b); }
void __restgprlr_24(Context& c, Base& b) { Restore(24, c, b); }
void __restgprlr_25(Context& c, Base& b) { Restore(25, c, b); }

void Body_82DF1CD0(Context&, Base&);
void Body_82DF1AA0(Context&, Base&);
void Body_82DF44A8(Context&, Base&);
void Body_82DF1D94(Context&, Base&);
void Body_82DF4600(Context&, Base&);

void Accepted(GuestAddress entry, Base& base)
{
    if (entry == 0x82df43f0u)
    {
        if (!crt_stream_open_pipeline::ApplyLower(entry, base.memory,
                base.dependencies.open.open, base.full))
            throw std::logic_error("missing accepted open-position body");
        return;
    }
    if (entry == 0x82b863f0u)
    {
        auto& s = base.full;
        crt_stream_pointer_unlock::Registers x{s.r[1],s.lr,s.r[3],
            s.r[9],s.r[10],s.r[11],s.r[12],s.r[30],s.r[31],s.xer_ca};
        if (!crt_stream_pointer_unlock::Apply(entry, base.memory,
                base.dependencies.open.unlock, x))
            throw std::logic_error("missing accepted pointer unlock");
        s.r[1]=x.sp;s.lr=x.lr;s.r[3]=x.r3;s.r[9]=x.r9;
        s.r[10]=x.r10;s.r[11]=x.r11;s.r[12]=x.r12;
        s.r[30]=x.r30;s.r[31]=x.r31;s.xer_ca=x.xer_ca;
        return;
    }
    if (entry == 0x82b7b708u)
    {
        auto lower=crt_context_adapter::ToStream(base.full);
        if (!crt_stream_bulk_close_routes::Apply(entry,base.memory,
                base.dependencies.close,lower))
            throw std::logic_error("missing accepted record close body");
        crt_context_adapter::FromStream(base.full,lower);
        return;
    }
    if (entry == 0x82b7fd78u || entry == 0x82b7fec0u ||
        entry == 0x82b7b7c8u)
    {
        if (!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
                base.memory,base.dependencies,base.full))
            throw std::logic_error("missing accepted shared close lower");
        return;
    }
    auto lower=crt_context_adapter::ToStream(base.full);
    if (entry != 0x82b7fdb0u && entry != 0x82b81648u &&
        entry != 0x82b862f8u)
        throw std::logic_error("unselected close file direct callee");
    if (!crt_stream_operations::ApplyAcceptedCallee(entry,base.memory,
            base.dependencies.close.pipeline.close.accepted,lower))
        throw std::logic_error("missing accepted stream lower");
    crt_context_adapter::FromStream(base.full,lower);
}

void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full,ctx);
    switch (entry)
    {
    case 0x82df1cd0u:Body_82DF1CD0(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df1aa0u:Body_82DF1AA0(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df44a8u:Body_82DF44A8(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df1d94u:Body_82DF1D94(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df4600u:Body_82DF4600(ctx,base);ToFull(base.full,ctx);break;
    default:Accepted(entry,base);break;
    }
    FromFull(ctx,base.full);
}
void sub_82DF1AA0(Context& c,Base& b) {Direct(0x82df1aa0u,c,b);}
void sub_82DF44A8(Context& c,Base& b) {Direct(0x82df44a8u,c,b);}
void sub_82DF1D94(Context& c,Base& b) {Direct(0x82df1d94u,c,b);}
void sub_82DF4600(Context& c,Base& b) {Direct(0x82df4600u,c,b);}
void sub_82B7FD78(Context& c,Base& b) {Direct(0x82b7fd78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b) {Direct(0x82b7fec0u,c,b);}
void sub_82B7B708(Context& c,Base& b) {Direct(0x82b7b708u,c,b);}
void sub_82B7B7C8(Context& c,Base& b) {Direct(0x82b7b7c8u,c,b);}
void sub_82B7FDB0(Context& c,Base& b) {Direct(0x82b7fdb0u,c,b);}
void sub_82B81648(Context& c,Base& b) {Direct(0x82b81648u,c,b);}
void sub_82B862F8(Context& c,Base& b) {Direct(0x82b862f8u,c,b);}
void sub_82DF43F0(Context& c,Base& b) {Direct(0x82df43f0u,c,b);}
void sub_82B863F0(Context& c,Base& b) {Direct(0x82b863f0u,c,b);}

uint64_t RotateLeft64(uint64_t v,int n) {return std::rotl(v,n);}
uint32_t RotateLeft32(uint32_t v,int n) {return std::rotl(v,n);}
#define __builtin_rotateleft64(v,n) RotateLeft64((v),(n))
#define __builtin_rotateleft32(v,n) RotateLeft32((v),(n))
#define __builtin_clz(v) std::countl_zero(uint32_t(v))
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),uint32_t(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),uint64_t(v))

void Body_82DF1CD0(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r30,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[30].u64);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// addi r31,r1,-112
	ctx.r[31].s64 = ctx.r[1].s64 + -112;
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// stw r30,132(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 132, ctx.r[30].u32);
	// cntlzw r11,r30
	ctx.r[11].u64 = ctx.r[30].u32 == 0 ? 32 : __builtin_clz(ctx.r[30].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df1d34
	if (!ctx.cr0.eq) goto loc_82DF1D34;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF1D08;
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
	ctx.lr = 0x82DF1D2C;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df1d5c
	goto loc_82DF1D5C;
loc_82DF1D34:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b708
	ctx.lr = 0x82DF1D3C;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df1aa0
	ctx.lr = 0x82DF1D48;
	sub_82DF1AA0(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,112
	ctx.r[12].s64 = ctx.r[31].s64 + 112;
	// bl 0x82df1d94
	ctx.lr = 0x82DF1D58;
	sub_82DF1D94(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82DF1D5C:
	// addi r1,r31,112
	ctx.r[1].s64 = ctx.r[31].s64 + 112;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r30,-24(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -24);
	// ld r31,-16(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// blr
	return;
}

void Body_82DF1AA0(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6d8
	ctx.lr = 0x82DF1AA8;
	__savegprlr_24(ctx, base);
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r27,r3
	ctx.r[27].u64 = ctx.r[3].u64;
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// bne cr6,0x82df1ae8
	if (!ctx.cr6.eq) goto loc_82DF1AE8;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF1ABC;
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
	ctx.lr = 0x82DF1AE0;
	sub_82B7FEC0(ctx, base);
loc_82DF1AE0:
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df1cc0
	goto loc_82DF1CC0;
loc_82DF1AE8:
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF1AF0;
	sub_82B81648(ctx, base);
	// lwz r11,4(r27)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 4);
	// mr r28,r3
	ctx.r[28].u64 = ctx.r[3].u64;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bge cr6,0x82df1b08
	if (!ctx.cr6.lt) goto loc_82DF1B08;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,4(r27)
	PPC_STORE_U32(ctx.r[27].u32 + 4, ctx.r[11].u32);
loc_82DF1B08:
	// li r5,1
	ctx.r[5].s64 = 1;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// bl 0x82df44a8
	ctx.lr = 0x82DF1B18;
	sub_82DF44A8(ctx, base);
	// mr. r24,r3
	ctx.r[24].u64 = ctx.r[3].u64;
	ctx.cr0.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// blt 0x82df1ae0
	if (ctx.cr0.lt) goto loc_82DF1AE0;
	// lwz r7,12(r27)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 12);
	// andi. r11,r7,264
	ctx.r[11].u64 = ctx.r[7].u64 & 264;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df1b3c
	if (!ctx.cr0.eq) goto loc_82DF1B3C;
	// lwz r11,4(r27)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 4);
	// subf r3,r11,r24
	ctx.r[3].s64 = ctx.r[24].s64 - ctx.r[11].s64;
	// b 0x82df1cc0
	goto loc_82DF1CC0;
loc_82DF1B3C:
	// clrlwi. r11,r7,30
	ctx.r[11].u64 = ctx.r[7].u32 & 0x3;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// lwz r9,0(r27)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 0);
	// lwz r8,8(r27)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 8);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// subf r25,r8,r9
	ctx.r[25].s64 = ctx.r[9].s64 - ctx.r[8].s64;
	// addi r26,r11,-29312
	ctx.r[26].s64 = ctx.r[11].s64 + -29312;
	// beq 0x82df1bb4
	if (ctx.cr0.eq) goto loc_82DF1BB4;
	// srawi r11,r28,5
	ctx.xer.ca = std::uint8_t(ctx.r[28].s32 < 0) &
		std::uint8_t((ctx.r[28].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[28].s32 >> 5;
	// rlwinm r10,r28,6,21,25
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 6) & 0x7C0;
	// rlwinm r11,r11,2,0,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r26
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + ctx.r[26].u32);
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// rlwinm. r11,r11,0,0,24
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFF80;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df1ba4
	if (ctx.cr0.eq) goto loc_82DF1BA4;
	// mr r11,r8
	ctx.r[11].u64 = ctx.r[8].u64;
	// cmplw cr6,r11,r9
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[9].u32, ctx.xer);
	// bge cr6,0x82df1ba4
	if (!ctx.cr6.lt) goto loc_82DF1BA4;
	// rotlwi r10,r9,0
	ctx.r[10].u64 = __builtin_rotateleft32(ctx.r[9].u32, 0);
loc_82DF1B88:
	// lbz r6,0(r11)
	ctx.r[6].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// cmplwi cr6,r6,10
	ctx.cr6.compare<uint32_t>(ctx.r[6].u32, 10, ctx.xer);
	// bne cr6,0x82df1b98
	if (!ctx.cr6.eq) goto loc_82DF1B98;
	// addi r25,r25,1
	ctx.r[25].s64 = ctx.r[25].s64 + 1;
loc_82DF1B98:
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// blt cr6,0x82df1b88
	if (ctx.cr6.lt) goto loc_82DF1B88;
loc_82DF1BA4:
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// bne cr6,0x82df1bd4
	if (!ctx.cr6.eq) goto loc_82DF1BD4;
	// mr r3,r25
	ctx.r[3].u64 = ctx.r[25].u64;
	// b 0x82df1cc0
	goto loc_82DF1CC0;
loc_82DF1BB4:
	// rlwinm. r11,r7,0,24,24
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[7].u32 | (ctx.r[7].u64 << 32), 0) & 0x80;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df1ba4
	if (!ctx.cr0.eq) goto loc_82DF1BA4;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF1BC0;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,22
	ctx.r[10].s64 = 22;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// b 0x82df1cc0
	goto loc_82DF1CC0;
loc_82DF1BD4:
	// clrlwi. r11,r7,31
	ctx.r[11].u64 = ctx.r[7].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df1cbc
	if (ctx.cr0.eq) goto loc_82DF1CBC;
	// lwz r10,4(r27)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 4);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82df1bf0
	if (!ctx.cr0.eq) goto loc_82DF1BF0;
	// li r25,0
	ctx.r[25].s64 = 0;
	// b 0x82df1cbc
	goto loc_82DF1CBC;
loc_82DF1BF0:
	// srawi r11,r28,5
	ctx.xer.ca = std::uint8_t(ctx.r[28].s32 < 0) &
		std::uint8_t((ctx.r[28].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[28].s32 >> 5;
	// rlwinm r30,r28,6,21,25
	ctx.r[30].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 6) & 0x7C0;
	// rlwinm r29,r11,2,0,29
	ctx.r[29].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// subf r11,r8,r9
	ctx.r[11].s64 = ctx.r[9].s64 - ctx.r[8].s64;
	// add r31,r11,r10
	ctx.r[31].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// lwzx r11,r29,r26
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[26].u32);
	// add r11,r11,r30
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[30].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// rlwinm. r11,r11,0,0,24
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFF80;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df1cb8
	if (ctx.cr0.eq) goto loc_82DF1CB8;
	// li r5,2
	ctx.r[5].s64 = 2;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// bl 0x82df44a8
	ctx.lr = 0x82DF1C28;
	sub_82DF44A8(ctx, base);
	// cmpw cr6,r3,r24
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, ctx.r[24].s32, ctx.xer);
	// bne cr6,0x82df1c64
	if (!ctx.cr6.eq) goto loc_82DF1C64;
	// lwz r11,8(r27)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 8);
	// add r10,r11,r31
	ctx.r[10].u64 = ctx.r[11].u64 + ctx.r[31].u64;
	// b 0x82df1c50
	goto loc_82DF1C50;
loc_82DF1C3C:
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// cmplwi cr6,r9,10
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 10, ctx.xer);
	// bne cr6,0x82df1c4c
	if (!ctx.cr6.eq) goto loc_82DF1C4C;
	// addi r31,r31,1
	ctx.r[31].s64 = ctx.r[31].s64 + 1;
loc_82DF1C4C:
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
loc_82DF1C50:
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// blt cr6,0x82df1c3c
	if (ctx.cr6.lt) goto loc_82DF1C3C;
	// lwz r11,12(r27)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 12);
	// rlwinm. r11,r11,0,18,18
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// b 0x82df1cb0
	goto loc_82DF1CB0;
loc_82DF1C64:
	// li r5,0
	ctx.r[5].s64 = 0;
	// mr r4,r24
	ctx.r[4].u64 = ctx.r[24].u64;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// bl 0x82df44a8
	ctx.lr = 0x82DF1C74;
	sub_82DF44A8(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// blt 0x82df1ae0
	if (ctx.cr0.lt) goto loc_82DF1AE0;
	// cmplwi cr6,r31,512
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 512, ctx.xer);
	// bgt cr6,0x82df1c9c
	if (ctx.cr6.gt) goto loc_82DF1C9C;
	// lwz r11,12(r27)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 12);
	// rlwinm. r10,r11,0,28,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82df1c9c
	if (ctx.cr0.eq) goto loc_82DF1C9C;
	// rlwinm. r11,r11,0,21,21
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x400;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// li r31,512
	ctx.r[31].s64 = 512;
	// beq 0x82df1ca0
	if (ctx.cr0.eq) goto loc_82DF1CA0;
loc_82DF1C9C:
	// lwz r31,24(r27)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 24);
loc_82DF1CA0:
	// lwzx r11,r29,r26
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[26].u32);
	// add r11,r11,r30
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[30].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// rlwinm. r11,r11,0,29,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
loc_82DF1CB0:
	// beq 0x82df1cb8
	if (ctx.cr0.eq) goto loc_82DF1CB8;
	// addi r31,r31,1
	ctx.r[31].s64 = ctx.r[31].s64 + 1;
loc_82DF1CB8:
	// subf r24,r31,r24
	ctx.r[24].s64 = ctx.r[24].s64 - ctx.r[31].s64;
loc_82DF1CBC:
	// add r3,r25,r24
	ctx.r[3].u64 = ctx.r[25].u64 + ctx.r[24].u64;
loc_82DF1CC0:
	// addi r1,r1,160
	ctx.r[1].s64 = ctx.r[1].s64 + 160;
	// b 0x82b7a728
	__restgprlr_24(ctx, base);
	return;
}

void Body_82DF44A8(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x82DF44B0;
	__savegprlr_25(ctx, base);
	// addi r31,r1,-160
	ctx.r[31].s64 = ctx.r[1].s64 + -160;
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// stw r30,180(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 180, ctx.r[30].u32);
	// mr r26,r4
	ctx.r[26].u64 = ctx.r[4].u64;
	// mr r25,r5
	ctx.r[25].u64 = ctx.r[5].u64;
	// cmpwi cr6,r30,-2
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, -2, ctx.xer);
	// bne cr6,0x82df44f4
	if (!ctx.cr6.eq) goto loc_82DF44F4;
	// bl 0x82b7fdb0
	ctx.lr = 0x82DF44D4;
	sub_82B7FDB0(ctx, base);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fd78
	ctx.lr = 0x82DF44E0;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,9
	ctx.r[10].s64 = 9;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// b 0x82df45d8
	goto loc_82DF45D8;
loc_82DF44F4:
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// blt cr6,0x82df450c
	if (ctx.cr6.lt) goto loc_82DF450C;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// lwz r11,-29336(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -29336);
	// cmplw cr6,r30,r11
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x82df4548
	if (ctx.cr6.lt) goto loc_82DF4548;
loc_82DF450C:
	// bl 0x82b7fdb0
	ctx.lr = 0x82DF4510;
	sub_82B7FDB0(ctx, base);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fd78
	ctx.lr = 0x82DF451C;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,9
	ctx.r[10].s64 = 9;
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
	ctx.lr = 0x82DF4540;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df45d8
	goto loc_82DF45D8;
loc_82DF4548:
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
	// srawi r11,r30,5
	ctx.xer.ca = std::uint8_t(ctx.r[30].s32 < 0) &
		std::uint8_t((ctx.r[30].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[30].s32 >> 5;
	// rlwinm r27,r11,2,0,29
	ctx.r[27].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r28,r30,6,21,25
	ctx.r[28].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 6) & 0x7C0;
	// lwzx r11,r27,r29
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + ctx.r[29].u32);
	// add r11,r11,r28
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[28].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df450c
	if (ctx.cr0.eq) goto loc_82DF450C;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b862f8
	ctx.lr = 0x82DF4578;
	sub_82B862F8(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwzx r11,r27,r29
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + ctx.r[29].u32);
	// add r11,r11,r28
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[28].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df45a8
	if (ctx.cr0.eq) goto loc_82DF45A8;
	// mr r5,r25
	ctx.r[5].u64 = ctx.r[25].u64;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df43f0
	ctx.lr = 0x82DF45A0;
	sub_82DF43F0(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// b 0x82df45c8
	goto loc_82DF45C8;
loc_82DF45A8:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF45AC;
	sub_82B7FD78(ctx, base);
	// li r11,9
	ctx.r[11].s64 = 9;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fdb0
	ctx.lr = 0x82DF45B8;
	sub_82B7FDB0(ctx, base);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// li r11,-1
	ctx.r[11].s64 = -1;
	// stw r11,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[11].u32);
loc_82DF45C8:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,160
	ctx.r[12].s64 = ctx.r[31].s64 + 160;
	// bl 0x82df4600
	ctx.lr = 0x82DF45D4;
	sub_82DF4600(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82DF45D8:
	// addi r1,r31,160
	ctx.r[1].s64 = ctx.r[31].s64 + 160;
	// b 0x82b7a72c
	__restgprlr_25(ctx, base);
	return;
}

void Body_82DF1D94(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-112
	ctx.r[31].s64 = ctx.r[12].s64 + -112;
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
	ctx.lr = 0x82DF1DB4;
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

void Body_82DF4600(Context& ctx,Base& base)
{
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-160
	ctx.r[31].s64 = ctx.r[12].s64 + -160;
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
	// bl 0x82b863f0
	ctx.lr = 0x82DF4620;
	sub_82B863F0(ctx, base);
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
    case 0x82df1cd0u:case 0x82df1aa0u:case 0x82df44a8u:
    case 0x82df1d94u:case 0x82df4600u:break;
    default:return false;
    }
    Context ctx{};FromFull(ctx,state);
    Base base{memory,dependencies,state};
    Direct(entry,ctx,base);ToFull(state,ctx);
    return true;
}
bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82df43f0u:case 0x82b863f0u:case 0x82b7b708u:
    case 0x82b7fd78u:case 0x82b7fec0u:case 0x82b7b7c8u:
    case 0x82b7fdb0u:case 0x82b81648u:case 0x82b862f8u:break;
    default:return false;
    }
    Context ctx{};FromFull(ctx,state);
    Base base{memory,dependencies,state};
    Direct(entry,ctx,base);ToFull(state,ctx);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_close_file_lower
