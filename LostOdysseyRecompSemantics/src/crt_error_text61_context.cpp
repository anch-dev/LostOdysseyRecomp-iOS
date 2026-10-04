#include "lo_semantics/crt_error_text61_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <cstdint>
namespace lo::semantic::gpu::crt_error_text61_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; };
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82DF6928(Context& ctx, [[maybe_unused]] Base& base) {
	// lis r11,-31966
	ctx.r[11].s64 = -2094923776;
	// addi r3,r11,-72
	ctx.r[3].s64 = ctx.r[11].s64 + -72;
	// blr
	return;
}
void sub_82DF6928(Context& c,Base& b) { Body_82DF6928(c,b); }
void Body_82DF6938(Context& ctx, [[maybe_unused]] Base& base) {
	// lis r11,-31966
	ctx.r[11].s64 = -2094923776;
	// addi r3,r11,-248
	ctx.r[3].s64 = ctx.r[11].s64 + -248;
	// blr
	return;
}
void sub_82DF6938(Context& c,Base& b) { Body_82DF6938(c,b); }
void Body_82DF4218(Context& ctx, [[maybe_unused]] Base& base) {
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
	// cmpwi cr6,r31,0
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// blt cr6,0x82df4244
	if (ctx.cr6.lt) goto loc_82DF4244;
	// bl 0x82df6928
	ctx.lr = 0x82DF4238;
	sub_82DF6928(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// cmpw cr6,r31,r11
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, ctx.r[11].s32, ctx.xer);
	// blt cr6,0x82df424c
	if (ctx.cr6.lt) goto loc_82DF424C;
loc_82DF4244:
	// bl 0x82df6928
	ctx.lr = 0x82DF4248;
	sub_82DF6928(ctx, base);
	// lwz r31,0(r3)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
loc_82DF424C:
	// bl 0x82df6938
	ctx.lr = 0x82DF4250;
	sub_82DF6938(ctx, base);
	// rlwinm r11,r31,2,0,29
	ctx.r[11].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r3,r3,r11
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[3].u32 + ctx.r[11].u32);
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
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace
bool Apply(GuestAddress entry,GuestMemory& memory,Registers& state)
{
    if(entry!=0x82df4218u) return false;
    Context c{};FromFull(c,state);Base b{memory};
    Body_82DF4218(c,b);ToFull(state,c);return true;
}
} // namespace lo::semantic::gpu::crt_error_text61_context
