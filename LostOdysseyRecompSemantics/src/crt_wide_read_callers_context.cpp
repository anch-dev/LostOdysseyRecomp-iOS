#include "lo_semantics/crt_wide_read_callers_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_wide_read_callers_context {
namespace {
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };
void Direct(GuestAddress entry, Context& ctx, Base& base) {
    ToFull(base.full,ctx); bool found=false;
    if(entry==0x82358db0u) found=crt_reader_upper61::Apply(entry,base.memory,base.dependencies.accepted,base.full);
    else if(entry==0x82b81360u) found=crt_stream_byte_read_context::Apply(entry,base.memory,base.dependencies.accepted,base.full);
    else if(entry==0x82b87cf8u) found=crt_stream_pushback_context::Apply(entry,base.memory,base.dependencies.pushback,base.full);
    else found=crt_stream_close_shared_lower::ApplyAcceptedLower(entry,base.memory,base.dependencies.accepted,base.full);
    if(!found) throw std::logic_error("missing wide read caller accepted lower");
    FromFull(ctx,base.full);
}
void __imp__RtlEnterCriticalSection(Context& ctx, Base& base) {
    ToFull(base.full,ctx);
    base.dependencies.accepted.native.EnterCriticalSection(base.memory,base.full);
    FromFull(ctx,base.full);
}
void Body_823588C0(Context&, Base&);
void Body_82B87ED8(Context&, Base&);
void sub_82B87ED8(Context& c, Base& b) { Body_82B87ED8(c,b); }
void Body_82B87F8C(Context&, Base&);
void sub_82B87F8C(Context& c, Base& b) { Body_82B87F8C(c,b); }
void Body_82B7B708(Context&, Base&);
void sub_82B7B708(Context& c, Base& b) { Body_82B7B708(c,b); }
void Body_82B7A680(Context&, Base&);
void sub_82B7A680(Context& c, Base& b) { Body_82B7A680(c,b); }
void Body_822A07A0(Context&, Base&);
void sub_822A07A0(Context& c, Base& b) { Body_822A07A0(c,b); }
void Body_82B81648(Context&, Base&);
void sub_82B81648(Context& c, Base& b) { Body_82B81648(c,b); }
void sub_82358DB0(Context& c, Base& b) { Direct(0x82358db0u,c,b); }
void sub_82B81360(Context& c, Base& b) { Direct(0x82b81360u,c,b); }
void sub_82B87CF8(Context& c, Base& b) { Direct(0x82b87cf8u,c,b); }
void sub_82B7FD78(Context& c, Base& b) { Direct(0x82b7fd78u,c,b); }
void sub_82B7FEC0(Context& c, Base& b) { Direct(0x82b7fec0u,c,b); }
void sub_82B81B28(Context& c, Base& b) { Direct(0x82b81b28u,c,b); }
void sub_82B7B7C8(Context& c, Base& b) { Direct(0x82b7b7c8u,c,b); }
void __savegprlr_28(Context& c, Base& b) { Save(28,c,b); }
void __restgprlr_28(Context& c, Base& b) { Restore(28,c,b); }
void __savegprlr_29(Context& c, Base& b) { Save(29,c,b); }
void __restgprlr_29(Context& c, Base& b) { Restore(29,c,b); }
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) base.memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),(v))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_823588C0(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x823588C8;
	__savegprlr_28(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r28,r11,21272
	ctx.r[28].s64 = ctx.r[11].s64 + 21272;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
	// bne 0x823589c0
	if (!ctx.cr0.eq) goto loc_823589C0;
	// bl 0x82b81648
	ctx.lr = 0x823588F0;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82358930
	if (ctx.cr6.eq) goto loc_82358930;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358900;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82358930
	if (ctx.cr6.eq) goto loc_82358930;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82358910;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// rlwinm r30,r11,2,0,29
	ctx.r[30].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82358920;
	sub_82B81648(ctx, base);
	// lwzx r10,r30,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82358934
	goto loc_82358934;
loc_82358930:
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
loc_82358934:
	// lbz r11,40(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 40);
	// rlwinm. r11,r11,0,0,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x823589c0
	if (ctx.cr0.eq) goto loc_823589C0;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// blt 0x82358964
	if (ctx.cr0.lt) goto loc_82358964;
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[10].u32);
	// b 0x8235896c
	goto loc_8235896C;
loc_82358964:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81360
	ctx.lr = 0x8235896C;
	sub_82B81360(ctx, base);
loc_8235896C:
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// bne cr6,0x8235897c
	if (!ctx.cr6.eq) goto loc_8235897C;
loc_82358974:
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82358b24
	goto loc_82358B24;
loc_8235897C:
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// stb r3,80(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 80, ctx.r[3].u8);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// blt 0x823589a4
	if (ctx.cr0.lt) goto loc_823589A4;
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[10].u32);
	// b 0x823589ac
	goto loc_823589AC;
loc_823589A4:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81360
	ctx.lr = 0x823589AC;
	sub_82B81360(ctx, base);
loc_823589AC:
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82358974
	if (ctx.cr6.eq) goto loc_82358974;
	// stb r3,81(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 81, ctx.r[3].u8);
loc_823589B8:
	// lhz r3,80(r1)
	ctx.r[3].u64 = PPC_LOAD_U16(ctx.r[1].u32 + 80);
	// b 0x82358b24
	goto loc_82358B24;
loc_823589C0:
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82358af4
	if (!ctx.cr0.eq) goto loc_82358AF4;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x823589D4;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82358a14
	if (ctx.cr6.eq) goto loc_82358A14;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x823589E4;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82358a14
	if (ctx.cr6.eq) goto loc_82358A14;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x823589F4;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// rlwinm r30,r11,2,0,29
	ctx.r[30].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82358A04;
	sub_82B81648(ctx, base);
	// lwzx r10,r30,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82358a18
	goto loc_82358A18;
loc_82358A14:
	// mr r11,r28
	ctx.r[11].u64 = ctx.r[28].u64;
loc_82358A18:
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// rlwinm. r11,r11,0,0,24
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFF80;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82358af4
	if (ctx.cr0.eq) goto loc_82358AF4;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// li r30,1
	ctx.r[30].s64 = 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// blt 0x82358a4c
	if (ctx.cr0.lt) goto loc_82358A4C;
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[10].u32);
	// b 0x82358a54
	goto loc_82358A54;
loc_82358A4C:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81360
	ctx.lr = 0x82358A54;
	sub_82B81360(ctx, base);
loc_82358A54:
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82358974
	if (ctx.cr6.eq) goto loc_82358974;
	// extsb r11,r3
	ctx.r[11].s64 = ctx.r[3].s8;
	// clrlwi r3,r11,24
	ctx.r[3].u64 = ctx.r[11].u32 & 0xFF;
	// stb r11,84(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 84, ctx.r[11].u8);
	// bl 0x82b7a680
	ctx.lr = 0x82358A6C;
	sub_82B7A680(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82358ac4
	if (ctx.cr0.eq) goto loc_82358AC4;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// blt 0x82358a98
	if (ctx.cr0.lt) goto loc_82358A98;
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[10].u32);
	// b 0x82358aa0
	goto loc_82358AA0;
loc_82358A98:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81360
	ctx.lr = 0x82358AA0;
	sub_82B81360(ctx, base);
loc_82358AA0:
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// bne cr6,0x82358abc
	if (!ctx.cr6.eq) goto loc_82358ABC;
	// lbz r11,84(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 84);
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// extsb r3,r11
	ctx.r[3].s64 = ctx.r[11].s8;
	// bl 0x82b87ed8
	ctx.lr = 0x82358AB8;
	sub_82B87ED8(ctx, base);
	// b 0x82358974
	goto loc_82358974;
loc_82358ABC:
	// stb r3,85(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 85, ctx.r[3].u8);
	// li r30,2
	ctx.r[30].s64 = 2;
loc_82358AC4:
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// addi r4,r1,84
	ctx.r[4].s64 = ctx.r[1].s64 + 84;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// bl 0x822a07a0
	ctx.lr = 0x82358AD4;
	sub_822A07A0(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// bne cr6,0x823589b8
	if (!ctx.cr6.eq) goto loc_823589B8;
	// bl 0x82b7fd78
	ctx.lr = 0x82358AE0;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,42
	ctx.r[10].s64 = 42;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// b 0x82358b24
	goto loc_82358B24;
loc_82358AF4:
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// addic. r11,r11,-2
	ctx.xer.ca = ctx.r[11].u32 > 1;
	ctx.r[11].s64 = ctx.r[11].s64 + -2;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[11].u32);
	// blt 0x82358b18
	if (ctx.cr0.lt) goto loc_82358B18;
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// lhz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U16(ctx.r[11].u32 + 0);
	// addi r11,r11,2
	ctx.r[11].s64 = ctx.r[11].s64 + 2;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
	// b 0x82358b24
	goto loc_82358B24;
loc_82358B18:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82358db0
	ctx.lr = 0x82358B20;
	sub_82358DB0(ctx, base);
	// clrlwi r3,r3,16
	ctx.r[3].u64 = ctx.r[3].u32 & 0xFFFF;
loc_82358B24:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}

void Body_82B87ED8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82B87EE0;
	__savegprlr_29(ctx, base);
	// addi r31,r1,-128
	ctx.r[31].s64 = ctx.r[1].s64 + -128;
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// stw r30,156(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 156, ctx.r[30].u32);
	// cntlzw r11,r30
	ctx.r[11].u64 = ctx.r[30].u32 == 0 ? 32 : std::countl_zero(ctx.r[30].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b87f38
	if (!ctx.cr0.eq) goto loc_82B87F38;
	// bl 0x82b7fd78
	ctx.lr = 0x82B87F0C;
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
	ctx.lr = 0x82B87F30;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b87f64
	goto loc_82B87F64;
loc_82B87F38:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b708
	ctx.lr = 0x82B87F40;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82B87F50;
	sub_82B87CF8(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,128
	ctx.r[12].s64 = ctx.r[31].s64 + 128;
	// bl 0x82b87f8c
	ctx.lr = 0x82B87F60;
	sub_82B87F8C(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82B87F64:
	// addi r1,r31,128
	ctx.r[1].s64 = ctx.r[31].s64 + 128;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}

void Body_82B87F8C(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-128
	ctx.r[31].s64 = ctx.r[12].s64 + -128;
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
	ctx.lr = 0x82B87FAC;
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

void Body_82B7A680(Context& ctx, Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// rlwinm r10,r3,1,23,30
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 1) & 0x1FE;
	// lwz r11,21248(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 21248);
	// lwz r11,200(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 200);
	// lhzx r11,r10,r11
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[10].u32 + ctx.r[11].u32);
	// rlwinm r3,r11,0,0,16
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFF8000;
	// blr
	return;
}

void Body_822A07A0(Context& ctx, Base& base) {
	// cmplwi cr6,r4,0
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 0, ctx.xer);
	// beq cr6,0x822a07cc
	if (ctx.cr6.eq) goto loc_822A07CC;
	// cmplwi cr6,r5,0
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, 0, ctx.xer);
	// beq cr6,0x822a07cc
	if (ctx.cr6.eq) goto loc_822A07CC;
	// lbz r11,0(r4)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[4].u32 + 0);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne 0x822a07d4
	if (!ctx.cr0.eq) goto loc_822A07D4;
	// beq cr6,0x822a07cc
	if (ctx.cr6.eq) goto loc_822A07CC;
	// li r11,0
	ctx.r[11].s64 = 0;
	// sth r11,0(r3)
	PPC_STORE_U16(ctx.r[3].u32 + 0, ctx.r[11].u16);
loc_822A07CC:
	// li r3,0
	ctx.r[3].s64 = 0;
	// blr
	return;
loc_822A07D4:
	// beq cr6,0x822a07e0
	if (ctx.cr6.eq) goto loc_822A07E0;
	// clrlwi r11,r11,24
	ctx.r[11].u64 = ctx.r[11].u32 & 0xFF;
	// sth r11,0(r3)
	PPC_STORE_U16(ctx.r[3].u32 + 0, ctx.r[11].u16);
loc_822A07E0:
	// li r3,1
	ctx.r[3].s64 = 1;
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

#undef PPC_LOAD_U8
#undef PPC_LOAD_U16
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U16
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry!=0x823588c0u && entry!=0x82b87ed8u) return false;
    Context ctx{};FromFull(ctx,state);Base base{memory,dependencies,state};
    if(entry==0x823588c0u) Body_823588C0(ctx,base);else Body_82B87ED8(ctx,base);
    ToFull(state,ctx);return true;
}
}
