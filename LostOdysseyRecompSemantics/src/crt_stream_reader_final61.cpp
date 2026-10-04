#include "lo_semantics/crt_stream_reader_final61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_stream_reader_final61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Direct(GuestAddress entry,Context& ctx,Base& base) {
    ToFull(base.full,ctx);
    if(!ApplyAcceptedLower(entry,base.memory,base.dependencies,base.full))
        throw std::logic_error("missing accepted flush wrapper lower");
    FromFull(ctx,base.full);
}
void __imp__RtlEnterCriticalSection(Context& ctx,Base& base) {
    ToFull(base.full,ctx);base.dependencies.accepted.native.EnterCriticalSection(base.memory,base.full);
    FromFull(ctx,base.full);
}
void __savegprlr_27(Context& c,Base& b){Save(27,c,b);}
void __restgprlr_27(Context& c,Base& b){Restore(27,c,b);}
void __savegprlr_28(Context& c,Base& b){Save(28,c,b);}
void __restgprlr_28(Context& c,Base& b){Restore(28,c,b);}
void Body_82B7BB38(Context&,Base&);
void Body_822A03C8(Context&,Base&);
void sub_822A03C8(Context& c,Base& b){Body_822A03C8(c,b);}
void Body_82B7B708(Context&,Base&);
void sub_82B7B708(Context& c,Base& b){Body_82B7B708(c,b);}
void Body_82B7BBF4(Context&,Base&);
void sub_82B7BBF4(Context& c,Base& b){Body_82B7BBF4(c,b);}
void Body_82B7B8D0(Context&,Base&);
void sub_82B7B8D0(Context& c,Base& b){Body_82B7B8D0(c,b);}
void Body_82B7B950(Context&,Base&);
void sub_82B7B950(Context& c,Base& b){Body_82B7B950(c,b);}
void Body_82B7BAC8(Context&,Base&);
void sub_82B7BAC8(Context& c,Base& b){Body_82B7BAC8(c,b);}
void Body_82B7BA78(Context&,Base&);
void sub_82B7BA78(Context& c,Base& b){Body_82B7BA78(c,b);}
void Body_82B7B838(Context&,Base&);
void sub_82B7B838(Context& c,Base& b){Body_82B7B838(c,b);}
void Body_82B81648(Context&,Base&);
void sub_82B81648(Context& c,Base& b){Body_82B81648(c,b);}
void sub_82B7FD78(Context& c,Base& b){Direct(0x82b7fd78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b){Direct(0x82b7fec0u,c,b);}
void sub_82B81B28(Context& c,Base& b){Direct(0x82b81b28u,c,b);}
void sub_82B7B7C8(Context& c,Base& b){Direct(0x82b7b7c8u,c,b);}
void sub_82B7B778(Context& c,Base& b){Direct(0x82b7b778u,c,b);}
void sub_82B7B810(Context& c,Base& b){Direct(0x82b7b810u,c,b);}
void sub_82B819C8(Context& c,Base& b){Direct(0x82b819c8u,c,b);}
void sub_82B81DE0(Context& c,Base& b){Direct(0x82b81de0u,c,b);}
void sub_82B81F78(Context& c,Base& b){Direct(0x82b81f78u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82B7BB38(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r30,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[30].u64);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// addi r31,r1,-112
	ctx.r[31].s64 = ctx.r[1].s64 + -112;
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// stw r30,132(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 132, ctx.r[30].u32);
	// bl 0x822a03c8
	ctx.lr = 0x82B7BB5C;
	sub_822A03C8(ctx, base);
	// addi r11,r3,32
	ctx.r[11].s64 = ctx.r[3].s64 + 32;
	// cmplw cr6,r30,r11
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[11].u32, ctx.xer);
	// beq cr6,0x82b7bbb8
	if (ctx.cr6.eq) goto loc_82B7BBB8;
	// bl 0x822a03c8
	ctx.lr = 0x82B7BB6C;
	sub_822A03C8(ctx, base);
	// addi r11,r3,64
	ctx.r[11].s64 = ctx.r[3].s64 + 64;
	// cmplw cr6,r30,r11
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[11].u32, ctx.xer);
	// beq cr6,0x82b7bbb8
	if (ctx.cr6.eq) goto loc_82B7BBB8;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// bne cr6,0x82b7bb8c
	if (!ctx.cr6.eq) goto loc_82B7BB8C;
	// li r3,0
	ctx.r[3].s64 = 0;
	// bl 0x82b7b950
	ctx.lr = 0x82B7BB88;
	sub_82B7B950(ctx, base);
	// b 0x82b7bbbc
	goto loc_82B7BBBC;
loc_82B7BB8C:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b708
	ctx.lr = 0x82B7BB94;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b8d0
	ctx.lr = 0x82B7BBA0;
	sub_82B7B8D0(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,112
	ctx.r[12].s64 = ctx.r[31].s64 + 112;
	// bl 0x82b7bbf4
	ctx.lr = 0x82B7BBB0;
	sub_82B7BBF4(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// b 0x82b7bbbc
	goto loc_82B7BBBC;
loc_82B7BBB8:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82B7BBBC:
	// addi r1,r31,112
	ctx.r[1].s64 = ctx.r[31].s64 + 112;
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

void Body_822A03C8(Context& ctx, Base&) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r3,r11,19184
	ctx.r[3].s64 = ctx.r[11].s64 + 19184;
	// blr
	return;
}

void Body_82B7B708(Context& ctx, Base& base) {
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
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// addi r11,r11,19184
	ctx.r[11].s64 = ctx.r[11].s64 + 19184;
	// cmplw cr6,r31,r11
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x82b7b758
	if (ctx.cr6.lt) goto loc_82B7B758;
	// addi r10,r11,608
	ctx.r[10].s64 = ctx.r[11].s64 + 608;
	// cmplw cr6,r31,r10
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[10].u32, ctx.xer);
	// bgt cr6,0x82b7b758
	if (ctx.cr6.gt) goto loc_82B7B758;
	// subf r11,r11,r31
	ctx.r[11].s64 = ctx.r[31].s64 - ctx.r[11].s64;
	// srawi r11,r11,5
	ctx.xer.ca = std::uint8_t((ctx.r[11].s32 < 0)) & std::uint8_t(((ctx.r[11].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[11].s32 >> 5;
	// addi r3,r11,16
	ctx.r[3].s64 = ctx.r[11].s64 + 16;
	// bl 0x82b81b28
	ctx.lr = 0x82B7B748;
	sub_82B81B28(ctx, base);
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// ori r11,r11,32768
	ctx.r[11].u64 = ctx.r[11].u64 | 32768;
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// b 0x82b7b760
	goto loc_82B7B760;
loc_82B7B758:
	// addi r3,r31,32
	ctx.r[3].s64 = ctx.r[31].s64 + 32;
	// bl 0x830d9c6c
	ctx.lr = 0x82B7B760;
	__imp__RtlEnterCriticalSection(ctx, base);
loc_82B7B760:
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

void Body_82B7BBF4(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-112
	ctx.r[31].s64 = ctx.r[12].s64 + -112;
	// std r30,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[30].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b7c8
	ctx.lr = 0x82B7BC14;
	sub_82B7B7C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r30,-16(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// lwz r12,-24(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -24);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82B7B8D0(Context& ctx, Base& base) {
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
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82b7b8f4
	if (!ctx.cr6.eq) goto loc_82B7B8F4;
	// bl 0x82b7b950
	ctx.lr = 0x82B7B8F0;
	sub_82B7B950(ctx, base);
	// b 0x82b7b934
	goto loc_82B7B934;
loc_82B7B8F4:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b7b838
	ctx.lr = 0x82B7B8FC;
	sub_82B7B838(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82b7b90c
	if (ctx.cr0.eq) goto loc_82B7B90C;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b7b934
	goto loc_82B7B934;
loc_82B7B90C:
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r11,r11,0,17,17
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b7b930
	if (ctx.cr0.eq) goto loc_82B7B930;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B920;
	sub_82B81648(ctx, base);
	// bl 0x82b81f78
	ctx.lr = 0x82B7B924;
	sub_82B81F78(ctx, base);
	// subfic r11,r3,0
	ctx.xer.ca = ctx.r[3].u32 <= 0;
	ctx.r[11].s64 = 0 - ctx.r[3].s64;
	// subfe r3,r11,r11
	temp.u8 = std::uint8_t((~ctx.r[11].u32 + ctx.r[11].u32 < ~ctx.r[11].u32)) | std::uint8_t((~ctx.r[11].u32 + ctx.r[11].u32 + ctx.xer.ca < ctx.xer.ca));
	ctx.r[3].u64 = ~ctx.r[11].u64 + ctx.r[11].u64 + ctx.xer.ca;
	ctx.xer.ca = temp.u8;
	// b 0x82b7b934
	goto loc_82B7B934;
loc_82B7B930:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82B7B934:
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

void Body_82B7B950(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x82B7B958;
	__savegprlr_27(ctx, base);
	// addi r31,r1,-144
	ctx.r[31].s64 = ctx.r[1].s64 + -144;
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r27,r3
	ctx.r[27].u64 = ctx.r[3].u64;
	// stw r27,164(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 164, ctx.r[27].u32);
	// li r3,1
	ctx.r[3].s64 = 1;
	// li r28,0
	ctx.r[28].s64 = 0;
	// stw r28,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[28].u32);
	// stw r28,88(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 88, ctx.r[28].u32);
	// bl 0x82b81b28
	ctx.lr = 0x82B7B97C;
	sub_82B81B28(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// stw r28,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[28].u32);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29040
	ctx.r[30].s64 = ctx.r[11].s64 + -29040;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r10,r11,-29036
	ctx.r[10].s64 = ctx.r[11].s64 + -29036;
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
loc_82B7B998:
	// lwz r9,0(r10)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// cmpw cr6,r28,r9
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, ctx.r[9].s32, ctx.xer);
	// bge cr6,0x82b7ba50
	if (!ctx.cr6.lt) goto loc_82B7BA50;
	// rlwinm r29,r28,2,0,29
	ctx.r[29].u64 = std::rotl(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r9,r29,r11
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[11].u32);
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// beq cr6,0x82b7ba40
	if (ctx.cr6.eq) goto loc_82B7BA40;
	// rotlwi r4,r9,0
	ctx.r[4].u64 = std::rotl(ctx.r[9].u32, 0);
	// lwz r9,12(r4)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[4].u32 + 12);
	// andi. r9,r9,131
	ctx.r[9].u64 = ctx.r[9].u64 & 131;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// cmpwi r9,0
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x82b7ba40
	if (ctx.cr0.eq) goto loc_82B7BA40;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// bl 0x82b7b778
	ctx.lr = 0x82B7B9D0;
	sub_82B7B778(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// lwzx r3,r29,r11
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[11].u32);
	// lwz r11,12(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 12);
	// andi. r10,r11,131
	ctx.r[10].u64 = ctx.r[11].u64 & 131;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b7ba34
	if (ctx.cr0.eq) goto loc_82B7BA34;
	// cmpwi cr6,r27,1
	ctx.cr6.compare<int32_t>(ctx.r[27].s32, 1, ctx.xer);
	// bne cr6,0x82b7ba10
	if (!ctx.cr6.eq) goto loc_82B7BA10;
	// bl 0x82b7b8d0
	ctx.lr = 0x82B7B9F8;
	sub_82B7B8D0(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82b7ba34
	if (ctx.cr6.eq) goto loc_82B7BA34;
	// lwz r11,84(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 84);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// stw r11,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[11].u32);
	// b 0x82b7ba34
	goto loc_82B7BA34;
loc_82B7BA10:
	// cmpwi cr6,r27,0
	ctx.cr6.compare<int32_t>(ctx.r[27].s32, 0, ctx.xer);
	// bne cr6,0x82b7ba34
	if (!ctx.cr6.eq) goto loc_82B7BA34;
	// rlwinm. r11,r11,0,30,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b7ba34
	if (ctx.cr0.eq) goto loc_82B7BA34;
	// bl 0x82b7b8d0
	ctx.lr = 0x82B7BA24;
	sub_82B7B8D0(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// bne cr6,0x82b7ba34
	if (!ctx.cr6.eq) goto loc_82B7BA34;
	// li r11,-1
	ctx.r[11].s64 = -1;
	// stw r11,88(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 88, ctx.r[11].u32);
loc_82B7BA34:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,144
	ctx.r[12].s64 = ctx.r[31].s64 + 144;
	// bl 0x82b7bac8
	ctx.lr = 0x82B7BA40;
	sub_82B7BAC8(ctx, base);
loc_82B7BA40:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r28,r28,1
	ctx.r[28].s64 = ctx.r[28].s64 + 1;
	// stw r28,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[28].u32);
	// b 0x82b7b998
	goto loc_82B7B998;
loc_82B7BA50:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,144
	ctx.r[12].s64 = ctx.r[31].s64 + 144;
	// bl 0x82b7ba78
	ctx.lr = 0x82B7BA5C;
	sub_82B7BA78(ctx, base);
	// lwz r11,164(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 164);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 1, ctx.xer);
	// lwz r3,84(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 84);
	// beq cr6,0x82b7ba70
	if (ctx.cr6.eq) goto loc_82B7BA70;
	// lwz r3,88(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 88);
loc_82B7BA70:
	// addi r1,r31,144
	ctx.r[1].s64 = ctx.r[31].s64 + 144;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
}

void Body_82B7BAC8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-144
	ctx.r[31].s64 = ctx.r[12].s64 + -144;
	// std r30,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[30].u64);
	// std r28,-24(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -24, ctx.r[28].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-32(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -32, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// rlwinm r11,r28,2,0,29
	ctx.r[11].u64 = std::rotl(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 2) & 0xFFFFFFFC;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// lwz r10,0(r30)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// lwzx r4,r11,r10
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[11].u32 + ctx.r[10].u32);
	// bl 0x82b7b810
	ctx.lr = 0x82B7BAF8;
	sub_82B7B810(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29040
	ctx.r[30].s64 = ctx.r[11].s64 + -29040;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r10,r11,-29036
	ctx.r[10].s64 = ctx.r[11].s64 + -29036;
	// lwz r27,164(r31)
	ctx.r[27].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 164);
	// lwz r28,80(r31)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r30,-16(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// ld r28,-24(r1)
	ctx.r[28].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -24);
	// lwz r12,-32(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -32);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82B7BA78(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r3,1
	ctx.r[3].s64 = 1;
	// bl 0x82b819c8
	ctx.lr = 0x82B7BA8C;
	sub_82B819C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82B7B838(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x82B7B840;
	__savegprlr_28(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// li r28,0
	ctx.r[28].s64 = 0;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// clrlwi r10,r11,30
	ctx.r[10].u64 = ctx.r[11].u32 & 0x3;
	// cmpwi cr6,r10,2
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 2, ctx.xer);
	// bne cr6,0x82b7b8b0
	if (!ctx.cr6.eq) goto loc_82B7B8B0;
	// andi. r11,r11,264
	ctx.r[11].u64 = ctx.r[11].u64 & 264;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b7b8b0
	if (ctx.cr0.eq) goto loc_82B7B8B0;
	// lwz r29,8(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// subf. r30,r29,r11
	ctx.r[30].s64 = ctx.r[11].s64 - ctx.r[29].s64;
	ctx.cr0.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// ble 0x82b7b8b0
	if (!ctx.cr0.gt) goto loc_82B7B8B0;
	// bl 0x82b81648
	ctx.lr = 0x82B7B87C;
	sub_82B81648(ctx, base);
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x82b81de0
	ctx.lr = 0x82B7B888;
	sub_82B81DE0(ctx, base);
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// cmpw cr6,r3,r30
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, ctx.r[30].s32, ctx.xer);
	// bne cr6,0x82b7b8a4
	if (!ctx.cr6.eq) goto loc_82B7B8A4;
	// rlwinm. r10,r11,0,24,24
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x80;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b7b8b0
	if (ctx.cr0.eq) goto loc_82B7B8B0;
	// rlwinm r11,r11,0,31,29
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFFFFFFFFFD;
	// b 0x82b7b8ac
	goto loc_82B7B8AC;
loc_82B7B8A4:
	// li r28,-1
	ctx.r[28].s64 = -1;
	// ori r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 | 32;
loc_82B7B8AC:
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
loc_82B7B8B0:
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}

void Body_82B81648(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// bne cr6,0x82b8168c
	if (!ctx.cr6.eq) goto loc_82B8168C;
	// bl 0x82b7fd78
	ctx.lr = 0x82B81660;
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
	ctx.lr = 0x82B81684;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b81690
	goto loc_82B81690;
loc_82B8168C:
	// lwz r3,16(r3)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 16);
loc_82B81690:
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
#undef PPC_STORE_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U64
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry==0x82b81de0u||entry==0x82b81f78u) {
        auto lower=crt_context_adapter::ToStream(state);
        const auto& accepted=dependencies.accepted.close.pipeline.close.accepted;
        const bool found=entry==0x82b81de0u
            ?crt_stream_operations::Apply(entry,memory,accepted,lower)
            :crt_stream_flush_context::Apply(entry,memory,accepted,dependencies.flush_native,lower);
        if(!found)return false;
        crt_context_adapter::FromStream(state,lower);return true;
    }
    return crt_stream_close_shared_lower::ApplyAcceptedLower(entry,memory,dependencies.accepted,state);
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry!=0x82b7bb38u)return false;
    Context ctx{};FromFull(ctx,state);Base base{memory,dependencies,state};
    Body_82B7BB38(ctx,base);ToFull(state,ctx);return true;
}
}
