#include "lo_semantics/crt_reader_float61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_float61 {
namespace {
using detail::ppc_integer_context::Address;
using detail::ppc_integer_context::ReadU64;
using detail::ppc_integer_context::WriteU64;
using std::uint64_t;
union PpcRegister {std::uint64_t u64;std::uint32_t u32;float f32;double f64;PpcRegister():u64(0) {}};
struct Fpscr {
 std::uint32_t csr=0;float_triplet_transfer::NativeServices* native=nullptr;
 void disableFlushMode() {if(csr&0x8040u) {csr&=~0x8040u;native->SetHostFpControl(csr);}}
};
struct Context:detail::ppc_integer_context::Context {PpcRegister f1{},f31{};Fpscr fpscr{};};
struct Base {GuestMemory& memory;Dependencies deps;Registers& full;};
void FromFull(Context& c,const Registers& s) {detail::ppc_integer_context::FromFull(c,s);c.f1.u64=s.fpr_bits[1];c.f31.u64=s.fpr_bits[31];c.fpscr.csr=s.cached_fp_control;}
void ToFull(Registers& s,const Context& c) {detail::ppc_integer_context::ToFull(s,c);s.fpr_bits[1]=c.f1.u64;s.fpr_bits[31]=c.f31.u64;s.cached_fp_control=c.fpscr.csr;}
void Direct(GuestAddress entry,Context& c,Base& b) {
 ToFull(b.full,c);if(!crt_close_recursive_buffer_context::Apply(entry,b.memory,b.deps.guest,b.full)) throw std::logic_error("missing accepted float append lower");FromFull(c,b.full);
}
void sub_82BD0C18(Context& c,Base& b) {Direct(0x82bd0c18u,c,b);}
void sub_82BD07D8(Context& c,Base& b) {Direct(0x82bd07d8u,c,b);}
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82BD10D8(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stfd f31,-24(r1)
	ctx.fpscr.disableFlushMode();
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.f31.u64);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// fmr f31,f1
	ctx.f31.f64 = ctx.f1.f64;
	// bl 0x82bd0c18
	ctx.lr = 0x82BD10F8;
	sub_82BD0C18(ctx, base);
	// lwz r4,0(r3)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r11,4(r4)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 4);
	// lwz r10,8(r4)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 8);
	// addi r11,r11,4
	ctx.r[11].s64 = ctx.r[11].s64 + 4;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// ble cr6,0x82bd1118
	if (!ctx.cr6.gt) goto loc_82BD1118;
	// li r5,0
	ctx.r[5].s64 = 0;
	// bl 0x82bd07d8
	ctx.lr = 0x82BD1118;
	sub_82BD07D8(ctx, base);
loc_82BD1118:
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r9,4(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// lwz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// stw r10,16(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 16, ctx.r[10].u32);
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// stw r10,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[10].u32);
	// lwz r11,16(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 16);
	// stfs f31,0(r11)
	ctx.fpscr.disableFlushMode();
	temp.f32 = float(ctx.f31.f64);
	PPC_STORE_U32(ctx.r[11].u32 + 0, temp.u32);
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// lfd f31,-24(r1)
	ctx.f31.u64 = PPC_LOAD_U64(ctx.r[1].u32 + -24);
	// ld r31,-16(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// blr
	return;
}
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
 if(entry!=0x82bd10d8u) return false;
 Context ctx{};ctx.fpscr.native=&deps.fp;FromFull(ctx,state);Base base{memory,deps,state};Body_82BD10D8(ctx,base);ToFull(state,ctx);return true;
}
}
