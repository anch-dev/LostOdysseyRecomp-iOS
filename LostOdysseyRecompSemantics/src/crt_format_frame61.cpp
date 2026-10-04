#include "lo_semantics/crt_format_frame61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <stdexcept>
namespace lo::semantic::gpu::crt_format_frame61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies deps;Registers& full;};
void sub_82B7B7C8(Context& c,Base& b) {ToFull(b.full,c);if(!crt_stream_close_shared_lower::ApplyAcceptedLower(0x82b7b7c8u,b.memory,b.deps,b.full)) throw std::logic_error("missing accepted frame unlock lower");FromFull(c,b.full);}
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82DF2864(Context& ctx, [[maybe_unused]] Base& base) {
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
	// lwz r30,188(r31)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 188);
	// b 0x82df289c
	goto loc_82DF289C;
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
loc_82DF289C:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b7c8
	ctx.lr = 0x82DF28A4;
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
void Body_82DF2884(Context& ctx, [[maybe_unused]] Base& base) {
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
	ctx.lr = 0x82DF28A4;
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
void Body_82DF2AC8(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-160
	ctx.r[31].s64 = ctx.r[12].s64 + -160;
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-16(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -16, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lwz r3,88(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 88);
	// bl 0x82b7b7c8
	ctx.lr = 0x82DF2AE4;
	sub_82B7B7C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// lwz r12,-16(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -16);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
 if(entry!=0x82df2864u && entry!=0x82df2884u && entry!=0x82df2ac8u) return false;
 Context ctx{};FromFull(ctx,state);Base base{memory,deps,state};
 switch(entry) {case 0x82df2864u:Body_82DF2864(ctx,base);break;case 0x82df2884u:Body_82DF2884(ctx,base);break;default:Body_82DF2AC8(ctx,base);}
 ToFull(state,ctx);return true;
}
void ApplySupport_DF2AC8(GuestMemory& memory,Dependencies deps,Registers& state) {
 (void)crt_format_frame61::Apply(0x82df2ac8u,memory,deps,state);
}

}
