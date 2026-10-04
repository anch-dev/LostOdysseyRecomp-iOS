#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <stdexcept>
namespace lo::semantic::gpu::crt_close_next61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; GuestServices& guest; Registers& full; };
void Direct(GuestAddress entry,Context& c,Base& b) {
 ToFull(b.full,c);
 if (!crt_close_recursive_buffer_context::Apply(entry,b.memory,b.guest,b.full)) throw std::logic_error("missing accepted recursive lower");
 FromFull(c,b.full);
}
void sub_82BD0C18(Context& c,Base& b) { Direct(0x82bd0c18u,c,b); }
void sub_82BD07D8(Context& c,Base& b) { Direct(0x82bd07d8u,c,b); }
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82BD0900(Context& ctx, Base& base) {
	// lwz r11,4(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 4);
	// li r9,0
	ctx.r[9].s64 = 0;
	// lwz r10,12(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82bd092c
	if (ctx.cr6.eq) goto loc_82BD092C;
loc_82BD0914:
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
	// bne cr6,0x82bd0914
	if (!ctx.cr6.eq) goto loc_82BD0914;
loc_82BD092C:
	// lwz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// add r3,r11,r9
	ctx.r[3].u64 = ctx.r[11].u64 + ctx.r[9].u64;
	// blr
	return;
}
void Body_82BD0938(Context& ctx, Base& base) {
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
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// bl 0x82bd0c18
	ctx.lr = 0x82BD0958;
	sub_82BD0C18(ctx, base);
	// lwz r4,0(r3)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r11,4(r4)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 4);
	// lwz r10,8(r4)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 8);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// ble cr6,0x82bd0978
	if (!ctx.cr6.gt) goto loc_82BD0978;
	// li r5,0
	ctx.r[5].s64 = 0;
	// bl 0x82bd07d8
	ctx.lr = 0x82BD0978;
	sub_82BD07D8(ctx, base);
loc_82BD0978:
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lwz r9,4(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// stw r10,16(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 16, ctx.r[10].u32);
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// stw r10,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[10].u32);
	// lwz r11,16(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 16);
	// stb r30,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[30].u8);
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
void Body_82BD0FC8(Context& ctx, Base& base) {
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
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// bl 0x82bd0c18
	ctx.lr = 0x82BD0FE8;
	sub_82BD0C18(ctx, base);
	// lwz r4,0(r3)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r11,4(r4)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 4);
	// lwz r10,8(r4)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 8);
	// addi r11,r11,2
	ctx.r[11].s64 = ctx.r[11].s64 + 2;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// ble cr6,0x82bd1008
	if (!ctx.cr6.gt) goto loc_82BD1008;
	// li r5,0
	ctx.r[5].s64 = 0;
	// bl 0x82bd07d8
	ctx.lr = 0x82BD1008;
	sub_82BD07D8(ctx, base);
loc_82BD1008:
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lwz r9,4(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// stw r10,16(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 16, ctx.r[10].u32);
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// addi r10,r10,2
	ctx.r[10].s64 = ctx.r[10].s64 + 2;
	// stw r10,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[10].u32);
	// lwz r11,16(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 16);
	// sth r30,0(r11)
	PPC_STORE_U16(ctx.r[11].u32 + 0, ctx.r[30].u16);
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
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U16
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestServices& guest,Registers& state) {
 if(entry!=0x82bd0900u && entry!=0x82bd0938u && entry!=0x82bd0fc8u) return false;
 Context ctx{}; FromFull(ctx,state); Base base{memory,guest,state};
 switch(entry) { case 0x82bd0900u: Body_82BD0900(ctx,base); break; case 0x82bd0938u: Body_82BD0938(ctx,base); break; default: Body_82BD0FC8(ctx,base); }
 ToFull(state,ctx); return true;
}
}
