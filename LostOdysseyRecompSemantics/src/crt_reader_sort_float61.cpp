#include "lo_semantics/crt_reader_sort_float61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <cmath>
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_sort_float61 {
namespace {
using detail::ppc_integer_context::Address;
using detail::ppc_integer_context::ReadU64;
using detail::ppc_integer_context::WriteU64;
using std::uint64_t;
union PpcRegister {std::uint64_t u64;std::uint32_t u32;float f32;double f64;PpcRegister():u64(0){}};
struct Fpscr {
 std::uint32_t csr=0;float_triplet_transfer::NativeServices* native=nullptr;
 void disableFlushMode(){if(csr&0x8040u){csr&=~0x8040u;native->SetHostFpControl(csr);}}
};
struct Context:detail::ppc_integer_context::Context {PpcRegister f0{},f13{};Fpscr fpscr{};};
struct Base {GuestMemory& memory;Dependencies deps;Registers& full;};
void FromFull(Context& c,const Registers& s){detail::ppc_integer_context::FromFull(c,s);c.f0.u64=s.fpr_bits[0];c.f13.u64=s.fpr_bits[13];c.fpscr.csr=s.cached_fp_control;}
void ToFull(Registers& s,const Context& c){detail::ppc_integer_context::ToFull(s,c);s.fpr_bits[0]=c.f0.u64;s.fpr_bits[13]=c.f13.u64;s.cached_fp_control=c.fpscr.csr;}
void CompareFloat(detail::ppc_integer_context::Cr& cr,double a,double b){
 const bool un=std::isnan(a)||std::isnan(b);cr={std::uint8_t(!un&&a<b),std::uint8_t(!un&&a>b),std::uint8_t(!un&&a==b),std::uint8_t(un)};
}
void sub_82BD0798(Context& c,Base& b){
 ToFull(b.full,c);if(!crt_close_recursive_buffer_context::Apply(0x82bd0798u,b.memory,b.deps.guest,b.full))throw std::logic_error("missing accepted reader table");FromFull(c,b.full);
}
void Indirect(GuestAddress target,Context& c,Base& b){ToFull(b.full,c);b.deps.guest.CallIndirect(target,b.memory,b.full);FromFull(c,b.full);}
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
#define PPC_CALL_INDIRECT_FUNC(t) Indirect((t),ctx,base)
void Body_82BD2A28(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r30,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[30].u64);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// li r30,0
	ctx.r[30].s64 = 0;
	// lfs f13,12(r31)
	ctx.fpscr.disableFlushMode();
	temp.u32 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	ctx.f13.f64 = double(temp.f32);
	// lfs f0,3664(r11)
	temp.u32 = PPC_LOAD_U32(ctx.r[11].u32 + 3664);
	ctx.f0.f64 = double(temp.f32);
	// fcmpu cr6,f13,f0
	CompareFloat(ctx.cr6, ctx.f13.f64, ctx.f0.f64);
	// blt cr6,0x82bd2a80
	if (ctx.cr6.lt) goto loc_82BD2A80;
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd2a80
	if (ctx.cr6.eq) goto loc_82BD2A80;
	// bl 0x82bd0798
	ctx.lr = 0x82BD2A68;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r4,8(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// lwz r11,12(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD2A7C;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r30,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[30].u32);
loc_82BD2A80:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// stw r30,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[30].u32);
	// stw r30,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[30].u32);
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r30,-24(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -24);
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
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state){
 if(entry!=0x82bd2a28u)return false;
 Context c{};c.fpscr.native=&deps.fp;FromFull(c,state);Base b{memory,deps,state};Body_82BD2A28(c,b);ToFull(state,c);return true;
}
}
