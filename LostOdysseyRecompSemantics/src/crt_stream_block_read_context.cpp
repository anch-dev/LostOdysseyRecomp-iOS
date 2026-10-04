#include "lo_semantics/crt_stream_block_read_context.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_stream_refill_context.h"
#include "lo_semantics/crt_stream_byte_read_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::crt_stream_block_read_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory; Dependencies dependencies; Registers& full;};
void __savegprlr_20(Context& c,Base& b) { Save(20,c,b); }
void __restgprlr_20(Context& c,Base& b) { Restore(20,c,b); }
void __savegprlr_26(Context& c,Base& b) { Save(26,c,b); }
void __restgprlr_26(Context& c,Base& b) { Restore(26,c,b); }
void __savegprlr_29(Context& c,Base& b) { Save(29,c,b); }
void __restgprlr_29(Context& c,Base& b) { Restore(29,c,b); }
void Body_82DF2158(Context&,Base&);
void Body_82DF23F8(Context&,Base&);
void Body_82DF2520(Context&,Base&);
void Body_82DF4318(Context&,Base&);
void Body_82DF24E4(Context&,Base&);
void Body_82B7BC40(Context&,Base&);

void Direct(GuestAddress entry,Context& ctx,Base& base)
{
    ToFull(base.full,ctx);
    switch(entry)
    {
    case 0x82df2158u:Body_82DF2158(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df23f8u:Body_82DF23F8(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df4318u:Body_82DF4318(ctx,base);ToFull(base.full,ctx);break;
    case 0x82df24e4u:Body_82DF24E4(ctx,base);ToFull(base.full,ctx);break;
    case 0x82b7bc40u:Body_82B7BC40(ctx,base);ToFull(base.full,ctx);break;
    case 0x82b7a0b0u:
        if(!crt_copy_full_context::Apply(entry,base.memory,base.full))
            throw std::logic_error("missing accepted full copy");
        break;
    case 0x82b85a80u:
        if(!crt_stream_refill_context::Apply(entry,base.memory,base.dependencies,base.full))
            throw std::logic_error("missing refill lower");
        break;
    case 0x82b81360u:
        if(!crt_stream_byte_read_context::Apply(entry,base.memory,base.dependencies,base.full))
            throw std::logic_error("missing byte-read lower");
        break;
    default:
    {
        auto lower=crt_context_adapter::ToStream(base.full);
        bool found=false;
        if(entry==0x82b7b708u||entry==0x82b7b7c8u)
            found=crt_stream_bulk_close_routes::Apply(entry,base.memory,base.dependencies.close,lower);
        else
            found=crt_stream_operations::ApplyAcceptedCallee(entry,base.memory,base.dependencies.close.pipeline.close.accepted,lower);
        if(!found)throw std::logic_error("missing accepted block-read lower");
        crt_context_adapter::FromStream(base.full,lower);break;
    }
    }
    FromFull(ctx,base.full);
}
void sub_82B7A0B0(Context& c,Base& b) { Direct(0x82B7A0B0u,c,b); }
void sub_82B7B708(Context& c,Base& b) { Direct(0x82B7B708u,c,b); }
void sub_82B7B7C8(Context& c,Base& b) { Direct(0x82B7B7C8u,c,b); }
void sub_82B7BC40(Context& c,Base& b) { Direct(0x82B7BC40u,c,b); }
void sub_82B7FD78(Context& c,Base& b) { Direct(0x82B7FD78u,c,b); }
void sub_82B7FEC0(Context& c,Base& b) { Direct(0x82B7FEC0u,c,b); }
void sub_82B81360(Context& c,Base& b) { Direct(0x82B81360u,c,b); }
void sub_82B81648(Context& c,Base& b) { Direct(0x82B81648u,c,b); }
void sub_82B85A80(Context& c,Base& b) { Direct(0x82B85A80u,c,b); }
void sub_82DF2158(Context& c,Base& b) { Direct(0x82DF2158u,c,b); }
void sub_82DF23F8(Context& c,Base& b) { Direct(0x82DF23F8u,c,b); }
void sub_82DF24E4(Context& c,Base& b) { Direct(0x82DF24E4u,c,b); }
void sub_82DF4318(Context& c,Base& b) { Direct(0x82DF4318u,c,b); }

#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82DF2158(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6c8
	ctx.lr = 0x82DF2160;
	__savegprlr_20(ctx, base);
	// stwu r1,-192(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-192);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r20,r3
	ctx.r[20].u64 = ctx.r[3].u64;
	// mr r21,r4
	ctx.r[21].u64 = ctx.r[4].u64;
	// mr r24,r5
	ctx.r[24].u64 = ctx.r[5].u64;
	// mr r22,r6
	ctx.r[22].u64 = ctx.r[6].u64;
	// mr r29,r7
	ctx.r[29].u64 = ctx.r[7].u64;
	// mr r28,r20
	ctx.r[28].u64 = ctx.r[20].u64;
	// mr r27,r21
	ctx.r[27].u64 = ctx.r[21].u64;
	// cmplwi cr6,r24,0
	ctx.cr6.compare<uint32_t>(ctx.r[24].u32, 0, ctx.xer);
	// beq cr6,0x82df21c0
	if (ctx.cr6.eq) goto loc_82DF21C0;
	// cmplwi cr6,r22,0
	ctx.cr6.compare<uint32_t>(ctx.r[22].u32, 0, ctx.xer);
	// beq cr6,0x82df21c0
	if (ctx.cr6.eq) goto loc_82DF21C0;
	// cmplwi cr6,r20,0
	ctx.cr6.compare<uint32_t>(ctx.r[20].u32, 0, ctx.xer);
	// bne cr6,0x82df21cc
	if (!ctx.cr6.eq) goto loc_82DF21CC;
loc_82DF2198:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF219C;
	sub_82B7FD78(ctx, base);
	// li r10,22
	ctx.r[10].s64 = 22;
loc_82DF21A0:
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
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
	ctx.lr = 0x82DF21C0;
	sub_82B7FEC0(ctx, base);
loc_82DF21C0:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF21C4:
	// addi r1,r1,192
	ctx.r[1].s64 = ctx.r[1].s64 + 192;
	// b 0x82b7a718
	__restgprlr_20(ctx, base);
	return;
loc_82DF21CC:
	// li r31,-1
	ctx.r[31].s64 = -1;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82df21e8
	if (ctx.cr6.eq) goto loc_82DF21E8;
	// divwu r11,r31,r24
	ctx.r[11].u32 = ctx.r[31].u32 / ctx.r[24].u32;
	// twllei r24,0
	// cmplw cr6,r22,r11
	ctx.cr6.compare<uint32_t>(ctx.r[22].u32, ctx.r[11].u32, ctx.xer);
	// ble cr6,0x82df2218
	if (!ctx.cr6.gt) goto loc_82DF2218;
loc_82DF21E8:
	// cmpwi cr6,r21,-1
	ctx.cr6.compare<int32_t>(ctx.r[21].s32, -1, ctx.xer);
	// beq cr6,0x82df2200
	if (ctx.cr6.eq) goto loc_82DF2200;
	// mr r5,r21
	ctx.r[5].u64 = ctx.r[21].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r20
	ctx.r[3].u64 = ctx.r[20].u64;
	// bl 0x82b7bc40
	ctx.lr = 0x82DF2200;
	sub_82B7BC40(ctx, base);
loc_82DF2200:
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82df2198
	if (ctx.cr6.eq) goto loc_82DF2198;
	// divwu r11,r31,r24
	ctx.r[11].u32 = ctx.r[31].u32 / ctx.r[24].u32;
	// twllei r24,0
	// cmplw cr6,r22,r11
	ctx.cr6.compare<uint32_t>(ctx.r[22].u32, ctx.r[11].u32, ctx.xer);
	// bgt cr6,0x82df2198
	if (ctx.cr6.gt) goto loc_82DF2198;
loc_82DF2218:
	// lwz r11,12(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// mullw r23,r24,r22
	ctx.r[23].s64 = int64_t(ctx.r[24].s32) * int64_t(ctx.r[22].s32);
	// mr r31,r23
	ctx.r[31].u64 = ctx.r[23].u64;
	// andi. r11,r11,268
	ctx.r[11].u64 = ctx.r[11].u64 & 268;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df2238
	if (ctx.cr0.eq) goto loc_82DF2238;
	// lwz r26,24(r29)
	ctx.r[26].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 24);
	// b 0x82df223c
	goto loc_82DF223C;
loc_82DF2238:
	// li r26,4096
	ctx.r[26].s64 = 4096;
loc_82DF223C:
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x82df2388
	if (ctx.cr6.eq) goto loc_82DF2388;
	// lis r11,32767
	ctx.r[11].s64 = 2147418112;
	// ori r25,r11,65535
	ctx.r[25].u64 = ctx.r[11].u64 | 65535;
loc_82DF224C:
	// lwz r11,12(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// andi. r11,r11,268
	ctx.r[11].u64 = ctx.r[11].u64 & 268;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df22c0
	if (ctx.cr0.eq) goto loc_82DF22C0;
	// lwz r30,4(r29)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 4);
	// cmpwi r30,0
	ctx.cr0.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// beq 0x82df22c0
	if (ctx.cr0.eq) goto loc_82DF22C0;
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// blt cr6,0x82df23b4
	if (ctx.cr6.lt) goto loc_82DF23B4;
	// cmplw cr6,r31,r30
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[30].u32, ctx.xer);
	// bge cr6,0x82df227c
	if (!ctx.cr6.lt) goto loc_82DF227C;
	// mr r30,r31
	ctx.r[30].u64 = ctx.r[31].u64;
loc_82DF227C:
	// cmplw cr6,r30,r27
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[27].u32, ctx.xer);
	// bgt cr6,0x82df2390
	if (ctx.cr6.gt) goto loc_82DF2390;
	// mr r6,r30
	ctx.r[6].u64 = ctx.r[30].u64;
	// lwz r5,0(r29)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 0);
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// bl 0x82df4318
	ctx.lr = 0x82DF2298;
	sub_82DF4318(ctx, base);
	// lwz r10,4(r29)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 4);
	// lwz r11,0(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 0);
	// subf r31,r30,r31
	ctx.r[31].s64 = ctx.r[31].s64 - ctx.r[30].s64;
	// subf r10,r30,r10
	ctx.r[10].s64 = ctx.r[10].s64 - ctx.r[30].s64;
	// add r11,r11,r30
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[30].u64;
	// add r28,r30,r28
	ctx.r[28].u64 = ctx.r[30].u64 + ctx.r[28].u64;
	// subf r27,r30,r27
	ctx.r[27].s64 = ctx.r[27].s64 - ctx.r[30].s64;
	// stw r10,4(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 4, ctx.r[10].u32);
	// stw r11,0(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 0, ctx.r[11].u32);
	// b 0x82df2380
	goto loc_82DF2380;
loc_82DF22C0:
	// cmplw cr6,r31,r26
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[26].u32, ctx.xer);
	// blt cr6,0x82df2354
	if (ctx.cr6.lt) goto loc_82DF2354;
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82df2308
	if (ctx.cr6.eq) goto loc_82DF2308;
	// cmplw cr6,r31,r25
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[25].u32, ctx.xer);
	// twllei r26,0
	// ble cr6,0x82df22f4
	if (!ctx.cr6.gt) goto loc_82DF22F4;
	// mr r11,r25
	ctx.r[11].u64 = ctx.r[25].u64;
	// divwu r10,r11,r26
	ctx.r[10].u32 = ctx.r[11].u32 / ctx.r[26].u32;
	// mullw r10,r10,r26
	ctx.r[10].s64 = int64_t(ctx.r[10].s32) * int64_t(ctx.r[26].s32);
	// subf r11,r10,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[10].s64;
	// subf r30,r11,r25
	ctx.r[30].s64 = ctx.r[25].s64 - ctx.r[11].s64;
	// b 0x82df2318
	goto loc_82DF2318;
loc_82DF22F4:
	// divwu r11,r31,r26
	ctx.r[11].u32 = ctx.r[31].u32 / ctx.r[26].u32;
	// mullw r11,r11,r26
	ctx.r[11].s64 = int64_t(ctx.r[11].s32) * int64_t(ctx.r[26].s32);
	// subf r11,r11,r31
	ctx.r[11].s64 = ctx.r[31].s64 - ctx.r[11].s64;
	// subf r30,r11,r31
	ctx.r[30].s64 = ctx.r[31].s64 - ctx.r[11].s64;
	// b 0x82df2318
	goto loc_82DF2318;
loc_82DF2308:
	// cmplw cr6,r31,r25
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[25].u32, ctx.xer);
	// mr r30,r25
	ctx.r[30].u64 = ctx.r[25].u64;
	// bgt cr6,0x82df2318
	if (ctx.cr6.gt) goto loc_82DF2318;
	// mr r30,r31
	ctx.r[30].u64 = ctx.r[31].u64;
loc_82DF2318:
	// cmplw cr6,r30,r27
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[27].u32, ctx.xer);
	// bgt cr6,0x82df2390
	if (ctx.cr6.gt) goto loc_82DF2390;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF2328;
	sub_82B81648(ctx, base);
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x82b85a80
	ctx.lr = 0x82DF2334;
	sub_82B85A80(ctx, base);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq 0x82df23d0
	if (ctx.cr0.eq) goto loc_82DF23D0;
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df23b4
	if (ctx.cr6.eq) goto loc_82DF23B4;
	// subf r31,r3,r31
	ctx.r[31].s64 = ctx.r[31].s64 - ctx.r[3].s64;
	// add r28,r3,r28
	ctx.r[28].u64 = ctx.r[3].u64 + ctx.r[28].u64;
	// subf r27,r3,r27
	ctx.r[27].s64 = ctx.r[27].s64 - ctx.r[3].s64;
	// b 0x82df2380
	goto loc_82DF2380;
loc_82DF2354:
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF235C;
	sub_82B81360(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df23dc
	if (ctx.cr6.eq) goto loc_82DF23DC;
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x82df2390
	if (ctx.cr6.eq) goto loc_82DF2390;
	// stb r3,0(r28)
	PPC_STORE_U8(ctx.r[28].u32 + 0, ctx.r[3].u8);
	// addi r31,r31,-1
	ctx.r[31].s64 = ctx.r[31].s64 + -1;
	// lwz r26,24(r29)
	ctx.r[26].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 24);
	// addi r27,r27,-1
	ctx.r[27].s64 = ctx.r[27].s64 + -1;
	// addi r28,r28,1
	ctx.r[28].s64 = ctx.r[28].s64 + 1;
loc_82DF2380:
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82df224c
	if (!ctx.cr6.eq) goto loc_82DF224C;
loc_82DF2388:
	// mr r3,r22
	ctx.r[3].u64 = ctx.r[22].u64;
	// b 0x82df21c4
	goto loc_82DF21C4;
loc_82DF2390:
	// cmpwi cr6,r21,-1
	ctx.cr6.compare<int32_t>(ctx.r[21].s32, -1, ctx.xer);
	// beq cr6,0x82df23a8
	if (ctx.cr6.eq) goto loc_82DF23A8;
	// mr r5,r21
	ctx.r[5].u64 = ctx.r[21].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r20
	ctx.r[3].u64 = ctx.r[20].u64;
	// bl 0x82b7bc40
	ctx.lr = 0x82DF23A8;
	sub_82B7BC40(ctx, base);
loc_82DF23A8:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF23AC;
	sub_82B7FD78(ctx, base);
	// li r10,34
	ctx.r[10].s64 = 34;
	// b 0x82df21a0
	goto loc_82DF21A0;
loc_82DF23B4:
	// lwz r11,12(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// ori r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 | 32;
loc_82DF23BC:
	// subf r10,r31,r23
	ctx.r[10].s64 = ctx.r[23].s64 - ctx.r[31].s64;
	// stw r11,12(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 12, ctx.r[11].u32);
	// twllei r24,0
	// divwu r3,r10,r24
	ctx.r[3].u32 = ctx.r[10].u32 / ctx.r[24].u32;
	// b 0x82df21c4
	goto loc_82DF21C4;
loc_82DF23D0:
	// lwz r11,12(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// ori r11,r11,16
	ctx.r[11].u64 = ctx.r[11].u64 | 16;
	// b 0x82df23bc
	goto loc_82DF23BC;
loc_82DF23DC:
	// subf r11,r31,r23
	ctx.r[11].s64 = ctx.r[23].s64 - ctx.r[31].s64;
	// twllei r24,0
	// divwu r3,r11,r24
	ctx.r[3].u32 = ctx.r[11].u32 / ctx.r[24].u32;
	// b 0x82df21c4
	goto loc_82DF21C4;
}

void Body_82DF23F8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x82DF2400;
	__savegprlr_26(ctx, base);
	// addi r31,r1,-144
	ctx.r[31].s64 = ctx.r[1].s64 + -144;
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r26,r3
	ctx.r[26].u64 = ctx.r[3].u64;
	// mr r28,r4
	ctx.r[28].u64 = ctx.r[4].u64;
	// mr r29,r5
	ctx.r[29].u64 = ctx.r[5].u64;
	// mr r27,r6
	ctx.r[27].u64 = ctx.r[6].u64;
	// mr r30,r7
	ctx.r[30].u64 = ctx.r[7].u64;
	// stw r30,196(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 196, ctx.r[30].u32);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[11].u32);
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82df247c
	if (ctx.cr6.eq) goto loc_82DF247C;
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x82df247c
	if (ctx.cr6.eq) goto loc_82DF247C;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// bne cr6,0x82df2488
	if (!ctx.cr6.eq) goto loc_82DF2488;
	// cmpwi cr6,r28,-1
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, -1, ctx.xer);
	// beq cr6,0x82df2454
	if (ctx.cr6.eq) goto loc_82DF2454;
	// mr r5,r28
	ctx.r[5].u64 = ctx.r[28].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// bl 0x82b7bc40
	ctx.lr = 0x82DF2454;
	sub_82B7BC40(ctx, base);
loc_82DF2454:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF2458;
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
	ctx.lr = 0x82DF247C;
	sub_82B7FEC0(ctx, base);
loc_82DF247C:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF2480:
	// addi r1,r31,144
	ctx.r[1].s64 = ctx.r[31].s64 + 144;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
loc_82DF2488:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b708
	ctx.lr = 0x82DF2490;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// mr r7,r30
	ctx.r[7].u64 = ctx.r[30].u64;
	// mr r6,r27
	ctx.r[6].u64 = ctx.r[27].u64;
	// mr r5,r29
	ctx.r[5].u64 = ctx.r[29].u64;
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82df2158
	ctx.lr = 0x82DF24AC;
	sub_82DF2158(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,144
	ctx.r[12].s64 = ctx.r[31].s64 + 144;
	// bl 0x82df24e4
	ctx.lr = 0x82DF24BC;
	sub_82DF24E4(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// b 0x82df2480
	goto loc_82DF2480;
}

void Body_82DF2520(Context& ctx, Base& base) {
	// mr r7,r6
	ctx.r[7].u64 = ctx.r[6].u64;
	// mr r6,r5
	ctx.r[6].u64 = ctx.r[5].u64;
	// mr r5,r4
	ctx.r[5].u64 = ctx.r[4].u64;
	// li r4,-1
	ctx.r[4].s64 = -1;
	// b 0x82df23f8
	sub_82DF23F8(ctx, base);
	return;
}

void Body_82DF4318(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82DF4320;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r6
	ctx.r[31].u64 = ctx.r[6].u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// mr r29,r5
	ctx.r[29].u64 = ctx.r[5].u64;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82df4340
	if (!ctx.cr6.eq) goto loc_82DF4340;
loc_82DF4338:
	// li r3,0
	ctx.r[3].s64 = 0;
	// b 0x82df4374
	goto loc_82DF4374;
loc_82DF4340:
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// bne cr6,0x82df437c
	if (!ctx.cr6.eq) goto loc_82DF437C;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF434C;
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
	ctx.lr = 0x82DF4370;
	sub_82B7FEC0(ctx, base);
loc_82DF4370:
	// li r3,22
	ctx.r[3].s64 = 22;
loc_82DF4374:
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
loc_82DF437C:
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82df439c
	if (ctx.cr6.eq) goto loc_82DF439C;
	// cmplw cr6,r30,r31
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[31].u32, ctx.xer);
	// blt cr6,0x82df439c
	if (ctx.cr6.lt) goto loc_82DF439C;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// bl 0x82b7a0b0
	ctx.lr = 0x82DF4398;
	sub_82B7A0B0(ctx, base);
	// b 0x82df4338
	goto loc_82DF4338;
loc_82DF439C:
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// bl 0x82b7bc40
	ctx.lr = 0x82DF43A8;
	sub_82B7BC40(ctx, base);
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// bne cr6,0x82df43dc
	if (!ctx.cr6.eq) goto loc_82DF43DC;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF43B4;
	sub_82B7FD78(ctx, base);
	// li r31,22
	ctx.r[31].s64 = 22;
loc_82DF43B8:
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
	ctx.lr = 0x82DF43D4;
	sub_82B7FEC0(ctx, base);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// b 0x82df4374
	goto loc_82DF4374;
loc_82DF43DC:
	// cmplw cr6,r30,r31
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[31].u32, ctx.xer);
	// bge cr6,0x82df4370
	if (!ctx.cr6.lt) goto loc_82DF4370;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF43E8;
	sub_82B7FD78(ctx, base);
	// li r31,34
	ctx.r[31].s64 = 34;
	// b 0x82df43b8
	goto loc_82DF43B8;
}

void Body_82DF24E4(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-144
	ctx.r[31].s64 = ctx.r[12].s64 + -144;
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
	ctx.lr = 0x82DF2504;
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

void Body_82B7BC40(Context& ctx, Base& base) {
	// addi r0,r5,1
	ctx.r[0].s64 = ctx.r[5].s64 + 1;
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
	// ori r6,r3,0
	ctx.r[6].u64 = ctx.r[3].u64 | 0;
	// b 0x82b7bc5c
	goto loc_82B7BC5C;
loc_82B7BC50:
	// addi r5,r5,-1
	ctx.r[5].s64 = ctx.r[5].s64 + -1;
	// stb r4,0(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 0, ctx.r[4].u8);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
loc_82B7BC5C:
	// andi. r0,r6,3
	ctx.r[0].u64 = ctx.r[6].u64 & 3;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// bdnzf eq,0x82b7bc50
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0 && !ctx.cr0.eq) goto loc_82B7BC50;
	// rlwimi r4,r4,8,16,23
	ctx.r[4].u64 = (std::rotl(ctx.r[4].u32 | (ctx.r[4].u64 << 32), 8) & 0xFF00) | (ctx.r[4].u64 & 0xFFFFFFFFFFFF00FF);
	// rlwinm. r0,r5,28,4,31
	ctx.r[0].u64 = std::rotl(ctx.r[5].u32 | (ctx.r[5].u64 << 32), 28) & 0xFFFFFFF;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// rlwimi r4,r4,16,0,15
	ctx.r[4].u64 = (std::rotl(ctx.r[4].u32 | (ctx.r[4].u64 << 32), 16) & 0xFFFF0000) | (ctx.r[4].u64 & 0xFFFFFFFF0000FFFF);
	// beq+ 0x82b7bc90
	if (ctx.cr0.eq) goto loc_82B7BC90;
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
loc_82B7BC78:
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// stw r4,4(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 4, ctx.r[4].u32);
	// stw r4,8(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 8, ctx.r[4].u32);
	// stw r4,12(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 12, ctx.r[4].u32);
	// addi r6,r6,16
	ctx.r[6].s64 = ctx.r[6].s64 + 16;
	// bdnz+ 0x82b7bc78
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7BC78;
loc_82B7BC90:
	// rlwinm. r0,r5,30,30,31
	ctx.r[0].u64 = std::rotl(ctx.r[5].u32 | (ctx.r[5].u64 << 32), 30) & 0x3;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// beq- 0x82b7bcbc
	if (ctx.cr0.eq) goto loc_82B7BCBC;
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// addi r6,r6,4
	ctx.r[6].s64 = ctx.r[6].s64 + 4;
	// bdz- 0x82b7bcbc
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) goto loc_82B7BCBC;
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// addi r6,r6,4
	ctx.r[6].s64 = ctx.r[6].s64 + 4;
	// bdz- 0x82b7bcbc
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) goto loc_82B7BCBC;
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// addi r6,r6,4
	ctx.r[6].s64 = ctx.r[6].s64 + 4;
loc_82B7BCBC:
	// andi. r0,r5,3
	ctx.r[0].u64 = ctx.r[5].u64 & 3;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
	// beqlr+
	if (ctx.cr0.eq) return;
	// stb r4,0(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 0, ctx.r[4].u8);
	// bdzlr-
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) return;
	// stb r4,1(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 1, ctx.r[4].u8);
	// bdzlr-
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) return;
	// stb r4,2(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 2, ctx.r[4].u8);
	// blr
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    Context context{};FromFull(context,state);Base base{memory,dependencies,state};
    switch(entry)
    {
    case 0x82df2158u:Body_82DF2158(context,base);break;
    case 0x82df23f8u:Body_82DF23F8(context,base);break;
    case 0x82df2520u:Body_82DF2520(context,base);break;
    case 0x82df4318u:Body_82DF4318(context,base);break;
    case 0x82df24e4u:Body_82DF24E4(context,base);break;
    default:return false;
    }
    ToFull(state,context);return true;
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    switch(entry)
    {
    case 0x82b7bc40u:case 0x82b7a0b0u:case 0x82b85a80u:case 0x82b81360u:
    case 0x82b7b708u:case 0x82b7b7c8u:case 0x82b81648u:
    case 0x82b7fd78u:case 0x82b7fec0u:break;
    default:return false;
    }
    Context context{};FromFull(context,state);Base base{memory,dependencies,state};
    Direct(entry,context,base);return true;
}
} // namespace lo::semantic::gpu::crt_stream_block_read_context
