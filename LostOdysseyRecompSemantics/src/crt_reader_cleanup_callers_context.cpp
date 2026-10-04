#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_cleanup_callers_context {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; GuestServices& guest; Registers& full; };
void __savegprlr_28(Context& c,Base& b) { Save(28u,c,b); }
void __restgprlr_28(Context& c,Base& b) { Restore(28u,c,b); }
void sub_82BD0798(Context& c,Base& b) {
    ToFull(b.full,c);
    if (!crt_close_recursive_buffer_context::Apply(0x82bd0798u,b.memory,b.guest,b.full))
        throw std::logic_error("missing accepted lazy table");
    FromFull(c,b.full);
}
void Indirect(GuestAddress target,Context& c,Base& b) {
    ToFull(b.full,c); b.guest.CallIndirect(target,b.memory,b.full); FromFull(c,b.full);
}
void Body_82BD0B48(Context&,Base&);
void sub_82BD0B48(Context& c,Base& b) { Body_82BD0B48(c,b); }
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_CALL_INDIRECT_FUNC(t) Indirect((t),ctx,base)
void Body_82BD0B48(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x82BD0B50;
	__savegprlr_28(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// li r28,0
	ctx.r[28].s64 = 0;
	// lwz r11,8(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 8);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd0b84
	if (ctx.cr6.eq) goto loc_82BD0B84;
	// bl 0x82bd0798
	ctx.lr = 0x82BD0B6C;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,8(r29)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 8);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0B80;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r28,8(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 8, ctx.r[28].u32);
loc_82BD0B84:
	// lwz r11,12(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd0bac
	if (ctx.cr6.eq) goto loc_82BD0BAC;
	// bl 0x82bd0798
	ctx.lr = 0x82BD0B94;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,12(r29)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 12);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0BA8;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r28,12(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 12, ctx.r[28].u32);
loc_82BD0BAC:
	// lwz r30,4(r29)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 4);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x82bd0c08
	if (ctx.cr6.eq) goto loc_82BD0C08;
loc_82BD0BB8:
	// mr r31,r30
	ctx.r[31].u64 = ctx.r[30].u64;
	// lwz r30,12(r30)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 12);
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd0be8
	if (ctx.cr6.eq) goto loc_82BD0BE8;
	// bl 0x82bd0798
	ctx.lr = 0x82BD0BD0;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,0(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0BE4;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r28,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[28].u32);
loc_82BD0BE8:
	// bl 0x82bd0798
	ctx.lr = 0x82BD0BEC;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0C00;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// bne cr6,0x82bd0bb8
	if (!ctx.cr6.eq) goto loc_82BD0BB8;
loc_82BD0C08:
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}
void Body_82BD0DF0(Context& ctx, Base& base) {
	// b 0x82bd0b48
	sub_82BD0B48(ctx, base);
	return;
}
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_CALL_INDIRECT_FUNC
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestServices& guest,Registers& state) {
    if (entry != 0x82bd0b48u && entry != 0x82bd0df0u) return false;
    Context ctx{}; FromFull(ctx,state); Base base{memory,guest,state};
    if (entry == 0x82bd0b48u) Body_82BD0B48(ctx,base); else Body_82BD0DF0(ctx,base);
    ToFull(state,ctx); return true;
}
}
