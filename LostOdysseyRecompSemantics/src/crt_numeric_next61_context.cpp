#include "lo_semantics/crt_numeric_next61_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_numeric_next61_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };

void sub_82B863F0(Context& ctx, Base& base)
{
    ToFull(base.full,ctx);
    auto& state=base.full;
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp=state.r[1];lower.lr=state.lr;
    lower.r3=state.r[3];lower.r9=state.r[9];lower.r10=state.r[10];
    lower.r11=state.r[11];lower.r12=state.r[12];
    lower.r30=state.r[30];lower.r31=state.r[31];lower.xer_ca=state.xer_ca;
    if(!crt_stream_pointer_unlock::Apply(0x82b863f0u,base.memory,
        base.dependencies.open.unlock,lower))
        throw std::logic_error("missing accepted pointer unlock");
    state.r[1]=lower.sp;state.lr=lower.lr;state.r[3]=lower.r3;
    state.r[9]=lower.r9;state.r[10]=lower.r10;state.r[11]=lower.r11;
    state.r[12]=lower.r12;state.r[30]=lower.r30;state.r[31]=lower.r31;
    state.xer_ca=lower.xer_ca;
    FromFull(ctx,state);
}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory, Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a), (v))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a), (v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory, Address(a), (v))

void Body_82DF45E0(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-160
	ctx.r[31].s64 = ctx.r[12].s64 + -160;
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
	// lwz r30,180(r31)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 180);
	// b 0x82df4618
	goto loc_82DF4618;
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-160
	ctx.r[31].s64 = ctx.r[12].s64 + -160;
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
loc_82DF4618:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b863f0
	ctx.lr = 0x82DF4620;
	sub_82B863F0(ctx, base);
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
void Body_82DF4600(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-160
	ctx.r[31].s64 = ctx.r[12].s64 + -160;
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
	// bl 0x82b863f0
	ctx.lr = 0x82DF4620;
	sub_82B863F0(ctx, base);
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
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    if(entry!=0x82df45e0u && entry!=0x82df4600u) return false;
    Context context{};FromFull(context,state);Base base{memory,dependencies,state};
    if(entry==0x82df45e0u) Body_82DF45E0(context,base);
    else Body_82DF4600(context,base);
    ToFull(state,context);return true;
}
} // namespace lo::semantic::gpu::crt_numeric_next61_context
