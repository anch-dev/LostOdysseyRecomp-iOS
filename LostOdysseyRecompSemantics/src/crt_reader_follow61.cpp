#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <stdexcept>
#include <bit>
namespace lo::semantic::gpu::crt_reader_follow61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; GuestServices& guest; Registers& full; };
void __savegprlr_29(Context& c,Base& b) { Save(29u,c,b); }
void __restgprlr_29(Context& c,Base& b) { Restore(29u,c,b); }
void sub_82BD0798(Context& c,Base& b) {
 ToFull(b.full,c);
 if(!crt_close_recursive_buffer_context::Apply(0x82bd0798u,b.memory,b.guest,b.full)) throw std::logic_error("missing accepted lazy table");
 FromFull(c,b.full);
}
void Indirect(GuestAddress target,Context& c,Base& b) { ToFull(b.full,c); b.guest.CallIndirect(target,b.memory,b.full); FromFull(c,b.full); }
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
#define PPC_CALL_INDIRECT_FUNC(t) Indirect((t),ctx,base)
void Body_82BD1AF8(Context& ctx, [[maybe_unused]] Base& base) {
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
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd1b38
	if (ctx.cr6.eq) goto loc_82BD1B38;
	// bl 0x82bd0798
	ctx.lr = 0x82BD1B1C;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,4(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD1B30;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
loc_82BD1B38:
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
void Body_82BD2C78(Context& ctx, [[maybe_unused]] Base& base) {
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
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// lbz r11,20(r31)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[31].u32 + 20);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2cf0
	if (ctx.cr6.eq) goto loc_82BD2CF0;
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// li r30,0
	ctx.r[30].s64 = 0;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2cc8
	if (ctx.cr6.eq) goto loc_82BD2CC8;
	// bl 0x82bd0798
	ctx.lr = 0x82BD2CB0;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,8(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2CC4;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r30,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[30].u32);
loc_82BD2CC8:
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2cf0
	if (ctx.cr6.eq) goto loc_82BD2CF0;
	// bl 0x82bd0798
	ctx.lr = 0x82BD2CD8;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,4(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2CEC;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r30,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[30].u32);
loc_82BD2CF0:
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
void Body_82BD2D08(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82BD2D10;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r29,r4
	ctx.r[29].u64 = ctx.r[4].u64;
	// lbz r11,20(r31)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[31].u32 + 20);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2de0
	if (ctx.cr6.eq) goto loc_82BD2DE0;
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// li r30,0
	ctx.r[30].s64 = 0;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2d54
	if (ctx.cr6.eq) goto loc_82BD2D54;
	// bl 0x82bd0798
	ctx.lr = 0x82BD2D3C;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,8(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2D50;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r30,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[30].u32);
loc_82BD2D54:
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2d7c
	if (ctx.cr6.eq) goto loc_82BD2D7C;
	// bl 0x82bd0798
	ctx.lr = 0x82BD2D64;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,4(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2D78;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r30,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[30].u32);
loc_82BD2D7C:
	// bl 0x82bd0798
	ctx.lr = 0x82BD2D80;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// rlwinm r30,r29,2,0,29
	ctx.r[30].u64 = std::rotl(ctx.r[29].u32 | (ctx.r[29].u64 << 32), 2) & 0xFFFFFFFC;
	// li r5,72
	ctx.r[5].s64 = 72;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2D9C;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// stw r3,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[3].u32);
	// bne cr6,0x82bd2db4
	if (!ctx.cr6.eq) goto loc_82BD2DB4;
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
loc_82BD2DB4:
	// bl 0x82bd0798
	ctx.lr = 0x82BD2DB8;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// li r5,73
	ctx.r[5].s64 = 73;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2DD0;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// stw r3,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[3].u32);
	// li r3,0
	ctx.r[3].s64 = 0;
	// beq cr6,0x82bd2de4
	if (ctx.cr6.eq) goto loc_82BD2DE4;
loc_82BD2DE0:
	// li r3,1
	ctx.r[3].s64 = 1;
loc_82BD2DE4:
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
#undef PPC_CALL_INDIRECT_FUNC
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestServices& guest,Registers& state) {
 if(entry!=0x82bd1af8u && entry!=0x82bd2c78u && entry!=0x82bd2d08u) return false;
 Context ctx{}; FromFull(ctx,state); Base base{memory,guest,state};
 switch(entry) {case 0x82bd1af8u: Body_82BD1AF8(ctx,base);break;case 0x82bd2c78u: Body_82BD2C78(ctx,base);break;default: Body_82BD2D08(ctx,base);}
 ToFull(state,ctx);return true;
}
}
