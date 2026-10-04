#include "lo_semantics/crt_error_format_upper61.h"
#include "lo_semantics/crt_error_text61_context.h"
#include "lo_semantics/crt_format_frame61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_error_format_upper61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base{GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Body_82DF4270(Context&,Base&);
void Body_82DF28C8(Context&,Base&);
void sub_82DF28C8(Context& c,Base& b){Body_82DF28C8(c,b);}
void __savegprlr_26(Context& c,Base& b){Save(26,c,b);}
void __restgprlr_26(Context& c,Base& b){Restore(26,c,b);}
void Direct(GuestAddress entry,Context& c,Base& b){
 ToFull(b.full,c);
 if(entry==0x82df4218u){
  if(!crt_error_text61_context::Apply(entry,b.memory,b.full))throw std::logic_error("missing accepted error text");
 }else if(entry==0x82b827c0u){
  if(!crt_narrow_formatter61::Apply(entry,b.memory,b.dependencies,b.full))throw std::logic_error("missing accepted narrow formatter");
 }else if(entry==0x82df2ac8u)
  crt_format_frame61::ApplySupport_DF2AC8(b.memory,b.dependencies.accepted,b.full);
 else if(entry==0x82b81648u)
  crt_reader_chain61::ApplySupport_B81648(b.memory,b.dependencies.accepted,b.full);
 else if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,b.memory,b.dependencies.accepted,b.full)){
  auto lower=crt_context_adapter::ToStream(b.full);const auto& accepted=b.dependencies.accepted;
  bool found=false;
  if(entry==0x82b7b708u)found=crt_stream_bulk_close_routes::Apply(entry,b.memory,accepted.close,lower);
  else if(entry==0x82b81278u||entry==0x82319e78u)found=crt_format_stream::Apply(entry,b.memory,accepted.close.pipeline.format,lower);
  else found=crt_stream_operations::ApplyAcceptedCallee(entry,b.memory,accepted.close.pipeline.close.accepted,lower);
  if(!found)throw std::logic_error("unselected error format direct lower");
  crt_context_adapter::FromStream(b.full,lower);
 }
 FromFull(c,b.full);
}
void sub_822A03C8(Context& c,Base& b){Direct(0x822A03C8u,c,b);}
void sub_82B7FD78(Context& c,Base& b){Direct(0x82B7FD78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b){Direct(0x82B7FEC0u,c,b);}
void sub_82DF4218(Context& c,Base& b){Direct(0x82DF4218u,c,b);}
void sub_82B7B708(Context& c,Base& b){Direct(0x82B7B708u,c,b);}
void sub_82B81648(Context& c,Base& b){Direct(0x82B81648u,c,b);}
void sub_82B81278(Context& c,Base& b){Direct(0x82B81278u,c,b);}
void sub_82B827C0(Context& c,Base& b){Direct(0x82B827C0u,c,b);}
void sub_82319E78(Context& c,Base& b){Direct(0x82319E78u,c,b);}
void sub_82DF2AC8(Context& c,Base& b){Direct(0x82DF2AC8u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82DF4270(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r30,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[30].u64);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x82df42d0
	if (ctx.cr6.eq) goto loc_82DF42D0;
	// lbz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[30].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82df42d0
	if (ctx.cr6.eq) goto loc_82DF42D0;
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// addi r31,r11,31760
	ctx.r[31].s64 = ctx.r[11].s64 + 31760;
	// bl 0x822a03c8
	ctx.lr = 0x82DF42A8;
	sub_822A03C8(ctx, base);
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// addi r3,r3,64
	ctx.r[3].s64 = ctx.r[3].s64 + 64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x82df28c8
	ctx.lr = 0x82DF42B8;
	sub_82DF28C8(ctx, base);
	// lis r11,-32235
	ctx.r[11].s64 = -2112552960;
	// addi r31,r11,32084
	ctx.r[31].s64 = ctx.r[11].s64 + 32084;
	// bl 0x822a03c8
	ctx.lr = 0x82DF42C4;
	sub_822A03C8(ctx, base);
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// addi r3,r3,64
	ctx.r[3].s64 = ctx.r[3].s64 + 64;
	// bl 0x82df28c8
	ctx.lr = 0x82DF42D0;
	sub_82DF28C8(ctx, base);
loc_82DF42D0:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF42D4;
	sub_82B7FD78(ctx, base);
	// lwz r3,0(r3)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// bl 0x82df4218
	ctx.lr = 0x82DF42DC;
	sub_82DF4218(ctx, base);
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// addi r31,r11,27232
	ctx.r[31].s64 = ctx.r[11].s64 + 27232;
	// bl 0x822a03c8
	ctx.lr = 0x82DF42EC;
	sub_822A03C8(ctx, base);
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// addi r3,r3,64
	ctx.r[3].s64 = ctx.r[3].s64 + 64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x82df28c8
	ctx.lr = 0x82DF42FC;
	sub_82DF28C8(ctx, base);
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
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
void Body_82DF28C8(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x82DF28D0;
	__savegprlr_26(ctx, base);
	// std r5,32(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 32, ctx.r[5].u64);
	// std r6,40(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 40, ctx.r[6].u64);
	// std r7,48(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 48, ctx.r[7].u64);
	// std r8,56(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 56, ctx.r[8].u64);
	// std r9,64(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 64, ctx.r[9].u64);
	// std r10,72(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 72, ctx.r[10].u64);
	// addi r31,r1,-160
	ctx.r[31].s64 = ctx.r[1].s64 + -160;
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// mr r27,r4
	ctx.r[27].u64 = ctx.r[4].u64;
	// cntlzw r11,r30
	ctx.r[11].u64 = ctx.r[30].u32 == 0 ? 32 : std::countl_zero(ctx.r[30].u32);
	// li r26,0
	ctx.r[26].s64 = 0;
	// stw r26,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[26].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df2944
	if (!ctx.cr0.eq) goto loc_82DF2944;
loc_82DF2914:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF2918;
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
	ctx.lr = 0x82DF293C;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df2ac0
	goto loc_82DF2AC0;
loc_82DF2944:
	// cntlzw r11,r27
	ctx.r[11].u64 = ctx.r[27].u32 == 0 ? 32 : std::countl_zero(ctx.r[27].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df2914
	if (ctx.cr0.eq) goto loc_82DF2914;
	// addi r11,r31,84
	ctx.r[11].s64 = ctx.r[31].s64 + 84;
	// addi r10,r31,192
	ctx.r[10].s64 = ctx.r[31].s64 + 192;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// stw r30,88(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 88, ctx.r[30].u32);
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// bl 0x82b7b708
	ctx.lr = 0x82DF2970;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwz r11,12(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df2a78
	if (!ctx.cr0.eq) goto loc_82DF2A78;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF2988;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df29d8
	if (ctx.cr6.eq) goto loc_82DF29D8;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF2998;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82df29d8
	if (ctx.cr6.eq) goto loc_82DF29D8;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF29A8;
	sub_82B81648(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// rlwinm r28,r11,2,0,29
	ctx.r[28].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF29C0;
	sub_82B81648(ctx, base);
	// lwzx r10,r28,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r10,r11,r10
	ctx.r[10].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r28,r11,21272
	ctx.r[28].s64 = ctx.r[11].s64 + 21272;
	// b 0x82df29ec
	goto loc_82DF29EC;
loc_82DF29D8:
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r28,r11,21272
	ctx.r[28].s64 = ctx.r[11].s64 + 21272;
	// mr r10,r28
	ctx.r[10].u64 = ctx.r[28].u64;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
loc_82DF29EC:
	// lbz r11,40(r10)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[10].u32 + 40);
	// rlwinm. r11,r11,0,0,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df2a48
	if (!ctx.cr0.eq) goto loc_82DF2A48;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF2A00;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df2a3c
	if (ctx.cr6.eq) goto loc_82DF2A3C;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF2A10;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82df2a3c
	if (ctx.cr6.eq) goto loc_82DF2A3C;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF2A20;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// rlwinm r28,r11,2,0,29
	ctx.r[28].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF2A30;
	sub_82B81648(ctx, base);
	// lwzx r10,r28,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r28,r11,r10
	ctx.r[28].u64 = ctx.r[11].u64 + ctx.r[10].u64;
loc_82DF2A3C:
	// lbz r11,40(r28)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[28].u32 + 40);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df2a78
	if (ctx.cr0.eq) goto loc_82DF2A78;
loc_82DF2A48:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF2A4C;
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
	ctx.lr = 0x82DF2A70;
	sub_82B7FEC0(ctx, base);
	// li r26,-1
	ctx.r[26].s64 = -1;
	// stw r26,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[26].u32);
loc_82DF2A78:
	// cmpwi cr6,r26,0
	ctx.cr6.compare<int32_t>(ctx.r[26].s32, 0, ctx.xer);
	// bne cr6,0x82df2ab0
	if (!ctx.cr6.eq) goto loc_82DF2AB0;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81278
	ctx.lr = 0x82DF2A88;
	sub_82B81278(ctx, base);
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// lwz r6,84(r31)
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 84);
	// li r5,0
	ctx.r[5].s64 = 0;
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b827c0
	ctx.lr = 0x82DF2AA0;
	sub_82B827C0(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82319e78
	ctx.lr = 0x82DF2AB0;
	sub_82319E78(ctx, base);
loc_82DF2AB0:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,160
	ctx.r[12].s64 = ctx.r[31].s64 + 160;
	// bl 0x82df2ac8
	ctx.lr = 0x82DF2ABC;
	sub_82DF2AC8(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82DF2AC0:
	// addi r1,r31,160
	ctx.r[1].s64 = ctx.r[31].s64 + 160;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
}
#undef PPC_LOAD_U8
#undef PPC_STORE_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state){
 Context c{};FromFull(c,state);Base b{memory,dependencies,state};
 switch(entry){case 0x82df4270u:Body_82DF4270(c,b);break;
 case 0x82df28c8u:Body_82DF28C8(c,b);break;default:return false;}
 ToFull(state,c);return true;
}
}
