#include "lo_semantics/crt_numeric_upper61_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::crt_numeric_upper61_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };
void Direct(GuestAddress entry,Context& c,Base& b)
{
    ToFull(b.full,c);
    if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,b.memory,
        b.dependencies.accepted.accepted,b.full)) throw std::logic_error("missing accepted scanner errno");
    FromFull(c,b.full);
}
void sub_82B7FD78(Context& c,Base& b) { Direct(0x82b7fd78u,c,b); }
void sub_82B7FEC0(Context& c,Base& b) { Direct(0x82b7fec0u,c,b); }
void Indirect(GuestAddress target,Context& c,Base& b)
{
    ToFull(b.full,c);
    if(target==0x82df4af8u)
    {
        if(!crt_stream_scan_context::Apply(target,b.memory,b.dependencies.accepted,b.full))
            throw std::logic_error("missing accepted actual scanner");
    }
    else b.dependencies.callback.CallIndirect(target,b.memory,b.full);
    FromFull(c,b.full);
}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
#define PPC_CALL_INDIRECT_FUNC(t) Indirect((t),ctx,base)
void Body_82DF3378(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r11,r4
	ctx.r[11].u64 = ctx.r[4].u64;
	// mr r4,r5
	ctx.r[4].u64 = ctx.r[5].u64;
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
	// mr r8,r3
	ctx.r[8].u64 = ctx.r[3].u64;
	// mr r5,r6
	ctx.r[5].u64 = ctx.r[6].u64;
	// mr r9,r10
	ctx.r[9].u64 = ctx.r[10].u64;
loc_82DF339C:
	// lbz r6,0(r10)
	ctx.r[6].u64 = PPC_LOAD_U8(ctx.r[10].u32 + 0);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// cmplwi cr6,r6,0
	ctx.cr6.compare<uint32_t>(ctx.r[6].u32, 0, ctx.xer);
	// bne cr6,0x82df339c
	if (!ctx.cr6.eq) goto loc_82DF339C;
	// subf r10,r9,r10
	ctx.r[10].s64 = ctx.r[10].s64 - ctx.r[9].s64;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// rotlwi r10,r10,0
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 0);
	// bne cr6,0x82df33f0
	if (!ctx.cr6.eq) goto loc_82DF33F0;
loc_82DF33C0:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF33C4;
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
	ctx.lr = 0x82DF33E8;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df3430
	goto loc_82DF3430;
loc_82DF33F0:
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 0, ctx.xer);
	// beq cr6,0x82df33c0
	if (ctx.cr6.eq) goto loc_82DF33C0;
	// lis r9,32767
	ctx.r[9].s64 = 2147418112;
	// stw r11,88(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 88, ctx.r[11].u32);
	// li r6,73
	ctx.r[6].s64 = 73;
	// stw r11,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[11].u32);
	// ori r9,r9,65535
	ctx.r[9].u64 = ctx.r[9].u64 | 65535;
	// cmplw cr6,r10,r9
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[9].u32, ctx.xer);
	// stw r6,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[6].u32);
	// stw r9,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[9].u32);
	// bgt cr6,0x82df3420
	if (ctx.cr6.gt) goto loc_82DF3420;
	// stw r10,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[10].u32);
loc_82DF3420:
	// mr r6,r7
	ctx.r[6].u64 = ctx.r[7].u64;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// mtctr r8
	ctx.ctr.u64 = ctx.r[8].u64;
	// bctrl
	ctx.lr = 0x82DF3430;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
loc_82DF3430:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
void sub_82DF3378(Context& c,Base& b) { Body_82DF3378(c,b); }
void Body_82DF3440(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
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
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// addi r10,r1,80
	ctx.r[10].s64 = ctx.r[1].s64 + 80;
	// addi r9,r1,128
	ctx.r[9].s64 = ctx.r[1].s64 + 128;
	// lis r11,-32033
	ctx.r[11].s64 = -2099314688;
	// mr r5,r4
	ctx.r[5].u64 = ctx.r[4].u64;
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// li r6,0
	ctx.r[6].s64 = 0;
	// addi r3,r11,19192
	ctx.r[3].s64 = ctx.r[11].s64 + 19192;
	// stw r9,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[9].u32);
	// lwz r7,80(r1)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// bl 0x82df3378
	ctx.lr = 0x82DF348C;
	sub_82DF3378(ctx, base);
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_STORE_U64
#undef PPC_CALL_INDIRECT_FUNC
} // namespace
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    if(entry!=0x82df3378u&&entry!=0x82df3440u) return false;
    Context c{};FromFull(c,state);Base b{memory,dependencies,state};
    if(entry==0x82df3378u) Body_82DF3378(c,b);
    else Body_82DF3440(c,b);
    ToFull(state,c);return true;
}
} // namespace lo::semantic::gpu::crt_numeric_upper61_context
