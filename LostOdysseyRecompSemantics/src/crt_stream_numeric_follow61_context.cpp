#include "lo_semantics/crt_stream_numeric_follow61_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::crt_stream_numeric_follow61_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };
void Direct(GuestAddress entry,Context& c,Base& b)
{
    ToFull(b.full,c);
    const bool applied=entry==0x82df3e80u?
        crt_temp_path_chain61_context::Apply(entry,b.memory,b.dependencies,b.full):
        crt_stream_close_shared_lower::ApplyAcceptedLower(entry,b.memory,b.dependencies.accepted,b.full);
    if(!applied) throw std::logic_error("missing accepted numeric follow lower");
    FromFull(c,b.full);
}
void sub_82DF3E80(Context& c,Base& b) { Direct(0x82df3e80u,c,b); }
void sub_82B7B7C8(Context& c,Base& b) { Direct(0x82b7b7c8u,c,b); }
void sub_82B819C8(Context& c,Base& b) { Direct(0x82b819c8u,c,b); }
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82DF4144(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-176
	ctx.r[31].s64 = ctx.r[12].s64 + -176;
	// std r29,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[29].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lwz r29,88(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 88);
	// b 0x82df417c
	goto loc_82DF417C;
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-176
	ctx.r[31].s64 = ctx.r[12].s64 + -176;
	// std r29,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[29].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
loc_82DF417C:
	// lwz r11,92(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 92);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq cr6,0x82df4190
	if (ctx.cr6.eq) goto loc_82DF4190;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82b7b7c8
	ctx.lr = 0x82DF4190;
	sub_82B7B7C8(ctx, base);
loc_82DF4190:
	// li r3,2
	ctx.r[3].s64 = 2;
	// bl 0x82b819c8
	ctx.lr = 0x82DF4198;
	sub_82B819C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r29,-16(r1)
	ctx.r[29].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// lwz r12,-24(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -24);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
void Body_82DF41B0(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r11,0
	ctx.r[11].s64 = 0;
	// li r4,64
	ctx.r[4].s64 = 64;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// stw r11,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[11].u32);
	// bl 0x82df3e80
	ctx.lr = 0x82DF41D0;
	sub_82DF3E80(ctx, base);
	// lwz r3,80(r1)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    if(entry!=0x82df4144u&&entry!=0x82df41b0u) return false;
    Context c{};FromFull(c,state);Base b{memory,dependencies,state};
    if(entry==0x82df4144u) Body_82DF4144(c,b);
    else Body_82DF41B0(c,b);
    ToFull(state,c);return true;
}
} // namespace lo::semantic::gpu::crt_stream_numeric_follow61_context
