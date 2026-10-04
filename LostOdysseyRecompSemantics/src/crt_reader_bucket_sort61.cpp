#include "lo_semantics/crt_reader_bucket_sort61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_bucket_sort61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies deps;Registers& full;};
void __savegprlr_23(Context& c,Base& b) {Save(23u,c,b);}
void __restgprlr_23(Context& c,Base& b) {Restore(23u,c,b);}
void sub_82BD2D08(Context& c,Base& b) {ToFull(b.full,c);if(!crt_reader_follow61::Apply(0x82bd2d08u,b.memory,b.deps.guest,b.full)) throw std::logic_error("missing accepted sort allocation");FromFull(c,b.full);}
void sub_82B7BC40(Context& c,Base& b) {ToFull(b.full,c);crt_reader_chain61::ApplySupport_B7BC40(b.memory,b.deps.accepted,b.full);FromFull(c,b.full);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
void Body_82BD2DF0(Context& ctx, [[maybe_unused]] Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6d4
	ctx.lr = 0x82BD2DF8;
	__savegprlr_23(ctx, base);
	// ld r12,-4096(r1)
	ctx.r[12].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -4096);
	// stwu r1,-5280(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-5280);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r27,r4
	ctx.r[27].u64 = ctx.r[4].u64;
	// mr r28,r3
	ctx.r[28].u64 = ctx.r[3].u64;
	// mr r26,r5
	ctx.r[26].u64 = ctx.r[5].u64;
	// mr r24,r6
	ctx.r[24].u64 = ctx.r[6].u64;
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x82bd35b4
	if (ctx.cr6.eq) goto loc_82BD35B4;
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82bd35b4
	if (ctx.cr6.eq) goto loc_82BD35B4;
	// rlwinm r11,r26,0,0,0
	ctx.r[11].u64 = std::rotl(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 0) & 0x80000000;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82bd35b4
	if (!ctx.cr6.eq) goto loc_82BD35B4;
	// lwz r10,12(r28)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 12);
	// lwz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// clrlwi r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u32 & 0x7FFFFFFF;
	// cmplw cr6,r26,r11
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, ctx.r[11].u32, ctx.xer);
	// stw r10,12(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 12, ctx.r[10].u32);
	// beq cr6,0x82bd2e5c
	if (ctx.cr6.eq) goto loc_82BD2E5C;
	// ble cr6,0x82bd2e54
	if (!ctx.cr6.gt) goto loc_82BD2E54;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// bl 0x82bd2d08
	ctx.lr = 0x82BD2E54;
	sub_82BD2D08(ctx, base);
loc_82BD2E54:
	// oris r11,r26,32768
	ctx.r[11].u64 = ctx.r[26].u64 | 2147483648;
	// stw r11,0(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 0, ctx.r[11].u32);
loc_82BD2E5C:
	// cmpwi cr6,r24,1
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 1, ctx.xer);
	// li r5,4096
	ctx.r[5].s64 = 4096;
	// li r4,0
	ctx.r[4].s64 = 0;
	// addi r3,r1,1104
	ctx.r[3].s64 = ctx.r[1].s64 + 1104;
	// bne cr6,0x82bd30e4
	if (!ctx.cr6.eq) goto loc_82BD30E4;
	// bl 0x82b7bc40
	ctx.lr = 0x82BD2E74;
	sub_82B7BC40(ctx, base);
	// lwz r10,0(r28)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// rlwinm r25,r26,2,0,29
	ctx.r[25].u64 = std::rotl(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r10,r10,0,0,0
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x80000000;
	// mr r11,r27
	ctx.r[11].u64 = ctx.r[27].u64;
	// li r30,1
	ctx.r[30].s64 = 1;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// add r5,r25,r27
	ctx.r[5].u64 = ctx.r[25].u64 + ctx.r[27].u64;
	// beq cr6,0x82bd2f88
	if (ctx.cr6.eq) goto loc_82BD2F88;
	// lwz r3,0(r27)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 0);
	// mr r31,r27
	ctx.r[31].u64 = ctx.r[27].u64;
	// cmplw cr6,r27,r5
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, ctx.r[5].u32, ctx.xer);
	// beq cr6,0x82bd2f38
	if (ctx.cr6.eq) goto loc_82BD2F38;
loc_82BD2EA4:
	// lwz r4,0(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r31,r31,4
	ctx.r[31].s64 = ctx.r[31].s64 + 4;
	// cmplw cr6,r4,r3
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, ctx.r[3].u32, ctx.xer);
	// blt cr6,0x82bd2f34
	if (ctx.cr6.lt) goto loc_82BD2F34;
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r6,r1,4176
	ctx.r[6].s64 = ctx.r[1].s64 + 4176;
	// mr r3,r4
	ctx.r[3].u64 = ctx.r[4].u64;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r7,r1,3152
	ctx.r[7].s64 = ctx.r[1].s64 + 3152;
	// addi r8,r1,2128
	ctx.r[8].s64 = ctx.r[1].s64 + 2128;
	// addi r9,r1,1104
	ctx.r[9].s64 = ctx.r[1].s64 + 1104;
	// lwzx r4,r10,r6
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[6].u32);
	// addi r4,r4,1
	ctx.r[4].s64 = ctx.r[4].s64 + 1;
	// stwx r4,r10,r6
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[6].u32, ctx.r[4].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r6,r10,r7
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[7].u32);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// stwx r6,r10,r7
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[7].u32, ctx.r[6].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r7,r10,r8
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[8].u32);
	// addi r7,r7,1
	ctx.r[7].s64 = ctx.r[7].s64 + 1;
	// stwx r7,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[7].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// lwzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[9].u32);
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// stwx r8,r10,r9
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u32);
	// bne cr6,0x82bd2ea4
	if (!ctx.cr6.eq) goto loc_82BD2EA4;
	// b 0x82bd2f38
	goto loc_82BD2F38;
loc_82BD2F34:
	// li r30,0
	ctx.r[30].s64 = 0;
loc_82BD2F38:
	// clrlwi r10,r30,24
	ctx.r[10].u64 = ctx.r[30].u32 & 0xFF;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82bd3060
	if (ctx.cr6.eq) goto loc_82BD3060;
	// lwz r10,16(r28)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 16);
	// li r11,0
	ctx.r[11].s64 = 0;
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// stw r10,16(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 16, ctx.r[10].u32);
	// beq cr6,0x82bd35b4
	if (ctx.cr6.eq) goto loc_82BD35B4;
	// li r10,0
	ctx.r[10].s64 = 0;
loc_82BD2F60:
	// lwz r8,4(r28)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// mr r9,r11
	ctx.r[9].u64 = ctx.r[11].u64;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplw cr6,r11,r26
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[26].u32, ctx.xer);
	// stwx r9,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[9].u32);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// blt cr6,0x82bd2f60
	if (ctx.cr6.lt) goto loc_82BD2F60;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// addi r1,r1,5280
	ctx.r[1].s64 = ctx.r[1].s64 + 5280;
	// b 0x82b7a724
	__restgprlr_23(ctx, base);
	return;
loc_82BD2F88:
	// lwz r31,4(r28)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// cmplw cr6,r27,r5
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, ctx.r[5].u32, ctx.xer);
	// lwz r10,0(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// rlwinm r10,r10,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r3,r10,r27
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[27].u32);
	// beq cr6,0x82bd303c
	if (ctx.cr6.eq) goto loc_82BD303C;
loc_82BD2FA0:
	// lwz r10,0(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r31,r31,4
	ctx.r[31].s64 = ctx.r[31].s64 + 4;
	// rlwinm r10,r10,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r4,r10,r27
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[27].u32);
	// cmplw cr6,r4,r3
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, ctx.r[3].u32, ctx.xer);
	// blt cr6,0x82bd3038
	if (ctx.cr6.lt) goto loc_82BD3038;
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r6,r1,4176
	ctx.r[6].s64 = ctx.r[1].s64 + 4176;
	// mr r3,r4
	ctx.r[3].u64 = ctx.r[4].u64;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r7,r1,3152
	ctx.r[7].s64 = ctx.r[1].s64 + 3152;
	// addi r8,r1,2128
	ctx.r[8].s64 = ctx.r[1].s64 + 2128;
	// addi r9,r1,1104
	ctx.r[9].s64 = ctx.r[1].s64 + 1104;
	// lwzx r4,r10,r6
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[6].u32);
	// addi r4,r4,1
	ctx.r[4].s64 = ctx.r[4].s64 + 1;
	// stwx r4,r10,r6
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[6].u32, ctx.r[4].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r6,r10,r7
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[7].u32);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// stwx r6,r10,r7
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[7].u32, ctx.r[6].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r7,r10,r8
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[8].u32);
	// addi r7,r7,1
	ctx.r[7].s64 = ctx.r[7].s64 + 1;
	// stwx r7,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[7].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// lwzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[9].u32);
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// stwx r8,r10,r9
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u32);
	// bne cr6,0x82bd2fa0
	if (!ctx.cr6.eq) goto loc_82BD2FA0;
	// b 0x82bd303c
	goto loc_82BD303C;
loc_82BD3038:
	// li r30,0
	ctx.r[30].s64 = 0;
loc_82BD303C:
	// clrlwi r10,r30,24
	ctx.r[10].u64 = ctx.r[30].u32 & 0xFF;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82bd3060
	if (ctx.cr6.eq) goto loc_82BD3060;
loc_82BD3048:
	// lwz r11,16(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 16);
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// stw r11,16(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 16, ctx.r[11].u32);
	// addi r1,r1,5280
	ctx.r[1].s64 = ctx.r[1].s64 + 5280;
	// b 0x82b7a724
	__restgprlr_23(ctx, base);
	return;
loc_82BD3060:
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// beq cr6,0x82bd333c
	if (ctx.cr6.eq) goto loc_82BD333C;
loc_82BD3068:
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r6,r1,4176
	ctx.r[6].s64 = ctx.r[1].s64 + 4176;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// addi r7,r1,3152
	ctx.r[7].s64 = ctx.r[1].s64 + 3152;
	// addi r8,r1,2128
	ctx.r[8].s64 = ctx.r[1].s64 + 2128;
	// addi r9,r1,1104
	ctx.r[9].s64 = ctx.r[1].s64 + 1104;
	// lwzx r4,r10,r6
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[6].u32);
	// addi r4,r4,1
	ctx.r[4].s64 = ctx.r[4].s64 + 1;
	// stwx r4,r10,r6
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[6].u32, ctx.r[4].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r6,r10,r7
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[7].u32);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// stwx r6,r10,r7
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[7].u32, ctx.r[6].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r7,r10,r8
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[8].u32);
	// addi r7,r7,1
	ctx.r[7].s64 = ctx.r[7].s64 + 1;
	// stwx r7,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[7].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// lwzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[9].u32);
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// stwx r8,r10,r9
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u32);
	// bne cr6,0x82bd3068
	if (!ctx.cr6.eq) goto loc_82BD3068;
	// b 0x82bd333c
	goto loc_82BD333C;
loc_82BD30E4:
	// bl 0x82b7bc40
	ctx.lr = 0x82BD30E8;
	sub_82B7BC40(ctx, base);
	// lwz r10,0(r28)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// rlwinm r25,r26,2,0,29
	ctx.r[25].u64 = std::rotl(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r10,r10,0,0,0
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x80000000;
	// mr r11,r27
	ctx.r[11].u64 = ctx.r[27].u64;
	// li r30,1
	ctx.r[30].s64 = 1;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// add r5,r25,r27
	ctx.r[5].u64 = ctx.r[25].u64 + ctx.r[27].u64;
	// beq cr6,0x82bd31fc
	if (ctx.cr6.eq) goto loc_82BD31FC;
	// lwz r3,0(r27)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 0);
	// mr r31,r27
	ctx.r[31].u64 = ctx.r[27].u64;
	// cmplw cr6,r27,r5
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, ctx.r[5].u32, ctx.xer);
	// beq cr6,0x82bd31ac
	if (ctx.cr6.eq) goto loc_82BD31AC;
loc_82BD3118:
	// lwz r4,0(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r31,r31,4
	ctx.r[31].s64 = ctx.r[31].s64 + 4;
	// cmpw cr6,r4,r3
	ctx.cr6.compare<int32_t>(ctx.r[4].s32, ctx.r[3].s32, ctx.xer);
	// blt cr6,0x82bd31a8
	if (ctx.cr6.lt) goto loc_82BD31A8;
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r6,r1,4176
	ctx.r[6].s64 = ctx.r[1].s64 + 4176;
	// mr r3,r4
	ctx.r[3].u64 = ctx.r[4].u64;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r7,r1,3152
	ctx.r[7].s64 = ctx.r[1].s64 + 3152;
	// addi r8,r1,2128
	ctx.r[8].s64 = ctx.r[1].s64 + 2128;
	// addi r9,r1,1104
	ctx.r[9].s64 = ctx.r[1].s64 + 1104;
	// lwzx r4,r10,r6
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[6].u32);
	// addi r4,r4,1
	ctx.r[4].s64 = ctx.r[4].s64 + 1;
	// stwx r4,r10,r6
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[6].u32, ctx.r[4].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r6,r10,r7
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[7].u32);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// stwx r6,r10,r7
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[7].u32, ctx.r[6].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r7,r10,r8
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[8].u32);
	// addi r7,r7,1
	ctx.r[7].s64 = ctx.r[7].s64 + 1;
	// stwx r7,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[7].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// lwzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[9].u32);
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// stwx r8,r10,r9
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u32);
	// bne cr6,0x82bd3118
	if (!ctx.cr6.eq) goto loc_82BD3118;
	// b 0x82bd31ac
	goto loc_82BD31AC;
loc_82BD31A8:
	// li r30,0
	ctx.r[30].s64 = 0;
loc_82BD31AC:
	// clrlwi r10,r30,24
	ctx.r[10].u64 = ctx.r[30].u32 & 0xFF;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82bd32bc
	if (ctx.cr6.eq) goto loc_82BD32BC;
	// lwz r10,16(r28)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 16);
	// li r11,0
	ctx.r[11].s64 = 0;
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// stw r10,16(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 16, ctx.r[10].u32);
	// beq cr6,0x82bd35b4
	if (ctx.cr6.eq) goto loc_82BD35B4;
	// li r10,0
	ctx.r[10].s64 = 0;
loc_82BD31D4:
	// lwz r8,4(r28)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// mr r9,r11
	ctx.r[9].u64 = ctx.r[11].u64;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplw cr6,r11,r26
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[26].u32, ctx.xer);
	// stwx r9,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[9].u32);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// blt cr6,0x82bd31d4
	if (ctx.cr6.lt) goto loc_82BD31D4;
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// addi r1,r1,5280
	ctx.r[1].s64 = ctx.r[1].s64 + 5280;
	// b 0x82b7a724
	__restgprlr_23(ctx, base);
	return;
loc_82BD31FC:
	// lwz r31,4(r28)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// cmplw cr6,r27,r5
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, ctx.r[5].u32, ctx.xer);
	// lwz r10,0(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// rlwinm r10,r10,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r3,r10,r27
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[27].u32);
	// beq cr6,0x82bd32b0
	if (ctx.cr6.eq) goto loc_82BD32B0;
loc_82BD3214:
	// lwz r10,0(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r31,r31,4
	ctx.r[31].s64 = ctx.r[31].s64 + 4;
	// rlwinm r10,r10,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r4,r10,r27
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[27].u32);
	// cmpw cr6,r4,r3
	ctx.cr6.compare<int32_t>(ctx.r[4].s32, ctx.r[3].s32, ctx.xer);
	// blt cr6,0x82bd32ac
	if (ctx.cr6.lt) goto loc_82BD32AC;
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r6,r1,4176
	ctx.r[6].s64 = ctx.r[1].s64 + 4176;
	// mr r3,r4
	ctx.r[3].u64 = ctx.r[4].u64;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r7,r1,3152
	ctx.r[7].s64 = ctx.r[1].s64 + 3152;
	// addi r8,r1,2128
	ctx.r[8].s64 = ctx.r[1].s64 + 2128;
	// addi r9,r1,1104
	ctx.r[9].s64 = ctx.r[1].s64 + 1104;
	// lwzx r4,r10,r6
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[6].u32);
	// addi r4,r4,1
	ctx.r[4].s64 = ctx.r[4].s64 + 1;
	// stwx r4,r10,r6
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[6].u32, ctx.r[4].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r6,r10,r7
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[7].u32);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// stwx r6,r10,r7
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[7].u32, ctx.r[6].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r7,r10,r8
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[8].u32);
	// addi r7,r7,1
	ctx.r[7].s64 = ctx.r[7].s64 + 1;
	// stwx r7,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[7].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// lwzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[9].u32);
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// stwx r8,r10,r9
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u32);
	// bne cr6,0x82bd3214
	if (!ctx.cr6.eq) goto loc_82BD3214;
	// b 0x82bd32b0
	goto loc_82BD32B0;
loc_82BD32AC:
	// li r30,0
	ctx.r[30].s64 = 0;
loc_82BD32B0:
	// clrlwi r10,r30,24
	ctx.r[10].u64 = ctx.r[30].u32 & 0xFF;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// bne cr6,0x82bd3048
	if (!ctx.cr6.eq) goto loc_82BD3048;
loc_82BD32BC:
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// beq cr6,0x82bd333c
	if (ctx.cr6.eq) goto loc_82BD333C;
loc_82BD32C4:
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r6,r1,4176
	ctx.r[6].s64 = ctx.r[1].s64 + 4176;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// addi r7,r1,3152
	ctx.r[7].s64 = ctx.r[1].s64 + 3152;
	// addi r8,r1,2128
	ctx.r[8].s64 = ctx.r[1].s64 + 2128;
	// addi r9,r1,1104
	ctx.r[9].s64 = ctx.r[1].s64 + 1104;
	// lwzx r4,r10,r6
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[6].u32);
	// addi r4,r4,1
	ctx.r[4].s64 = ctx.r[4].s64 + 1;
	// stwx r4,r10,r6
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[6].u32, ctx.r[4].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r6,r10,r7
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[7].u32);
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// stwx r6,r10,r7
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[7].u32, ctx.r[6].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r7,r10,r8
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[8].u32);
	// addi r7,r7,1
	ctx.r[7].s64 = ctx.r[7].s64 + 1;
	// stwx r7,r10,r8
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[8].u32, ctx.r[7].u32);
	// lbz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// cmplw cr6,r11,r5
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[5].u32, ctx.xer);
	// lwzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[9].u32);
	// addi r8,r8,1
	ctx.r[8].s64 = ctx.r[8].s64 + 1;
	// stwx r8,r10,r9
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u32);
	// bne cr6,0x82bd32c4
	if (!ctx.cr6.eq) goto loc_82BD32C4;
loc_82BD333C:
	// li r29,0
	ctx.r[29].s64 = 0;
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// bne cr6,0x82bd3380
	if (!ctx.cr6.eq) goto loc_82BD3380;
	// addi r11,r1,4696
	ctx.r[11].s64 = ctx.r[1].s64 + 4696;
	// li r10,32
	ctx.r[10].s64 = 32;
loc_82BD3350:
	// lwz r8,-4(r11)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -4);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// lwz r9,-8(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -8);
	// lwz r7,4(r11)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// add r9,r9,r8
	ctx.r[9].u64 = ctx.r[9].u64 + ctx.r[8].u64;
	// lwz r8,0(r11)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// addi r11,r11,16
	ctx.r[11].s64 = ctx.r[11].s64 + 16;
	// add r9,r9,r7
	ctx.r[9].u64 = ctx.r[9].u64 + ctx.r[7].u64;
	// add r9,r9,r8
	ctx.r[9].u64 = ctx.r[9].u64 + ctx.r[8].u64;
	// add r29,r9,r29
	ctx.r[29].u64 = ctx.r[9].u64 + ctx.r[29].u64;
	// bne cr6,0x82bd3350
	if (!ctx.cr6.eq) goto loc_82BD3350;
loc_82BD3380:
	// li r30,0
	ctx.r[30].s64 = 0;
loc_82BD3384:
	// lbzx r9,r30,r27
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[30].u32 + ctx.r[27].u32);
	// rlwinm r11,r30,10,0,21
	ctx.r[11].u64 = std::rotl(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 10) & 0xFFFFFC00;
	// addi r10,r1,1104
	ctx.r[10].s64 = ctx.r[1].s64 + 1104;
	// rotlwi r8,r9,2
	ctx.r[8].u64 = std::rotl(ctx.r[9].u32, 2);
	// add r9,r11,r10
	ctx.r[9].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// li r11,1
	ctx.r[11].s64 = 1;
	// lwzx r10,r8,r9
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[8].u32 + ctx.r[9].u32);
	// cmplw cr6,r10,r26
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[26].u32, ctx.xer);
	// bne cr6,0x82bd33ac
	if (!ctx.cr6.eq) goto loc_82BD33AC;
	// li r11,0
	ctx.r[11].s64 = 0;
loc_82BD33AC:
	// clrlwi r11,r11,24
	ctx.r[11].u64 = ctx.r[11].u32 & 0xFF;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82bd35a8
	if (ctx.cr6.eq) goto loc_82BD35A8;
	// cmplwi cr6,r30,3
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 3, ctx.xer);
	// bne cr6,0x82bd3448
	if (!ctx.cr6.eq) goto loc_82BD3448;
	// cmpwi cr6,r24,1
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 1, ctx.xer);
	// beq cr6,0x82bd3448
	if (ctx.cr6.eq) goto loc_82BD3448;
	// lwz r5,8(r28)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 8);
	// rlwinm r8,r29,2,0,29
	ctx.r[8].u64 = std::rotl(ctx.r[29].u32 | (ctx.r[29].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r10,r1,80
	ctx.r[10].s64 = ctx.r[1].s64 + 80;
	// add r8,r8,r5
	ctx.r[8].u64 = ctx.r[8].u64 + ctx.r[5].u64;
	// subf r6,r10,r9
	ctx.r[6].s64 = ctx.r[9].s64 - ctx.r[10].s64;
	// addi r11,r1,80
	ctx.r[11].s64 = ctx.r[1].s64 + 80;
	// li r10,127
	ctx.r[10].s64 = 127;
	// stw r8,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[8].u32);
loc_82BD33E8:
	// lwzx r8,r6,r11
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[6].u32 + ctx.r[11].u32);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// lwz r7,0(r11)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// rlwinm r8,r8,2,0,29
	ctx.r[8].u64 = std::rotl(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 2) & 0xFFFFFFFC;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// add r8,r8,r7
	ctx.r[8].u64 = ctx.r[8].u64 + ctx.r[7].u64;
	// stw r8,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[8].u32);
	// addi r11,r11,4
	ctx.r[11].s64 = ctx.r[11].s64 + 4;
	// bne cr6,0x82bd33e8
	if (!ctx.cr6.eq) goto loc_82BD33E8;
	// stw r5,592(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 592, ctx.r[5].u32);
	// addi r11,r1,596
	ctx.r[11].s64 = ctx.r[1].s64 + 596;
	// addi r9,r9,512
	ctx.r[9].s64 = ctx.r[9].s64 + 512;
	// li r10,127
	ctx.r[10].s64 = 127;
loc_82BD341C:
	// lwz r7,0(r9)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[9].u32 + 0);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// lwz r8,-4(r11)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -4);
	// addi r9,r9,4
	ctx.r[9].s64 = ctx.r[9].s64 + 4;
	// rlwinm r7,r7,2,0,29
	ctx.r[7].u64 = std::rotl(ctx.r[7].u32 | (ctx.r[7].u64 << 32), 2) & 0xFFFFFFFC;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// add r8,r8,r7
	ctx.r[8].u64 = ctx.r[8].u64 + ctx.r[7].u64;
	// stw r8,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[8].u32);
	// addi r11,r11,4
	ctx.r[11].s64 = ctx.r[11].s64 + 4;
	// bne cr6,0x82bd341c
	if (!ctx.cr6.eq) goto loc_82BD341C;
	// b 0x82bd34d0
	goto loc_82BD34D0;
loc_82BD3448:
	// addi r8,r1,80
	ctx.r[8].s64 = ctx.r[1].s64 + 80;
	// addi r10,r9,12
	ctx.r[10].s64 = ctx.r[9].s64 + 12;
	// subf r31,r8,r9
	ctx.r[31].s64 = ctx.r[9].s64 - ctx.r[8].s64;
	// lwz r8,8(r28)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 8);
	// addi r11,r1,80
	ctx.r[11].s64 = ctx.r[1].s64 + 80;
	// li r9,51
	ctx.r[9].s64 = 51;
	// stw r8,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[8].u32);
loc_82BD3464:
	// lwzx r7,r31,r11
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[31].u32 + ctx.r[11].u32);
	// addi r8,r11,20
	ctx.r[8].s64 = ctx.r[11].s64 + 20;
	// lwz r6,0(r11)
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// addi r9,r9,-1
	ctx.r[9].s64 = ctx.r[9].s64 + -1;
	// rlwinm r7,r7,2,0,29
	ctx.r[7].u64 = std::rotl(ctx.r[7].u32 | (ctx.r[7].u64 << 32), 2) & 0xFFFFFFFC;
	// lwz r5,-8(r10)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[10].u32 + -8);
	// lwz r4,-4(r10)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[10].u32 + -4);
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// add r7,r7,r6
	ctx.r[7].u64 = ctx.r[7].u64 + ctx.r[6].u64;
	// lwz r23,0(r10)
	ctx.r[23].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// rlwinm r6,r5,2,0,29
	ctx.r[6].u64 = std::rotl(ctx.r[5].u32 | (ctx.r[5].u64 << 32), 2) & 0xFFFFFFFC;
	// lwz r5,4(r10)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 4);
	// rlwinm r3,r4,2,0,29
	ctx.r[3].u64 = std::rotl(ctx.r[4].u32 | (ctx.r[4].u64 << 32), 2) & 0xFFFFFFFC;
	// add r6,r6,r7
	ctx.r[6].u64 = ctx.r[6].u64 + ctx.r[7].u64;
	// rlwinm r4,r23,2,0,29
	ctx.r[4].u64 = std::rotl(ctx.r[23].u32 | (ctx.r[23].u64 << 32), 2) & 0xFFFFFFFC;
	// stw r7,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[7].u32);
	// add r7,r3,r6
	ctx.r[7].u64 = ctx.r[3].u64 + ctx.r[6].u64;
	// rlwinm r5,r5,2,0,29
	ctx.r[5].u64 = std::rotl(ctx.r[5].u32 | (ctx.r[5].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r10,r10,20
	ctx.r[10].s64 = ctx.r[10].s64 + 20;
	// stw r6,8(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 8, ctx.r[6].u32);
	// add r6,r4,r7
	ctx.r[6].u64 = ctx.r[4].u64 + ctx.r[7].u64;
	// stw r7,12(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 12, ctx.r[7].u32);
	// add r7,r5,r6
	ctx.r[7].u64 = ctx.r[5].u64 + ctx.r[6].u64;
	// stw r6,16(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 16, ctx.r[6].u32);
	// mr r11,r8
	ctx.r[11].u64 = ctx.r[8].u64;
	// stw r7,0(r8)
	PPC_STORE_U32(ctx.r[8].u32 + 0, ctx.r[7].u32);
	// bne cr6,0x82bd3464
	if (!ctx.cr6.eq) goto loc_82BD3464;
loc_82BD34D0:
	// lwz r10,0(r28)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// subf r11,r30,r27
	ctx.r[11].s64 = ctx.r[27].s64 - ctx.r[30].s64;
	// rlwinm r10,r10,0,0,0
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x80000000;
	// addi r7,r11,3
	ctx.r[7].s64 = ctx.r[11].s64 + 3;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x82bd3548
	if (ctx.cr6.eq) goto loc_82BD3548;
	// li r10,0
	ctx.r[10].s64 = 0;
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82bd3538
	if (ctx.cr6.eq) goto loc_82BD3538;
	// mr r11,r7
	ctx.r[11].u64 = ctx.r[7].u64;
loc_82BD34F8:
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r7,r1,80
	ctx.r[7].s64 = ctx.r[1].s64 + 80;
	// mr r6,r10
	ctx.r[6].u64 = ctx.r[10].u64;
	// rotlwi r9,r9,2
	ctx.r[9].u64 = std::rotl(ctx.r[9].u32, 2);
	// addi r8,r1,80
	ctx.r[8].s64 = ctx.r[1].s64 + 80;
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// cmplw cr6,r10,r26
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[26].u32, ctx.xer);
	// lwzx r9,r9,r7
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[9].u32 + ctx.r[7].u32);
	// stw r6,0(r9)
	PPC_STORE_U32(ctx.r[9].u32 + 0, ctx.r[6].u32);
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,4
	ctx.r[11].s64 = ctx.r[11].s64 + 4;
	// rotlwi r9,r9,2
	ctx.r[9].u64 = std::rotl(ctx.r[9].u32, 2);
	// lwzx r7,r9,r8
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[9].u32 + ctx.r[8].u32);
	// addi r7,r7,4
	ctx.r[7].s64 = ctx.r[7].s64 + 4;
	// stwx r7,r9,r8
	PPC_STORE_U32(ctx.r[9].u32 + ctx.r[8].u32, ctx.r[7].u32);
	// blt cr6,0x82bd34f8
	if (ctx.cr6.lt) goto loc_82BD34F8;
loc_82BD3538:
	// lwz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// clrlwi r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u32 & 0x7FFFFFFF;
	// stw r11,0(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 0, ctx.r[11].u32);
	// b 0x82bd3598
	goto loc_82BD3598;
loc_82BD3548:
	// lwz r11,4(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// add r6,r25,r11
	ctx.r[6].u64 = ctx.r[25].u64 + ctx.r[11].u64;
	// cmplw cr6,r11,r6
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[6].u32, ctx.xer);
	// beq cr6,0x82bd3598
	if (ctx.cr6.eq) goto loc_82BD3598;
loc_82BD3558:
	// lwz r10,0(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// addi r5,r1,80
	ctx.r[5].s64 = ctx.r[1].s64 + 80;
	// addi r9,r1,80
	ctx.r[9].s64 = ctx.r[1].s64 + 80;
	// rlwinm r8,r10,2,0,29
	ctx.r[8].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r11,r11,4
	ctx.r[11].s64 = ctx.r[11].s64 + 4;
	// cmplw cr6,r11,r6
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[6].u32, ctx.xer);
	// lbzx r4,r8,r7
	ctx.r[4].u64 = PPC_LOAD_U8(ctx.r[8].u32 + ctx.r[7].u32);
	// rotlwi r4,r4,2
	ctx.r[4].u64 = std::rotl(ctx.r[4].u32, 2);
	// lwzx r5,r4,r5
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[4].u32 + ctx.r[5].u32);
	// stw r10,0(r5)
	PPC_STORE_U32(ctx.r[5].u32 + 0, ctx.r[10].u32);
	// lbzx r10,r8,r7
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[8].u32 + ctx.r[7].u32);
	// rotlwi r10,r10,2
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 2);
	// lwzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[9].u32);
	// addi r8,r8,4
	ctx.r[8].s64 = ctx.r[8].s64 + 4;
	// stwx r8,r10,r9
	PPC_STORE_U32(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u32);
	// bne cr6,0x82bd3558
	if (!ctx.cr6.eq) goto loc_82BD3558;
loc_82BD3598:
	// lwz r10,8(r28)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 8);
	// lwz r11,4(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 4);
	// stw r10,4(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 4, ctx.r[10].u32);
	// stw r11,8(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 8, ctx.r[11].u32);
loc_82BD35A8:
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// cmplwi cr6,r30,4
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 4, ctx.xer);
	// blt cr6,0x82bd3384
	if (ctx.cr6.lt) goto loc_82BD3384;
loc_82BD35B4:
	// mr r3,r28
	ctx.r[3].u64 = ctx.r[28].u64;
	// addi r1,r1,5280
	ctx.r[1].s64 = ctx.r[1].s64 + 5280;
	// b 0x82b7a724
	__restgprlr_23(ctx, base);
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
 if(entry!=0x82bd2df0u) return false;
 Context ctx{};FromFull(ctx,state);Base base{memory,deps,state};Body_82BD2DF0(ctx,base);ToFull(state,ctx);return true;
}
}
