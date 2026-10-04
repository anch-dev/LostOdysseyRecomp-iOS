#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_chain61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Direct(GuestAddress entry,Context& ctx,Base& base) {
    ToFull(base.full,ctx);
    if(!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,base.memory,base.dependencies,base.full))
        throw std::logic_error("missing accepted wide pushback lower");
    FromFull(ctx,base.full);
}
void __savegprlr_26(Context& c,Base& b){Save(26,c,b);}
void __restgprlr_26(Context& c,Base& b){Restore(26,c,b);}
void Body_82358B30(Context&,Base&);
void Body_82B86BE0(Context&,Base&);
void sub_82B86BE0(Context& c,Base& b){Body_82B86BE0(c,b);}
void Body_82B86AB8(Context&,Base&);
void sub_82B86AB8(Context& c,Base& b){Body_82B86AB8(c,b);}
void Body_82B7BC40(Context&,Base&);
void sub_82B7BC40(Context& c,Base& b){Body_82B7BC40(c,b);}
void Body_82B81648(Context&,Base&);
void sub_82B81648(Context& c,Base& b){Body_82B81648(c,b);}
void Body_82B85C40(Context&,Base&);
void sub_82B85C40(Context& c,Base& b){Body_82B85C40(c,b);}
void sub_82B7FD78(Context& c,Base& b){Direct(0x82b7fd78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b){Direct(0x82b7fec0u,c,b);}
void sub_823ACBD0(Context& c,Base& b){Direct(0x823acbd0u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_LOAD_U16(a) base.memory.ReadU16(Address(a))
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),(v))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82358B30(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x82358B38;
	__savegprlr_26(ctx, base);
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r26,r3
	ctx.r[26].u64 = ctx.r[3].u64;
	// mr r31,r4
	ctx.r[31].u64 = ctx.r[4].u64;
	// clrlwi r27,r26,16
	ctx.r[27].u64 = ctx.r[26].u32 & 0xFFFF;
	// cmplwi cr6,r27,65535
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 65535, ctx.xer);
	// sth r26,182(r1)
	PPC_STORE_U16(ctx.r[1].u32 + 182, ctx.r[26].u16);
	// beq cr6,0x82358d74
	if (ctx.cr6.eq) goto loc_82358D74;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// clrlwi. r10,r11,31
	ctx.r[10].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82358b70
	if (!ctx.cr0.eq) goto loc_82358B70;
	// rlwinm. r10,r11,0,24,24
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x80;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82358d74
	if (ctx.cr0.eq) goto loc_82358D74;
	// rlwinm. r11,r11,0,30,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82358d74
	if (!ctx.cr0.eq) goto loc_82358D74;
loc_82358B70:
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82358b84
	if (!ctx.cr6.eq) goto loc_82358B84;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b85c40
	ctx.lr = 0x82358B84;
	sub_82B85C40(ctx, base);
loc_82358B84:
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82358d18
	if (!ctx.cr0.eq) goto loc_82358D18;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358B98;
	sub_82B81648(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r28,r11,21272
	ctx.r[28].s64 = ctx.r[11].s64 + 21272;
	// beq cr6,0x82358be8
	if (ctx.cr6.eq) goto loc_82358BE8;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358BB8;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82358be8
	if (ctx.cr6.eq) goto loc_82358BE8;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358BC8;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// rlwinm r30,r11,2,0,29
	ctx.r[30].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82358BD8;
	sub_82B81648(ctx, base);
	// lwzx r10,r30,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82358bec
	goto loc_82358BEC;
loc_82358BE8:
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
loc_82358BEC:
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// rlwinm. r11,r11,0,0,24
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFF80;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82358d18
	if (ctx.cr0.eq) goto loc_82358D18;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358C00;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82358c40
	if (ctx.cr6.eq) goto loc_82358C40;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358C10;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82358c40
	if (ctx.cr6.eq) goto loc_82358C40;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358C20;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// rlwinm r30,r11,2,0,29
	ctx.r[30].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82358C30;
	sub_82B81648(ctx, base);
	// lwzx r10,r30,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82358c44
	goto loc_82358C44;
loc_82358C40:
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
loc_82358C44:
	// lbz r11,40(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 40);
	// rlwinm. r11,r11,0,0,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82358c6c
	if (ctx.cr0.eq) goto loc_82358C6C;
	// lbz r11,182(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 182);
	// li r10,2
	ctx.r[10].s64 = 2;
	// stb r11,84(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 84, ctx.r[11].u8);
	// lbz r11,183(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 183);
	// stw r10,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[10].u32);
	// stb r11,85(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 85, ctx.r[11].u8);
	// b 0x82358c8c
	goto loc_82358C8C;
loc_82358C6C:
	// mr r6,r26
	ctx.r[6].u64 = ctx.r[26].u64;
	// li r5,5
	ctx.r[5].s64 = 5;
	// addi r4,r1,84
	ctx.r[4].s64 = ctx.r[1].s64 + 84;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// bl 0x82b86be0
	ctx.lr = 0x82358C80;
	sub_82B86BE0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82358d74
	if (!ctx.cr0.eq) goto loc_82358D74;
	// lwz r10,80(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
loc_82358C8C:
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// lwz r9,0(r31)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// cmplw cr6,r9,r11
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, ctx.r[11].u32, ctx.xer);
	// bge cr6,0x82358cbc
	if (!ctx.cr6.lt) goto loc_82358CBC;
	// lwz r9,4(r31)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// cmpwi cr6,r9,0
	ctx.cr6.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// bne cr6,0x82358d74
	if (!ctx.cr6.eq) goto loc_82358D74;
	// lwz r9,24(r31)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 24);
	// cmpw cr6,r10,r9
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, ctx.r[9].s32, ctx.xer);
	// bgt cr6,0x82358d74
	if (ctx.cr6.gt) goto loc_82358D74;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_82358CBC:
	// addic. r11,r10,-1
	ctx.xer.ca = ctx.r[10].u32 > 0;
	ctx.r[11].s64 = ctx.r[10].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// blt 0x82358cf0
	if (ctx.cr0.lt) goto loc_82358CF0;
	// addi r10,r1,84
	ctx.r[10].s64 = ctx.r[1].s64 + 84;
	// add r10,r11,r10
	ctx.r[10].u64 = ctx.r[11].u64 + ctx.r[10].u64;
loc_82358CCC:
	// lwz r9,0(r31)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// lbz r8,0(r10)
	ctx.r[8].u64 = PPC_LOAD_U8(ctx.r[10].u32 + 0);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// addi r9,r9,-1
	ctx.r[9].s64 = ctx.r[9].s64 + -1;
	// stw r9,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[9].u32);
	// stb r8,0(r9)
	PPC_STORE_U8(ctx.r[9].u32 + 0, ctx.r[8].u8);
	// bge 0x82358ccc
	if (!ctx.cr0.lt) goto loc_82358CCC;
	// lwz r10,80(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
loc_82358CF0:
	// lwz r9,12(r31)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// li r8,1
	ctx.r[8].s64 = 1;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// rlwimi r9,r8,0,31,31
	ctx.r[9].u64 = (std::rotl(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 0) & 0x1) | (ctx.r[9].u64 & 0xFFFFFFFFFFFFFFFE);
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// rlwimi r9,r8,0,27,27
	ctx.r[9].u64 = (std::rotl(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 0) & 0x10) | (ctx.r[9].u64 & 0xFFFFFFFFFFFFFFEF);
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// stw r9,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[9].u32);
	// b 0x82358d78
	goto loc_82358D78;
loc_82358D18:
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// lwz r10,0(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r11,r11,2
	ctx.r[11].s64 = ctx.r[11].s64 + 2;
	// cmplw cr6,r10,r11
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[11].u32, ctx.xer);
	// bge cr6,0x82358d48
	if (!ctx.cr6.lt) goto loc_82358D48;
	// lwz r10,4(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne cr6,0x82358d74
	if (!ctx.cr6.eq) goto loc_82358D74;
	// lwz r10,24(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 24);
	// cmplwi cr6,r10,2
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 2, ctx.xer);
	// blt cr6,0x82358d74
	if (ctx.cr6.lt) goto loc_82358D74;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_82358D48:
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r11,r11,-2
	ctx.r[11].s64 = ctx.r[11].s64 + -2;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
	// beq 0x82358d80
	if (ctx.cr0.eq) goto loc_82358D80;
	// lhz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U16(ctx.r[11].u32 + 0);
	// cmplw cr6,r10,r27
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[27].u32, ctx.xer);
	// beq cr6,0x82358d84
	if (ctx.cr6.eq) goto loc_82358D84;
	// addi r11,r11,2
	ctx.r[11].s64 = ctx.r[11].s64 + 2;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_82358D74:
	// li r3,-1
	ctx.r[3].s64 = -1;
loc_82358D78:
	// addi r1,r1,160
	ctx.r[1].s64 = ctx.r[1].s64 + 160;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
loc_82358D80:
	// sth r26,0(r11)
	PPC_STORE_U16(ctx.r[11].u32 + 0, ctx.r[26].u16);
loc_82358D84:
	// lwz r10,12(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// li r9,1
	ctx.r[9].s64 = 1;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// rlwimi r10,r9,0,31,31
	ctx.r[10].u64 = (std::rotl(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0x1) | (ctx.r[10].u64 & 0xFFFFFFFFFFFFFFFE);
	// addi r11,r11,2
	ctx.r[11].s64 = ctx.r[11].s64 + 2;
	// rlwimi r10,r9,0,27,27
	ctx.r[10].u64 = (std::rotl(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0x10) | (ctx.r[10].u64 & 0xFFFFFFFFFFFFFFEF);
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// stw r10,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[10].u32);
	// b 0x82358d78
	goto loc_82358D78;
}

void Body_82B86BE0(Context& ctx, Base& base) {
	// li r7,0
	ctx.r[7].s64 = 0;
	// b 0x82b86ab8
	sub_82B86AB8(ctx, base);
	return;
}

void Body_82B86AB8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r11,r4
	ctx.r[11].u64 = ctx.r[4].u64;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82b86ae4
	if (!ctx.cr6.eq) goto loc_82B86AE4;
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, 0, ctx.xer);
	// beq cr6,0x82b86ae4
	if (ctx.cr6.eq) goto loc_82B86AE4;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x82b86bc8
	if (ctx.cr6.eq) goto loc_82B86BC8;
	// b 0x82b86bc4
	goto loc_82B86BC4;
loc_82B86AE4:
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x82b86af4
	if (ctx.cr6.eq) goto loc_82B86AF4;
	// li r10,-1
	ctx.r[10].s64 = -1;
	// stw r10,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[10].u32);
loc_82B86AF4:
	// lis r10,32767
	ctx.r[10].s64 = 2147418112;
	// ori r10,r10,65535
	ctx.r[10].u64 = ctx.r[10].u64 | 65535;
	// cmplw cr6,r5,r10
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, ctx.r[10].u32, ctx.xer);
	// ble cr6,0x82b86b34
	if (!ctx.cr6.gt) goto loc_82B86B34;
	// bl 0x82b7fd78
	ctx.lr = 0x82B86B08;
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
	ctx.lr = 0x82B86B2C;
	sub_82B7FEC0(ctx, base);
	// li r3,22
	ctx.r[3].s64 = 22;
	// b 0x82b86bcc
	goto loc_82B86BCC;
loc_82B86B34:
	// clrlwi r10,r6,16
	ctx.r[10].u64 = ctx.r[6].u32 & 0xFFFF;
	// cmplwi cr6,r10,255
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 255, ctx.xer);
	// ble cr6,0x82b86b74
	if (!ctx.cr6.gt) goto loc_82B86B74;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82b86b5c
	if (ctx.cr6.eq) goto loc_82B86B5C;
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, 0, ctx.xer);
	// beq cr6,0x82b86b5c
	if (ctx.cr6.eq) goto loc_82B86B5C;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r11
	ctx.r[3].u64 = ctx.r[11].u64;
	// bl 0x82b7bc40
	ctx.lr = 0x82B86B5C;
	sub_82B7BC40(ctx, base);
loc_82B86B5C:
	// bl 0x82b7fd78
	ctx.lr = 0x82B86B60;
	sub_82B7FD78(ctx, base);
	// li r11,42
	ctx.r[11].s64 = 42;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fd78
	ctx.lr = 0x82B86B6C;
	sub_82B7FD78(ctx, base);
	// lwz r3,0(r3)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// b 0x82b86bcc
	goto loc_82B86BCC;
loc_82B86B74:
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82b86bb8
	if (ctx.cr6.eq) goto loc_82B86BB8;
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, 0, ctx.xer);
	// bne cr6,0x82b86bb4
	if (!ctx.cr6.eq) goto loc_82B86BB4;
	// bl 0x82b7fd78
	ctx.lr = 0x82B86B88;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,34
	ctx.r[10].s64 = 34;
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
	ctx.lr = 0x82B86BAC;
	sub_82B7FEC0(ctx, base);
	// li r3,34
	ctx.r[3].s64 = 34;
	// b 0x82b86bcc
	goto loc_82B86BCC;
loc_82B86BB4:
	// stb r6,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[6].u8);
loc_82B86BB8:
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x82b86bc8
	if (ctx.cr6.eq) goto loc_82B86BC8;
	// li r11,1
	ctx.r[11].s64 = 1;
loc_82B86BC4:
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
loc_82B86BC8:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82B86BCC:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82B7BC40(Context& ctx, Base& base) {
	// addi r0,r5,1
	ctx.r[0].s64 = ctx.r[5].s64 + 1;
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
	// ori r6,r3,0
	ctx.r[6].u64 = ctx.r[3].u64 | 0;
	// b 0x82b7bc5c
	goto loc_82B7BC5C;
loc_82B7BC50:
	// addi r5,r5,-1
	ctx.r[5].s64 = ctx.r[5].s64 + -1;
	// stb r4,0(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 0, ctx.r[4].u8);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
loc_82B7BC5C:
	// andi. r0,r6,3
	ctx.r[0].u64 = ctx.r[6].u64 & 3;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// bdnzf eq,0x82b7bc50
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0 && !ctx.cr0.eq) goto loc_82B7BC50;
	// rlwimi r4,r4,8,16,23
	ctx.r[4].u64 = (std::rotl(ctx.r[4].u32 | (ctx.r[4].u64 << 32), 8) & 0xFF00) | (ctx.r[4].u64 & 0xFFFFFFFFFFFF00FF);
	// rlwinm. r0,r5,28,4,31
	ctx.r[0].u64 = std::rotl(ctx.r[5].u32 | (ctx.r[5].u64 << 32), 28) & 0xFFFFFFF;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// rlwimi r4,r4,16,0,15
	ctx.r[4].u64 = (std::rotl(ctx.r[4].u32 | (ctx.r[4].u64 << 32), 16) & 0xFFFF0000) | (ctx.r[4].u64 & 0xFFFFFFFF0000FFFF);
	// beq+ 0x82b7bc90
	if (ctx.cr0.eq) goto loc_82B7BC90;
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
loc_82B7BC78:
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// stw r4,4(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 4, ctx.r[4].u32);
	// stw r4,8(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 8, ctx.r[4].u32);
	// stw r4,12(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 12, ctx.r[4].u32);
	// addi r6,r6,16
	ctx.r[6].s64 = ctx.r[6].s64 + 16;
	// bdnz+ 0x82b7bc78
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7BC78;
loc_82B7BC90:
	// rlwinm. r0,r5,30,30,31
	ctx.r[0].u64 = std::rotl(ctx.r[5].u32 | (ctx.r[5].u64 << 32), 30) & 0x3;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// beq- 0x82b7bcbc
	if (ctx.cr0.eq) goto loc_82B7BCBC;
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// addi r6,r6,4
	ctx.r[6].s64 = ctx.r[6].s64 + 4;
	// bdz- 0x82b7bcbc
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) goto loc_82B7BCBC;
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// addi r6,r6,4
	ctx.r[6].s64 = ctx.r[6].s64 + 4;
	// bdz- 0x82b7bcbc
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) goto loc_82B7BCBC;
	// stw r4,0(r6)
	PPC_STORE_U32(ctx.r[6].u32 + 0, ctx.r[4].u32);
	// addi r6,r6,4
	ctx.r[6].s64 = ctx.r[6].s64 + 4;
loc_82B7BCBC:
	// andi. r0,r5,3
	ctx.r[0].u64 = ctx.r[5].u64 & 3;
	ctx.cr0.compare<int32_t>(ctx.r[0].s32, 0, ctx.xer);
	// mtctr r0
	ctx.ctr.u64 = ctx.r[0].u64;
	// beqlr+
	if (ctx.cr0.eq) return;
	// stb r4,0(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 0, ctx.r[4].u8);
	// bdzlr-
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) return;
	// stb r4,1(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 1, ctx.r[4].u8);
	// bdzlr-
	--ctx.ctr.u64;
	if (ctx.ctr.u32 == 0) return;
	// stb r4,2(r6)
	PPC_STORE_U8(ctx.r[6].u32 + 2, ctx.r[4].u8);
	// blr
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
#undef PPC_STORE_U8
#undef PPC_LOAD_U16
#undef PPC_STORE_U16
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry!=0x82358b30u)return false;
    Context ctx{};FromFull(ctx,state);Base base{memory,dependencies,state};
    Body_82358B30(ctx,base);ToFull(state,ctx);return true;
}
}
