#include "lo_semantics/crt_stream_scan_adjacent_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_scan_adjacent_context
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

void Body_82DF5D18(Context& ctx, Base& base) {
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
	// mr r10,r4
	ctx.r[10].u64 = ctx.r[4].u64;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x82df5d3c
	if (ctx.cr6.eq) goto loc_82DF5D3C;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// bne cr6,0x82df5d6c
	if (!ctx.cr6.eq) goto loc_82DF5D6C;
loc_82DF5D3C:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF5D40;
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
	ctx.lr = 0x82DF5D64;
	sub_82B7FEC0(ctx, base);
	// li r3,22
	ctx.r[3].s64 = 22;
	// b 0x82df5e18
	goto loc_82DF5E18;
loc_82DF5D6C:
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, 0, ctx.xer);
	// bne cr6,0x82df5d80
	if (!ctx.cr6.eq) goto loc_82DF5D80;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// b 0x82df5d3c
	goto loc_82DF5D3C;
loc_82DF5D80:
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
loc_82DF5D84:
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// beq cr6,0x82df5d9c
	if (ctx.cr6.eq) goto loc_82DF5D9C;
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r[10].u32 > 0;
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// bne 0x82df5d84
	if (!ctx.cr0.eq) goto loc_82DF5D84;
loc_82DF5D9C:
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// bne cr6,0x82df5dd8
	if (!ctx.cr6.eq) goto loc_82DF5DD8;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// bl 0x82b7fd78
	ctx.lr = 0x82DF5DB0;
	sub_82B7FD78(ctx, base);
	// li r31,22
	ctx.r[31].s64 = 22;
loc_82DF5DB4:
	// stw r31,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[31].u32);
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
	// bl 0x82b7fec0
	ctx.lr = 0x82DF5DD0;
	sub_82B7FEC0(ctx, base);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// b 0x82df5e18
	goto loc_82DF5E18;
loc_82DF5DD8:
	// lbz r9,0(r5)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[5].u32 + 0);
	// addi r5,r5,1
	ctx.r[5].s64 = ctx.r[5].s64 + 1;
	// cmplwi r9,0
	ctx.cr0.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// stb r9,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[9].u8);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// beq 0x82df5df8
	if (ctx.cr0.eq) goto loc_82DF5DF8;
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r[10].u32 > 0;
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82df5dd8
	if (!ctx.cr0.eq) goto loc_82DF5DD8;
loc_82DF5DF8:
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// bne cr6,0x82df5e14
	if (!ctx.cr6.eq) goto loc_82DF5E14;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// bl 0x82b7fd78
	ctx.lr = 0x82DF5E0C;
	sub_82B7FD78(ctx, base);
	// li r31,34
	ctx.r[31].s64 = 34;
	// b 0x82df5db4
	goto loc_82DF5DB4;
loc_82DF5E14:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF5E18:
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

void Body_82DF5E30(Context& ctx, Base& base) {
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
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 0, ctx.xer);
	// bne cr6,0x82df5e78
	if (!ctx.cr6.eq) goto loc_82DF5E78;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF5E4C;
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
	ctx.lr = 0x82DF5E70;
	sub_82B7FEC0(ctx, base);
	// li r3,22
	ctx.r[3].s64 = 22;
	// b 0x82df5f98
	goto loc_82DF5F98;
loc_82DF5E78:
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, 0, ctx.xer);
	// bne cr6,0x82df5eac
	if (!ctx.cr6.eq) goto loc_82DF5EAC;
loc_82DF5E80:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF5E84;
	sub_82B7FD78(ctx, base);
	// li r31,22
	ctx.r[31].s64 = 22;
	// stw r31,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[31].u32);
loc_82DF5E8C:
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
	// bl 0x82b7fec0
	ctx.lr = 0x82DF5EA4;
	sub_82B7FEC0(ctx, base);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// b 0x82df5f98
	goto loc_82DF5F98;
loc_82DF5EAC:
	// cntlzw r11,r7
	ctx.r[11].u64 = ctx.r[7].u32 == 0 ? 32 : std::countl_zero(ctx.r[7].u32);
	// li r31,0
	ctx.r[31].s64 = 0;
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// stb r31,0(r4)
	PPC_STORE_U8(ctx.r[4].u32 + 0, ctx.r[31].u8);
	// cmplw cr6,r5,r11
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, ctx.r[11].u32, ctx.xer);
	// bgt cr6,0x82df5ee0
	if (ctx.cr6.gt) goto loc_82DF5EE0;
loc_82DF5ECC:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF5ED0;
	sub_82B7FD78(ctx, base);
	// li r11,34
	ctx.r[11].s64 = 34;
	// mr r31,r11
	ctx.r[31].u64 = ctx.r[11].u64;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// b 0x82df5e8c
	goto loc_82DF5E8C;
loc_82DF5EE0:
	// addi r11,r6,-2
	ctx.r[11].s64 = ctx.r[6].s64 + -2;
	// cmplwi cr6,r11,34
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 34, ctx.xer);
	// bgt cr6,0x82df5e80
	if (ctx.cr6.gt) goto loc_82DF5E80;
	// mr r9,r31
	ctx.r[9].u64 = ctx.r[31].u64;
	// mr r11,r4
	ctx.r[11].u64 = ctx.r[4].u64;
	// cmpwi cr6,r7,0
	ctx.cr6.compare<int32_t>(ctx.r[7].s32, 0, ctx.xer);
	// beq cr6,0x82df5f10
	if (ctx.cr6.eq) goto loc_82DF5F10;
	// li r10,45
	ctx.r[10].s64 = 45;
	// addi r11,r4,1
	ctx.r[11].s64 = ctx.r[4].s64 + 1;
	// li r9,1
	ctx.r[9].s64 = 1;
	// neg r3,r3
	ctx.r[3].s64 = -ctx.r[3].s64;
	// stb r10,0(r4)
	PPC_STORE_U8(ctx.r[4].u32 + 0, ctx.r[10].u8);
loc_82DF5F10:
	// mr r8,r11
	ctx.r[8].u64 = ctx.r[11].u64;
loc_82DF5F14:
	// divwu r10,r3,r6
	ctx.r[10].u32 = ctx.r[3].u32 / ctx.r[6].u32;
	// twllei r6,0
	// mullw r10,r10,r6
	ctx.r[10].s64 = int64_t(ctx.r[10].s32) * int64_t(ctx.r[6].s32);
	// subf r10,r10,r3
	ctx.r[10].s64 = ctx.r[3].s64 - ctx.r[10].s64;
	// divwu r3,r3,r6
	ctx.r[3].u32 = ctx.r[3].u32 / ctx.r[6].u32;
	// twllei r6,0
	// cmplwi cr6,r10,9
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 9, ctx.xer);
	// ble cr6,0x82df5f3c
	if (!ctx.cr6.gt) goto loc_82DF5F3C;
	// addi r10,r10,87
	ctx.r[10].s64 = ctx.r[10].s64 + 87;
	// b 0x82df5f40
	goto loc_82DF5F40;
loc_82DF5F3C:
	// addi r10,r10,48
	ctx.r[10].s64 = ctx.r[10].s64 + 48;
loc_82DF5F40:
	// stb r10,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[10].u8);
	// addi r9,r9,1
	ctx.r[9].s64 = ctx.r[9].s64 + 1;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x82df5f5c
	if (ctx.cr6.eq) goto loc_82DF5F5C;
	// cmplw cr6,r9,r5
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, ctx.r[5].u32, ctx.xer);
	// blt cr6,0x82df5f14
	if (ctx.cr6.lt) goto loc_82DF5F14;
loc_82DF5F5C:
	// cmplw cr6,r9,r5
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, ctx.r[5].u32, ctx.xer);
	// blt cr6,0x82df5f6c
	if (ctx.cr6.lt) goto loc_82DF5F6C;
	// stb r31,0(r4)
	PPC_STORE_U8(ctx.r[4].u32 + 0, ctx.r[31].u8);
	// b 0x82df5ecc
	goto loc_82DF5ECC;
loc_82DF5F6C:
	// stb r31,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[31].u8);
	// addi r11,r11,-1
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
loc_82DF5F74:
	// lbz r9,0(r8)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[8].u32 + 0);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stb r9,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[9].u8);
	// addi r11,r11,-1
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	// stb r10,0(r8)
	PPC_STORE_U8(ctx.r[8].u32 + 0, ctx.r[10].u8);
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// cmplw cr6,r8,r11
	ctx.cr6.compare<uint32_t>(ctx.r[8].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x82df5f74
	if (ctx.cr6.lt) goto loc_82DF5F74;
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF5F98:
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

#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (entry != 0x82df5d18u && entry != 0x82df5e30u) return false;
    Context context{};
    FromFull(context, state);
    Base base{memory, dependencies, state};
    if (entry == 0x82df5d18u) Body_82DF5D18(context, base);
    else Body_82DF5E30(context, base);
    ToFull(state, context);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_scan_adjacent_context
