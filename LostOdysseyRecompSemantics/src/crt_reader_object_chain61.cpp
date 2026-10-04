#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include <stdexcept>
#include <bit>
namespace lo::semantic::gpu::crt_reader_object_chain61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies deps; Registers& full; };
void __savegprlr_26(Context& c,Base& b) { Save(26u,c,b); }
void __restgprlr_26(Context& c,Base& b) { Restore(26u,c,b); }
void __savegprlr_29(Context& c,Base& b) { Save(29u,c,b); }
void __restgprlr_29(Context& c,Base& b) { Restore(29u,c,b); }
void Direct(GuestAddress entry,Context& c,Base& b) {
 ToFull(b.full,c); bool ok=false;
 switch(entry) {
 case 0x82bd0cd0u: ok=crt_close_upper61::Apply(entry,b.memory,b.deps,b.full);break;
 case 0x82bd0df0u: ok=crt_reader_cleanup_callers_context::Apply(entry,b.memory,b.deps.guest,b.full);break;
 case 0x82bd0900u: ok=crt_close_next61::Apply(entry,b.memory,b.deps.guest,b.full);break;
 case 0x82b7a0b0u: ok=crt_copy_full_context::Apply(entry,b.memory,b.full);break;
 default: ok=crt_close_recursive_buffer_context::Apply(entry,b.memory,b.deps.guest,b.full);
 }
 if(!ok) throw std::logic_error("missing accepted object-chain lower"); FromFull(c,b.full);
}
void Indirect(GuestAddress target,Context& c,Base& b) { ToFull(b.full,c); b.deps.guest.CallIndirect(target,b.memory,b.full); FromFull(c,b.full); }
void Body_82BD0EA8(Context&,Base&); void Body_82BADA00(Context&,Base&);
void sub_82BD0EA8(Context& c,Base& b) { Body_82BD0EA8(c,b); }
void sub_82BADA00(Context& c,Base& b) { Body_82BADA00(c,b); }
void sub_82BD0CD0(Context& c,Base& b) { Direct(0x82bd0cd0u,c,b); }
void sub_82BD0DF0(Context& c,Base& b) { Direct(0x82bd0df0u,c,b); }
void sub_82BD0900(Context& c,Base& b) { Direct(0x82bd0900u,c,b); }
void sub_82B7A0B0(Context& c,Base& b) { Direct(0x82b7a0b0u,c,b); }
void sub_82BD0C18(Context& c,Base& b) { Direct(0x82bd0c18u,c,b); }
void sub_82BD0798(Context& c,Base& b) { Direct(0x82bd0798u,c,b); }
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_CALL_INDIRECT_FUNC(t) Indirect((t),ctx,base)
void Body_82BD0EA8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x82BD0EB0;
	__savegprlr_26(ctx, base);
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r28,r3
	ctx.r[28].u64 = ctx.r[3].u64;
	// mr r31,r4
	ctx.r[31].u64 = ctx.r[4].u64;
	// bl 0x82bd0c18
	ctx.lr = 0x82BD0EC0;
	sub_82BD0C18(ctx, base);
	// lwz r29,4(r28)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// li r26,0
	ctx.r[26].s64 = 0;
	// bne cr6,0x82bd0f60
	if (!ctx.cr6.eq) goto loc_82BD0F60;
	// lwz r11,8(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 8);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd0ef8
	if (ctx.cr6.eq) goto loc_82BD0EF8;
	// bl 0x82bd0798
	ctx.lr = 0x82BD0EE0;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,8(r28)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 8);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0EF4;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r26,8(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 8, ctx.r[26].u32);
loc_82BD0EF8:
	// lwz r11,4(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// mr r9,r26
	ctx.r[9].u64 = ctx.r[26].u64;
	// lwz r10,12(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82bd0f24
	if (ctx.cr6.eq) goto loc_82BD0F24;
loc_82BD0F0C:
	// lwz r8,4(r11)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// mr r11,r10
	ctx.r[11].u64 = ctx.r[10].u64;
	// add r9,r8,r9
	ctx.r[9].u64 = ctx.r[8].u64 + ctx.r[9].u64;
	// lwz r10,12(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// bne cr6,0x82bd0f0c
	if (!ctx.cr6.eq) goto loc_82BD0F0C;
loc_82BD0F24:
	// lwz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// add r31,r11,r9
	ctx.r[31].u64 = ctx.r[11].u64 + ctx.r[9].u64;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x82bd0f54
	if (ctx.cr6.eq) goto loc_82BD0F54;
	// bl 0x82bd0798
	ctx.lr = 0x82BD0F38;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// li r5,65
	ctx.r[5].s64 = 65;
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0F50;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// b 0x82bd0f58
	goto loc_82BD0F58;
loc_82BD0F54:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
loc_82BD0F58:
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// stw r3,8(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 8, ctx.r[3].u32);
loc_82BD0F60:
	// mr r27,r31
	ctx.r[27].u64 = ctx.r[31].u64;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x82bd0fb8
	if (ctx.cr6.eq) goto loc_82BD0FB8;
	// lwz r11,12(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd0fa4
	if (ctx.cr6.eq) goto loc_82BD0FA4;
loc_82BD0F78:
	// addi r30,r29,4
	ctx.r[30].s64 = ctx.r[29].s64 + 4;
	// lwz r4,0(r29)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 0);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r5,0(r30)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// bl 0x82b7a0b0
	ctx.lr = 0x82BD0F8C;
	sub_82B7A0B0(ctx, base);
	// lwz r29,12(r29)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// add r31,r31,r11
	ctx.r[31].u64 = ctx.r[31].u64 + ctx.r[11].u64;
	// lwz r11,12(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82bd0f78
	if (!ctx.cr6.eq) goto loc_82BD0F78;
loc_82BD0FA4:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r5,4(r29)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 4);
	// lwz r4,0(r29)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 0);
	// bl 0x82b7a0b0
	ctx.lr = 0x82BD0FB4;
	sub_82B7A0B0(ctx, base);
	// sth r26,20(r28)
	PPC_STORE_U16(ctx.r[28].u32 + 20, ctx.r[26].u16);
loc_82BD0FB8:
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// addi r1,r1,144
	ctx.r[1].s64 = ctx.r[1].s64 + 144;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
}
void Body_82BADA00(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// clrlwi r10,r4,24
	ctx.r[10].u64 = ctx.r[4].u32 & 0xFF;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// stw r11,116(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 116, ctx.r[11].u32);
	// beq cr6,0x82bada44
	if (ctx.cr6.eq) goto loc_82BADA44;
	// lbz r11,119(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 119);
	// stb r11,80(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 80, ctx.r[11].u8);
	// lbz r11,118(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 118);
	// stb r11,81(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 81, ctx.r[11].u8);
	// lbz r11,117(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 117);
	// stb r11,82(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 82, ctx.r[11].u8);
	// lbz r11,116(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 116);
	// stb r11,83(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 83, ctx.r[11].u8);
	// lwz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
loc_82BADA44:
	// lwz r10,0(r5)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[5].u32 + 0);
	// mr r4,r11
	ctx.r[4].u64 = ctx.r[11].u64;
	// mr r3,r5
	ctx.r[3].u64 = ctx.r[5].u64;
	// lwz r11,36(r10)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 36);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BADA5C;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
void Body_82BB44A8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82BB44B0;
	__savegprlr_29(ctx, base);
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// mr r29,r4
	ctx.r[29].u64 = ctx.r[4].u64;
	// mr r31,r5
	ctx.r[31].u64 = ctx.r[5].u64;
	// li r5,0
	ctx.r[5].s64 = 0;
	// li r4,4096
	ctx.r[4].s64 = 4096;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// bl 0x82bd0cd0
	ctx.lr = 0x82BB44D0;
	sub_82BD0CD0(ctx, base);
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// lwz r11,24(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 24);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BB44EC;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// bl 0x82bd0900
	ctx.lr = 0x82BB44F4;
	sub_82BD0900(ctx, base);
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// bl 0x82bada00
	ctx.lr = 0x82BB4504;
	sub_82BADA00(ctx, base);
	// li r4,0
	ctx.r[4].s64 = 0;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// lwz r29,0(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// bl 0x82bd0ea8
	ctx.lr = 0x82BB4514;
	sub_82BD0EA8(ctx, base);
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// lwz r11,48(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 48);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BB452C;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// bl 0x82bd0df0
	ctx.lr = 0x82BB4534;
	sub_82BD0DF0(ctx, base);
	// addi r1,r1,144
	ctx.r[1].s64 = ctx.r[1].s64 + 144;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U8
#undef PPC_STORE_U16
#undef PPC_STORE_U32
#undef PPC_CALL_INDIRECT_FUNC
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
 if(entry!=0x82bd0ea8u && entry!=0x82bada00u && entry!=0x82bb44a8u) return false;
 Context ctx{}; FromFull(ctx,state); Base base{memory,deps,state};
 switch(entry) {case 0x82bd0ea8u: Body_82BD0EA8(ctx,base);break;case 0x82bada00u: Body_82BADA00(ctx,base);break;default: Body_82BB44A8(ctx,base);}
 ToFull(state,ctx);return true;
}
}
