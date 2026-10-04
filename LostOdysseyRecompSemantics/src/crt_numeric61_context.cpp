#include "lo_semantics/crt_numeric61_context.h"
#include "lo_semantics/crt_stream_scan_adjacent_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_numeric61_context
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
        throw std::logic_error("missing accepted scan-adjacent lower");
    FromFull(ctx, base.full);
}
void sub_82B7FD78(Context& ctx, Base& base)
{ Direct(0x82b7fd78u, ctx, base); }
void sub_82B7FEC0(Context& ctx, Base& base)
{ Direct(0x82b7fec0u, ctx, base); }

#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory, Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a), (v))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a), (v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory, Address(a), (v))

void sub_82DF5E30(Context&, Base&);
void sub_82DF5FB8(Context&, Base&);
void sub_82B7E580(Context&, Base&);
void Body_82B7E580(Context& ctx, Base& base) {
	PpcRegister temp{};
	// lbz r5,0(r3)
	ctx.r[5].u64 = PPC_LOAD_U8(ctx.r[3].u32 + 0);
	// mr r9,r3
	ctx.r[9].u64 = ctx.r[3].u64;
	// cmpwi cr6,r4,0
	ctx.cr6.compare<int32_t>(ctx.r[4].s32, 0, ctx.xer);
	// cmpw r5,r4
	ctx.cr0.compare<int32_t>(ctx.r[5].s32, ctx.r[4].s32, ctx.xer);
	// beq cr6,0x82b7e5c4
	if (ctx.cr6.eq) goto loc_82B7E5C4;
	// li r3,0
	ctx.r[3].s64 = 0;
	// beq 0x82b7e5b0
	if (ctx.cr0.eq) goto loc_82B7E5B0;
loc_82B7E59C:
	// cmpwi cr6,r5,0
	ctx.cr6.compare<int32_t>(ctx.r[5].s32, 0, ctx.xer);
	// beq cr6,0x82b7e5d4
	if (ctx.cr6.eq) goto loc_82B7E5D4;
	// lbzu r5,1(r9)
	temp.u64 = ctx.r[9].u64 + uint64_t(1);
	ctx.r[5].u64 = PPC_LOAD_U8(temp.u32);
	ctx.r[9].u64 = temp.u64;
	// cmpw r4,r5
	ctx.cr0.compare<int32_t>(ctx.r[4].s32, ctx.r[5].s32, ctx.xer);
	// bne 0x82b7e59c
	if (!ctx.cr0.eq) goto loc_82B7E59C;
loc_82B7E5B0:
	// mr r3,r9
	ctx.r[3].u64 = ctx.r[9].u64;
	// lbzu r5,1(r9)
	temp.u64 = ctx.r[9].u64 + uint64_t(1);
	ctx.r[5].u64 = PPC_LOAD_U8(temp.u32);
	ctx.r[9].u64 = temp.u64;
	// cmpw r4,r5
	ctx.cr0.compare<int32_t>(ctx.r[4].s32, ctx.r[5].s32, ctx.xer);
	// beq 0x82b7e5b0
	if (ctx.cr0.eq) goto loc_82B7E5B0;
	// b 0x82b7e59c
	goto loc_82B7E59C;
loc_82B7E5C4:
	// beq 0x82b7e5d4
	if (ctx.cr0.eq) goto loc_82B7E5D4;
	// lbzu r5,1(r3)
	temp.u64 = ctx.r[3].u64 + uint64_t(1);
	ctx.r[5].u64 = PPC_LOAD_U8(temp.u32);
	ctx.r[3].u64 = temp.u64;
	// cmpwi r5,0
	ctx.cr0.compare<int32_t>(ctx.r[5].s32, 0, ctx.xer);
	// b 0x82b7e5c4
	goto loc_82B7E5C4;
loc_82B7E5D4:
	// blr
	return;
}
void sub_82B7E580(Context& c, Base& b) { Body_82B7E580(c,b); }
void sub_82DF5E30(Context& c, Base& b)
{
    ToFull(b.full,c);
    if(!crt_stream_scan_adjacent_context::Apply(0x82df5e30u,b.memory,b.dependencies,b.full))
        throw std::logic_error("missing accepted numeric format");
    FromFull(c,b.full);
}
void Body_82DF5FB8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// li r7,0
	ctx.r[7].s64 = 0;
	// addi r11,r11,21248
	ctx.r[11].s64 = ctx.r[11].s64 + 21248;
	// lwz r8,4(r11)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// lwz r11,8(r8)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[8].u32 + 8);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82df5fe8
	if (!ctx.cr6.eq) goto loc_82DF5FE8;
	// bl 0x82b7e580
	ctx.lr = 0x82DF5FE4;
	sub_82B7E580(ctx, base);
	// b 0x82df6088
	goto loc_82DF6088;
loc_82DF5FE8:
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// bne cr6,0x82df6020
	if (!ctx.cr6.eq) goto loc_82DF6020;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF5FF4;
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
	ctx.lr = 0x82DF6018;
	sub_82B7FEC0(ctx, base);
	// li r3,0
	ctx.r[3].s64 = 0;
	// b 0x82df6088
	goto loc_82DF6088;
loc_82DF6020:
	// lbz r10,0(r3)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[3].u32 + 0);
	// mr r11,r10
	ctx.r[11].u64 = ctx.r[10].u64;
	// clrlwi r9,r11,24
	ctx.r[9].u64 = ctx.r[11].u32 & 0xFF;
	// add r9,r9,r8
	ctx.r[9].u64 = ctx.r[9].u64 + ctx.r[8].u64;
	// lbz r9,29(r9)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[9].u32 + 29);
	// rlwinm. r9,r9,0,29,29
	ctx.r[9].u64 = std::rotl(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x82df606c
	if (ctx.cr0.eq) goto loc_82DF606C;
	// addi r3,r3,1
	ctx.r[3].s64 = ctx.r[3].s64 + 1;
	// lbz r10,0(r3)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[3].u32 + 0);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq 0x82df6064
	if (ctx.cr0.eq) goto loc_82DF6064;
	// rlwinm r11,r11,8,0,23
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 8) & 0xFFFFFF00;
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
	// cmplw cr6,r4,r11
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, ctx.r[11].u32, ctx.xer);
	// bne cr6,0x82df6078
	if (!ctx.cr6.eq) goto loc_82DF6078;
	// addi r7,r3,-1
	ctx.r[7].s64 = ctx.r[3].s64 + -1;
	// b 0x82df6078
	goto loc_82DF6078;
loc_82DF6064:
	// cmplwi cr6,r7,0
	ctx.cr6.compare<uint32_t>(ctx.r[7].u32, 0, ctx.xer);
	// b 0x82df6070
	goto loc_82DF6070;
loc_82DF606C:
	// cmplw cr6,r4,r11
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, ctx.r[11].u32, ctx.xer);
loc_82DF6070:
	// bne cr6,0x82df6078
	if (!ctx.cr6.eq) goto loc_82DF6078;
	// mr r7,r3
	ctx.r[7].u64 = ctx.r[3].u64;
loc_82DF6078:
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// addi r3,r3,1
	ctx.r[3].s64 = ctx.r[3].s64 + 1;
	// bne cr6,0x82df6020
	if (!ctx.cr6.eq) goto loc_82DF6020;
	// mr r3,r7
	ctx.r[3].u64 = ctx.r[7].u64;
loc_82DF6088:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
void sub_82DF5FB8(Context& c, Base& b) { Body_82DF5FB8(c,b); }
void Body_82DF5FB0(Context& ctx, Base& base) {
	// li r7,0
	ctx.r[7].s64 = 0;
	// b 0x82df5e30
	sub_82DF5E30(ctx, base);
	return;
}
void Body_82DF6098(Context& ctx, Base& base) {
	// li r5,0
	ctx.r[5].s64 = 0;
	// b 0x82df5fb8
	sub_82DF5FB8(ctx, base);
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
    if(entry!=0x82df5fb0u && entry!=0x82df5fb8u && entry!=0x82df6098u) return false;
    Context context{}; FromFull(context,state); Base base{memory,dependencies,state};
    if(entry==0x82df5fb0u) Body_82DF5FB0(context,base);
    else if(entry==0x82df5fb8u) Body_82DF5FB8(context,base);
    else Body_82DF6098(context,base);
    ToFull(state,context); return true;
}
} // namespace lo::semantic::gpu::crt_numeric61_context
