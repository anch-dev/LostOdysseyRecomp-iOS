#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <stdexcept>
namespace lo::semantic::gpu::crt_close_upper61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };
void sub_82BD0A60(Context& ctx, Base& base) {
    ToFull(base.full,ctx);
    if (!crt_close_reader_callers_context::Apply(0x82bd0a60u,base.memory,base.dependencies,base.full))
        throw std::logic_error("missing accepted reader lower");
    FromFull(ctx,base.full);
}
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82BD0CD0(Context& ctx, Base& base) {
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
	// li r11,0
	ctx.r[11].s64 = 0;
	// mr r7,r4
	ctx.r[7].u64 = ctx.r[4].u64;
	// li r6,0
	ctx.r[6].s64 = 0;
	// stw r11,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[11].u32);
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// sth r11,20(r31)
	PPC_STORE_U16(ctx.r[31].u32 + 20, ctx.r[11].u16);
	// sth r11,22(r31)
	PPC_STORE_U16(ctx.r[31].u32 + 22, ctx.r[11].u16);
	// stb r11,24(r31)
	PPC_STORE_U8(ctx.r[31].u32 + 24, ctx.r[11].u8);
	// stb r11,25(r31)
	PPC_STORE_U8(ctx.r[31].u32 + 25, ctx.r[11].u8);
	// bl 0x82bd0a60
	ctx.lr = 0x82BD0D0C;
	sub_82BD0A60(ctx, base);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
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
#undef PPC_STORE_U8
#undef PPC_STORE_U16
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies dependencies, Registers& state) {
    if (entry != 0x82bd0cd0u) return false;
    Context ctx{}; FromFull(ctx,state); Base base{memory,dependencies,state};
    Body_82BD0CD0(ctx,base); ToFull(state,ctx); return true;
}
}
