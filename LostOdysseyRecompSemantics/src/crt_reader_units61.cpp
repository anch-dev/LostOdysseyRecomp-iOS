#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_units61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; GuestServices& guest; Registers& full; };
void __savegprlr_28(Context& c,Base& b) { Save(28u,c,b); }
void __restgprlr_28(Context& c,Base& b) { Restore(28u,c,b); }
void Direct(GuestAddress entry,Context& c,Base& b) {
 ToFull(b.full,c);
 if (!crt_close_recursive_buffer_context::Apply(entry,b.memory,b.guest,b.full)) throw std::logic_error("missing accepted recursive lower");
 FromFull(c,b.full);
}
void sub_82BD0C18(Context& c,Base& b) { Direct(0x82bd0c18u,c,b); }
void sub_82BD07D8(Context& c,Base& b) { Direct(0x82bd07d8u,c,b); }
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82BD1050(Context& ctx, [[maybe_unused]] Base& base) {
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
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// bl 0x82bd0c18
	ctx.lr = 0x82BD1070;
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
	// ble cr6,0x82bd1090
	if (!ctx.cr6.gt) goto loc_82BD1090;
	// li r5,0
	ctx.r[5].s64 = 0;
	// bl 0x82bd07d8
	ctx.lr = 0x82BD1090;
	sub_82BD07D8(ctx, base);
loc_82BD1090:
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lwz r9,4(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
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
	// stw r30,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[30].u32);
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
void Body_82BD1160(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x82BD1168;
	__savegprlr_28(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r29,r4
	ctx.r[29].u64 = ctx.r[4].u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r30,r5
	ctx.r[30].u64 = ctx.r[5].u64;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82bd11f4
	if (ctx.cr6.eq) goto loc_82BD11F4;
	// bl 0x82bd0c18
	ctx.lr = 0x82BD1184;
	sub_82BD0C18(ctx, base);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x82bd11f0
	if (ctx.cr6.eq) goto loc_82BD11F0;
loc_82BD118C:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lbz r28,0(r29)
	ctx.r[28].u64 = PPC_LOAD_U8(ctx.r[29].u32 + 0);
	// addi r30,r30,-1
	ctx.r[30].s64 = ctx.r[30].s64 + -1;
	// bl 0x82bd0c18
	ctx.lr = 0x82BD119C;
	sub_82BD0C18(ctx, base);
	// lwz r4,0(r3)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r11,4(r4)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 4);
	// lwz r10,8(r4)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 8);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// ble cr6,0x82bd11bc
	if (!ctx.cr6.gt) goto loc_82BD11BC;
	// li r5,0
	ctx.r[5].s64 = 0;
	// bl 0x82bd07d8
	ctx.lr = 0x82BD11BC;
	sub_82BD07D8(ctx, base);
loc_82BD11BC:
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r29,r29,1
	ctx.r[29].s64 = ctx.r[29].s64 + 1;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// lwz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// stw r10,16(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 16, ctx.r[10].u32);
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// stw r10,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[10].u32);
	// lwz r11,16(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 16);
	// stb r28,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[28].u8);
	// bne cr6,0x82bd118c
	if (!ctx.cr6.eq) goto loc_82BD118C;
loc_82BD11F0:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
loc_82BD11F4:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}
void Body_82BD09C0(Context& ctx, [[maybe_unused]] Base& base) {
	// li r10,0
	ctx.r[10].s64 = 0;
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// stb r10,24(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 24, ctx.r[10].u8);
	// lwz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lwz r9,4(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// stw r10,16(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 16, ctx.r[10].u32);
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// stw r10,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[10].u32);
	// lwz r11,16(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 16);
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// blr
	return;
}
void Body_82BD09F8(Context& ctx, [[maybe_unused]] Base& base) {
	// li r10,0
	ctx.r[10].s64 = 0;
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// stb r10,24(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 24, ctx.r[10].u8);
	// lwz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lwz r9,4(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// stw r10,16(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 16, ctx.r[10].u32);
	// lwz r10,4(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// stw r10,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[10].u32);
	// lwz r11,16(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 16);
	// lwz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// blr
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestServices& guest,Registers& state) {
 if(entry!=0x82bd1050u && entry!=0x82bd1160u && entry!=0x82bd09c0u && entry!=0x82bd09f8u) return false;
 Context ctx{}; FromFull(ctx,state); Base base{memory,guest,state};
 switch(entry) {case 0x82bd1050u: Body_82BD1050(ctx,base);break;case 0x82bd1160u: Body_82BD1160(ctx,base);break;case 0x82bd09c0u: Body_82BD09C0(ctx,base);break;default: Body_82BD09F8(ctx,base);}
 ToFull(state,ctx);return true;
}
}
