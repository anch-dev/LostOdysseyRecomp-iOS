#include "lo_semantics/crt_stream_output_upper61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_stream_output_upper61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Direct(GuestAddress entry,Context& ctx,Base& base) {
    ToFull(base.full,ctx);
    if(!crt_stream_output_upper61::ApplyAcceptedLower(entry,base.memory,base.dependencies,base.full))
        throw std::logic_error("missing actual output upper lower");
    FromFull(ctx,base.full);
}
void Body_82319328(Context&,Base&);
void Body_822A03C8(Context&,Base&);
void sub_82319328(Context& c,Base& b){Body_82319328(c,b);}
void sub_822A03C8(Context& c,Base& b){Body_822A03C8(c,b);}
void sub_82B7D158(Context& c,Base& b){Direct(0x82b7d158u,c,b);}
void sub_82B7B1D0(Context& c,Base& b){Direct(0x82b7b1d0u,c,b);}
void sub_82B7B2F8(Context& c,Base& b){Direct(0x82b7b2f8u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_827C8648(Context& ctx, [[maybe_unused]] Base& base) {
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
	// ld r12,-4096(r1)
	ctx.r[12].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -4096);
	// ld r12,-8192(r1)
	ctx.r[12].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8192);
	// stwu r1,-8560(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-8560);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// addi r11,r1,80
	ctx.r[11].s64 = ctx.r[1].s64 + 80;
	// stw r4,8588(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 8588, ctx.r[4].u32);
	// addi r10,r1,8592
	ctx.r[10].s64 = ctx.r[1].s64 + 8592;
	// addi r5,r1,8588
	ctx.r[5].s64 = ctx.r[1].s64 + 8588;
	// li r4,4096
	ctx.r[4].s64 = 4096;
	// addi r3,r1,352
	ctx.r[3].s64 = ctx.r[1].s64 + 352;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// lwz r6,80(r1)
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// bl 0x82319328
	ctx.lr = 0x827C8698;
	sub_82319328(ctx, base);
	// lis r11,-31951
	ctx.r[11].s64 = -2093940736;
	// lwz r11,13880(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 13880);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq cr6,0x827c8734
	if (ctx.cr6.eq) goto loc_827C8734;
	// addi r3,r1,352
	ctx.r[3].s64 = ctx.r[1].s64 + 352;
	// bl 0x82b7b1d0
	ctx.lr = 0x827C86B0;
	sub_82B7B1D0(ctx, base);
	// lis r11,-32254
	ctx.r[11].s64 = -2113798144;
	// addi r3,r11,3436
	ctx.r[3].s64 = ctx.r[11].s64 + 3436;
	// bl 0x82b7b1d0
	ctx.lr = 0x827C86BC;
	sub_82B7B1D0(ctx, base);
	// lis r11,-31951
	ctx.r[11].s64 = -2093940736;
	// lwz r11,13916(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 13916);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 1, ctx.xer);
	// beq cr6,0x827c8728
	if (ctx.cr6.eq) goto loc_827C8728;
	// lis r11,-31951
	ctx.r[11].s64 = -2093940736;
	// lwz r11,13912(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 13912);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 1, ctx.xer);
	// beq cr6,0x827c8728
	if (ctx.cr6.eq) goto loc_827C8728;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,96(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 96, ctx.r[11].u8);
	// bl 0x822a03c8
	ctx.lr = 0x827C86E8;
	sub_822A03C8(ctx, base);
	// mr r5,r3
	ctx.r[5].u64 = ctx.r[3].u64;
	// li r4,256
	ctx.r[4].s64 = 256;
	// addi r3,r1,96
	ctx.r[3].s64 = ctx.r[1].s64 + 96;
	// bl 0x82b7b2f8
	ctx.lr = 0x827C86F8;
	sub_82B7B2F8(ctx, base);
	// lbz r11,96(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 96);
	// extsb r11,r11
	ctx.r[11].s64 = ctx.r[11].s8;
	// cmpwi cr6,r11,89
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 89, ctx.xer);
	// beq cr6,0x827c8734
	if (ctx.cr6.eq) goto loc_827C8734;
	// cmpwi cr6,r11,121
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 121, ctx.xer);
	// li r3,0
	ctx.r[3].s64 = 0;
	// bne cr6,0x827c8738
	if (!ctx.cr6.eq) goto loc_827C8738;
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,8560
	ctx.r[1].s64 = ctx.r[1].s64 + 8560;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
loc_827C8728:
	// lis r11,-32230
	ctx.r[11].s64 = -2112225280;
	// addi r3,r11,24184
	ctx.r[3].s64 = ctx.r[11].s64 + 24184;
	// bl 0x82b7b1d0
	ctx.lr = 0x827C8734;
	sub_82B7B1D0(ctx, base);
loc_827C8734:
	// li r3,1
	ctx.r[3].s64 = 1;
loc_827C8738:
	// addi r1,r1,8560
	ctx.r[1].s64 = ctx.r[1].s64 + 8560;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82319328(Context& ctx, [[maybe_unused]] Base& base) {
	// lwz r5,0(r5)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[5].u32 + 0);
	// b 0x82b7d158
	sub_82B7D158(ctx, base);
	return;
}

void Body_822A03C8(Context& ctx, [[maybe_unused]] Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r3,r11,19184
	ctx.r[3].s64 = ctx.r[11].s64 + 19184;
	// blr
	return;
}
#undef PPC_LOAD_U8
#undef PPC_STORE_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U64
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry==0x82b7b2f8u)return crt_reader_next61::Apply(entry,memory,dependencies,state);
    auto lower=crt_context_adapter::ToStream(state);
    const auto& format=dependencies.close.pipeline.format;
    const bool found=entry==0x82b7d158u
        ?crt_formatter::Apply(entry,memory,format.formatter,lower)
        :crt_format_stream::Apply(entry,memory,format,lower);
    if(!found)return false;
    crt_context_adapter::FromStream(state,lower);return true;
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry!=0x827c8648u)return false;
    Context ctx{};FromFull(ctx,state);Base base{memory,dependencies,state};
    Body_827C8648(ctx,base);ToFull(state,ctx);return true;
}
}
