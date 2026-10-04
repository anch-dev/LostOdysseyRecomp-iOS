#include "lo_semantics/crt_narrow_callers61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_stream_counted_output.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_narrow_callers61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Direct(GuestAddress entry,Context& ctx,Base& base) {
 ToFull(base.full,ctx);
 if(!crt_narrow_callers61::ApplyAcceptedLower(entry,base.memory,base.dependencies,base.full))throw std::logic_error("missing actual narrow caller lower");
 FromFull(ctx,base.full);
}
void __savegprlr_29(Context& c,Base& b){Save(29,c,b);}
void __restgprlr_29(Context& c,Base& b){Restore(29,c,b);}
void Body_822A03C8(Context&,Base&);
void sub_822A03C8(Context& c,Base& b){Body_822A03C8(c,b);}
void Body_82B853F0(Context&,Base&);
void sub_82B853F0(Context& c,Base& b){Body_82B853F0(c,b);}
void sub_82B7FD78(Context& c,Base& b){Direct(0x82B7FD78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b){Direct(0x82B7FEC0u,c,b);}
void sub_82B827C0(Context& c,Base& b){Direct(0x82B827C0u,c,b);}
void sub_82B82558(Context& c,Base& b){Direct(0x82B82558u,c,b);}
void sub_82B7B778(Context& c,Base& b){Direct(0x82B7B778u,c,b);}
void sub_82B81278(Context& c,Base& b){Direct(0x82B81278u,c,b);}
void sub_82319E78(Context& c,Base& b){Direct(0x82319E78u,c,b);}
void sub_82B7B810(Context& c,Base& b){Direct(0x82B7B810u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82B7CB20(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
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
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 0, ctx.xer);
	// bne cr6,0x82b7cb84
	if (!ctx.cr6.eq) goto loc_82B7CB84;
loc_82B7CB54:
	// bl 0x82b7fd78
	ctx.lr = 0x82B7CB58;
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
	ctx.lr = 0x82B7CB7C;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b7cbf8
	goto loc_82B7CBF8;
loc_82B7CB84:
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82b7cb54
	if (ctx.cr6.eq) goto loc_82B7CB54;
	// addi r10,r1,80
	ctx.r[10].s64 = ctx.r[1].s64 + 80;
	// stw r11,104(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 104, ctx.r[11].u32);
	// addi r9,r1,176
	ctx.r[9].s64 = ctx.r[1].s64 + 176;
	// stw r11,96(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 96, ctx.r[11].u32);
	// lis r8,32767
	ctx.r[8].s64 = 2147418112;
	// li r5,0
	ctx.r[5].s64 = 0;
	// ori r8,r8,65535
	ctx.r[8].u64 = ctx.r[8].u64 | 65535;
	// addi r3,r1,96
	ctx.r[3].s64 = ctx.r[1].s64 + 96;
	// stw r9,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[9].u32);
	// li r10,66
	ctx.r[10].s64 = 66;
	// lwz r6,80(r1)
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// stw r8,100(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 100, ctx.r[8].u32);
	// stw r10,108(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 108, ctx.r[10].u32);
	// bl 0x82b827c0
	ctx.lr = 0x82B7CBC4;
	sub_82B827C0(ctx, base);
	// lwz r11,100(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 100);
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,100(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 100, ctx.r[11].u32);
	// blt 0x82b7cbe8
	if (ctx.cr0.lt) goto loc_82B7CBE8;
	// lwz r10,96(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 96);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r10)
	PPC_STORE_U8(ctx.r[10].u32 + 0, ctx.r[11].u8);
	// b 0x82b7cbf4
	goto loc_82B7CBF4;
loc_82B7CBE8:
	// addi r4,r1,96
	ctx.r[4].s64 = ctx.r[1].s64 + 96;
	// li r3,0
	ctx.r[3].s64 = 0;
	// bl 0x82b82558
	ctx.lr = 0x82B7CBF4;
	sub_82B82558(ctx, base);
loc_82B7CBF4:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
loc_82B7CBF8:
	// addi r1,r1,144
	ctx.r[1].s64 = ctx.r[1].s64 + 144;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r31,-16(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// blr
	return;
}

void Body_82B85300(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82B85308;
	__savegprlr_29(ctx, base);
	// std r4,24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 24, ctx.r[4].u64);
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
	// addi r31,r1,-128
	ctx.r[31].s64 = ctx.r[1].s64 + -128;
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// cntlzw r11,r29
	ctx.r[11].u64 = ctx.r[29].u32 == 0 ? 32 : std::countl_zero(ctx.r[29].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b85374
	if (!ctx.cr0.eq) goto loc_82B85374;
	// bl 0x82b7fd78
	ctx.lr = 0x82B85348;
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
	ctx.lr = 0x82B8536C;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b853e8
	goto loc_82B853E8;
loc_82B85374:
	// addi r11,r31,80
	ctx.r[11].s64 = ctx.r[31].s64 + 80;
	// addi r10,r31,152
	ctx.r[10].s64 = ctx.r[31].s64 + 152;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// bl 0x822a03c8
	ctx.lr = 0x82B85384;
	sub_822A03C8(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r4,r11,32
	ctx.r[4].s64 = ctx.r[11].s64 + 32;
	// bl 0x82b7b778
	ctx.lr = 0x82B85394;
	sub_82B7B778(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// bl 0x822a03c8
	ctx.lr = 0x82B8539C;
	sub_822A03C8(ctx, base);
	// addi r3,r3,32
	ctx.r[3].s64 = ctx.r[3].s64 + 32;
	// bl 0x82b81278
	ctx.lr = 0x82B853A4;
	sub_82B81278(ctx, base);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// bl 0x822a03c8
	ctx.lr = 0x82B853AC;
	sub_822A03C8(ctx, base);
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// addi r3,r3,32
	ctx.r[3].s64 = ctx.r[3].s64 + 32;
	// li r5,0
	ctx.r[5].s64 = 0;
	// lwz r6,80(r31)
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// bl 0x82b827c0
	ctx.lr = 0x82B853C0;
	sub_82B827C0(ctx, base);
	// stw r3,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[3].u32);
	// bl 0x822a03c8
	ctx.lr = 0x82B853C8;
	sub_822A03C8(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// addi r4,r11,32
	ctx.r[4].s64 = ctx.r[11].s64 + 32;
	// bl 0x82319e78
	ctx.lr = 0x82B853D8;
	sub_82319E78(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,128
	ctx.r[12].s64 = ctx.r[31].s64 + 128;
	// bl 0x82b853f0
	ctx.lr = 0x82B853E4;
	sub_82B853F0(ctx, base);
	// lwz r3,84(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 84);
loc_82B853E8:
	// addi r1,r31,128
	ctx.r[1].s64 = ctx.r[31].s64 + 128;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}

void Body_82B853F0(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// bl 0x822a03c8
	ctx.lr = 0x82B85400;
	sub_822A03C8(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r4,r11,32
	ctx.r[4].s64 = ctx.r[11].s64 + 32;
	// bl 0x82b7b810
	ctx.lr = 0x82B85410;
	sub_82B7B810(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_822A03C8(Context& ctx, [[maybe_unused]] Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r3,r11,19184
	ctx.r[3].s64 = ctx.r[11].s64 + 19184;
	// blr
	return;
}
#undef PPC_LOAD_U8
#undef PPC_STORE_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U64
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,Dependencies d,Registers& state) {
 if(entry==0x82b827c0u)return crt_narrow_formatter61::Apply(entry,memory,d,state);
 auto lower=crt_context_adapter::ToStream(state);
 const auto& streams=d.accepted.close.pipeline.close.accepted;
 bool found=crt_format_stream::Apply(entry,memory,d.accepted.close.pipeline.format,lower);
 if(!found)found=crt_stream_counted_output::Apply(entry,memory,streams,lower);
 if(found){crt_context_adapter::FromStream(state,lower);return true;}
 return crt_stream_close_shared_lower::ApplyAcceptedLower(entry,memory,d.accepted,state);
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies d,Registers& state) {
 Context ctx{};FromFull(ctx,state);Base base{memory,d,state};
 switch(entry){case 0x82b7cb20u:Body_82B7CB20(ctx,base);break;
 case 0x82b85300u:Body_82B85300(ctx,base);break;
 case 0x82b853f0u:Body_82B853F0(ctx,base);break;default:return false;}
 ToFull(state,ctx);return true;
}
}
