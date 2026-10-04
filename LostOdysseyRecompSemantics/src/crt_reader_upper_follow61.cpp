#include "lo_semantics/crt_reader_upper_follow61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include "lo_semantics/pointer_fields.h"
#include <array>
#include <bit>
namespace lo::semantic::gpu::crt_reader_upper_follow61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; GuestServices& guest; Registers& full; };
void sub_82BD12D0(Context& c,Base& b) {
 PointerFieldRegisters fields{};fields.r3=c.r[3].u64;
 constexpr auto table=static_cast<std::uint64_t>(std::int64_t(-2113077248)+27576);
 constexpr std::array assignments{ConstantFieldAssignment{PointerFieldRegister::R10,table},ConstantFieldAssignment{PointerFieldRegister::R11,0u}};
 constexpr std::array writes{ConstantFieldWrite{0,PointerFieldWidth::Word,static_cast<std::uint32_t>(table)},ConstantFieldWrite{4,PointerFieldWidth::Word,0u},ConstantFieldWrite{8,PointerFieldWidth::Word,0u},ConstantFieldWrite{12,PointerFieldWidth::Word,0u},ConstantFieldWrite{16,PointerFieldWidth::Word,0u}};
 InitializeConstantFields(b.memory,fields,PointerFieldRegister::R3,assignments,writes);
 c.r[10].u64=fields.r10;c.r[11].u64=fields.r11;
}
void Indirect(GuestAddress target,Context& c,Base& b) {ToFull(b.full,c);b.guest.CallIndirect(target,b.memory,b.full);FromFull(c,b.full);}
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
#define PPC_CALL_INDIRECT_FUNC(t) Indirect((t),ctx,base)
void Body_82BD1AA8(Context& ctx, [[maybe_unused]] Base& base) {
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
	// bl 0x82bd12d0
	ctx.lr = 0x82BD1AC0;
	sub_82BD12D0(ctx, base);
	// lis r11,-32243
	ctx.r[11].s64 = -2113077248;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// addi r10,r11,27700
	ctx.r[10].s64 = ctx.r[11].s64 + 27700;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r10,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[10].u32);
	// stw r11,20(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 20, ctx.r[11].u32);
	// stw r11,24(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 24, ctx.r[11].u32);
	// stw r11,28(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 28, ctx.r[11].u32);
	// stw r11,32(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 32, ctx.r[11].u32);
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
void Body_82BD2030(Context& ctx, [[maybe_unused]] Base& base) {
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
	// li r3,0
	ctx.r[3].s64 = 0;
	// lwz r11,16(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 16);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2068
	if (ctx.cr6.eq) goto loc_82BD2068;
	// rotlwi r3,r11,0
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32, 0);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r11,28(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 28);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2068;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
loc_82BD2068:
	// lwz r11,32(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 32);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2080
	if (ctx.cr6.eq) goto loc_82BD2080;
	// lwz r11,28(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 28);
	// rlwinm r11,r11,2,0,29
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// add r3,r11,r3
	ctx.r[3].u64 = ctx.r[11].u64 + ctx.r[3].u64;
loc_82BD2080:
	// lwz r11,24(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 24);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2098
	if (ctx.cr6.eq) goto loc_82BD2098;
	// lwz r11,20(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 20);
	// rlwinm r11,r11,2,0,29
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// add r3,r11,r3
	ctx.r[3].u64 = ctx.r[11].u64 + ctx.r[3].u64;
loc_82BD2098:
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
#undef PPC_CALL_INDIRECT_FUNC
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestServices& guest,Registers& state) {
 if(entry!=0x82bd1aa8u && entry!=0x82bd2030u) return false;
 Context ctx{};FromFull(ctx,state);Base base{memory,guest,state};
 if(entry==0x82bd1aa8u) Body_82BD1AA8(ctx,base);else Body_82BD2030(ctx,base);
 ToFull(state,ctx);return true;
}
}
