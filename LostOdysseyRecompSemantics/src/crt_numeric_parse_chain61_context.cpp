#include "lo_semantics/crt_numeric_parse_chain61_context.h"
#include "lo_semantics/crt_numeric61_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_numeric_parse_chain61_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };

void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    if (!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
            base.memory, base.dependencies, base.full))
        throw std::logic_error("missing accepted scan-adjacent lower");
    FromFull(ctx, base.full);
}
void sub_82B7FD78(Context& ctx, Base& base)
{ Direct(0x82b7fd78u, ctx, base); }
void sub_82B7FEC0(Context& ctx, Base& base)
{ Direct(0x82b7fec0u, ctx, base); }

#define PPC_LOAD_U16(a) base.memory.ReadU16(Address(a))
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory, Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a), (v))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a), (v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory, Address(a), (v))

void __savegprlr_23(Context& c, Base& b) { Save(23,c,b); }
void __restgprlr_23(Context& c, Base& b) { Restore(23,c,b); }
void __savegprlr_28(Context& c, Base& b) { Save(28,c,b); }
void __restgprlr_28(Context& c, Base& b) { Restore(28,c,b); }
void Numeric(GuestAddress entry,Context& c,Base& b)
{
    ToFull(b.full,c);
    if(!crt_numeric61_context::Apply(entry,b.memory,b.dependencies,b.full))
        throw std::logic_error("missing accepted numeric tail");
    FromFull(c,b.full);
}
void sub_82DF6098(Context& c,Base& b) { Numeric(0x82df6098u,c,b); }
void sub_82DF5FB0(Context& c,Base& b) { Numeric(0x82df5fb0u,c,b); }
void sub_82B7FF08(Context& c,Base& b)
{
    ToFull(b.full,c);
    if(!crt_stream_open_pipeline::ApplyLower(0x82b7ff08u,b.memory,
        b.dependencies.open.open,b.full)) throw std::logic_error("missing accepted invalid parameter");
    FromFull(c,b.full);
}
void Body_8231B0D0(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x8231b0f0
	if (ctx.cr6.eq) goto loc_8231B0F0;
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 0, ctx.xer);
	// bne cr6,0x8231b120
	if (!ctx.cr6.eq) goto loc_8231B120;
loc_8231B0F0:
	// bl 0x82b7fd78
	ctx.lr = 0x8231B0F4;
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
	ctx.lr = 0x8231B118;
	sub_82B7FEC0(ctx, base);
	// li r3,22
	ctx.r[3].s64 = 22;
	// b 0x8231b1a0
	goto loc_8231B1A0;
loc_8231B120:
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, 0, ctx.xer);
	// bne cr6,0x8231b15c
	if (!ctx.cr6.eq) goto loc_8231B15C;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// bl 0x82b7fd78
	ctx.lr = 0x8231B134;
	sub_82B7FD78(ctx, base);
	// li r31,22
	ctx.r[31].s64 = 22;
loc_8231B138:
	// stw r31,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[31].u32);
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
	// bl 0x82b7fec0
	ctx.lr = 0x8231B154;
	sub_82B7FEC0(ctx, base);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// b 0x8231b1a0
	goto loc_8231B1A0;
loc_8231B15C:
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
loc_8231B160:
	// lbz r10,0(r5)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[5].u32 + 0);
	// addi r5,r5,1
	ctx.r[5].s64 = ctx.r[5].s64 + 1;
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// stb r10,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[10].u8);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// beq 0x8231b180
	if (ctx.cr0.eq) goto loc_8231B180;
	// addic. r4,r4,-1
	ctx.xer.ca = ctx.r[4].u32 > 0;
	ctx.r[4].s64 = ctx.r[4].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[4].s32, 0, ctx.xer);
	// bne 0x8231b160
	if (!ctx.cr0.eq) goto loc_8231B160;
loc_8231B180:
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 0, ctx.xer);
	// bne cr6,0x8231b19c
	if (!ctx.cr6.eq) goto loc_8231B19C;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// bl 0x82b7fd78
	ctx.lr = 0x8231B194;
	sub_82B7FD78(ctx, base);
	// li r31,34
	ctx.r[31].s64 = 34;
	// b 0x8231b138
	goto loc_8231B138;
loc_8231B19C:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_8231B1A0:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r31,-16(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// blr
	return;
}
void sub_8231B0D0(Context& c,Base& b) { Body_8231B0D0(c,b); }
void Body_82B834A0(Context& ctx, Base& base) {
	// addi r11,r3,1
	ctx.r[11].s64 = ctx.r[3].s64 + 1;
	// cmplwi cr6,r11,256
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 256, ctx.xer);
	// bgt cr6,0x82b834c8
	if (ctx.cr6.gt) goto loc_82B834C8;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// rlwinm r10,r3,1,0,30
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 1) & 0xFFFFFFFE;
	// lwz r11,21248(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 21248);
	// lwz r11,200(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 200);
	// lhzx r11,r11,r10
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[11].u32 + ctx.r[10].u32);
	// and r3,r11,r4
	ctx.r[3].u64 = ctx.r[11].u64 & ctx.r[4].u64;
	// blr
	return;
loc_82B834C8:
	// li r3,0
	ctx.r[3].s64 = 0;
	// blr
	return;
}
void sub_82B834A0(Context& c,Base& b) { Body_82B834A0(c,b); }
void Body_82B86BE8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6d4
	ctx.lr = 0x82B86BF0;
	__savegprlr_23(ctx, base);
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r23,r5
	ctx.r[23].u64 = ctx.r[5].u64;
	// mr r25,r4
	ctx.r[25].u64 = ctx.r[4].u64;
	// mr r28,r6
	ctx.r[28].u64 = ctx.r[6].u64;
	// mr r24,r7
	ctx.r[24].u64 = ctx.r[7].u64;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x82b86c10
	if (ctx.cr6.eq) goto loc_82B86C10;
	// stw r25,0(r23)
	PPC_STORE_U32(ctx.r[23].u32 + 0, ctx.r[25].u32);
loc_82B86C10:
	// cmplwi cr6,r25,0
	ctx.cr6.compare<uint32_t>(ctx.r[25].u32, 0, ctx.xer);
	// bne cr6,0x82b86c44
	if (!ctx.cr6.eq) goto loc_82B86C44;
loc_82B86C18:
	// bl 0x82b7fd78
	ctx.lr = 0x82B86C1C;
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
	ctx.lr = 0x82B86C40;
	sub_82B7FEC0(ctx, base);
	// b 0x82b86ed0
	goto loc_82B86ED0;
loc_82B86C44:
	// cmpwi cr6,r28,0
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 0, ctx.xer);
	// beq cr6,0x82b86c5c
	if (ctx.cr6.eq) goto loc_82B86C5C;
	// cmpwi cr6,r28,2
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 2, ctx.xer);
	// blt cr6,0x82b86c18
	if (ctx.cr6.lt) goto loc_82B86C18;
	// cmpwi cr6,r28,36
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 36, ctx.xer);
	// bgt cr6,0x82b86c18
	if (ctx.cr6.gt) goto loc_82B86C18;
loc_82B86C5C:
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// lbz r31,0(r25)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[25].u32 + 0);
	// li r26,0
	ctx.r[26].s64 = 0;
	// addi r30,r11,21248
	ctx.r[30].s64 = ctx.r[11].s64 + 21248;
	// addi r29,r25,1
	ctx.r[29].s64 = ctx.r[25].s64 + 1;
	// lwz r10,0(r30)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
loc_82B86C74:
	// lwz r11,172(r10)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 172);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 1, ctx.xer);
	// ble cr6,0x82b86c98
	if (!ctx.cr6.gt) goto loc_82B86C98;
	// li r4,8
	ctx.r[4].s64 = 8;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// clrlwi r3,r31,24
	ctx.r[3].u64 = ctx.r[31].u32 & 0xFF;
	// bl 0x82b834a0
	ctx.lr = 0x82B86C90;
	sub_82B834A0(ctx, base);
	// lwz r10,0(r30)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// b 0x82b86ca8
	goto loc_82B86CA8;
loc_82B86C98:
	// lwz r11,200(r10)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 200);
	// rlwinm r9,r31,1,23,30
	ctx.r[9].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 1) & 0x1FE;
	// lhzx r11,r9,r11
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[9].u32 + ctx.r[11].u32);
	// rlwinm r3,r11,0,28,28
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x8;
loc_82B86CA8:
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x82b86cbc
	if (ctx.cr6.eq) goto loc_82B86CBC;
	// lbz r31,0(r29)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// addi r29,r29,1
	ctx.r[29].s64 = ctx.r[29].s64 + 1;
	// b 0x82b86c74
	goto loc_82B86C74;
loc_82B86CBC:
	// extsb r11,r31
	ctx.r[11].s64 = ctx.r[31].s8;
	// cmpwi cr6,r11,45
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 45, ctx.xer);
	// bne cr6,0x82b86cd0
	if (!ctx.cr6.eq) goto loc_82B86CD0;
	// ori r24,r24,2
	ctx.r[24].u64 = ctx.r[24].u64 | 2;
	// b 0x82b86cd8
	goto loc_82B86CD8;
loc_82B86CD0:
	// cmpwi cr6,r11,43
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 43, ctx.xer);
	// bne cr6,0x82b86ce0
	if (!ctx.cr6.eq) goto loc_82B86CE0;
loc_82B86CD8:
	// lbz r31,0(r29)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// addi r29,r29,1
	ctx.r[29].s64 = ctx.r[29].s64 + 1;
loc_82B86CE0:
	// cmpwi cr6,r28,0
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 0, ctx.xer);
	// blt cr6,0x82b86ec4
	if (ctx.cr6.lt) goto loc_82B86EC4;
	// cmpwi cr6,r28,1
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 1, ctx.xer);
	// beq cr6,0x82b86ec4
	if (ctx.cr6.eq) goto loc_82B86EC4;
	// cmpwi cr6,r28,36
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 36, ctx.xer);
	// bgt cr6,0x82b86ec4
	if (ctx.cr6.gt) goto loc_82B86EC4;
	// cmpwi cr6,r28,0
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 0, ctx.xer);
	// bne cr6,0x82b86d3c
	if (!ctx.cr6.eq) goto loc_82B86D3C;
	// extsb r11,r31
	ctx.r[11].s64 = ctx.r[31].s8;
	// cmpwi cr6,r11,48
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 48, ctx.xer);
	// beq cr6,0x82b86d14
	if (ctx.cr6.eq) goto loc_82B86D14;
	// li r28,10
	ctx.r[28].s64 = 10;
	// b 0x82b86d74
	goto loc_82B86D74;
loc_82B86D14:
	// lbz r11,0(r29)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// extsb r11,r11
	ctx.r[11].s64 = ctx.r[11].s8;
	// cmpwi cr6,r11,120
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 120, ctx.xer);
	// beq cr6,0x82b86d34
	if (ctx.cr6.eq) goto loc_82B86D34;
	// cmpwi cr6,r11,88
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 88, ctx.xer);
	// beq cr6,0x82b86d34
	if (ctx.cr6.eq) goto loc_82B86D34;
	// li r28,8
	ctx.r[28].s64 = 8;
	// b 0x82b86d74
	goto loc_82B86D74;
loc_82B86D34:
	// li r28,16
	ctx.r[28].s64 = 16;
	// b 0x82b86d44
	goto loc_82B86D44;
loc_82B86D3C:
	// cmpwi cr6,r28,16
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 16, ctx.xer);
	// bne cr6,0x82b86d74
	if (!ctx.cr6.eq) goto loc_82B86D74;
loc_82B86D44:
	// extsb r11,r31
	ctx.r[11].s64 = ctx.r[31].s8;
	// cmpwi cr6,r11,48
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 48, ctx.xer);
	// bne cr6,0x82b86d74
	if (!ctx.cr6.eq) goto loc_82B86D74;
	// lbz r11,0(r29)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// extsb r11,r11
	ctx.r[11].s64 = ctx.r[11].s8;
	// cmpwi cr6,r11,120
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 120, ctx.xer);
	// beq cr6,0x82b86d68
	if (ctx.cr6.eq) goto loc_82B86D68;
	// cmpwi cr6,r11,88
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 88, ctx.xer);
	// bne cr6,0x82b86d74
	if (!ctx.cr6.eq) goto loc_82B86D74;
loc_82B86D68:
	// addi r11,r29,1
	ctx.r[11].s64 = ctx.r[29].s64 + 1;
	// addi r29,r11,1
	ctx.r[29].s64 = ctx.r[11].s64 + 1;
	// lbz r31,0(r11)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
loc_82B86D74:
	// li r27,-1
	ctx.r[27].s64 = -1;
	// lwz r8,200(r10)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 200);
	// twllei r28,0
	// divwu r9,r27,r28
	ctx.r[9].u32 = ctx.r[27].u32 / ctx.r[28].u32;
loc_82B86D84:
	// rlwinm r11,r31,1,23,30
	ctx.r[11].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 1) & 0x1FE;
	// lhzx r11,r11,r8
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[11].u32 + ctx.r[8].u32);
	// rlwinm. r10,r11,0,29,29
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b86da0
	if (ctx.cr0.eq) goto loc_82B86DA0;
	// extsb r11,r31
	ctx.r[11].s64 = ctx.r[31].s8;
	// addi r11,r11,-48
	ctx.r[11].s64 = ctx.r[11].s64 + -48;
	// b 0x82b86dc8
	goto loc_82B86DC8;
loc_82B86DA0:
	// andi. r11,r11,259
	ctx.r[11].u64 = ctx.r[11].u64 & 259;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b86e08
	if (ctx.cr0.eq) goto loc_82B86E08;
	// extsb r11,r31
	ctx.r[11].s64 = ctx.r[31].s8;
	// cmpwi cr6,r11,97
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 97, ctx.xer);
	// blt cr6,0x82b86dc4
	if (ctx.cr6.lt) goto loc_82B86DC4;
	// cmpwi cr6,r11,122
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 122, ctx.xer);
	// bgt cr6,0x82b86dc4
	if (ctx.cr6.gt) goto loc_82B86DC4;
	// addi r11,r11,-32
	ctx.r[11].s64 = ctx.r[11].s64 + -32;
loc_82B86DC4:
	// addi r11,r11,-55
	ctx.r[11].s64 = ctx.r[11].s64 + -55;
loc_82B86DC8:
	// cmplw cr6,r11,r28
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[28].u32, ctx.xer);
	// bge cr6,0x82b86e08
	if (!ctx.cr6.lt) goto loc_82B86E08;
	// ori r24,r24,8
	ctx.r[24].u64 = ctx.r[24].u64 | 8;
	// cmplw cr6,r26,r9
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, ctx.r[9].u32, ctx.xer);
	// blt cr6,0x82b86e28
	if (ctx.cr6.lt) goto loc_82B86E28;
	// bne cr6,0x82b86dfc
	if (!ctx.cr6.eq) goto loc_82B86DFC;
	// mr r10,r27
	ctx.r[10].u64 = ctx.r[27].u64;
	// twllei r28,0
	// divwu r7,r10,r28
	ctx.r[7].u32 = ctx.r[10].u32 / ctx.r[28].u32;
	// mullw r7,r7,r28
	ctx.r[7].s64 = int64_t(ctx.r[7].s32) * int64_t(ctx.r[28].s32);
	// subf r10,r7,r10
	ctx.r[10].s64 = ctx.r[10].s64 - ctx.r[7].s64;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// ble cr6,0x82b86e28
	if (!ctx.cr6.gt) goto loc_82B86E28;
loc_82B86DFC:
	// ori r24,r24,4
	ctx.r[24].u64 = ctx.r[24].u64 | 4;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// bne cr6,0x82b86e30
	if (!ctx.cr6.eq) goto loc_82B86E30;
loc_82B86E08:
	// rlwinm. r11,r24,0,28,28
	ctx.r[11].u64 = std::rotl(ctx.r[24].u32 | (ctx.r[24].u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// addi r29,r29,-1
	ctx.r[29].s64 = ctx.r[29].s64 + -1;
	// bne 0x82b86e3c
	if (!ctx.cr0.eq) goto loc_82B86E3C;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x82b86e20
	if (ctx.cr6.eq) goto loc_82B86E20;
	// mr r29,r25
	ctx.r[29].u64 = ctx.r[25].u64;
loc_82B86E20:
	// li r26,0
	ctx.r[26].s64 = 0;
	// b 0x82b86ea4
	goto loc_82B86EA4;
loc_82B86E28:
	// mullw r10,r26,r28
	ctx.r[10].s64 = int64_t(ctx.r[26].s32) * int64_t(ctx.r[28].s32);
	// add r26,r10,r11
	ctx.r[26].u64 = ctx.r[10].u64 + ctx.r[11].u64;
loc_82B86E30:
	// lbz r31,0(r29)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// addi r29,r29,1
	ctx.r[29].s64 = ctx.r[29].s64 + 1;
	// b 0x82b86d84
	goto loc_82B86D84;
loc_82B86E3C:
	// lis r10,32767
	ctx.r[10].s64 = 2147418112;
	// rlwinm. r11,r24,0,29,29
	ctx.r[11].u64 = std::rotl(ctx.r[24].u32 | (ctx.r[24].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// ori r31,r10,65535
	ctx.r[31].u64 = ctx.r[10].u64 | 65535;
	// lis r30,-32768
	ctx.r[30].s64 = -2147483648;
	// bne 0x82b86e78
	if (!ctx.cr0.eq) goto loc_82B86E78;
	// clrlwi. r11,r24,31
	ctx.r[11].u64 = ctx.r[24].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b86ea4
	if (!ctx.cr0.eq) goto loc_82B86EA4;
	// rlwinm. r11,r24,0,30,30
	ctx.r[11].u64 = std::rotl(ctx.r[24].u32 | (ctx.r[24].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b86e68
	if (ctx.cr0.eq) goto loc_82B86E68;
	// cmplw cr6,r26,r30
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, ctx.r[30].u32, ctx.xer);
	// bgt cr6,0x82b86e78
	if (ctx.cr6.gt) goto loc_82B86E78;
loc_82B86E68:
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82b86ea4
	if (!ctx.cr6.eq) goto loc_82B86EA4;
	// cmplw cr6,r26,r31
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, ctx.r[31].u32, ctx.xer);
	// ble cr6,0x82b86ea4
	if (!ctx.cr6.gt) goto loc_82B86EA4;
loc_82B86E78:
	// bl 0x82b7fd78
	ctx.lr = 0x82B86E7C;
	sub_82B7FD78(ctx, base);
	// li r11,34
	ctx.r[11].s64 = 34;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// clrlwi. r10,r24,31
	ctx.r[10].u64 = ctx.r[24].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b86e94
	if (ctx.cr0.eq) goto loc_82B86E94;
	// mr r26,r27
	ctx.r[26].u64 = ctx.r[27].u64;
	// b 0x82b86ea4
	goto loc_82B86EA4;
loc_82B86E94:
	// rlwinm. r11,r24,0,30,30
	ctx.r[11].u64 = std::rotl(ctx.r[24].u32 | (ctx.r[24].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// mr r26,r30
	ctx.r[26].u64 = ctx.r[30].u64;
	// bne 0x82b86ea4
	if (!ctx.cr0.eq) goto loc_82B86EA4;
	// mr r26,r31
	ctx.r[26].u64 = ctx.r[31].u64;
loc_82B86EA4:
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x82b86eb0
	if (ctx.cr6.eq) goto loc_82B86EB0;
	// stw r29,0(r23)
	PPC_STORE_U32(ctx.r[23].u32 + 0, ctx.r[29].u32);
loc_82B86EB0:
	// rlwinm. r11,r24,0,30,30
	ctx.r[11].u64 = std::rotl(ctx.r[24].u32 | (ctx.r[24].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b86ebc
	if (ctx.cr0.eq) goto loc_82B86EBC;
	// neg r26,r26
	ctx.r[26].s64 = -ctx.r[26].s64;
loc_82B86EBC:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// b 0x82b86ed4
	goto loc_82B86ED4;
loc_82B86EC4:
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x82b86ed0
	if (ctx.cr6.eq) goto loc_82B86ED0;
	// stw r25,0(r23)
	PPC_STORE_U32(ctx.r[23].u32 + 0, ctx.r[25].u32);
loc_82B86ED0:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82B86ED4:
	// addi r1,r1,160
	ctx.r[1].s64 = ctx.r[1].s64 + 160;
	// b 0x82b7a724
	__restgprlr_23(ctx, base);
	return;
}
void sub_82B86BE8(Context& c,Base& b) { Body_82B86BE8(c,b); }
void Body_82B86F00(Context& ctx, Base& base) {
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// lis r10,-31967
	ctx.r[10].s64 = -2094989312;
	// mr r6,r5
	ctx.r[6].u64 = ctx.r[5].u64;
	// mr r5,r4
	ctx.r[5].u64 = ctx.r[4].u64;
	// addi r3,r10,21248
	ctx.r[3].s64 = ctx.r[10].s64 + 21248;
	// li r7,1
	ctx.r[7].s64 = 1;
	// mr r4,r11
	ctx.r[4].u64 = ctx.r[11].u64;
	// b 0x82b86be8
	sub_82B86BE8(ctx, base);
	return;
}
void sub_82B86F00(Context& c,Base& b) { Body_82B86F00(c,b); }
void Body_82DF3DC0(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x82DF3DC8;
	__savegprlr_28(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r28,r4
	ctx.r[28].u64 = ctx.r[4].u64;
	// li r4,46
	ctx.r[4].s64 = 46;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// mr r29,r5
	ctx.r[29].u64 = ctx.r[5].u64;
	// bl 0x82df6098
	ctx.lr = 0x82DF3DE0;
	sub_82DF6098(ctx, base);
	// addi r31,r3,1
	ctx.r[31].s64 = ctx.r[3].s64 + 1;
	// li r5,32
	ctx.r[5].s64 = 32;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b86f00
	ctx.lr = 0x82DF3DF4;
	sub_82B86F00(ctx, base);
	// addi r3,r3,1
	ctx.r[3].s64 = ctx.r[3].s64 + 1;
	// cmplw cr6,r3,r29
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, ctx.r[29].u32, ctx.xer);
	// blt cr6,0x82df3e08
	if (ctx.cr6.lt) goto loc_82DF3E08;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df3e70
	goto loc_82DF3E70;
loc_82DF3E08:
	// li r6,32
	ctx.r[6].s64 = 32;
	// li r5,8
	ctx.r[5].s64 = 8;
	// addi r4,r1,80
	ctx.r[4].s64 = ctx.r[1].s64 + 80;
	// bl 0x82df5fb0
	ctx.lr = 0x82DF3E18;
	sub_82DF5FB0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df3e38
	if (ctx.cr0.eq) goto loc_82DF3E38;
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
	// bl 0x82b7ff08
	ctx.lr = 0x82DF3E38;
	sub_82B7FF08(ctx, base);
loc_82DF3E38:
	// subf r11,r31,r30
	ctx.r[11].s64 = ctx.r[30].s64 - ctx.r[31].s64;
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// add r4,r11,r28
	ctx.r[4].u64 = ctx.r[11].u64 + ctx.r[28].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x8231b0d0
	ctx.lr = 0x82DF3E4C;
	sub_8231B0D0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df3e6c
	if (ctx.cr0.eq) goto loc_82DF3E6C;
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
	// bl 0x82b7ff08
	ctx.lr = 0x82DF3E6C;
	sub_82B7FF08(ctx, base);
loc_82DF3E6C:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF3E70:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U16
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    if(entry!=0x82b86be8u&&entry!=0x82df3dc0u&&entry!=0x82b834a0u&&entry!=0x82b86f00u) return false;
    Context c{};FromFull(c,state);Base b{memory,dependencies,state};
    if(entry==0x82b86be8u) Body_82B86BE8(c,b);
    else if(entry==0x82df3dc0u) Body_82DF3DC0(c,b);
    else if(entry==0x82b834a0u) Body_82B834A0(c,b);
    else Body_82B86F00(c,b);
    ToFull(state,c);return true;
}
} // namespace lo::semantic::gpu::crt_numeric_parse_chain61_context
