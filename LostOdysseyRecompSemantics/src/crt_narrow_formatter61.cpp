#include "lo_semantics/crt_narrow_formatter61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_stream_counted_output.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::crt_narrow_formatter61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Body_82B827C0(Context&,Base&);
void sub_82B827C0(Context& c,Base& b) {Body_82B827C0(c,b);}
void Body_82B83338(Context&,Base&);
void sub_82B83338(Context& c,Base& b) {Body_82B83338(c,b);}
void Body_82B7D168(Context&,Base&);
void sub_82B7D168(Context& c,Base& b) {Body_82B7D168(c,b);}
void Body_82B7D260(Context&,Base&);
void sub_82B7D260(Context& c,Base& b) {Body_82B7D260(c,b);}
void Body_82B7A680(Context&,Base&);
void sub_82B7A680(Context& c,Base& b) {Body_82B7A680(c,b);}
void Body_82B85420(Context&,Base&);
void sub_82B85420(Context& c,Base& b) {Body_82B85420(c,b);}
void Direct(GuestAddress entry,Context& c,Base& b) {
 ToFull(b.full,c);
 if(entry==0x82b86be0u) crt_reader_chain61::ApplySupport_B86BE0(b.memory,b.dependencies.accepted,b.full);
 else if(entry==0x82b81648u) crt_reader_chain61::ApplySupport_B81648(b.memory,b.dependencies.accepted,b.full);
 else if(entry==0x823add70u) b.dependencies.guest.CallOutput(b.memory,b.full);
 else if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,b.memory,b.dependencies.accepted,b.full)) {
  auto lower=crt_context_adapter::ToStream(b.full);
  const auto& streams=b.dependencies.accepted.close.pipeline.close.accepted;
  bool found=crt_stream_counted_output::Apply(entry,b.memory,streams,lower);
  if(!found) found=crt_stream_operations::Apply(entry,b.memory,streams,lower);
  if(!found) found=crt_stream_operations::ApplyAcceptedCallee(entry,b.memory,streams,lower);
  if(!found) throw std::logic_error("unselected narrow formatter lower");
  crt_context_adapter::FromStream(b.full,lower);
 }
 FromFull(c,b.full);
}
void Indirect(GuestAddress target,Context& c,Base& b) {
 ToFull(b.full,c);b.dependencies.guest.CallIndirect(target,b.memory,b.full);FromFull(c,b.full);
}
void __savegprlr_14(Context& c,Base& b){Save(14,c,b);}
void __restgprlr_14(Context& c,Base& b){Restore(14,c,b);}
void __savegprlr_28(Context& c,Base& b){Save(28,c,b);}
void __restgprlr_28(Context& c,Base& b){Restore(28,c,b);}
void sub_82B7FD78(Context& c,Base& b) {Direct(0x82B7FD78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b) {Direct(0x82B7FEC0u,c,b);}
void sub_82B81648(Context& c,Base& b) {Direct(0x82B81648u,c,b);}
void sub_822A03C8(Context& c,Base& b) {Direct(0x822A03C8u,c,b);}
void sub_82B82728(Context& c,Base& b) {Direct(0x82B82728u,c,b);}
void sub_82B82558(Context& c,Base& b) {Direct(0x82B82558u,c,b);}
void sub_82B86BE0(Context& c,Base& b) {Direct(0x82B86BE0u,c,b);}
void sub_823ACBD0(Context& c,Base& b) {Direct(0x823ACBD0u,c,b);}
void sub_823ADDC0(Context& c,Base& b) {Direct(0x823ADDC0u,c,b);}
void sub_823ADD70(Context& c,Base& b) {Direct(0x823ADD70u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_LOAD_U16(a) base.memory.ReadU16(Address(a))
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),v)
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
#define PPC_CALL_INDIRECT_FUNC(t) Indirect((t),ctx,base)
void Body_82B827C0(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6b0
	ctx.lr = 0x82B827C8;
	__savegprlr_14(ctx, base);
	// stwu r1,-1328(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-1328);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r27,0
	ctx.r[27].s64 = 0;
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
	// mr r19,r4
	ctx.r[19].u64 = ctx.r[4].u64;
	// mr r26,r6
	ctx.r[26].u64 = ctx.r[6].u64;
	// mr r25,r27
	ctx.r[25].u64 = ctx.r[27].u64;
	// stw r27,108(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 108, ctx.r[27].u32);
	// mr r16,r27
	ctx.r[16].u64 = ctx.r[27].u64;
	// stw r27,96(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 96, ctx.r[27].u32);
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// stw r27,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[27].u32);
	// stw r27,100(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 100, ctx.r[27].u32);
	// bne cr6,0x82b8282c
	if (!ctx.cr6.eq) goto loc_82B8282C;
loc_82B827FC:
	// bl 0x82b7fd78
	ctx.lr = 0x82B82800;
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
	ctx.lr = 0x82B82824;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b8332c
	goto loc_82B8332C;
loc_82B8282C:
	// lwz r11,12(r23)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[23].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b828f8
	if (!ctx.cr0.eq) goto loc_82B828F8;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B82840;
	sub_82B81648(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// addi r30,r11,-29312
	ctx.r[30].s64 = ctx.r[11].s64 + -29312;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r29,r11,21272
	ctx.r[29].s64 = ctx.r[11].s64 + 21272;
	// beq cr6,0x82b82890
	if (ctx.cr6.eq) goto loc_82B82890;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B82860;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82b82890
	if (ctx.cr6.eq) goto loc_82B82890;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B82870;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// rlwinm r31,r11,2,0,29
	ctx.r[31].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82B82880;
	sub_82B81648(ctx, base);
	// lwzx r10,r31,r30
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + ctx.r[30].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82b82894
	goto loc_82B82894;
loc_82B82890:
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
loc_82B82894:
	// lbz r11,40(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 40);
	// rlwinm. r11,r11,0,0,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b827fc
	if (!ctx.cr0.eq) goto loc_82B827FC;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B828A8;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82b828e8
	if (ctx.cr6.eq) goto loc_82B828E8;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B828B8;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82b828e8
	if (ctx.cr6.eq) goto loc_82B828E8;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B828C8;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// rlwinm r31,r11,2,0,29
	ctx.r[31].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82B828D8;
	sub_82B81648(ctx, base);
	// lwzx r10,r31,r30
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + ctx.r[30].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82b828ec
	goto loc_82B828EC;
loc_82B828E8:
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
loc_82B828EC:
	// lbz r11,40(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 40);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b827fc
	if (!ctx.cr0.eq) goto loc_82B827FC;
loc_82B828F8:
	// cmplwi cr6,r19,0
	ctx.cr6.compare<uint32_t>(ctx.r[19].u32, 0, ctx.xer);
	// beq cr6,0x82b827fc
	if (ctx.cr6.eq) goto loc_82B827FC;
	// bl 0x822a03c8
	ctx.lr = 0x82B82904;
	sub_822A03C8(ctx, base);
	// addi r11,r3,32
	ctx.r[11].s64 = ctx.r[3].s64 + 32;
	// cmplw cr6,r23,r11
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, ctx.r[11].u32, ctx.xer);
	// beq cr6,0x82b832f8
	if (ctx.cr6.eq) goto loc_82B832F8;
	// bl 0x822a03c8
	ctx.lr = 0x82B82914;
	sub_822A03C8(ctx, base);
	// addi r11,r3,64
	ctx.r[11].s64 = ctx.r[3].s64 + 64;
	// cmplw cr6,r23,r11
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, ctx.r[11].u32, ctx.xer);
	// beq cr6,0x82b832f8
	if (ctx.cr6.eq) goto loc_82B832F8;
	// lbz r29,0(r19)
	ctx.r[29].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// mr r24,r27
	ctx.r[24].u64 = ctx.r[27].u64;
	// mr r8,r27
	ctx.r[8].u64 = ctx.r[27].u64;
	// stw r27,104(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 104, ctx.r[27].u32);
	// mr r15,r27
	ctx.r[15].u64 = ctx.r[27].u64;
	// stw r24,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[24].u32);
	// stw r8,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[8].u32);
	// extsb. r10,r29
	ctx.r[10].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b832f0
	if (ctx.cr0.eq) goto loc_82B832F0;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// lwz r20,112(r1)
	ctx.r[20].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 112);
	// lwz r28,112(r1)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 112);
	// lis r14,-32243
	ctx.r[14].s64 = -2113077248;
	// addi r22,r11,21248
	ctx.r[22].s64 = ctx.r[11].s64 + 21248;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// lis r17,-32243
	ctx.r[17].s64 = -2113077248;
	// addi r21,r11,20424
	ctx.r[21].s64 = ctx.r[11].s64 + 20424;
	// lis r11,-32243
	ctx.r[11].s64 = -2113077248;
	// addi r18,r11,14304
	ctx.r[18].s64 = ctx.r[11].s64 + 14304;
loc_82B8296C:
	// addi r19,r19,1
	ctx.r[19].s64 = ctx.r[19].s64 + 1;
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// blt cr6,0x82b832f0
	if (ctx.cr6.lt) goto loc_82B832F0;
	// cmpwi cr6,r10,32
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 32, ctx.xer);
	// blt cr6,0x82b82998
	if (ctx.cr6.lt) goto loc_82B82998;
	// cmpwi cr6,r10,120
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 120, ctx.xer);
	// bgt cr6,0x82b82998
	if (ctx.cr6.gt) goto loc_82B82998;
	// add r11,r10,r18
	ctx.r[11].u64 = ctx.r[10].u64 + ctx.r[18].u64;
	// lbz r11,-32(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + -32);
	// clrlwi r11,r11,28
	ctx.r[11].u64 = ctx.r[11].u32 & 0xF;
	// b 0x82b8299c
	goto loc_82B8299C;
loc_82B82998:
	// li r11,0
	ctx.r[11].s64 = 0;
loc_82B8299C:
	// lwz r9,104(r1)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 104);
	// rlwinm r11,r11,3,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r9
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[9].u64;
	// lbzx r11,r11,r18
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + ctx.r[18].u32);
	// extsb r11,r11
	ctx.r[11].s64 = ctx.r[11].s8;
	// srawi r11,r11,4
	ctx.xer.ca = std::uint8_t((ctx.r[11].s32 < 0)) & std::uint8_t(((ctx.r[11].u32 & 0xF) != 0));
	ctx.r[11].s64 = ctx.r[11].s32 >> 4;
	// cmplwi cr6,r11,7
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 7, ctx.xer);
	// stw r11,104(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 104, ctx.r[11].u32);
	// bgt cr6,0x82b832dc
	if (ctx.cr6.gt) goto loc_82B832DC;
	// rotlwi r11,r11,0
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32, 0);
	// lis r12,-32243
	ctx.r[12].s64 = -2113077248;
	// addi r12,r12,14512
	ctx.r[12].s64 = ctx.r[12].s64 + 14512;
	// rlwinm r0,r11,1,0,30
	ctx.r[0].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 1) & 0xFFFFFFFE;
	// lhzx r0,r12,r0
	ctx.r[0].u64 = PPC_LOAD_U16(ctx.r[12].u32 + ctx.r[0].u32);
	// lis r12,-32072
	ctx.r[12].s64 = -2101870592;
	// addi r12,r12,10732
	ctx.r[12].s64 = ctx.r[12].s64 + 10732;
	// add r12,r12,r0
	ctx.r[12].u64 = ctx.r[12].u64 + ctx.r[0].u64;
	// mtctr r12
	ctx.ctr.u64 = ctx.r[12].u64;
	// nop
	// bctr
	switch (ctx.r[11].u32) {
	case 0:
		goto loc_82B82BB8;
	case 1:
		goto loc_82B829EC;
	case 2:
		goto loc_82B82A0C;
	case 3:
		goto loc_82B82A5C;
	case 4:
		goto loc_82B82AA8;
	case 5:
		goto loc_82B82AB0;
	case 6:
		goto loc_82B82AE8;
	case 7:
		goto loc_82B82C08;
	default:
		__builtin_unreachable();
	}
loc_82B829EC:
	// li r27,0
	ctx.r[27].s64 = 0;
	// li r25,-1
	ctx.r[25].s64 = -1;
	// mr r20,r27
	ctx.r[20].u64 = ctx.r[27].u64;
	// mr r16,r27
	ctx.r[16].u64 = ctx.r[27].u64;
	// stw r27,100(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 100, ctx.r[27].u32);
	// stw r27,96(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 96, ctx.r[27].u32);
	// stw r27,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[27].u32);
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82A0C:
	// cmpwi cr6,r10,32
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 32, ctx.xer);
	// beq cr6,0x82b82a54
	if (ctx.cr6.eq) goto loc_82B82A54;
	// cmpwi cr6,r10,35
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 35, ctx.xer);
	// beq cr6,0x82b82a4c
	if (ctx.cr6.eq) goto loc_82B82A4C;
	// cmpwi cr6,r10,43
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 43, ctx.xer);
	// beq cr6,0x82b82a44
	if (ctx.cr6.eq) goto loc_82B82A44;
	// cmpwi cr6,r10,45
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 45, ctx.xer);
	// beq cr6,0x82b82a3c
	if (ctx.cr6.eq) goto loc_82B82A3C;
	// cmpwi cr6,r10,48
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 48, ctx.xer);
	// bne cr6,0x82b832dc
	if (!ctx.cr6.eq) goto loc_82B832DC;
	// ori r27,r27,8
	ctx.r[27].u64 = ctx.r[27].u64 | 8;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82A3C:
	// ori r27,r27,4
	ctx.r[27].u64 = ctx.r[27].u64 | 4;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82A44:
	// ori r27,r27,1
	ctx.r[27].u64 = ctx.r[27].u64 | 1;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82A4C:
	// ori r27,r27,128
	ctx.r[27].u64 = ctx.r[27].u64 | 128;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82A54:
	// ori r27,r27,2
	ctx.r[27].u64 = ctx.r[27].u64 | 2;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82A5C:
	// cmpwi cr6,r10,42
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 42, ctx.xer);
	// bne cr6,0x82b82a90
	if (!ctx.cr6.eq) goto loc_82B82A90;
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// lwz r11,-4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,96(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 96, ctx.r[11].u32);
	// bge 0x82b832dc
	if (!ctx.cr0.lt) goto loc_82B832DC;
	// rotlwi r11,r11,0
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32, 0);
	// ori r27,r27,4
	ctx.r[27].u64 = ctx.r[27].u64 | 4;
	// neg r11,r11
	ctx.r[11].s64 = -ctx.r[11].s64;
	// b 0x82b82aa0
	goto loc_82B82AA0;
loc_82B82A90:
	// lwz r11,96(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 96);
	// mulli r11,r11,10
	ctx.r[11].s64 = ctx.r[11].s64 * 10;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// addi r11,r11,-48
	ctx.r[11].s64 = ctx.r[11].s64 + -48;
loc_82B82AA0:
	// stw r11,96(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 96, ctx.r[11].u32);
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82AA8:
	// li r25,0
	ctx.r[25].s64 = 0;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82AB0:
	// cmpwi cr6,r10,42
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 42, ctx.xer);
	// bne cr6,0x82b82ad8
	if (!ctx.cr6.eq) goto loc_82B82AD8;
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// lwz r25,-4(r26)
	ctx.r[25].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
	// cmpwi r25,0
	ctx.cr0.compare<int32_t>(ctx.r[25].s32, 0, ctx.xer);
	// bge 0x82b832dc
	if (!ctx.cr0.lt) goto loc_82B832DC;
	// li r25,-1
	ctx.r[25].s64 = -1;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82AD8:
	// mulli r11,r25,10
	ctx.r[11].s64 = ctx.r[25].s64 * 10;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// addi r25,r11,-48
	ctx.r[25].s64 = ctx.r[11].s64 + -48;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82AE8:
	// cmpwi cr6,r10,73
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 73, ctx.xer);
	// beq cr6,0x82b82b38
	if (ctx.cr6.eq) goto loc_82B82B38;
	// cmpwi cr6,r10,104
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 104, ctx.xer);
	// beq cr6,0x82b82b30
	if (ctx.cr6.eq) goto loc_82B82B30;
	// cmpwi cr6,r10,108
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 108, ctx.xer);
	// beq cr6,0x82b82b10
	if (ctx.cr6.eq) goto loc_82B82B10;
	// cmpwi cr6,r10,119
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 119, ctx.xer);
	// bne cr6,0x82b832dc
	if (!ctx.cr6.eq) goto loc_82B832DC;
	// ori r27,r27,2048
	ctx.r[27].u64 = ctx.r[27].u64 | 2048;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82B10:
	// lbz r11,0(r19)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// cmplwi cr6,r11,108
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 108, ctx.xer);
	// bne cr6,0x82b82b28
	if (!ctx.cr6.eq) goto loc_82B82B28;
	// addi r19,r19,1
	ctx.r[19].s64 = ctx.r[19].s64 + 1;
	// ori r27,r27,4096
	ctx.r[27].u64 = ctx.r[27].u64 | 4096;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82B28:
	// ori r27,r27,16
	ctx.r[27].u64 = ctx.r[27].u64 | 16;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82B30:
	// ori r27,r27,32
	ctx.r[27].u64 = ctx.r[27].u64 | 32;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82B38:
	// lbz r11,0(r19)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// extsb r11,r11
	ctx.r[11].s64 = ctx.r[11].s8;
	// cmpwi cr6,r11,54
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 54, ctx.xer);
	// bne cr6,0x82b82b60
	if (!ctx.cr6.eq) goto loc_82B82B60;
	// lbz r10,1(r19)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 1);
	// cmplwi cr6,r10,52
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 52, ctx.xer);
	// bne cr6,0x82b82b60
	if (!ctx.cr6.eq) goto loc_82B82B60;
	// addi r19,r19,2
	ctx.r[19].s64 = ctx.r[19].s64 + 2;
	// ori r27,r27,32768
	ctx.r[27].u64 = ctx.r[27].u64 | 32768;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82B60:
	// cmpwi cr6,r11,51
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 51, ctx.xer);
	// bne cr6,0x82b82b80
	if (!ctx.cr6.eq) goto loc_82B82B80;
	// lbz r10,1(r19)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 1);
	// cmplwi cr6,r10,50
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 50, ctx.xer);
	// bne cr6,0x82b82b80
	if (!ctx.cr6.eq) goto loc_82B82B80;
	// addi r19,r19,2
	ctx.r[19].s64 = ctx.r[19].s64 + 2;
	// rlwinm r27,r27,0,17,15
	ctx.r[27].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0xFFFFFFFFFFFF7FFF;
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82B80:
	// cmpwi cr6,r11,100
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 100, ctx.xer);
	// beq cr6,0x82b832dc
	if (ctx.cr6.eq) goto loc_82B832DC;
	// cmpwi cr6,r11,105
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 105, ctx.xer);
	// beq cr6,0x82b832dc
	if (ctx.cr6.eq) goto loc_82B832DC;
	// cmpwi cr6,r11,111
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 111, ctx.xer);
	// beq cr6,0x82b832dc
	if (ctx.cr6.eq) goto loc_82B832DC;
	// cmpwi cr6,r11,117
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 117, ctx.xer);
	// beq cr6,0x82b832dc
	if (ctx.cr6.eq) goto loc_82B832DC;
	// cmpwi cr6,r11,120
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 120, ctx.xer);
	// beq cr6,0x82b832dc
	if (ctx.cr6.eq) goto loc_82B832DC;
	// cmpwi cr6,r11,88
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 88, ctx.xer);
	// beq cr6,0x82b832dc
	if (ctx.cr6.eq) goto loc_82B832DC;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,104(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 104, ctx.r[11].u32);
loc_82B82BB8:
	// clrlwi r3,r29,24
	ctx.r[3].u64 = ctx.r[29].u32 & 0xFF;
	// mr r4,r22
	ctx.r[4].u64 = ctx.r[22].u64;
	// li r16,0
	ctx.r[16].s64 = 0;
	// bl 0x82b7a680
	ctx.lr = 0x82B82BC8;
	sub_82B7A680(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82b82bf0
	if (ctx.cr0.eq) goto loc_82B82BF0;
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// mr r4,r23
	ctx.r[4].u64 = ctx.r[23].u64;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82b82728
	ctx.lr = 0x82B82BE0;
	sub_82B82728(ctx, base);
	// lbz r29,0(r19)
	ctx.r[29].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// addi r19,r19,1
	ctx.r[19].s64 = ctx.r[19].s64 + 1;
	// cmplwi r29,0
	ctx.cr0.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq 0x82b827fc
	if (ctx.cr0.eq) goto loc_82B827FC;
loc_82B82BF0:
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// mr r4,r23
	ctx.r[4].u64 = ctx.r[23].u64;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82b82728
	ctx.lr = 0x82B82C00;
	sub_82B82728(ctx, base);
	// lwz r24,80(r1)
	ctx.r[24].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// b 0x82b832dc
	goto loc_82B832DC;
loc_82B82C08:
	// addi r11,r10,-65
	ctx.r[11].s64 = ctx.r[10].s64 + -65;
	// cmplwi cr6,r11,55
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 55, ctx.xer);
	// bgt cr6,0x82b83108
	if (ctx.cr6.gt) goto loc_82B83108;
	// lis r12,-32243
	ctx.r[12].s64 = -2113077248;
	// addi r12,r12,14400
	ctx.r[12].s64 = ctx.r[12].s64 + 14400;
	// rlwinm r0,r11,1,0,30
	ctx.r[0].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 1) & 0xFFFFFFFE;
	// lhzx r0,r12,r0
	ctx.r[0].u64 = PPC_LOAD_U16(ctx.r[12].u32 + ctx.r[0].u32);
	// lis r12,-32072
	ctx.r[12].s64 = -2101870592;
	// addi r12,r12,11324
	ctx.r[12].s64 = ctx.r[12].s64 + 11324;
	// add r12,r12,r0
	ctx.r[12].u64 = ctx.r[12].u64 + ctx.r[0].u64;
	// mtctr r12
	ctx.ctr.u64 = ctx.r[12].u64;
	// nop
	// bctr
	switch (ctx.r[11].u32) {
	case 0:
		goto loc_82B82E18;
	case 1:
		goto loc_82B83108;
	case 2:
		goto loc_82B82C3C;
	case 3:
		goto loc_82B83108;
	case 4:
		goto loc_82B82E18;
	case 5:
		goto loc_82B83108;
	case 6:
		goto loc_82B82E18;
	case 7:
		goto loc_82B83108;
	case 8:
		goto loc_82B83108;
	case 9:
		goto loc_82B83108;
	case 10:
		goto loc_82B83108;
	case 11:
		goto loc_82B83108;
	case 12:
		goto loc_82B83108;
	case 13:
		goto loc_82B83108;
	case 14:
		goto loc_82B83108;
	case 15:
		goto loc_82B83108;
	case 16:
		goto loc_82B83108;
	case 17:
		goto loc_82B83108;
	case 18:
		goto loc_82B82D24;
	case 19:
		goto loc_82B83108;
	case 20:
		goto loc_82B83108;
	case 21:
		goto loc_82B83108;
	case 22:
		goto loc_82B83108;
	case 23:
		goto loc_82B82F58;
	case 24:
		goto loc_82B83108;
	case 25:
		goto loc_82B82CA8;
	case 26:
		goto loc_82B83108;
	case 27:
		goto loc_82B83108;
	case 28:
		goto loc_82B83108;
	case 29:
		goto loc_82B83108;
	case 30:
		goto loc_82B83108;
	case 31:
		goto loc_82B83108;
	case 32:
		goto loc_82B82E24;
	case 33:
		goto loc_82B83108;
	case 34:
		goto loc_82B82C4C;
	case 35:
		goto loc_82B82F48;
	case 36:
		goto loc_82B82E24;
	case 37:
		goto loc_82B82E24;
	case 38:
		goto loc_82B82E24;
	case 39:
		goto loc_82B83108;
	case 40:
		goto loc_82B82F48;
	case 41:
		goto loc_82B83108;
	case 42:
		goto loc_82B83108;
	case 43:
		goto loc_82B83108;
	case 44:
		goto loc_82B83108;
	case 45:
		goto loc_82B82DDC;
	case 46:
		goto loc_82B82F90;
	case 47:
		goto loc_82B82F54;
	case 48:
		goto loc_82B83108;
	case 49:
		goto loc_82B83108;
	case 50:
		goto loc_82B82D34;
	case 51:
		goto loc_82B83108;
	case 52:
		goto loc_82B82F4C;
	case 53:
		goto loc_82B83108;
	case 54:
		goto loc_82B83108;
	case 55:
		goto loc_82B82F60;
	default:
		__builtin_unreachable();
	}
loc_82B82C3C:
	// andi. r11,r27,2096
	ctx.r[11].u64 = ctx.r[27].u64 & 2096;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b82c4c
	if (!ctx.cr0.eq) goto loc_82B82C4C;
	// ori r27,r27,2048
	ctx.r[27].u64 = ctx.r[27].u64 | 2048;
loc_82B82C4C:
	// andi. r11,r27,2064
	ctx.r[11].u64 = ctx.r[27].u64 & 2064;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// beq 0x82b82c8c
	if (ctx.cr0.eq) goto loc_82B82C8C;
	// li r5,512
	ctx.r[5].s64 = 512;
	// lhz r6,-2(r26)
	ctx.r[6].u64 = PPC_LOAD_U16(ctx.r[26].u32 + -2);
	// addi r4,r1,144
	ctx.r[4].s64 = ctx.r[1].s64 + 144;
	// addi r3,r1,84
	ctx.r[3].s64 = ctx.r[1].s64 + 84;
	// bl 0x82b86be0
	ctx.lr = 0x82B82C78;
	sub_82B86BE0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82b82c9c
	if (ctx.cr0.eq) goto loc_82B82C9C;
	// li r11,1
	ctx.r[11].s64 = 1;
	// stw r11,100(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 100, ctx.r[11].u32);
	// b 0x82b82c9c
	goto loc_82B82C9C;
loc_82B82C8C:
	// lwz r11,-4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
	// li r10,1
	ctx.r[10].s64 = 1;
	// stw r10,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[10].u32);
	// stb r11,144(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 144, ctx.r[11].u8);
loc_82B82C9C:
	// lwz r8,84(r1)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// addi r28,r1,144
	ctx.r[28].s64 = ctx.r[1].s64 + 144;
	// b 0x82b83108
	goto loc_82B83108;
loc_82B82CA8:
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// lwz r11,-4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq 0x82b82cf8
	if (ctx.cr0.eq) goto loc_82B82CF8;
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq 0x82b82cf8
	if (ctx.cr0.eq) goto loc_82B82CF8;
	// rlwinm. r9,r27,0,20,20
	ctx.r[9].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x800;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// mr r28,r10
	ctx.r[28].u64 = ctx.r[10].u64;
	// beq 0x82b82cec
	if (ctx.cr0.eq) goto loc_82B82CEC;
	// lha r11,0(r11)
	ctx.r[11].s64 = int16_t(PPC_LOAD_U16(ctx.r[11].u32 + 0));
	// li r16,1
	ctx.r[16].s64 = 1;
	// srawi r11,r11,1
	ctx.xer.ca = std::uint8_t((ctx.r[11].s32 < 0)) & std::uint8_t(((ctx.r[11].u32 & 0x1) != 0));
	ctx.r[11].s64 = ctx.r[11].s32 >> 1;
	// addze r8,r11
	temp.s64 = ctx.r[11].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[11].u32;
	ctx.r[8].s64 = temp.s64;
	// b 0x82b83104
	goto loc_82B83104;
loc_82B82CEC:
	// lha r8,0(r11)
	ctx.r[8].s64 = int16_t(PPC_LOAD_U16(ctx.r[11].u32 + 0));
	// li r16,0
	ctx.r[16].s64 = 0;
	// b 0x82b83104
	goto loc_82B83104;
loc_82B82CF8:
	// lwz r28,14296(r17)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[17].u32 + 14296);
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
loc_82B82D04:
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// bne cr6,0x82b82d04
	if (!ctx.cr6.eq) goto loc_82B82D04;
loc_82B82D14:
	// subf r11,r10,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[10].s64;
	// addi r11,r11,-1
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	// rotlwi r8,r11,0
	ctx.r[8].u64 = std::rotl(ctx.r[11].u32, 0);
	// b 0x82b83104
	goto loc_82B83104;
loc_82B82D24:
	// andi. r11,r27,2096
	ctx.r[11].u64 = ctx.r[27].u64 & 2096;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b82d34
	if (!ctx.cr0.eq) goto loc_82B82D34;
	// ori r27,r27,2048
	ctx.r[27].u64 = ctx.r[27].u64 | 2048;
loc_82B82D34:
	// cmpwi cr6,r25,-1
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, -1, ctx.xer);
	// bne cr6,0x82b82d48
	if (!ctx.cr6.eq) goto loc_82B82D48;
	// lis r10,32767
	ctx.r[10].s64 = 2147418112;
	// ori r10,r10,65535
	ctx.r[10].u64 = ctx.r[10].u64 | 65535;
	// b 0x82b82d4c
	goto loc_82B82D4C;
loc_82B82D48:
	// mr r10,r25
	ctx.r[10].u64 = ctx.r[25].u64;
loc_82B82D4C:
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// andi. r9,r27,2064
	ctx.r[9].u64 = ctx.r[27].u64 & 2064;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// cmpwi r9,0
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// lwz r28,-4(r26)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
	// cmplwi cr6,r28,0
	ctx.cr6.compare<uint32_t>(ctx.r[28].u32, 0, ctx.xer);
	// beq 0x82b82da8
	if (ctx.cr0.eq) goto loc_82B82DA8;
	// bne cr6,0x82b82d74
	if (!ctx.cr6.eq) goto loc_82B82D74;
	// lwz r28,14300(r14)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[14].u32 + 14300);
loc_82B82D74:
	// li r16,1
	ctx.r[16].s64 = 1;
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
	// b 0x82b82d94
	goto loc_82B82D94;
loc_82B82D80:
	// lhz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U16(ctx.r[11].u32 + 0);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// cmplwi r9,0
	ctx.cr0.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// beq 0x82b82d9c
	if (ctx.cr0.eq) goto loc_82B82D9C;
	// addi r11,r11,2
	ctx.r[11].s64 = ctx.r[11].s64 + 2;
loc_82B82D94:
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne cr6,0x82b82d80
	if (!ctx.cr6.eq) goto loc_82B82D80;
loc_82B82D9C:
	// subf r11,r28,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[28].s64;
	// srawi r8,r11,1
	ctx.xer.ca = std::uint8_t((ctx.r[11].s32 < 0)) & std::uint8_t(((ctx.r[11].u32 & 0x1) != 0));
	ctx.r[8].s64 = ctx.r[11].s32 >> 1;
	// b 0x82b83104
	goto loc_82B83104;
loc_82B82DA8:
	// bne cr6,0x82b82db0
	if (!ctx.cr6.eq) goto loc_82B82DB0;
	// lwz r28,14296(r17)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[17].u32 + 14296);
loc_82B82DB0:
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
	// b 0x82b82dcc
	goto loc_82B82DCC;
loc_82B82DB8:
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// beq cr6,0x82b82dd4
	if (ctx.cr6.eq) goto loc_82B82DD4;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
loc_82B82DCC:
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne cr6,0x82b82db8
	if (!ctx.cr6.eq) goto loc_82B82DB8;
loc_82B82DD4:
	// subf r8,r28,r11
	ctx.r[8].s64 = ctx.r[11].s64 - ctx.r[28].s64;
	// b 0x82b83104
	goto loc_82B83104;
loc_82B82DDC:
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// lwz r31,-4(r26)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
	// bl 0x82b85420
	ctx.lr = 0x82B82DF0;
	sub_82B85420(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82b827fc
	if (ctx.cr0.eq) goto loc_82B827FC;
	// rlwinm. r11,r27,0,26,26
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x20;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b82e08
	if (ctx.cr0.eq) goto loc_82B82E08;
	// sth r24,0(r31)
	PPC_STORE_U16(ctx.r[31].u32 + 0, ctx.r[24].u16);
	// b 0x82b82e0c
	goto loc_82B82E0C;
loc_82B82E08:
	// stw r24,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[24].u32);
loc_82B82E0C:
	// li r11,1
	ctx.r[11].s64 = 1;
	// stw r11,100(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 100, ctx.r[11].u32);
	// b 0x82b832c8
	goto loc_82B832C8;
loc_82B82E18:
	// addi r11,r10,32
	ctx.r[11].s64 = ctx.r[10].s64 + 32;
	// li r20,1
	ctx.r[20].s64 = 1;
	// extsb r29,r11
	ctx.r[29].s64 = ctx.r[11].s8;
loc_82B82E24:
	// ori r27,r27,64
	ctx.r[27].u64 = ctx.r[27].u64 | 64;
	// addi r28,r1,144
	ctx.r[28].s64 = ctx.r[1].s64 + 144;
	// li r30,512
	ctx.r[30].s64 = 512;
	// cmpwi cr6,r25,0
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, 0, ctx.xer);
	// bge cr6,0x82b82e40
	if (!ctx.cr6.lt) goto loc_82B82E40;
	// li r25,6
	ctx.r[25].s64 = 6;
	// b 0x82b82e90
	goto loc_82B82E90;
loc_82B82E40:
	// bne cr6,0x82b82e58
	if (!ctx.cr6.eq) goto loc_82B82E58;
	// extsb r11,r29
	ctx.r[11].s64 = ctx.r[29].s8;
	// cmpwi cr6,r11,103
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 103, ctx.xer);
	// bne cr6,0x82b82e90
	if (!ctx.cr6.eq) goto loc_82B82E90;
	// li r25,1
	ctx.r[25].s64 = 1;
	// b 0x82b82e90
	goto loc_82B82E90;
loc_82B82E58:
	// cmpwi cr6,r25,512
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, 512, ctx.xer);
	// ble cr6,0x82b82e64
	if (!ctx.cr6.gt) goto loc_82B82E64;
	// li r25,512
	ctx.r[25].s64 = 512;
loc_82B82E64:
	// cmpwi cr6,r25,163
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, 163, ctx.xer);
	// ble cr6,0x82b82e90
	if (!ctx.cr6.gt) goto loc_82B82E90;
	// addi r31,r25,349
	ctx.r[31].s64 = ctx.r[25].s64 + 349;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x823acbd0
	ctx.lr = 0x82B82E78;
	sub_823ACBD0(ctx, base);
	// mr. r15,r3
	ctx.r[15].u64 = ctx.r[3].u64;
	ctx.cr0.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// beq 0x82b82e8c
	if (ctx.cr0.eq) goto loc_82B82E8C;
	// mr r28,r15
	ctx.r[28].u64 = ctx.r[15].u64;
	// mr r30,r31
	ctx.r[30].u64 = ctx.r[31].u64;
	// b 0x82b82e90
	goto loc_82B82E90;
loc_82B82E8C:
	// li r25,163
	ctx.r[25].s64 = 163;
loc_82B82E90:
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// extsb r31,r29
	ctx.r[31].s64 = ctx.r[29].s8;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// mr r8,r20
	ctx.r[8].u64 = ctx.r[20].u64;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// lwz r11,24(r21)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[21].u32 + 24);
	// mr r9,r22
	ctx.r[9].u64 = ctx.r[22].u64;
	// mr r7,r25
	ctx.r[7].u64 = ctx.r[25].u64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// ld r10,-8(r26)
	ctx.r[10].u64 = PPC_LOAD_U64(ctx.r[26].u32 + -8);
	// addi r3,r1,128
	ctx.r[3].s64 = ctx.r[1].s64 + 128;
	// mr r6,r31
	ctx.r[6].u64 = ctx.r[31].u64;
	// std r10,128(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 128, ctx.r[10].u64);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82B82ED0;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// rlwinm. r30,r27,0,24,24
	ctx.r[30].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x80;
	ctx.cr0.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// beq 0x82b82ef4
	if (ctx.cr0.eq) goto loc_82B82EF4;
	// cmpwi cr6,r25,0
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, 0, ctx.xer);
	// bne cr6,0x82b82ef4
	if (!ctx.cr6.eq) goto loc_82B82EF4;
	// lwz r11,36(r21)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[21].u32 + 36);
	// mr r4,r22
	ctx.r[4].u64 = ctx.r[22].u64;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82B82EF4;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
loc_82B82EF4:
	// cmpwi cr6,r31,103
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 103, ctx.xer);
	// bne cr6,0x82b82f18
	if (!ctx.cr6.eq) goto loc_82B82F18;
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// bne cr6,0x82b82f18
	if (!ctx.cr6.eq) goto loc_82B82F18;
	// lwz r11,32(r21)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[21].u32 + 32);
	// mr r4,r22
	ctx.r[4].u64 = ctx.r[22].u64;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82B82F18;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
loc_82B82F18:
	// lbz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[28].u32 + 0);
	// cmplwi cr6,r11,45
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 45, ctx.xer);
	// bne cr6,0x82b82f2c
	if (!ctx.cr6.eq) goto loc_82B82F2C;
	// ori r27,r27,256
	ctx.r[27].u64 = ctx.r[27].u64 | 256;
	// addi r28,r28,1
	ctx.r[28].s64 = ctx.r[28].s64 + 1;
loc_82B82F2C:
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
loc_82B82F34:
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// bne cr6,0x82b82f34
	if (!ctx.cr6.eq) goto loc_82B82F34;
	// b 0x82b82d14
	goto loc_82B82D14;
loc_82B82F48:
	// ori r27,r27,64
	ctx.r[27].u64 = ctx.r[27].u64 | 64;
loc_82B82F4C:
	// li r8,10
	ctx.r[8].s64 = 10;
	// b 0x82b82fa0
	goto loc_82B82FA0;
loc_82B82F54:
	// li r25,8
	ctx.r[25].s64 = 8;
loc_82B82F58:
	// li r11,7
	ctx.r[11].s64 = 7;
	// b 0x82b82f64
	goto loc_82B82F64;
loc_82B82F60:
	// li r11,39
	ctx.r[11].s64 = 39;
loc_82B82F64:
	// stw r11,108(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 108, ctx.r[11].u32);
	// rlwinm. r10,r27,0,24,24
	ctx.r[10].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x80;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// li r8,16
	ctx.r[8].s64 = 16;
	// beq 0x82b82fa0
	if (ctx.cr0.eq) goto loc_82B82FA0;
	// addi r11,r11,81
	ctx.r[11].s64 = ctx.r[11].s64 + 81;
	// li r10,48
	ctx.r[10].s64 = 48;
	// stb r11,89(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 89, ctx.r[11].u8);
	// li r11,2
	ctx.r[11].s64 = 2;
	// stb r10,88(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 88, ctx.r[10].u8);
	// stw r11,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[11].u32);
	// b 0x82b82fa0
	goto loc_82B82FA0;
loc_82B82F90:
	// rlwinm. r11,r27,0,24,24
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x80;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// li r8,8
	ctx.r[8].s64 = 8;
	// beq 0x82b82fa0
	if (ctx.cr0.eq) goto loc_82B82FA0;
	// ori r27,r27,512
	ctx.r[27].u64 = ctx.r[27].u64 | 512;
loc_82B82FA0:
	// rlwinm. r11,r27,0,16,16
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x8000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b82fb0
	if (!ctx.cr0.eq) goto loc_82B82FB0;
	// rlwinm. r11,r27,0,19,19
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x1000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b82fc4
	if (ctx.cr0.eq) goto loc_82B82FC4;
loc_82B82FB0:
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// ld r11,-8(r26)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[26].u32 + -8);
	// b 0x82b83014
	goto loc_82B83014;
loc_82B82FC4:
	// rlwinm. r11,r27,0,26,26
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x20;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b82ff4
	if (ctx.cr0.eq) goto loc_82B82FF4;
	// rlwinm. r11,r27,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// lwz r11,-4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
	// beq 0x82b82fec
	if (ctx.cr0.eq) goto loc_82B82FEC;
	// extsh r11,r11
	ctx.r[11].s64 = ctx.r[11].s16;
	// b 0x82b83014
	goto loc_82B83014;
loc_82B82FEC:
	// clrlwi r11,r11,16
	ctx.r[11].u64 = ctx.r[11].u32 & 0xFFFF;
	// b 0x82b83014
	goto loc_82B83014;
loc_82B82FF4:
	// rlwinm. r11,r27,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// addi r11,r26,7
	ctx.r[11].s64 = ctx.r[26].s64 + 7;
	// rlwinm r11,r11,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r26,r11,8
	ctx.r[26].s64 = ctx.r[11].s64 + 8;
	// beq 0x82b83010
	if (ctx.cr0.eq) goto loc_82B83010;
	// lwa r11,-4(r26)
	ctx.r[11].s64 = int32_t(PPC_LOAD_U32(ctx.r[26].u32 + -4));
	// b 0x82b83014
	goto loc_82B83014;
loc_82B83010:
	// lwz r11,-4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + -4);
loc_82B83014:
	// rlwinm. r10,r27,0,25,25
	ctx.r[10].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b8302c
	if (ctx.cr0.eq) goto loc_82B8302C;
	// cmpdi cr6,r11,0
	ctx.cr6.compare<int64_t>(ctx.r[11].s64, 0, ctx.xer);
	// bge cr6,0x82b8302c
	if (!ctx.cr6.lt) goto loc_82B8302C;
	// neg r11,r11
	ctx.r[11].s64 = -ctx.r[11].s64;
	// ori r27,r27,256
	ctx.r[27].u64 = ctx.r[27].u64 | 256;
loc_82B8302C:
	// rlwinm. r10,r27,0,16,16
	ctx.r[10].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x8000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82b83040
	if (!ctx.cr0.eq) goto loc_82B83040;
	// rlwinm. r10,r27,0,19,19
	ctx.r[10].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x1000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82b83040
	if (!ctx.cr0.eq) goto loc_82B83040;
	// clrldi r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 & 0xFFFFFFFF;
loc_82B83040:
	// cmpwi cr6,r25,0
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, 0, ctx.xer);
	// bge cr6,0x82b83050
	if (!ctx.cr6.lt) goto loc_82B83050;
	// li r25,1
	ctx.r[25].s64 = 1;
	// b 0x82b83060
	goto loc_82B83060;
loc_82B83050:
	// rlwinm r27,r27,0,29,27
	ctx.r[27].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0xFFFFFFFFFFFFFFF7;
	// cmpwi cr6,r25,512
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, 512, ctx.xer);
	// ble cr6,0x82b83060
	if (!ctx.cr6.gt) goto loc_82B83060;
	// li r25,512
	ctx.r[25].s64 = 512;
loc_82B83060:
	// cmpldi cr6,r11,0
	ctx.cr6.compare<uint64_t>(ctx.r[11].u64, 0, ctx.xer);
	// bne cr6,0x82b83070
	if (!ctx.cr6.eq) goto loc_82B83070;
	// li r10,0
	ctx.r[10].s64 = 0;
	// stw r10,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[10].u32);
loc_82B83070:
	// addi r9,r1,655
	ctx.r[9].s64 = ctx.r[1].s64 + 655;
loc_82B83074:
	// cmpwi cr6,r25,0
	ctx.cr6.compare<int32_t>(ctx.r[25].s32, 0, ctx.xer);
	// addi r25,r25,-1
	ctx.r[25].s64 = ctx.r[25].s64 + -1;
	// bgt cr6,0x82b83088
	if (ctx.cr6.gt) goto loc_82B83088;
	// cmpldi cr6,r11,0
	ctx.cr6.compare<uint64_t>(ctx.r[11].u64, 0, ctx.xer);
	// beq cr6,0x82b830c8
	if (ctx.cr6.eq) goto loc_82B830C8;
loc_82B83088:
	// extsw r10,r8
	ctx.r[10].s64 = ctx.r[8].s32;
	// divdu r7,r11,r10
	ctx.r[7].u64 = ctx.r[11].u64 / ctx.r[10].u64;
	// tdllei r10,0
	// mulld r7,r7,r10
	ctx.r[7].s64 = ctx.r[7].s64 * ctx.r[10].s64;
	// subf r7,r7,r11
	ctx.r[7].s64 = ctx.r[11].s64 - ctx.r[7].s64;
	// divdu r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 / ctx.r[10].u64;
	// tdllei r10,0
	// rotlwi r10,r7,0
	ctx.r[10].u64 = std::rotl(ctx.r[7].u32, 0);
	// addi r10,r10,48
	ctx.r[10].s64 = ctx.r[10].s64 + 48;
	// cmpwi cr6,r10,57
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 57, ctx.xer);
	// ble cr6,0x82b830bc
	if (!ctx.cr6.gt) goto loc_82B830BC;
	// lwz r7,108(r1)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 108);
	// add r10,r10,r7
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[7].u64;
loc_82B830BC:
	// stb r10,0(r9)
	PPC_STORE_U8(ctx.r[9].u32 + 0, ctx.r[10].u8);
	// addi r9,r9,-1
	ctx.r[9].s64 = ctx.r[9].s64 + -1;
	// b 0x82b83074
	goto loc_82B83074;
loc_82B830C8:
	// addi r11,r1,655
	ctx.r[11].s64 = ctx.r[1].s64 + 655;
	// rlwinm. r10,r27,0,22,22
	ctx.r[10].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x200;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// subf r8,r9,r11
	ctx.r[8].s64 = ctx.r[11].s64 - ctx.r[9].s64;
	// addi r28,r9,1
	ctx.r[28].s64 = ctx.r[9].s64 + 1;
	// stw r8,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[8].u32);
	// beq 0x82b83108
	if (ctx.cr0.eq) goto loc_82B83108;
	// cmpwi cr6,r8,0
	ctx.cr6.compare<int32_t>(ctx.r[8].s32, 0, ctx.xer);
	// beq cr6,0x82b830f4
	if (ctx.cr6.eq) goto loc_82B830F4;
	// lbz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[28].u32 + 0);
	// cmplwi cr6,r11,48
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 48, ctx.xer);
	// beq cr6,0x82b83108
	if (ctx.cr6.eq) goto loc_82B83108;
loc_82B830F4:
	// addi r28,r28,-1
	ctx.r[28].s64 = ctx.r[28].s64 + -1;
	// li r11,48
	ctx.r[11].s64 = 48;
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// stb r11,0(r28)
	PPC_STORE_U8(ctx.r[28].u32 + 0, ctx.r[11].u8);
loc_82B83104:
	// stw r8,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[8].u32);
loc_82B83108:
	// lwz r11,100(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 100);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82b832c8
	if (!ctx.cr6.eq) goto loc_82B832C8;
	// rlwinm. r11,r27,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b83158
	if (ctx.cr0.eq) goto loc_82B83158;
	// rlwinm. r11,r27,0,23,23
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x100;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b8312c
	if (ctx.cr0.eq) goto loc_82B8312C;
	// li r11,45
	ctx.r[11].s64 = 45;
	// b 0x82b83138
	goto loc_82B83138;
loc_82B8312C:
	// clrlwi. r11,r27,31
	ctx.r[11].u64 = ctx.r[27].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b83148
	if (ctx.cr0.eq) goto loc_82B83148;
	// li r11,43
	ctx.r[11].s64 = 43;
loc_82B83138:
	// li r30,1
	ctx.r[30].s64 = 1;
	// stb r11,88(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 88, ctx.r[11].u8);
	// stw r30,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[30].u32);
	// b 0x82b8315c
	goto loc_82B8315C;
loc_82B83148:
	// rlwinm. r11,r27,0,30,30
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b83158
	if (ctx.cr0.eq) goto loc_82B83158;
	// li r11,32
	ctx.r[11].s64 = 32;
	// b 0x82b83138
	goto loc_82B83138;
loc_82B83158:
	// lwz r30,92(r1)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
loc_82B8315C:
	// lwz r11,96(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 96);
	// rlwinm. r10,r27,0,28,29
	ctx.r[10].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0xC;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// subf r11,r8,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[8].s64;
	// subf r29,r30,r11
	ctx.r[29].s64 = ctx.r[11].s64 - ctx.r[30].s64;
	// bne 0x82b831a4
	if (!ctx.cr0.eq) goto loc_82B831A4;
	// mr r31,r29
	ctx.r[31].u64 = ctx.r[29].u64;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// ble cr6,0x82b831a4
	if (!ctx.cr6.gt) goto loc_82B831A4;
loc_82B8317C:
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// mr r4,r23
	ctx.r[4].u64 = ctx.r[23].u64;
	// li r3,32
	ctx.r[3].s64 = 32;
	// addi r31,r31,-1
	ctx.r[31].s64 = ctx.r[31].s64 + -1;
	// bl 0x82b82728
	ctx.lr = 0x82B83190;
	sub_82B82728(ctx, base);
	// lwz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, -1, ctx.xer);
	// beq cr6,0x82b831a4
	if (ctx.cr6.eq) goto loc_82B831A4;
	// cmpwi cr6,r31,0
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// bgt cr6,0x82b8317c
	if (ctx.cr6.gt) goto loc_82B8317C;
loc_82B831A4:
	// addi r6,r1,80
	ctx.r[6].s64 = ctx.r[1].s64 + 80;
	// mr r5,r23
	ctx.r[5].u64 = ctx.r[23].u64;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// addi r3,r1,88
	ctx.r[3].s64 = ctx.r[1].s64 + 88;
	// bl 0x82b83338
	ctx.lr = 0x82B831B8;
	sub_82B83338(ctx, base);
	// rlwinm. r11,r27,0,28,28
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b831fc
	if (ctx.cr0.eq) goto loc_82B831FC;
	// rlwinm. r11,r27,0,29,29
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b831fc
	if (!ctx.cr0.eq) goto loc_82B831FC;
	// mr r31,r29
	ctx.r[31].u64 = ctx.r[29].u64;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// ble cr6,0x82b831fc
	if (!ctx.cr6.gt) goto loc_82B831FC;
loc_82B831D4:
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// mr r4,r23
	ctx.r[4].u64 = ctx.r[23].u64;
	// li r3,48
	ctx.r[3].s64 = 48;
	// addi r31,r31,-1
	ctx.r[31].s64 = ctx.r[31].s64 + -1;
	// bl 0x82b82728
	ctx.lr = 0x82B831E8;
	sub_82B82728(ctx, base);
	// lwz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, -1, ctx.xer);
	// beq cr6,0x82b831fc
	if (ctx.cr6.eq) goto loc_82B831FC;
	// cmpwi cr6,r31,0
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// bgt cr6,0x82b831d4
	if (ctx.cr6.gt) goto loc_82B831D4;
loc_82B831FC:
	// lwz r4,84(r1)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// cmpwi cr6,r16,0
	ctx.cr6.compare<int32_t>(ctx.r[16].s32, 0, ctx.xer);
	// beq cr6,0x82b83270
	if (ctx.cr6.eq) goto loc_82B83270;
	// cmpwi cr6,r4,0
	ctx.cr6.compare<int32_t>(ctx.r[4].s32, 0, ctx.xer);
	// ble cr6,0x82b83270
	if (!ctx.cr6.gt) goto loc_82B83270;
	// mr r30,r28
	ctx.r[30].u64 = ctx.r[28].u64;
	// mr r31,r4
	ctx.r[31].u64 = ctx.r[4].u64;
loc_82B83218:
	// li r5,6
	ctx.r[5].s64 = 6;
	// lhz r6,0(r30)
	ctx.r[6].u64 = PPC_LOAD_U16(ctx.r[30].u32 + 0);
	// addi r4,r1,116
	ctx.r[4].s64 = ctx.r[1].s64 + 116;
	// addi r3,r1,112
	ctx.r[3].s64 = ctx.r[1].s64 + 112;
	// addi r31,r31,-1
	ctx.r[31].s64 = ctx.r[31].s64 + -1;
	// addi r30,r30,2
	ctx.r[30].s64 = ctx.r[30].s64 + 2;
	// bl 0x82b86be0
	ctx.lr = 0x82B83234;
	sub_82B86BE0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82b83264
	if (!ctx.cr0.eq) goto loc_82B83264;
	// lwz r4,112(r1)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 112);
	// cmpwi cr6,r4,0
	ctx.cr6.compare<int32_t>(ctx.r[4].s32, 0, ctx.xer);
	// beq cr6,0x82b83264
	if (ctx.cr6.eq) goto loc_82B83264;
	// addi r6,r1,80
	ctx.r[6].s64 = ctx.r[1].s64 + 80;
	// mr r5,r23
	ctx.r[5].u64 = ctx.r[23].u64;
	// addi r3,r1,116
	ctx.r[3].s64 = ctx.r[1].s64 + 116;
	// bl 0x82b83338
	ctx.lr = 0x82B83258;
	sub_82B83338(ctx, base);
	// cmpwi cr6,r31,0
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// bne cr6,0x82b83218
	if (!ctx.cr6.eq) goto loc_82B83218;
	// b 0x82b83280
	goto loc_82B83280;
loc_82B83264:
	// li r24,-1
	ctx.r[24].s64 = -1;
	// stw r24,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[24].u32);
	// b 0x82b83284
	goto loc_82B83284;
loc_82B83270:
	// addi r6,r1,80
	ctx.r[6].s64 = ctx.r[1].s64 + 80;
	// mr r5,r23
	ctx.r[5].u64 = ctx.r[23].u64;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// bl 0x82b83338
	ctx.lr = 0x82B83280;
	sub_82B83338(ctx, base);
loc_82B83280:
	// lwz r24,80(r1)
	ctx.r[24].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
loc_82B83284:
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// blt cr6,0x82b832c8
	if (ctx.cr6.lt) goto loc_82B832C8;
	// rlwinm. r11,r27,0,29,29
	ctx.r[11].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b832c8
	if (ctx.cr0.eq) goto loc_82B832C8;
	// mr r31,r29
	ctx.r[31].u64 = ctx.r[29].u64;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// ble cr6,0x82b832c8
	if (!ctx.cr6.gt) goto loc_82B832C8;
loc_82B832A0:
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// mr r4,r23
	ctx.r[4].u64 = ctx.r[23].u64;
	// li r3,32
	ctx.r[3].s64 = 32;
	// addi r31,r31,-1
	ctx.r[31].s64 = ctx.r[31].s64 + -1;
	// bl 0x82b82728
	ctx.lr = 0x82B832B4;
	sub_82B82728(ctx, base);
	// lwz r24,80(r1)
	ctx.r[24].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// cmpwi cr6,r24,-1
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, -1, ctx.xer);
	// beq cr6,0x82b832c8
	if (ctx.cr6.eq) goto loc_82B832C8;
	// cmpwi cr6,r31,0
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// bgt cr6,0x82b832a0
	if (ctx.cr6.gt) goto loc_82B832A0;
loc_82B832C8:
	// cmplwi cr6,r15,0
	ctx.cr6.compare<uint32_t>(ctx.r[15].u32, 0, ctx.xer);
	// beq cr6,0x82b832dc
	if (ctx.cr6.eq) goto loc_82B832DC;
	// mr r3,r15
	ctx.r[3].u64 = ctx.r[15].u64;
	// bl 0x823addc0
	ctx.lr = 0x82B832D8;
	sub_823ADDC0(ctx, base);
	// li r15,0
	ctx.r[15].s64 = 0;
loc_82B832DC:
	// lbz r29,0(r19)
	ctx.r[29].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// extsb. r10,r29
	ctx.r[10].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b832f0
	if (ctx.cr0.eq) goto loc_82B832F0;
	// lwz r8,84(r1)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// b 0x82b8296c
	goto loc_82B8296C;
loc_82B832F0:
	// mr r3,r24
	ctx.r[3].u64 = ctx.r[24].u64;
	// b 0x82b8332c
	goto loc_82B8332C;
loc_82B832F8:
	// mr r6,r26
	ctx.r[6].u64 = ctx.r[26].u64;
	// mr r5,r19
	ctx.r[5].u64 = ctx.r[19].u64;
	// li r4,512
	ctx.r[4].s64 = 512;
	// addi r3,r1,656
	ctx.r[3].s64 = ctx.r[1].s64 + 656;
	// bl 0x82b7d260
	ctx.lr = 0x82B8330C;
	sub_82B7D260(ctx, base);
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// cmpwi cr6,r31,-1
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, -1, ctx.xer);
	// bne cr6,0x82b83320
	if (!ctx.cr6.eq) goto loc_82B83320;
	// li r31,511
	ctx.r[31].s64 = 511;
	// stb r27,1167(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 1167, ctx.r[27].u8);
loc_82B83320:
	// addi r3,r1,656
	ctx.r[3].s64 = ctx.r[1].s64 + 656;
	// bl 0x823add70
	ctx.lr = 0x82B83328;
	sub_823ADD70(ctx, base);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
loc_82B8332C:
	// addi r1,r1,1328
	ctx.r[1].s64 = ctx.r[1].s64 + 1328;
	// b 0x82b7a700
	__restgprlr_14(ctx, base);
	return;
}
void Body_82B83338(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x82B83340;
	__savegprlr_28(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r28,r5
	ctx.r[28].u64 = ctx.r[5].u64;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// mr r31,r6
	ctx.r[31].u64 = ctx.r[6].u64;
	// lwz r11,12(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b833c0
	if (ctx.cr0.eq) goto loc_82B833C0;
	// lwz r11,8(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 8);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82b833c0
	if (!ctx.cr6.eq) goto loc_82B833C0;
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// add r11,r11,r30
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[30].u64;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
	// b 0x82b833c8
	goto loc_82B833C8;
loc_82B8337C:
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// lbz r3,0(r29)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// addi r30,r30,-1
	ctx.r[30].s64 = ctx.r[30].s64 + -1;
	// bl 0x82b82728
	ctx.lr = 0x82B83390;
	sub_82B82728(ctx, base);
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r29,r29,1
	ctx.r[29].s64 = ctx.r[29].s64 + 1;
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, -1, ctx.xer);
	// bne cr6,0x82b833c0
	if (!ctx.cr6.eq) goto loc_82B833C0;
	// bl 0x82b7fd78
	ctx.lr = 0x82B833A4;
	sub_82B7FD78(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// cmpwi cr6,r11,42
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 42, ctx.xer);
	// bne cr6,0x82b833c8
	if (!ctx.cr6.eq) goto loc_82B833C8;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// li r3,63
	ctx.r[3].s64 = 63;
	// bl 0x82b82728
	ctx.lr = 0x82B833C0;
	sub_82B82728(ctx, base);
loc_82B833C0:
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// bgt cr6,0x82b8337c
	if (ctx.cr6.gt) goto loc_82B8337C;
loc_82B833C8:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}
void Body_82B7D168(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r30,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[30].u64);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r11,r5
	ctx.r[11].u64 = ctx.r[5].u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r5,r6
	ctx.r[5].u64 = ctx.r[6].u64;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82b7d1c0
	if (!ctx.cr6.eq) goto loc_82B7D1C0;
loc_82B7D190:
	// bl 0x82b7fd78
	ctx.lr = 0x82B7D194;
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
	ctx.lr = 0x82B7D1B8;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b7d244
	goto loc_82B7D244;
loc_82B7D1C0:
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 0, ctx.xer);
	// beq cr6,0x82b7d1d0
	if (ctx.cr6.eq) goto loc_82B7D1D0;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x82b7d190
	if (ctx.cr6.eq) goto loc_82B7D190;
loc_82B7D1D0:
	// lis r10,32767
	ctx.r[10].s64 = 2147418112;
	// ori r10,r10,65535
	ctx.r[10].u64 = ctx.r[10].u64 | 65535;
	// cmplw cr6,r4,r10
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, ctx.r[10].u32, ctx.xer);
	// stw r10,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[10].u32);
	// bgt cr6,0x82b7d1e8
	if (ctx.cr6.gt) goto loc_82B7D1E8;
	// stw r4,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[4].u32);
loc_82B7D1E8:
	// mr r4,r11
	ctx.r[4].u64 = ctx.r[11].u64;
	// stw r31,88(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 88, ctx.r[31].u32);
	// li r11,66
	ctx.r[11].s64 = 66;
	// stw r31,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[31].u32);
	// mr r6,r7
	ctx.r[6].u64 = ctx.r[7].u64;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// stw r11,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[11].u32);
	// bl 0x82b827c0
	ctx.lr = 0x82B7D208;
	sub_82B827C0(ctx, base);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x82b7d240
	if (ctx.cr6.eq) goto loc_82B7D240;
	// lwz r11,84(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[11].u32);
	// blt 0x82b7d234
	if (ctx.cr0.lt) goto loc_82B7D234;
	// lwz r10,80(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r10)
	PPC_STORE_U8(ctx.r[10].u32 + 0, ctx.r[11].u8);
	// b 0x82b7d240
	goto loc_82B7D240;
loc_82B7D234:
	// addi r4,r1,80
	ctx.r[4].s64 = ctx.r[1].s64 + 80;
	// li r3,0
	ctx.r[3].s64 = 0;
	// bl 0x82b82558
	ctx.lr = 0x82B7D240;
	sub_82B82558(ctx, base);
loc_82B7D240:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
loc_82B7D244:
	// addi r1,r1,144
	ctx.r[1].s64 = ctx.r[1].s64 + 144;
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
void Body_82B7D260(Context& ctx, [[maybe_unused]] Base& base) {
	// mr r7,r6
	ctx.r[7].u64 = ctx.r[6].u64;
	// li r6,0
	ctx.r[6].s64 = 0;
	// b 0x82b7d168
	sub_82B7D168(ctx, base);
	return;
}
void Body_82B7A680(Context& ctx, [[maybe_unused]] Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// rlwinm r10,r3,1,23,30
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 1) & 0x1FE;
	// lwz r11,21248(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 21248);
	// lwz r11,200(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 200);
	// lhzx r11,r10,r11
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[10].u32 + ctx.r[11].u32);
	// rlwinm r3,r11,0,0,16
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFF8000;
	// blr
	return;
}
void Body_82B85420(Context& ctx, [[maybe_unused]] Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// lwz r11,24576(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 24576);
	// ori r10,r11,1
	ctx.r[10].u64 = ctx.r[11].u64 | 1;
	// lis r11,-31955
	ctx.r[11].s64 = -2094202880;
	// lwz r11,15592(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 15592);
	// subf r11,r11,r10
	ctx.r[11].s64 = ctx.r[10].s64 - ctx.r[11].s64;
	// cntlzw r11,r11
	ctx.r[11].u64 = ctx.r[11].u32 == 0 ? 32 : std::countl_zero(ctx.r[11].u32);
	// rlwinm r3,r11,27,31,31
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// blr
	return;
}
#undef PPC_LOAD_U8
#undef PPC_STORE_U8
#undef PPC_LOAD_U16
#undef PPC_STORE_U16
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U64
#undef PPC_CALL_INDIRECT_FUNC
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
 Context ctx{};FromFull(ctx,state);Base base{memory,dependencies,state};
 switch(entry) {
 case 0x82b827c0u:Body_82B827C0(ctx,base);break;
 case 0x82b83338u:Body_82B83338(ctx,base);break;
 case 0x82b7d168u:Body_82B7D168(ctx,base);break;
 case 0x82b7d260u:Body_82B7D260(ctx,base);break;
 default:return false;
 }
 ToFull(state,ctx);return true;
}
}
