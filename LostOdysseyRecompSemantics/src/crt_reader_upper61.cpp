#include "lo_semantics/crt_reader_upper61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_upper61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };
void Direct(GuestAddress entry, Context& ctx, Base& base) {
    ToFull(base.full, ctx);
    const bool found = entry == 0x82b85a80u
        ? crt_stream_refill_context::Apply(entry, base.memory, base.dependencies, base.full)
        : crt_stream_close_shared_lower::ApplyAcceptedLower(entry, base.memory, base.dependencies, base.full);
    if (!found) throw std::logic_error("missing reader upper accepted lower");
    FromFull(ctx, base.full);
}
void sub_82B7FD78(Context& c, Base& b) { Direct(0x82b7fd78u,c,b); }
void sub_82B7FEC0(Context& c, Base& b) { Direct(0x82b7fec0u,c,b); }
void sub_823ACBD0(Context& c, Base& b) { Direct(0x823acbd0u,c,b); }
void sub_82B85A80(Context& c, Base& b) { Direct(0x82b85a80u,c,b); }
void Body_82B81648(Context&, Base&);
void Body_82B85C40(Context&, Base&);
void sub_82B81648(Context& c, Base& b) { Body_82B81648(c,b); }
void sub_82B85C40(Context& c, Base& b) { Body_82B85C40(c,b); }
void __savegprlr_29(Context& c, Base& b) { Save(29,c,b); }
void __restgprlr_29(Context& c, Base& b) { Restore(29,c,b); }
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) base.memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82358DB0(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82358DB8;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82358df4
	if (!ctx.cr6.eq) goto loc_82358DF4;
	// bl 0x82b7fd78
	ctx.lr = 0x82358DCC;
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
	ctx.lr = 0x82358DF0;
	sub_82B7FEC0(ctx, base);
	// b 0x82358f6c
	goto loc_82358F6C;
loc_82358DF4:
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// andi. r10,r11,131
	ctx.r[10].u64 = ctx.r[11].u64 & 131;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82358f6c
	if (ctx.cr0.eq) goto loc_82358F6C;
	// rlwinm. r10,r11,0,25,25
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82358f6c
	if (!ctx.cr0.eq) goto loc_82358F6C;
	// rlwinm. r10,r11,0,30,30
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82358e1c
	if (ctx.cr0.eq) goto loc_82358E1C;
	// ori r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 | 32;
	// b 0x82358f68
	goto loc_82358F68;
loc_82358E1C:
	// ori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 | 1;
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// andi. r10,r11,268
	ctx.r[10].u64 = ctx.r[11].u64 & 268;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82358e3c
	if (!ctx.cr0.eq) goto loc_82358E3C;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b85c40
	ctx.lr = 0x82358E38;
	sub_82B85C40(ctx, base);
	// b 0x82358e44
	goto loc_82358E44;
loc_82358E3C:
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_82358E44:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r30,24(r31)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 24);
	// lwz r29,8(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// bl 0x82b81648
	ctx.lr = 0x82358E54;
	sub_82B81648(ctx, base);
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x82b85a80
	ctx.lr = 0x82358E60;
	sub_82B85A80(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// stw r3,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[3].u32);
	// beq 0x82358f48
	if (ctx.cr0.eq) goto loc_82358F48;
	// cmpwi cr6,r3,1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 1, ctx.xer);
	// beq cr6,0x82358f48
	if (ctx.cr6.eq) goto loc_82358F48;
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82358f48
	if (ctx.cr6.eq) goto loc_82358F48;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// andi. r11,r11,130
	ctx.r[11].u64 = ctx.r[11].u64 & 130;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82358f00
	if (!ctx.cr0.eq) goto loc_82358F00;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358E94;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82358edc
	if (ctx.cr6.eq) goto loc_82358EDC;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358EA4;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82358edc
	if (ctx.cr6.eq) goto loc_82358EDC;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358EB4;
	sub_82B81648(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29312
	ctx.r[30].s64 = ctx.r[11].s64 + -29312;
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// rlwinm r29,r11,2,0,29
	ctx.r[29].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82358ECC;
	sub_82B81648(ctx, base);
	// lwzx r10,r29,r30
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[30].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82358ee4
	goto loc_82358EE4;
loc_82358EDC:
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r11,r11,21272
	ctx.r[11].s64 = ctx.r[11].s64 + 21272;
loc_82358EE4:
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// andi. r11,r11,130
	ctx.r[11].u64 = ctx.r[11].u64 & 130;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmplwi cr6,r11,130
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 130, ctx.xer);
	// bne cr6,0x82358f00
	if (!ctx.cr6.eq) goto loc_82358F00;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// ori r11,r11,8192
	ctx.r[11].u64 = ctx.r[11].u64 | 8192;
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
loc_82358F00:
	// lwz r11,24(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 24);
	// cmpwi cr6,r11,512
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 512, ctx.xer);
	// bne cr6,0x82358f28
	if (!ctx.cr6.eq) goto loc_82358F28;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r10,r11,0,28,28
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82358f28
	if (ctx.cr0.eq) goto loc_82358F28;
	// rlwinm. r11,r11,0,21,21
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x400;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82358f28
	if (!ctx.cr0.eq) goto loc_82358F28;
	// li r11,4096
	ctx.r[11].s64 = 4096;
	// stw r11,24(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 24, ctx.r[11].u32);
loc_82358F28:
	// lwz r10,4(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r10,r10,-2
	ctx.r[10].s64 = ctx.r[10].s64 + -2;
	// stw r10,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[10].u32);
	// lhz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U16(ctx.r[11].u32 + 0);
	// addi r11,r11,2
	ctx.r[11].s64 = ctx.r[11].s64 + 2;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
	// b 0x82358f74
	goto loc_82358F74;
loc_82358F48:
	// subfic r11,r3,0
	ctx.xer.ca = ctx.r[3].u32 <= 0;
	ctx.r[11].s64 = 0 - ctx.r[3].s64;
	// lwz r10,12(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// li r9,0
	ctx.r[9].s64 = 0;
	// subfe r11,r11,r11
	temp.u8 = std::uint8_t((~ctx.r[11].u32 + ctx.r[11].u32 < ~ctx.r[11].u32)) | std::uint8_t((~ctx.r[11].u32 + ctx.r[11].u32 + ctx.xer.ca < ctx.xer.ca));
	ctx.r[11].u64 = ~ctx.r[11].u64 + ctx.r[11].u64 + ctx.xer.ca;
	ctx.xer.ca = temp.u8;
	// rlwinm r11,r11,0,27,27
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x10;
	// addi r11,r11,16
	ctx.r[11].s64 = ctx.r[11].s64 + 16;
	// stw r9,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[9].u32);
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
loc_82358F68:
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
loc_82358F6C:
	// lis r3,0
	ctx.r[3].s64 = 0;
	// ori r3,r3,65535
	ctx.r[3].u64 = ctx.r[3].u64 | 65535;
loc_82358F74:
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
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

void Body_82B85C40(Context& ctx, Base& base) {
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
	// lis r11,-31955
	ctx.r[11].s64 = -2094202880;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// li r3,4096
	ctx.r[3].s64 = 4096;
	// lwz r10,15032(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 15032);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// stw r10,15032(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 15032, ctx.r[10].u32);
	// bl 0x823acbd0
	ctx.lr = 0x82B85C6C;
	sub_823ACBD0(ctx, base);
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// stw r3,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[3].u32);
	// beq 0x82b85c8c
	if (ctx.cr0.eq) goto loc_82B85C8C;
	// li r10,4096
	ctx.r[10].s64 = 4096;
	// ori r11,r11,8
	ctx.r[11].u64 = ctx.r[11].u64 | 8;
	// stw r10,24(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 24, ctx.r[10].u32);
	// b 0x82b85ca0
	goto loc_82B85CA0;
loc_82B85C8C:
	// addi r10,r31,20
	ctx.r[10].s64 = ctx.r[31].s64 + 20;
	// li r9,2
	ctx.r[9].s64 = 2;
	// ori r11,r11,4
	ctx.r[11].u64 = ctx.r[11].u64 | 4;
	// stw r10,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[10].u32);
	// stw r9,24(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 24, ctx.r[9].u32);
loc_82B85CA0:
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// li r10,0
	ctx.r[10].s64 = 0;
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// stw r10,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[10].u32);
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
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
#undef PPC_LOAD_U16
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies dependencies, Registers& state) {
    if (entry != 0x82358db0u) return false;
    Context ctx{}; FromFull(ctx,state); Base base{memory,dependencies,state};
    Body_82358DB0(ctx,base); ToFull(state,ctx); return true;
}
}
