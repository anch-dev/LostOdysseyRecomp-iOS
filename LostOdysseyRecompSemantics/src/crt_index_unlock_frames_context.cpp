#include "lo_semantics/crt_index_unlock_frames_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_index_unlock_frames_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };

void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    if (!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
            base.memory, base.dependencies, base.full))
        throw std::logic_error("missing accepted indexed-unlock lower");
    FromFull(ctx, base.full);
}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory, Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a), (v))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a), (v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory, Address(a), (v))

void sub_82B7B810(Context& c, Base& b) { Direct(0x82b7b810u,c,b); }
void Body_82DF61AC(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-128
	ctx.r[31].s64 = ctx.r[12].s64 + -128;
	// std r30,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[30].u64);
	// std r29,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[29].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-32(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -32, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29040
	ctx.r[30].s64 = ctx.r[11].s64 + -29040;
	// lwz r29,80(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// b 0x82df61f4
	goto loc_82DF61F4;
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-128
	ctx.r[31].s64 = ctx.r[12].s64 + -128;
	// std r30,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[30].u64);
	// std r29,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[29].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-32(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -32, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
loc_82DF61F4:
	// rlwinm r11,r29,2,0,29
	ctx.r[11].u64 = std::rotl(ctx.r[29].u32 | (ctx.r[29].u64 << 32), 2) & 0xFFFFFFFC;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// lwz r10,0(r30)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// lwzx r4,r11,r10
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[11].u32 + ctx.r[10].u32);
	// bl 0x82b7b810
	ctx.lr = 0x82DF6208;
	sub_82B7B810(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29040
	ctx.r[30].s64 = ctx.r[11].s64 + -29040;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r10,r11,-29036
	ctx.r[10].s64 = ctx.r[11].s64 + -29036;
	// lwz r29,80(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r30,-16(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// ld r29,-24(r1)
	ctx.r[29].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -24);
	// lwz r12,-32(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -32);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
void Body_82DF61D8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-128
	ctx.r[31].s64 = ctx.r[12].s64 + -128;
	// std r30,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[30].u64);
	// std r29,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[29].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-32(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -32, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// rlwinm r11,r29,2,0,29
	ctx.r[11].u64 = std::rotl(ctx.r[29].u32 | (ctx.r[29].u64 << 32), 2) & 0xFFFFFFFC;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// lwz r10,0(r30)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// lwzx r4,r11,r10
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[11].u32 + ctx.r[10].u32);
	// bl 0x82b7b810
	ctx.lr = 0x82DF6208;
	sub_82B7B810(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29040
	ctx.r[30].s64 = ctx.r[11].s64 + -29040;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r10,r11,-29036
	ctx.r[10].s64 = ctx.r[11].s64 + -29036;
	// lwz r29,80(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r30,-16(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// ld r29,-24(r1)
	ctx.r[29].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -24);
	// lwz r12,-32(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -32);
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
    if(entry!=0x82df61acu && entry!=0x82df61d8u) return false;
    Context context{}; FromFull(context,state); Base base{memory,dependencies,state};
    if(entry==0x82df61acu) Body_82DF61AC(context,base);
    else Body_82DF61D8(context,base);
    ToFull(state,context); return true;
}
} // namespace lo::semantic::gpu::crt_index_unlock_frames_context
