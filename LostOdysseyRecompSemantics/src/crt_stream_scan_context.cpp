#include "lo_semantics/crt_stream_scan_context.h"

#include "lo_semantics/crt_stream_byte_read_context.h"
#include "lo_semantics/crt_stream_pushback_context.h"
#include "lo_semantics/detail/ppc_integer_context.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_scan_context
{
namespace
{
using detail::ppc_integer_context::Context;
using detail::ppc_integer_context::PpcRegister;
using detail::ppc_integer_context::FromFull;
using detail::ppc_integer_context::ToFull;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using std::int32_t;
using std::uint32_t;
using std::uint64_t;

struct Base
{
    GuestMemory& memory;
    Dependencies dependencies;
    Registers& full;
};

void Body_82DF4AF8(Context&, Base&);
void Body_82B7CFE0(Context&, Base&);
void Body_823588A0(Context&, Base&);
void Body_82B7A680(Context&, Base&);
void Body_82B7CFC0(Context&, Base&);
void Body_822A07A0(Context&, Base&);
void Body_82B85420(Context&, Base&);
void Body_82B81648(Context&, Base&);

void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    switch (entry)
    {
    case 0x82B7CFE0u: Body_82B7CFE0(ctx, base); break;
    case 0x823588A0u: Body_823588A0(ctx, base); break;
    case 0x82B7A680u: Body_82B7A680(ctx, base); break;
    case 0x82B7CFC0u: Body_82B7CFC0(ctx, base); break;
    case 0x822A07A0u: Body_822A07A0(ctx, base); break;
    case 0x82B85420u: Body_82B85420(ctx, base); break;
    case 0x82B81648u: Body_82B81648(ctx, base); break;
    case 0x82B81360u:
        if (!crt_stream_byte_read_context::Apply(entry, base.memory,
                base.dependencies.accepted, base.full))
            throw std::logic_error("missing accepted stream byte read");
        FromFull(ctx, base.full);
        return;
    case 0x82B87CF8u:
        if (!crt_stream_pushback_context::Apply(entry, base.memory,
                {base.dependencies.growth.resize.reallocation}, base.full))
            throw std::logic_error("missing accepted stream pushback");
        FromFull(ctx, base.full);
        return;
    case 0x82DF4A58u:
        if (!crt_stream_buffer_growth_callers::Apply(entry, base.memory,
                base.dependencies.growth, base.full))
            throw std::logic_error("missing accepted buffer growth");
        FromFull(ctx, base.full);
        return;
    case 0x82B7FD78u: case 0x82B7FEC0u: case 0x823ADDC0u:
        if (!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
                base.memory, base.dependencies.accepted, base.full))
            throw std::logic_error("missing accepted CRT error/free lower");
        FromFull(ctx, base.full);
        return;
    default:
        throw std::logic_error("unselected scan direct guest callee");
    }
    ToFull(base.full, ctx);
}

void CallIndirect(GuestAddress target, Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    base.dependencies.guest.CallIndirect(target, base.memory, base.full);
    FromFull(ctx, base.full);
}

void __savegprlr_14(Context& ctx, Base& base)
{
    detail::ppc_integer_context::Save(14u, ctx, base);
}
void __restgprlr_14(Context& ctx, Base& base)
{
    detail::ppc_integer_context::Restore(14u, ctx, base);
}
void sub_82B7CFE0(Context& ctx, Base& base) { Direct(0x82B7CFE0u, ctx, base); }
void sub_823588A0(Context& ctx, Base& base) { Direct(0x823588A0u, ctx, base); }
void sub_82B7A680(Context& ctx, Base& base) { Direct(0x82B7A680u, ctx, base); }
void sub_82B7CFC0(Context& ctx, Base& base) { Direct(0x82B7CFC0u, ctx, base); }
void sub_822A07A0(Context& ctx, Base& base) { Direct(0x822A07A0u, ctx, base); }
void sub_82B85420(Context& ctx, Base& base) { Direct(0x82B85420u, ctx, base); }
void sub_82B81648(Context& ctx, Base& base) { Direct(0x82B81648u, ctx, base); }
void sub_82B7FD78(Context& ctx, Base& base) { Direct(0x82B7FD78u, ctx, base); }
void sub_82B7FEC0(Context& ctx, Base& base) { Direct(0x82B7FEC0u, ctx, base); }
void sub_82B81360(Context& ctx, Base& base) { Direct(0x82B81360u, ctx, base); }
void sub_82B87CF8(Context& ctx, Base& base) { Direct(0x82B87CF8u, ctx, base); }
void sub_82DF4A58(Context& ctx, Base& base) { Direct(0x82DF4A58u, ctx, base); }
void sub_823ADDC0(Context& ctx, Base& base) { Direct(0x823ADDC0u, ctx, base); }

#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) base.memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory, Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
#define PPC_CALL_INDIRECT_FUNC(t) CallIndirect((t),ctx,base)

void Body_82DF4AF8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6b0
	ctx.lr = 0x82DF4B00;
	__savegprlr_14(ctx, base);
	// stwu r1,-688(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-688);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// addi r11,r1,176
	ctx.r[11].s64 = ctx.r[1].s64 + 176;
	// stw r6,732(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 732, ctx.r[6].u32);
	// mr r28,r4
	ctx.r[28].u64 = ctx.r[4].u64;
	// li r23,0
	ctx.r[23].s64 = 0;
	// mr r26,r3
	ctx.r[26].u64 = ctx.r[3].u64;
	// cmplwi cr6,r28,0
	ctx.cr6.compare<uint32_t>(ctx.r[28].u32, 0, ctx.xer);
	// stw r11,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[11].u32);
	// li r11,350
	ctx.r[11].s64 = 350;
	// stw r28,716(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 716, ctx.r[28].u32);
	// stw r23,100(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 100, ctx.r[23].u32);
	// sth r23,96(r1)
	PPC_STORE_U16(ctx.r[1].u32 + 96, ctx.r[23].u16);
	// stw r11,104(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 104, ctx.r[11].u32);
	// bne cr6,0x82df4b68
	if (!ctx.cr6.eq) goto loc_82DF4B68;
loc_82DF4B38:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF4B3C;
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
	ctx.lr = 0x82DF4B60;
	sub_82B7FEC0(ctx, base);
loc_82DF4B60:
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82df5d0c
	goto loc_82DF5D0C;
loc_82DF4B68:
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82df4b38
	if (ctx.cr6.eq) goto loc_82DF4B38;
	// lwz r11,12(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df4c3c
	if (!ctx.cr0.eq) goto loc_82DF4C3C;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF4B84;
	sub_82B81648(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// addi r30,r11,-29312
	ctx.r[30].s64 = ctx.r[11].s64 + -29312;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r29,r11,21272
	ctx.r[29].s64 = ctx.r[11].s64 + 21272;
	// beq cr6,0x82df4bd4
	if (ctx.cr6.eq) goto loc_82DF4BD4;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF4BA4;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82df4bd4
	if (ctx.cr6.eq) goto loc_82DF4BD4;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF4BB4;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = static_cast<std::uint8_t>(ctx.r[3].s32 < 0) & static_cast<std::uint8_t>((ctx.r[3].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// rlwinm r31,r11,2,0,29
	ctx.r[31].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82DF4BC4;
	sub_82B81648(ctx, base);
	// lwzx r10,r31,r30
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + ctx.r[30].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82df4bd8
	goto loc_82DF4BD8;
loc_82DF4BD4:
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
loc_82DF4BD8:
	// lbz r11,40(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 40);
	// rlwinm. r11,r11,0,0,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df4b38
	if (!ctx.cr0.eq) goto loc_82DF4B38;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF4BEC;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df4c2c
	if (ctx.cr6.eq) goto loc_82DF4C2C;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF4BFC;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82df4c2c
	if (ctx.cr6.eq) goto loc_82DF4C2C;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF4C0C;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = static_cast<std::uint8_t>(ctx.r[3].s32 < 0) & static_cast<std::uint8_t>((ctx.r[3].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// rlwinm r31,r11,2,0,29
	ctx.r[31].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82DF4C1C;
	sub_82B81648(ctx, base);
	// lwzx r10,r31,r30
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + ctx.r[30].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82df4c30
	goto loc_82DF4C30;
loc_82DF4C2C:
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
loc_82DF4C30:
	// lbz r11,40(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 40);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df4b38
	if (!ctx.cr0.eq) goto loc_82DF4B38;
loc_82DF4C3C:
	// lbz r3,0(r28)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[28].u32 + 0);
	// mr r14,r23
	ctx.r[14].u64 = ctx.r[23].u64;
	// stb r23,80(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 80, ctx.r[23].u8);
	// stw r23,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[23].u32);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq 0x82df5d08
	if (ctx.cr0.eq) goto loc_82DF5D08;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// lwz r19,716(r1)
	ctx.r[19].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 716);
	// addi r11,r11,21248
	ctx.r[11].s64 = ctx.r[11].s64 + 21248;
	// stw r11,124(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 124, ctx.r[11].u32);
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r11,r11,20424
	ctx.r[11].s64 = ctx.r[11].s64 + 20424;
	// stw r11,128(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 128, ctx.r[11].u32);
loc_82DF4C70:
	// bl 0x82b7cfe0
	ctx.lr = 0x82DF4C74;
	sub_82B7CFE0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df4cf0
	if (ctx.cr0.eq) goto loc_82DF4CF0;
	// addi r14,r14,-1
	ctx.r[14].s64 = ctx.r[14].s64 + -1;
loc_82DF4C80:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df4ca8
	if (ctx.cr0.lt) goto loc_82DF4CA8;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r31,0(r11)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df4cb4
	goto loc_82DF4CB4;
loc_82DF4CA8:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF4CB0;
	sub_82B81360(ctx, base);
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
loc_82DF4CB4:
	// cmpwi cr6,r31,-1
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, -1, ctx.xer);
	// beq cr6,0x82df4cd8
	if (ctx.cr6.eq) goto loc_82DF4CD8;
	// clrlwi r3,r31,24
	ctx.r[3].u64 = ctx.r[31].u32 & 0xFF;
	// bl 0x82b7cfe0
	ctx.lr = 0x82DF4CC4;
	sub_82B7CFE0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df4c80
	if (!ctx.cr0.eq) goto loc_82DF4C80;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF4CD8;
	sub_82B87CF8(ctx, base);
loc_82DF4CD8:
	// addi r19,r19,1
	ctx.r[19].s64 = ctx.r[19].s64 + 1;
	// lbz r3,0(r19)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// bl 0x82b7cfe0
	ctx.lr = 0x82DF4CE4;
	sub_82B7CFE0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df4cd8
	if (!ctx.cr0.eq) goto loc_82DF4CD8;
	// b 0x82df5c9c
	goto loc_82DF5C9C;
loc_82DF4CF0:
	// lbz r11,0(r19)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// cmplwi cr6,r11,37
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 37, ctx.xer);
	// bne cr6,0x82df5bd4
	if (!ctx.cr6.eq) goto loc_82DF5BD4;
	// li r22,0
	ctx.r[22].s64 = 0;
	// li r27,0
	ctx.r[27].s64 = 0;
	// li r18,0
	ctx.r[18].s64 = 0;
	// li r17,0
	ctx.r[17].s64 = 0;
	// li r15,0
	ctx.r[15].s64 = 0;
	// li r25,0
	ctx.r[25].s64 = 0;
	// li r20,0
	ctx.r[20].s64 = 0;
	// li r29,0
	ctx.r[29].s64 = 0;
	// li r30,0
	ctx.r[30].s64 = 0;
	// li r24,0
	ctx.r[24].s64 = 0;
	// li r28,0
	ctx.r[28].s64 = 0;
	// li r16,1
	ctx.r[16].s64 = 1;
	// li r21,0
	ctx.r[21].s64 = 0;
loc_82DF4D30:
	// addi r19,r19,1
	ctx.r[19].s64 = ctx.r[19].s64 + 1;
	// lbz r31,0(r19)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x823588a0
	ctx.lr = 0x82DF4D40;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df4d5c
	if (ctx.cr0.eq) goto loc_82DF4D5C;
	// mulli r11,r15,10
	ctx.r[11].s64 = ctx.r[15].s64 * 10;
	// add r11,r11,r31
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[31].u64;
	// addi r17,r17,1
	ctx.r[17].s64 = ctx.r[17].s64 + 1;
	// addi r15,r11,-48
	ctx.r[15].s64 = ctx.r[11].s64 + -48;
	// b 0x82df4e70
	goto loc_82DF4E70;
loc_82DF4D5C:
	// cmpwi cr6,r31,78
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 78, ctx.xer);
	// bgt cr6,0x82df4e1c
	if (ctx.cr6.gt) goto loc_82DF4E1C;
	// beq cr6,0x82df4e70
	if (ctx.cr6.eq) goto loc_82DF4E70;
	// cmpwi cr6,r31,42
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 42, ctx.xer);
	// beq cr6,0x82df4e10
	if (ctx.cr6.eq) goto loc_82DF4E10;
	// cmpwi cr6,r31,70
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 70, ctx.xer);
	// beq cr6,0x82df4e70
	if (ctx.cr6.eq) goto loc_82DF4E70;
	// cmpwi cr6,r31,73
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 73, ctx.xer);
	// beq cr6,0x82df4d94
	if (ctx.cr6.eq) goto loc_82DF4D94;
	// cmpwi cr6,r31,76
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 76, ctx.xer);
	// bne cr6,0x82df4e34
	if (!ctx.cr6.eq) goto loc_82DF4E34;
	// addi r11,r16,1
	ctx.r[11].s64 = ctx.r[16].s64 + 1;
	// extsb r16,r11
	ctx.r[16].s64 = ctx.r[11].s8;
	// b 0x82df4e70
	goto loc_82DF4E70;
loc_82DF4D94:
	// lbz r10,1(r19)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 1);
	// cmplwi cr6,r10,54
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 54, ctx.xer);
	// bne cr6,0x82df4dc4
	if (!ctx.cr6.eq) goto loc_82DF4DC4;
	// lbz r9,2(r19)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 2);
	// addi r11,r19,2
	ctx.r[11].s64 = ctx.r[19].s64 + 2;
	// cmplwi cr6,r9,52
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 52, ctx.xer);
	// bne cr6,0x82df4dc4
	if (!ctx.cr6.eq) goto loc_82DF4DC4;
loc_82DF4DB0:
	// mr r19,r11
	ctx.r[19].u64 = ctx.r[11].u64;
	// li r11,0
	ctx.r[11].s64 = 0;
	// addi r21,r21,1
	ctx.r[21].s64 = ctx.r[21].s64 + 1;
	// std r11,112(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 112, ctx.r[11].u64);
	// b 0x82df4e70
	goto loc_82DF4E70;
loc_82DF4DC4:
	// cmplwi cr6,r10,51
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 51, ctx.xer);
	// bne cr6,0x82df4de4
	if (!ctx.cr6.eq) goto loc_82DF4DE4;
	// lbz r9,2(r19)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 2);
	// addi r11,r19,2
	ctx.r[11].s64 = ctx.r[19].s64 + 2;
	// cmplwi cr6,r9,50
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 50, ctx.xer);
	// bne cr6,0x82df4de4
	if (!ctx.cr6.eq) goto loc_82DF4DE4;
	// mr r19,r11
	ctx.r[19].u64 = ctx.r[11].u64;
	// b 0x82df4e70
	goto loc_82DF4E70;
loc_82DF4DE4:
	// cmplwi cr6,r10,100
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 100, ctx.xer);
	// beq cr6,0x82df4e70
	if (ctx.cr6.eq) goto loc_82DF4E70;
	// cmplwi cr6,r10,105
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 105, ctx.xer);
	// beq cr6,0x82df4e70
	if (ctx.cr6.eq) goto loc_82DF4E70;
	// cmplwi cr6,r10,111
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 111, ctx.xer);
	// beq cr6,0x82df4e70
	if (ctx.cr6.eq) goto loc_82DF4E70;
	// cmplwi cr6,r10,120
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 120, ctx.xer);
	// beq cr6,0x82df4e70
	if (ctx.cr6.eq) goto loc_82DF4E70;
	// cmplwi cr6,r10,88
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 88, ctx.xer);
	// bne cr6,0x82df4e34
	if (!ctx.cr6.eq) goto loc_82DF4E34;
	// b 0x82df4e70
	goto loc_82DF4E70;
loc_82DF4E10:
	// addi r11,r29,1
	ctx.r[11].s64 = ctx.r[29].s64 + 1;
	// extsb r29,r11
	ctx.r[29].s64 = ctx.r[11].s8;
	// b 0x82df4e70
	goto loc_82DF4E70;
loc_82DF4E1C:
	// cmpwi cr6,r31,104
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 104, ctx.xer);
	// beq cr6,0x82df4e60
	if (ctx.cr6.eq) goto loc_82DF4E60;
	// cmpwi cr6,r31,108
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 108, ctx.xer);
	// beq cr6,0x82df4e40
	if (ctx.cr6.eq) goto loc_82DF4E40;
	// cmpwi cr6,r31,119
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, 119, ctx.xer);
	// beq cr6,0x82df4e58
	if (ctx.cr6.eq) goto loc_82DF4E58;
loc_82DF4E34:
	// addi r11,r30,1
	ctx.r[11].s64 = ctx.r[30].s64 + 1;
	// extsb r30,r11
	ctx.r[30].s64 = ctx.r[11].s8;
	// b 0x82df4e70
	goto loc_82DF4E70;
loc_82DF4E40:
	// lbz r10,1(r19)
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 1);
	// addi r11,r19,1
	ctx.r[11].s64 = ctx.r[19].s64 + 1;
	// cmplwi cr6,r10,108
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 108, ctx.xer);
	// beq cr6,0x82df4db0
	if (ctx.cr6.eq) goto loc_82DF4DB0;
	// addi r11,r16,1
	ctx.r[11].s64 = ctx.r[16].s64 + 1;
	// extsb r16,r11
	ctx.r[16].s64 = ctx.r[11].s8;
loc_82DF4E58:
	// addi r11,r28,1
	ctx.r[11].s64 = ctx.r[28].s64 + 1;
	// b 0x82df4e6c
	goto loc_82DF4E6C;
loc_82DF4E60:
	// addi r10,r16,-1
	ctx.r[10].s64 = ctx.r[16].s64 + -1;
	// addi r11,r28,-1
	ctx.r[11].s64 = ctx.r[28].s64 + -1;
	// extsb r16,r10
	ctx.r[16].s64 = ctx.r[10].s8;
loc_82DF4E6C:
	// extsb r28,r11
	ctx.r[28].s64 = ctx.r[11].s8;
loc_82DF4E70:
	// extsb. r11,r30
	ctx.r[11].s64 = ctx.r[30].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df4d30
	if (ctx.cr0.eq) goto loc_82DF4D30;
	// extsb. r11,r29
	ctx.r[11].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r19,716(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 716, ctx.r[19].u32);
	// stw r11,108(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 108, ctx.r[11].u32);
	// bne 0x82df4ea8
	if (!ctx.cr0.eq) goto loc_82DF4EA8;
	// lwz r11,732(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 732);
	// addi r10,r11,7
	ctx.r[10].s64 = ctx.r[11].s64 + 7;
	// stw r11,132(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 132, ctx.r[11].u32);
	// rlwinm r11,r10,0,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFF8;
	// addi r11,r11,8
	ctx.r[11].s64 = ctx.r[11].s64 + 8;
	// stw r11,732(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 732, ctx.r[11].u32);
	// lwz r11,-4(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -4);
	// b 0x82df4eac
	goto loc_82DF4EAC;
loc_82DF4EA8:
	// li r11,0
	ctx.r[11].s64 = 0;
loc_82DF4EAC:
	// stw r11,88(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 88, ctx.r[11].u32);
	// li r29,0
	ctx.r[29].s64 = 0;
	// extsb. r11,r28
	ctx.r[11].s64 = ctx.r[28].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df4ed8
	if (!ctx.cr0.eq) goto loc_82DF4ED8;
	// lbz r11,0(r19)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// cmplwi cr6,r11,83
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 83, ctx.xer);
	// beq cr6,0x82df4ed4
	if (ctx.cr6.eq) goto loc_82DF4ED4;
	// cmplwi cr6,r11,67
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 67, ctx.xer);
	// li r28,-1
	ctx.r[28].s64 = -1;
	// bne cr6,0x82df4ed8
	if (!ctx.cr6.eq) goto loc_82DF4ED8;
loc_82DF4ED4:
	// li r28,1
	ctx.r[28].s64 = 1;
loc_82DF4ED8:
	// lwz r10,716(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 716);
	// lbz r11,0(r10)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[10].u32 + 0);
	// ori r19,r11,32
	ctx.r[19].u64 = ctx.r[11].u64 | 32;
	// cmpwi cr6,r19,110
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 110, ctx.xer);
	// beq cr6,0x82df4f90
	if (ctx.cr6.eq) goto loc_82DF4F90;
	// cmpwi cr6,r19,99
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 99, ctx.xer);
	// beq cr6,0x82df4f50
	if (ctx.cr6.eq) goto loc_82DF4F50;
	// cmpwi cr6,r19,123
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 123, ctx.xer);
	// beq cr6,0x82df4f50
	if (ctx.cr6.eq) goto loc_82DF4F50;
loc_82DF4EFC:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df4f24
	if (ctx.cr0.lt) goto loc_82DF4F24;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r31,0(r11)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df4f30
	goto loc_82DF4F30;
loc_82DF4F24:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF4F2C;
	sub_82B81360(ctx, base);
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
loc_82DF4F30:
	// cmpwi cr6,r31,-1
	ctx.cr6.compare<int32_t>(ctx.r[31].s32, -1, ctx.xer);
	// beq cr6,0x82df4f48
	if (ctx.cr6.eq) goto loc_82DF4F48;
	// clrlwi r3,r31,24
	ctx.r[3].u64 = ctx.r[31].u32 & 0xFF;
	// bl 0x82b7cfe0
	ctx.lr = 0x82DF4F40;
	sub_82B7CFE0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df4efc
	if (!ctx.cr0.eq) goto loc_82DF4EFC;
loc_82DF4F48:
	// mr r23,r31
	ctx.r[23].u64 = ctx.r[31].u64;
	// b 0x82df4f84
	goto loc_82DF4F84;
loc_82DF4F50:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df4f78
	if (ctx.cr0.lt) goto loc_82DF4F78;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df4f84
	goto loc_82DF4F84;
loc_82DF4F78:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF4F80;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF4F84:
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df5cd0
	if (ctx.cr6.eq) goto loc_82DF5CD0;
	// lwz r10,716(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 716);
loc_82DF4F90:
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// beq cr6,0x82df4fa0
	if (ctx.cr6.eq) goto loc_82DF4FA0;
	// cmpwi cr6,r15,0
	ctx.cr6.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// beq cr6,0x82df5cbc
	if (ctx.cr6.eq) goto loc_82DF5CBC;
loc_82DF4FA0:
	// addi r11,r19,-99
	ctx.r[11].s64 = ctx.r[19].s64 + -99;
	// cmplwi cr6,r11,24
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 24, ctx.xer);
	// bgt cr6,0x82df5b88
	if (ctx.cr6.gt) goto loc_82DF5B88;
	// lis r12,-32235
	ctx.r[12].s64 = -2112552960;
	// addi r12,r12,32088
	ctx.r[12].s64 = ctx.r[12].s64 + 32088;
	// rlwinm r0,r11,1,0,30
	ctx.r[0].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 1) & 0xFFFFFFFE;
	// lhzx r0,r12,r0
	ctx.r[0].u64 = PPC_LOAD_U16(ctx.r[12].u32 + ctx.r[0].u32);
	// lis r12,-32033
	ctx.r[12].s64 = -2099314688;
	// addi r12,r12,20436
	ctx.r[12].s64 = ctx.r[12].s64 + 20436;
	// add r12,r12,r0
	ctx.r[12].u64 = ctx.r[12].u64 + ctx.r[0].u64;
	// mtctr r12
	ctx.ctr.u64 = ctx.r[12].u64;
	// nop
	// bctr
	switch (ctx.r[11].u32) {
	case 0:
		goto loc_82DF4FD4;
	case 1:
		goto loc_82DF5470;
	case 2:
		goto loc_82DF57B0;
	case 3:
		goto loc_82DF57B0;
	case 4:
		goto loc_82DF57B0;
	case 5:
		goto loc_82DF5B88;
	case 6:
		goto loc_82DF5310;
	case 7:
		goto loc_82DF5B88;
	case 8:
		goto loc_82DF5B88;
	case 9:
		goto loc_82DF5B88;
	case 10:
		goto loc_82DF5B88;
	case 11:
		goto loc_82DF575C;
	case 12:
		goto loc_82DF5470;
	case 13:
		goto loc_82DF546C;
	case 14:
		goto loc_82DF5B88;
	case 15:
		goto loc_82DF5B88;
	case 16:
		goto loc_82DF4FE4;
	case 17:
		goto loc_82DF5B88;
	case 18:
		goto loc_82DF5470;
	case 19:
		goto loc_82DF5B88;
	case 20:
		goto loc_82DF5B88;
	case 21:
		goto loc_82DF5314;
	case 22:
		goto loc_82DF5B88;
	case 23:
		goto loc_82DF5B88;
	case 24:
		goto loc_82DF4FF4;
	default:
		__builtin_unreachable();
	}
loc_82DF4FD4:
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// bne cr6,0x82df4fe4
	if (!ctx.cr6.eq) goto loc_82DF4FE4;
	// li r17,1
	ctx.r[17].s64 = 1;
	// addi r15,r15,1
	ctx.r[15].s64 = ctx.r[15].s64 + 1;
loc_82DF4FE4:
	// extsb. r11,r28
	ctx.r[11].s64 = ctx.r[28].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// ble 0x82df5134
	if (!ctx.cr0.gt) goto loc_82DF5134;
	// li r24,1
	ctx.r[24].s64 = 1;
	// b 0x82df5134
	goto loc_82DF5134;
loc_82DF4FF4:
	// extsb. r11,r28
	ctx.r[11].s64 = ctx.r[28].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// ble 0x82df5000
	if (!ctx.cr0.gt) goto loc_82DF5000;
	// li r24,1
	ctx.r[24].s64 = 1;
loc_82DF5000:
	// addi r6,r10,1
	ctx.r[6].s64 = ctx.r[10].s64 + 1;
	// lbz r11,0(r6)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[6].u32 + 0);
	// stw r6,716(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 716, ctx.r[6].u32);
	// cmplwi cr6,r11,94
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 94, ctx.xer);
	// bne cr6,0x82df501c
	if (!ctx.cr6.eq) goto loc_82DF501C;
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// li r25,-1
	ctx.r[25].s64 = -1;
loc_82DF501C:
	// addi r11,r1,144
	ctx.r[11].s64 = ctx.r[1].s64 + 144;
	// li r5,0
	ctx.r[5].s64 = 0;
	// cmpwi cr6,r19,123
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 123, ctx.xer);
	// std r5,0(r11)
	PPC_STORE_U64(ctx.r[11].u32 + 0, ctx.r[5].u64);
	// std r5,8(r11)
	PPC_STORE_U64(ctx.r[11].u32 + 8, ctx.r[5].u64);
	// std r5,16(r11)
	PPC_STORE_U64(ctx.r[11].u32 + 16, ctx.r[5].u64);
	// std r5,24(r11)
	PPC_STORE_U64(ctx.r[11].u32 + 24, ctx.r[5].u64);
	// bne cr6,0x82df5058
	if (!ctx.cr6.eq) goto loc_82DF5058;
	// lbz r11,0(r6)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[6].u32 + 0);
	// cmplwi cr6,r11,93
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 93, ctx.xer);
	// bne cr6,0x82df5058
	if (!ctx.cr6.eq) goto loc_82DF5058;
	// li r11,32
	ctx.r[11].s64 = 32;
	// li r27,93
	ctx.r[27].s64 = 93;
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// stb r11,155(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 155, ctx.r[11].u8);
loc_82DF5058:
	// lbz r11,0(r6)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[6].u32 + 0);
	// cmplwi cr6,r11,93
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 93, ctx.xer);
	// beq cr6,0x82df511c
	if (ctx.cr6.eq) goto loc_82DF511C;
	// li r4,1
	ctx.r[4].s64 = 1;
loc_82DF5068:
	// mr r8,r11
	ctx.r[8].u64 = ctx.r[11].u64;
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// clrlwi r9,r8,24
	ctx.r[9].u64 = ctx.r[8].u32 & 0xFF;
	// cmplwi cr6,r9,45
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 45, ctx.xer);
	// bne cr6,0x82df50f0
	if (!ctx.cr6.eq) goto loc_82DF50F0;
	// clrlwi. r10,r27,24
	ctx.r[10].u64 = ctx.r[27].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82df50f0
	if (ctx.cr0.eq) goto loc_82DF50F0;
	// lbz r11,0(r6)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[6].u32 + 0);
	// cmplwi cr6,r11,93
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 93, ctx.xer);
	// beq cr6,0x82df50f0
	if (ctx.cr6.eq) goto loc_82DF50F0;
	// clrlwi r9,r11,24
	ctx.r[9].u64 = ctx.r[11].u32 & 0xFF;
	// addi r6,r6,1
	ctx.r[6].s64 = ctx.r[6].s64 + 1;
	// cmplw cr6,r10,r9
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[9].u32, ctx.xer);
	// bge cr6,0x82df50a8
	if (!ctx.cr6.lt) goto loc_82DF50A8;
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
	// b 0x82df50b0
	goto loc_82DF50B0;
loc_82DF50A8:
	// mr r10,r27
	ctx.r[10].u64 = ctx.r[27].u64;
	// mr r27,r11
	ctx.r[27].u64 = ctx.r[11].u64;
loc_82DF50B0:
	// clrlwi r7,r10,24
	ctx.r[7].u64 = ctx.r[10].u32 & 0xFF;
	// clrlwi r11,r27,24
	ctx.r[11].u64 = ctx.r[27].u32 & 0xFF;
	// b 0x82df50e0
	goto loc_82DF50E0;
loc_82DF50BC:
	// rlwinm r10,r11,29,3,31
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 29) & 0x1FFFFFFF;
	// addi r8,r11,1
	ctx.r[8].s64 = ctx.r[11].s64 + 1;
	// clrlwi r11,r11,29
	ctx.r[11].u64 = ctx.r[11].u32 & 0x7;
	// addi r9,r1,144
	ctx.r[9].s64 = ctx.r[1].s64 + 144;
	// slw r3,r4,r11
	ctx.r[3].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[4].u32 << (ctx.r[11].u8 & 0x3F));
	// clrlwi r11,r8,24
	ctx.r[11].u64 = ctx.r[8].u32 & 0xFF;
	// lbzx r8,r10,r9
	ctx.r[8].u64 = PPC_LOAD_U8(ctx.r[10].u32 + ctx.r[9].u32);
	// or r8,r3,r8
	ctx.r[8].u64 = ctx.r[3].u64 | ctx.r[8].u64;
	// stbx r8,r10,r9
	PPC_STORE_U8(ctx.r[10].u32 + ctx.r[9].u32, ctx.r[8].u8);
loc_82DF50E0:
	// cmplw cr6,r11,r7
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[7].u32, ctx.xer);
	// ble cr6,0x82df50bc
	if (!ctx.cr6.gt) goto loc_82DF50BC;
	// mr r27,r5
	ctx.r[27].u64 = ctx.r[5].u64;
	// b 0x82df5110
	goto loc_82DF5110;
loc_82DF50F0:
	// rlwinm r11,r9,29,3,31
	ctx.r[11].u64 = std::rotl(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 29) & 0x1FFFFFFF;
	// clrlwi r9,r9,29
	ctx.r[9].u64 = ctx.r[9].u32 & 0x7;
	// addi r10,r1,144
	ctx.r[10].s64 = ctx.r[1].s64 + 144;
	// mr r27,r8
	ctx.r[27].u64 = ctx.r[8].u64;
	// lbzx r8,r11,r10
	ctx.r[8].u64 = PPC_LOAD_U8(ctx.r[11].u32 + ctx.r[10].u32);
	// slw r9,r4,r9
	ctx.r[9].u64 = ctx.r[9].u8 & 0x20 ? 0 : (ctx.r[4].u32 << (ctx.r[9].u8 & 0x3F));
	// or r9,r9,r8
	ctx.r[9].u64 = ctx.r[9].u64 | ctx.r[8].u64;
	// stbx r9,r11,r10
	PPC_STORE_U8(ctx.r[11].u32 + ctx.r[10].u32, ctx.r[9].u8);
loc_82DF5110:
	// lbz r11,0(r6)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[6].u32 + 0);
	// cmplwi cr6,r11,93
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 93, ctx.xer);
	// bne cr6,0x82df5068
	if (!ctx.cr6.eq) goto loc_82DF5068;
loc_82DF511C:
	// lbz r11,0(r6)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[6].u32 + 0);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq 0x82df5cd0
	if (ctx.cr0.eq) goto loc_82DF5CD0;
	// cmpwi cr6,r19,123
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 123, ctx.xer);
	// bne cr6,0x82df5134
	if (!ctx.cr6.eq) goto loc_82DF5134;
	// stw r6,716(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 716, ctx.r[6].u32);
loc_82DF5134:
	// lwz r30,88(r1)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// addi r14,r14,-1
	ctx.r[14].s64 = ctx.r[14].s64 + -1;
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// mr r31,r30
	ctx.r[31].u64 = ctx.r[30].u64;
	// beq cr6,0x82df5154
	if (ctx.cr6.eq) goto loc_82DF5154;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF5154;
	sub_82B87CF8(ctx, base);
loc_82DF5154:
	// lwz r29,124(r1)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 124);
loc_82DF5158:
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// beq cr6,0x82df516c
	if (ctx.cr6.eq) goto loc_82DF516C;
	// cmpwi cr6,r15,0
	ctx.cr6.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// addi r15,r15,-1
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	// beq cr6,0x82df52c4
	if (ctx.cr6.eq) goto loc_82DF52C4;
loc_82DF516C:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5194
	if (ctx.cr0.lt) goto loc_82DF5194;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df51a0
	goto loc_82DF51A0;
loc_82DF5194:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF519C;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF51A0:
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df52ac
	if (ctx.cr6.eq) goto loc_82DF52AC;
	// cmpwi cr6,r19,99
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 99, ctx.xer);
	// beq cr6,0x82df5204
	if (ctx.cr6.eq) goto loc_82DF5204;
	// cmpwi cr6,r19,115
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 115, ctx.xer);
	// bne cr6,0x82df51d0
	if (!ctx.cr6.eq) goto loc_82DF51D0;
	// cmpwi cr6,r23,9
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 9, ctx.xer);
	// blt cr6,0x82df51c8
	if (ctx.cr6.lt) goto loc_82DF51C8;
	// cmpwi cr6,r23,13
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 13, ctx.xer);
	// ble cr6,0x82df52ac
	if (!ctx.cr6.gt) goto loc_82DF52AC;
loc_82DF51C8:
	// cmpwi cr6,r23,32
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 32, ctx.xer);
	// bne cr6,0x82df5204
	if (!ctx.cr6.eq) goto loc_82DF5204;
loc_82DF51D0:
	// cmpwi cr6,r19,123
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 123, ctx.xer);
	// bne cr6,0x82df52ac
	if (!ctx.cr6.eq) goto loc_82DF52AC;
	// srawi r9,r23,3
	ctx.xer.ca = static_cast<std::uint8_t>(ctx.r[23].s32 < 0) & static_cast<std::uint8_t>((ctx.r[23].u32 & 0x7) != 0);
	ctx.r[9].s64 = ctx.r[23].s32 >> 3;
	// addi r10,r1,144
	ctx.r[10].s64 = ctx.r[1].s64 + 144;
	// clrlwi r11,r23,29
	ctx.r[11].u64 = ctx.r[23].u32 & 0x7;
	// extsb r8,r25
	ctx.r[8].s64 = ctx.r[25].s8;
	// lbzx r10,r9,r10
	ctx.r[10].u64 = PPC_LOAD_U8(ctx.r[9].u32 + ctx.r[10].u32);
	// li r9,1
	ctx.r[9].s64 = 1;
	// extsb r10,r10
	ctx.r[10].s64 = ctx.r[10].s8;
	// xor r10,r10,r8
	ctx.r[10].u64 = ctx.r[10].u64 ^ ctx.r[8].u64;
	// slw r11,r9,r11
	ctx.r[11].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[9].u32 << (ctx.r[11].u8 & 0x3F));
	// and. r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 & ctx.r[10].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df52ac
	if (ctx.cr0.eq) goto loc_82DF52AC;
loc_82DF5204:
	// lwz r11,108(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 108);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82df52a4
	if (!ctx.cr6.eq) goto loc_82DF52A4;
	// extsb. r11,r24
	ctx.r[11].s64 = ctx.r[24].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df5294
	if (ctx.cr0.eq) goto loc_82DF5294;
	// extsb r11,r23
	ctx.r[11].s64 = ctx.r[23].s8;
	// clrlwi r3,r11,24
	ctx.r[3].u64 = ctx.r[11].u32 & 0xFF;
	// stb r11,120(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 120, ctx.r[11].u8);
	// bl 0x82b7a680
	ctx.lr = 0x82DF5228;
	sub_82B7A680(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5264
	if (ctx.cr0.eq) goto loc_82DF5264;
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5258
	if (ctx.cr0.lt) goto loc_82DF5258;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5260
	goto loc_82DF5260;
loc_82DF5258:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5260;
	sub_82B81360(ctx, base);
loc_82DF5260:
	// stb r3,121(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 121, ctx.r[3].u8);
loc_82DF5264:
	// li r11,63
	ctx.r[11].s64 = 63;
	// addi r4,r1,120
	ctx.r[4].s64 = ctx.r[1].s64 + 120;
	// mr r6,r29
	ctx.r[6].u64 = ctx.r[29].u64;
	// addi r3,r1,96
	ctx.r[3].s64 = ctx.r[1].s64 + 96;
	// sth r11,96(r1)
	PPC_STORE_U16(ctx.r[1].u32 + 96, ctx.r[11].u16);
	// lwz r11,0(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 0);
	// lwz r5,172(r11)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 172);
	// bl 0x822a07a0
	ctx.lr = 0x82DF5284;
	sub_822A07A0(ctx, base);
	// lhz r11,96(r1)
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[1].u32 + 96);
	// sth r11,0(r30)
	PPC_STORE_U16(ctx.r[30].u32 + 0, ctx.r[11].u16);
	// addi r30,r30,2
	ctx.r[30].s64 = ctx.r[30].s64 + 2;
	// b 0x82df529c
	goto loc_82DF529C;
loc_82DF5294:
	// stb r23,0(r30)
	PPC_STORE_U8(ctx.r[30].u32 + 0, ctx.r[23].u8);
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
loc_82DF529C:
	// stw r30,88(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 88, ctx.r[30].u32);
	// b 0x82df5158
	goto loc_82DF5158;
loc_82DF52A4:
	// addi r31,r31,1
	ctx.r[31].s64 = ctx.r[31].s64 + 1;
	// b 0x82df5158
	goto loc_82DF5158;
loc_82DF52AC:
	// addi r14,r14,-1
	ctx.r[14].s64 = ctx.r[14].s64 + -1;
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df52c4
	if (ctx.cr6.eq) goto loc_82DF52C4;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF52C4;
	sub_82B87CF8(ctx, base);
loc_82DF52C4:
	// cmplw cr6,r31,r30
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[30].u32, ctx.xer);
	// beq cr6,0x82df5cd0
	if (ctx.cr6.eq) goto loc_82DF5CD0;
	// lwz r11,108(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 108);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82df5bb4
	if (!ctx.cr6.eq) goto loc_82DF5BB4;
	// lwz r11,92(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
	// cmpwi cr6,r19,99
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 99, ctx.xer);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// stw r11,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[11].u32);
	// beq cr6,0x82df5bb4
	if (ctx.cr6.eq) goto loc_82DF5BB4;
	// lwz r10,88(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// extsb. r11,r24
	ctx.r[11].s64 = ctx.r[24].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df5304
	if (ctx.cr0.eq) goto loc_82DF5304;
	// li r11,0
	ctx.r[11].s64 = 0;
	// sth r11,0(r10)
	PPC_STORE_U16(ctx.r[10].u32 + 0, ctx.r[11].u16);
	// b 0x82df5bb4
	goto loc_82DF5BB4;
loc_82DF5304:
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r10)
	PPC_STORE_U8(ctx.r[10].u32 + 0, ctx.r[11].u8);
	// b 0x82df5bb4
	goto loc_82DF5BB4;
loc_82DF5310:
	// li r19,100
	ctx.r[19].s64 = 100;
loc_82DF5314:
	// cmpwi cr6,r23,45
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 45, ctx.xer);
	// bne cr6,0x82df5324
	if (!ctx.cr6.eq) goto loc_82DF5324;
	// li r20,1
	ctx.r[20].s64 = 1;
	// b 0x82df532c
	goto loc_82DF532C;
loc_82DF5324:
	// cmpwi cr6,r23,43
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 43, ctx.xer);
	// bne cr6,0x82df5378
	if (!ctx.cr6.eq) goto loc_82DF5378;
loc_82DF532C:
	// addic. r15,r15,-1
	ctx.xer.ca = ctx.r[15].u32 > 0;
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// bne 0x82df5344
	if (!ctx.cr0.eq) goto loc_82DF5344;
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// beq cr6,0x82df5344
	if (ctx.cr6.eq) goto loc_82DF5344;
	// li r29,1
	ctx.r[29].s64 = 1;
	// b 0x82df5378
	goto loc_82DF5378;
loc_82DF5344:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df536c
	if (ctx.cr0.lt) goto loc_82DF536C;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5378
	goto loc_82DF5378;
loc_82DF536C:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5374;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5378:
	// cmpwi cr6,r23,48
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 48, ctx.xer);
	// bne cr6,0x82df54d4
	if (!ctx.cr6.eq) goto loc_82DF54D4;
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df53a8
	if (ctx.cr0.lt) goto loc_82DF53A8;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df53b4
	goto loc_82DF53B4;
loc_82DF53A8:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF53B0;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF53B4:
	// extsb r11,r23
	ctx.r[11].s64 = ctx.r[23].s8;
	// cmpwi cr6,r11,120
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 120, ctx.xer);
	// beq cr6,0x82df5414
	if (ctx.cr6.eq) goto loc_82DF5414;
	// cmpwi cr6,r11,88
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 88, ctx.xer);
	// beq cr6,0x82df5414
	if (ctx.cr6.eq) goto loc_82DF5414;
	// li r18,1
	ctx.r[18].s64 = 1;
	// cmpwi cr6,r19,120
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 120, ctx.xer);
	// beq cr6,0x82df53f4
	if (ctx.cr6.eq) goto loc_82DF53F4;
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// beq cr6,0x82df53ec
	if (ctx.cr6.eq) goto loc_82DF53EC;
	// addic. r15,r15,-1
	ctx.xer.ca = ctx.r[15].u32 > 0;
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// bne 0x82df53ec
	if (!ctx.cr0.eq) goto loc_82DF53EC;
	// addi r11,r29,1
	ctx.r[11].s64 = ctx.r[29].s64 + 1;
	// extsb r29,r11
	ctx.r[29].s64 = ctx.r[11].s8;
loc_82DF53EC:
	// li r19,111
	ctx.r[19].s64 = 111;
	// b 0x82df54d4
	goto loc_82DF54D4;
loc_82DF53F4:
	// addi r14,r14,-1
	ctx.r[14].s64 = ctx.r[14].s64 + -1;
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df540c
	if (ctx.cr6.eq) goto loc_82DF540C;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF540C;
	sub_82B87CF8(ctx, base);
loc_82DF540C:
	// li r23,48
	ctx.r[23].s64 = 48;
	// b 0x82df54d4
	goto loc_82DF54D4;
loc_82DF5414:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df543c
	if (ctx.cr0.lt) goto loc_82DF543C;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5448
	goto loc_82DF5448;
loc_82DF543C:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5444;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5448:
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// beq cr6,0x82df5464
	if (ctx.cr6.eq) goto loc_82DF5464;
	// addi r15,r15,-2
	ctx.r[15].s64 = ctx.r[15].s64 + -2;
	// cmpwi cr6,r15,1
	ctx.cr6.compare<int32_t>(ctx.r[15].s32, 1, ctx.xer);
	// bge cr6,0x82df5464
	if (!ctx.cr6.lt) goto loc_82DF5464;
	// addi r11,r29,1
	ctx.r[11].s64 = ctx.r[29].s64 + 1;
	// extsb r29,r11
	ctx.r[29].s64 = ctx.r[11].s8;
loc_82DF5464:
	// li r19,120
	ctx.r[19].s64 = 120;
	// b 0x82df54d4
	goto loc_82DF54D4;
loc_82DF546C:
	// li r16,1
	ctx.r[16].s64 = 1;
loc_82DF5470:
	// cmpwi cr6,r23,45
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 45, ctx.xer);
	// bne cr6,0x82df5480
	if (!ctx.cr6.eq) goto loc_82DF5480;
	// li r20,1
	ctx.r[20].s64 = 1;
	// b 0x82df5488
	goto loc_82DF5488;
loc_82DF5480:
	// cmpwi cr6,r23,43
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 43, ctx.xer);
	// bne cr6,0x82df54d4
	if (!ctx.cr6.eq) goto loc_82DF54D4;
loc_82DF5488:
	// addic. r15,r15,-1
	ctx.xer.ca = ctx.r[15].u32 > 0;
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// bne 0x82df54a0
	if (!ctx.cr0.eq) goto loc_82DF54A0;
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// beq cr6,0x82df54a0
	if (ctx.cr6.eq) goto loc_82DF54A0;
	// li r29,1
	ctx.r[29].s64 = 1;
	// b 0x82df54d4
	goto loc_82DF54D4;
loc_82DF54A0:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df54c8
	if (ctx.cr0.lt) goto loc_82DF54C8;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df54d4
	goto loc_82DF54D4;
loc_82DF54C8:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF54D0;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF54D4:
	// cmpwi cr6,r21,0
	ctx.cr6.compare<int32_t>(ctx.r[21].s32, 0, ctx.xer);
	// extsb. r31,r29
	ctx.r[31].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// beq cr6,0x82df561c
	if (ctx.cr6.eq) goto loc_82DF561C;
	// bne 0x82df5604
	if (!ctx.cr0.eq) goto loc_82DF5604;
loc_82DF54E4:
	// cmpwi cr6,r19,120
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 120, ctx.xer);
	// beq cr6,0x82df5530
	if (ctx.cr6.eq) goto loc_82DF5530;
	// cmpwi cr6,r19,112
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 112, ctx.xer);
	// beq cr6,0x82df5530
	if (ctx.cr6.eq) goto loc_82DF5530;
	// clrlwi r3,r23,24
	ctx.r[3].u64 = ctx.r[23].u32 & 0xFF;
	// bl 0x823588a0
	ctx.lr = 0x82DF54FC;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df556c
	if (ctx.cr0.eq) goto loc_82DF556C;
	// cmpwi cr6,r19,111
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 111, ctx.xer);
	// bne cr6,0x82df5524
	if (!ctx.cr6.eq) goto loc_82DF5524;
	// cmpwi cr6,r23,56
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 56, ctx.xer);
	// bge cr6,0x82df556c
	if (!ctx.cr6.lt) goto loc_82DF556C;
	// ld r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 112);
	// rldicr r11,r11,3,60
	ctx.r[11].u64 = std::rotl(ctx.r[11].u64, 3) & 0xFFFFFFFFFFFFFFF8;
loc_82DF551C:
	// std r11,112(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 112, ctx.r[11].u64);
	// b 0x82df5574
	goto loc_82DF5574;
loc_82DF5524:
	// ld r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 112);
	// mulli r11,r11,10
	ctx.r[11].s64 = ctx.r[11].s64 * 10;
	// b 0x82df551c
	goto loc_82DF551C;
loc_82DF5530:
	// clrlwi r30,r23,24
	ctx.r[30].u64 = ctx.r[23].u32 & 0xFF;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7cfc0
	ctx.lr = 0x82DF553C;
	sub_82B7CFC0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df556c
	if (ctx.cr0.eq) goto loc_82DF556C;
	// ld r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 112);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// rldicr r11,r11,4,59
	ctx.r[11].u64 = std::rotl(ctx.r[11].u64, 4) & 0xFFFFFFFFFFFFFFF0;
	// std r11,112(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 112, ctx.r[11].u64);
	// bl 0x823588a0
	ctx.lr = 0x82DF5558;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df5574
	if (!ctx.cr0.eq) goto loc_82DF5574;
	// rlwinm r11,r23,0,27,25
	ctx.r[11].u64 = std::rotl(ctx.r[23].u32 | (ctx.r[23].u64 << 32), 0) & 0xFFFFFFFFFFFFFFDF;
	// addi r23,r11,-7
	ctx.r[23].s64 = ctx.r[11].s64 + -7;
	// b 0x82df5574
	goto loc_82DF5574;
loc_82DF556C:
	// addi r11,r31,1
	ctx.r[11].s64 = ctx.r[31].s64 + 1;
	// extsb r29,r11
	ctx.r[29].s64 = ctx.r[11].s8;
loc_82DF5574:
	// extsb. r11,r29
	ctx.r[11].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df55e4
	if (!ctx.cr0.eq) goto loc_82DF55E4;
	// addi r11,r23,-48
	ctx.r[11].s64 = ctx.r[23].s64 + -48;
	// ld r10,112(r1)
	ctx.r[10].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 112);
	// addi r18,r18,1
	ctx.r[18].s64 = ctx.r[18].s64 + 1;
	// extsw r11,r11
	ctx.r[11].s64 = ctx.r[11].s32;
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// std r11,112(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 112, ctx.r[11].u64);
	// beq cr6,0x82df55ac
	if (ctx.cr6.eq) goto loc_82DF55AC;
	// addic. r15,r15,-1
	ctx.xer.ca = ctx.r[15].u32 > 0;
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// bne 0x82df55ac
	if (!ctx.cr0.eq) goto loc_82DF55AC;
	// li r29,1
	ctx.r[29].s64 = 1;
	// b 0x82df55fc
	goto loc_82DF55FC;
loc_82DF55AC:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df55d4
	if (ctx.cr0.lt) goto loc_82DF55D4;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df55fc
	goto loc_82DF55FC;
loc_82DF55D4:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF55DC;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
	// b 0x82df55fc
	goto loc_82DF55FC;
loc_82DF55E4:
	// addi r14,r14,-1
	ctx.r[14].s64 = ctx.r[14].s64 + -1;
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df55fc
	if (ctx.cr6.eq) goto loc_82DF55FC;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF55FC;
	sub_82B87CF8(ctx, base);
loc_82DF55FC:
	// extsb. r31,r29
	ctx.r[31].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// beq 0x82df54e4
	if (ctx.cr0.eq) goto loc_82DF54E4;
loc_82DF5604:
	// extsb. r11,r20
	ctx.r[11].s64 = ctx.r[20].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df572c
	if (ctx.cr0.eq) goto loc_82DF572C;
	// ld r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 112);
	// neg r11,r11
	ctx.r[11].s64 = -ctx.r[11].s64;
	// std r11,112(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 112, ctx.r[11].u64);
	// b 0x82df572c
	goto loc_82DF572C;
loc_82DF561C:
	// bne 0x82df5720
	if (!ctx.cr0.eq) goto loc_82DF5720;
loc_82DF5620:
	// cmpwi cr6,r19,120
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 120, ctx.xer);
	// beq cr6,0x82df5660
	if (ctx.cr6.eq) goto loc_82DF5660;
	// cmpwi cr6,r19,112
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 112, ctx.xer);
	// beq cr6,0x82df5660
	if (ctx.cr6.eq) goto loc_82DF5660;
	// clrlwi r3,r23,24
	ctx.r[3].u64 = ctx.r[23].u32 & 0xFF;
	// bl 0x823588a0
	ctx.lr = 0x82DF5638;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5694
	if (ctx.cr0.eq) goto loc_82DF5694;
	// cmpwi cr6,r19,111
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 111, ctx.xer);
	// bne cr6,0x82df5658
	if (!ctx.cr6.eq) goto loc_82DF5658;
	// cmpwi cr6,r23,56
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 56, ctx.xer);
	// bge cr6,0x82df5694
	if (!ctx.cr6.lt) goto loc_82DF5694;
	// rlwinm r22,r22,3,0,28
	ctx.r[22].u64 = std::rotl(ctx.r[22].u32 | (ctx.r[22].u64 << 32), 3) & 0xFFFFFFF8;
	// b 0x82df569c
	goto loc_82DF569C;
loc_82DF5658:
	// mulli r22,r22,10
	ctx.r[22].s64 = ctx.r[22].s64 * 10;
	// b 0x82df569c
	goto loc_82DF569C;
loc_82DF5660:
	// clrlwi r30,r23,24
	ctx.r[30].u64 = ctx.r[23].u32 & 0xFF;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7cfc0
	ctx.lr = 0x82DF566C;
	sub_82B7CFC0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5694
	if (ctx.cr0.eq) goto loc_82DF5694;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// rlwinm r22,r22,4,0,27
	ctx.r[22].u64 = std::rotl(ctx.r[22].u32 | (ctx.r[22].u64 << 32), 4) & 0xFFFFFFF0;
	// bl 0x823588a0
	ctx.lr = 0x82DF5680;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df569c
	if (!ctx.cr0.eq) goto loc_82DF569C;
	// rlwinm r11,r23,0,27,25
	ctx.r[11].u64 = std::rotl(ctx.r[23].u32 | (ctx.r[23].u64 << 32), 0) & 0xFFFFFFFFFFFFFFDF;
	// addi r23,r11,-7
	ctx.r[23].s64 = ctx.r[11].s64 + -7;
	// b 0x82df569c
	goto loc_82DF569C;
loc_82DF5694:
	// addi r11,r31,1
	ctx.r[11].s64 = ctx.r[31].s64 + 1;
	// extsb r29,r11
	ctx.r[29].s64 = ctx.r[11].s8;
loc_82DF569C:
	// extsb. r11,r29
	ctx.r[11].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df5700
	if (!ctx.cr0.eq) goto loc_82DF5700;
	// add r11,r22,r23
	ctx.r[11].u64 = ctx.r[22].u64 + ctx.r[23].u64;
	// addi r18,r18,1
	ctx.r[18].s64 = ctx.r[18].s64 + 1;
	// addi r22,r11,-48
	ctx.r[22].s64 = ctx.r[11].s64 + -48;
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// beq cr6,0x82df56c8
	if (ctx.cr6.eq) goto loc_82DF56C8;
	// addic. r15,r15,-1
	ctx.xer.ca = ctx.r[15].u32 > 0;
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// bne 0x82df56c8
	if (!ctx.cr0.eq) goto loc_82DF56C8;
	// li r29,1
	ctx.r[29].s64 = 1;
	// b 0x82df5718
	goto loc_82DF5718;
loc_82DF56C8:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df56f0
	if (ctx.cr0.lt) goto loc_82DF56F0;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5718
	goto loc_82DF5718;
loc_82DF56F0:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF56F8;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
	// b 0x82df5718
	goto loc_82DF5718;
loc_82DF5700:
	// addi r14,r14,-1
	ctx.r[14].s64 = ctx.r[14].s64 + -1;
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df5718
	if (ctx.cr6.eq) goto loc_82DF5718;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF5718;
	sub_82B87CF8(ctx, base);
loc_82DF5718:
	// extsb. r31,r29
	ctx.r[31].s64 = ctx.r[29].s8;
	ctx.cr0.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// beq 0x82df5620
	if (ctx.cr0.eq) goto loc_82DF5620;
loc_82DF5720:
	// extsb. r11,r20
	ctx.r[11].s64 = ctx.r[20].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df572c
	if (ctx.cr0.eq) goto loc_82DF572C;
	// neg r22,r22
	ctx.r[22].s64 = -ctx.r[22].s64;
loc_82DF572C:
	// cmpwi cr6,r19,70
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 70, ctx.xer);
	// bne cr6,0x82df5738
	if (!ctx.cr6.eq) goto loc_82DF5738;
	// li r18,0
	ctx.r[18].s64 = 0;
loc_82DF5738:
	// cmpwi cr6,r18,0
	ctx.cr6.compare<int32_t>(ctx.r[18].s32, 0, ctx.xer);
	// beq cr6,0x82df5cd0
	if (ctx.cr6.eq) goto loc_82DF5CD0;
	// lwz r11,108(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 108);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82df5bb4
	if (!ctx.cr6.eq) goto loc_82DF5BB4;
	// lwz r11,92(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// stw r11,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[11].u32);
	// b 0x82df5778
	goto loc_82DF5778;
loc_82DF575C:
	// lwz r11,108(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 108);
	// mr r22,r14
	ctx.r[22].u64 = ctx.r[14].u64;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82df5bb4
	if (!ctx.cr6.eq) goto loc_82DF5BB4;
	// bl 0x82b85420
	ctx.lr = 0x82DF5770;
	sub_82B85420(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5bb4
	if (ctx.cr0.eq) goto loc_82DF5BB4;
loc_82DF5778:
	// cmpwi cr6,r21,0
	ctx.cr6.compare<int32_t>(ctx.r[21].s32, 0, ctx.xer);
	// beq cr6,0x82df5790
	if (ctx.cr6.eq) goto loc_82DF5790;
	// ld r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 112);
	// lwz r10,88(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// std r11,0(r10)
	PPC_STORE_U64(ctx.r[10].u32 + 0, ctx.r[11].u64);
	// b 0x82df5bb4
	goto loc_82DF5BB4;
loc_82DF5790:
	// extsb. r11,r16
	ctx.r[11].s64 = ctx.r[16].s8;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df57a4
	if (ctx.cr0.eq) goto loc_82DF57A4;
	// lwz r11,88(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
	// b 0x82df5bb4
	goto loc_82DF5BB4;
loc_82DF57A4:
	// lwz r10,88(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// sth r22,0(r10)
	PPC_STORE_U16(ctx.r[10].u32 + 0, ctx.r[22].u16);
	// b 0x82df5bb4
	goto loc_82DF5BB4;
loc_82DF57B0:
	// li r30,0
	ctx.r[30].s64 = 0;
	// cmpwi cr6,r23,45
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 45, ctx.xer);
	// bne cr6,0x82df57d0
	if (!ctx.cr6.eq) goto loc_82DF57D0;
	// lwz r10,84(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// li r11,45
	ctx.r[11].s64 = 45;
	// li r30,1
	ctx.r[30].s64 = 1;
	// stb r11,0(r10)
	PPC_STORE_U8(ctx.r[10].u32 + 0, ctx.r[11].u8);
	// b 0x82df57d8
	goto loc_82DF57D8;
loc_82DF57D0:
	// cmpwi cr6,r23,43
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 43, ctx.xer);
	// bne cr6,0x82df5810
	if (!ctx.cr6.eq) goto loc_82DF5810;
loc_82DF57D8:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r15,r15,-1
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5804
	if (ctx.cr0.lt) goto loc_82DF5804;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5810
	goto loc_82DF5810;
loc_82DF5804:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF580C;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5810:
	// cmpwi cr6,r17,0
	ctx.cr6.compare<int32_t>(ctx.r[17].s32, 0, ctx.xer);
	// bne cr6,0x82df5890
	if (!ctx.cr6.eq) goto loc_82DF5890;
	// li r15,-1
	ctx.r[15].s64 = -1;
	// b 0x82df5890
	goto loc_82DF5890;
loc_82DF5820:
	// cmpwi cr6,r15,0
	ctx.cr6.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// addi r15,r15,-1
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	// beq cr6,0x82df58a4
	if (ctx.cr6.eq) goto loc_82DF58A4;
	// lwz r11,84(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// addi r7,r1,100
	ctx.r[7].s64 = ctx.r[1].s64 + 100;
	// addi r6,r1,176
	ctx.r[6].s64 = ctx.r[1].s64 + 176;
	// addi r5,r1,84
	ctx.r[5].s64 = ctx.r[1].s64 + 84;
	// addi r4,r1,104
	ctx.r[4].s64 = ctx.r[1].s64 + 104;
	// addi r18,r18,1
	ctx.r[18].s64 = ctx.r[18].s64 + 1;
	// stbx r31,r30,r11
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[31].u8);
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df4a58
	ctx.lr = 0x82DF5854;
	sub_82DF4A58(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5cd0
	if (ctx.cr0.eq) goto loc_82DF5CD0;
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5884
	if (ctx.cr0.lt) goto loc_82DF5884;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5890
	goto loc_82DF5890;
loc_82DF5884:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF588C;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5890:
	// clrlwi r31,r23,24
	ctx.r[31].u64 = ctx.r[23].u32 & 0xFF;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x823588a0
	ctx.lr = 0x82DF589C;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df5820
	if (!ctx.cr0.eq) goto loc_82DF5820;
loc_82DF58A4:
	// lwz r11,124(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 124);
	// extsb r10,r23
	ctx.r[10].s64 = ctx.r[23].s8;
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lwz r11,188(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 188);
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// lbz r31,0(r11)
	ctx.r[31].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// extsb r11,r31
	ctx.r[11].s64 = ctx.r[31].s8;
	// cmpw cr6,r11,r10
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[10].s32, ctx.xer);
	// bne cr6,0x82df59bc
	if (!ctx.cr6.eq) goto loc_82DF59BC;
	// cmpwi cr6,r15,0
	ctx.cr6.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// addi r15,r15,-1
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	// beq cr6,0x82df59bc
	if (ctx.cr6.eq) goto loc_82DF59BC;
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df58fc
	if (ctx.cr0.lt) goto loc_82DF58FC;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5908
	goto loc_82DF5908;
loc_82DF58FC:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5904;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5908:
	// lwz r11,84(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// addi r7,r1,100
	ctx.r[7].s64 = ctx.r[1].s64 + 100;
	// addi r6,r1,176
	ctx.r[6].s64 = ctx.r[1].s64 + 176;
	// addi r5,r1,84
	ctx.r[5].s64 = ctx.r[1].s64 + 84;
	// addi r4,r1,104
	ctx.r[4].s64 = ctx.r[1].s64 + 104;
	// stbx r31,r30,r11
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[31].u8);
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df4a58
	ctx.lr = 0x82DF592C;
	sub_82DF4A58(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5cd0
	if (ctx.cr0.eq) goto loc_82DF5CD0;
	// b 0x82df59a8
	goto loc_82DF59A8;
loc_82DF5938:
	// cmpwi cr6,r15,0
	ctx.cr6.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// addi r15,r15,-1
	ctx.r[15].s64 = ctx.r[15].s64 + -1;
	// beq cr6,0x82df59bc
	if (ctx.cr6.eq) goto loc_82DF59BC;
	// lwz r11,84(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// addi r7,r1,100
	ctx.r[7].s64 = ctx.r[1].s64 + 100;
	// addi r6,r1,176
	ctx.r[6].s64 = ctx.r[1].s64 + 176;
	// addi r5,r1,84
	ctx.r[5].s64 = ctx.r[1].s64 + 84;
	// addi r4,r1,104
	ctx.r[4].s64 = ctx.r[1].s64 + 104;
	// addi r18,r18,1
	ctx.r[18].s64 = ctx.r[18].s64 + 1;
	// stbx r31,r30,r11
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[31].u8);
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df4a58
	ctx.lr = 0x82DF596C;
	sub_82DF4A58(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5cd0
	if (ctx.cr0.eq) goto loc_82DF5CD0;
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df599c
	if (ctx.cr0.lt) goto loc_82DF599C;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df59a8
	goto loc_82DF59A8;
loc_82DF599C:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF59A4;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF59A8:
	// clrlwi r31,r23,24
	ctx.r[31].u64 = ctx.r[23].u32 & 0xFF;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x823588a0
	ctx.lr = 0x82DF59B4;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df5938
	if (!ctx.cr0.eq) goto loc_82DF5938;
loc_82DF59BC:
	// cmpwi cr6,r18,0
	ctx.cr6.compare<int32_t>(ctx.r[18].s32, 0, ctx.xer);
	// beq cr6,0x82df5b20
	if (ctx.cr6.eq) goto loc_82DF5B20;
	// cmpwi cr6,r23,101
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 101, ctx.xer);
	// beq cr6,0x82df59d4
	if (ctx.cr6.eq) goto loc_82DF59D4;
	// cmpwi cr6,r23,69
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 69, ctx.xer);
	// bne cr6,0x82df5b20
	if (!ctx.cr6.eq) goto loc_82DF5B20;
loc_82DF59D4:
	// cmpwi cr6,r15,0
	ctx.cr6.compare<int32_t>(ctx.r[15].s32, 0, ctx.xer);
	// addi r29,r15,-1
	ctx.r[29].s64 = ctx.r[15].s64 + -1;
	// beq cr6,0x82df5b20
	if (ctx.cr6.eq) goto loc_82DF5B20;
	// lwz r10,84(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// li r11,101
	ctx.r[11].s64 = 101;
	// addi r7,r1,100
	ctx.r[7].s64 = ctx.r[1].s64 + 100;
	// addi r6,r1,176
	ctx.r[6].s64 = ctx.r[1].s64 + 176;
	// addi r5,r1,84
	ctx.r[5].s64 = ctx.r[1].s64 + 84;
	// addi r4,r1,104
	ctx.r[4].s64 = ctx.r[1].s64 + 104;
	// stbx r11,r30,r10
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[10].u32, ctx.r[11].u8);
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df4a58
	ctx.lr = 0x82DF5A08;
	sub_82DF4A58(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5cd0
	if (ctx.cr0.eq) goto loc_82DF5CD0;
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5a38
	if (ctx.cr0.lt) goto loc_82DF5A38;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5a44
	goto loc_82DF5A44;
loc_82DF5A38:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5A40;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5A44:
	// cmpwi cr6,r23,45
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 45, ctx.xer);
	// bne cr6,0x82df5a80
	if (!ctx.cr6.eq) goto loc_82DF5A80;
	// lwz r11,84(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// li r10,45
	ctx.r[10].s64 = 45;
	// addi r7,r1,100
	ctx.r[7].s64 = ctx.r[1].s64 + 100;
	// addi r6,r1,176
	ctx.r[6].s64 = ctx.r[1].s64 + 176;
	// addi r5,r1,84
	ctx.r[5].s64 = ctx.r[1].s64 + 84;
	// addi r4,r1,104
	ctx.r[4].s64 = ctx.r[1].s64 + 104;
	// stbx r10,r30,r11
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[10].u8);
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df4a58
	ctx.lr = 0x82DF5A74;
	sub_82DF4A58(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5cd0
	if (ctx.cr0.eq) goto loc_82DF5CD0;
	// b 0x82df5a88
	goto loc_82DF5A88;
loc_82DF5A80:
	// cmpwi cr6,r23,43
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, 43, ctx.xer);
	// bne cr6,0x82df5b0c
	if (!ctx.cr6.eq) goto loc_82DF5B0C;
loc_82DF5A88:
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// addi r29,r29,-1
	ctx.r[29].s64 = ctx.r[29].s64 + -1;
	// bne cr6,0x82df5ad8
	if (!ctx.cr6.eq) goto loc_82DF5AD8;
	// li r29,0
	ctx.r[29].s64 = 0;
	// b 0x82df5b0c
	goto loc_82DF5B0C;
loc_82DF5A9C:
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// addi r29,r29,-1
	ctx.r[29].s64 = ctx.r[29].s64 + -1;
	// beq cr6,0x82df5b20
	if (ctx.cr6.eq) goto loc_82DF5B20;
	// lwz r11,84(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// addi r7,r1,100
	ctx.r[7].s64 = ctx.r[1].s64 + 100;
	// addi r6,r1,176
	ctx.r[6].s64 = ctx.r[1].s64 + 176;
	// addi r5,r1,84
	ctx.r[5].s64 = ctx.r[1].s64 + 84;
	// addi r4,r1,104
	ctx.r[4].s64 = ctx.r[1].s64 + 104;
	// addi r18,r18,1
	ctx.r[18].s64 = ctx.r[18].s64 + 1;
	// stbx r31,r30,r11
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[31].u8);
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df4a58
	ctx.lr = 0x82DF5AD0;
	sub_82DF4A58(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5cd0
	if (ctx.cr0.eq) goto loc_82DF5CD0;
loc_82DF5AD8:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5b00
	if (ctx.cr0.lt) goto loc_82DF5B00;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5b0c
	goto loc_82DF5B0C;
loc_82DF5B00:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5B08;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5B0C:
	// clrlwi r31,r23,24
	ctx.r[31].u64 = ctx.r[23].u32 & 0xFF;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x823588a0
	ctx.lr = 0x82DF5B18;
	sub_823588A0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df5a9c
	if (!ctx.cr0.eq) goto loc_82DF5A9C;
loc_82DF5B20:
	// addi r14,r14,-1
	ctx.r[14].s64 = ctx.r[14].s64 + -1;
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df5b38
	if (ctx.cr6.eq) goto loc_82DF5B38;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF5B38;
	sub_82B87CF8(ctx, base);
loc_82DF5B38:
	// cmpwi cr6,r18,0
	ctx.cr6.compare<int32_t>(ctx.r[18].s32, 0, ctx.xer);
	// beq cr6,0x82df5cd0
	if (ctx.cr6.eq) goto loc_82DF5CD0;
	// lwz r11,108(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 108);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82df5bb4
	if (!ctx.cr6.eq) goto loc_82DF5BB4;
	// lwz r10,92(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
	// extsb r11,r16
	ctx.r[11].s64 = ctx.r[16].s8;
	// lwz r5,84(r1)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// lwz r6,124(r1)
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 124);
	// lwz r4,88(r1)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// addi r3,r11,-1
	ctx.r[3].s64 = ctx.r[11].s64 + -1;
	// stw r10,92(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 92, ctx.r[10].u32);
	// li r10,0
	ctx.r[10].s64 = 0;
	// stbx r10,r30,r5
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[5].u32, ctx.r[10].u8);
	// lwz r10,128(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 128);
	// lwz r10,28(r10)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 28);
	// mtctr r10
	ctx.ctr.u64 = ctx.r[10].u64;
	// bctrl
	ctx.lr = 0x82DF5B84;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// b 0x82df5bb4
	goto loc_82DF5BB4;
loc_82DF5B88:
	// lbz r11,0(r10)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[10].u32 + 0);
	// cmpw cr6,r11,r23
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[23].s32, ctx.xer);
	// bne cr6,0x82df5cbc
	if (!ctx.cr6.eq) goto loc_82DF5CBC;
	// lbz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 80);
	// lwz r10,108(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 108);
	// addi r11,r11,-1
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// stb r11,80(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 80, ctx.r[11].u8);
	// bne cr6,0x82df5bb4
	if (!ctx.cr6.eq) goto loc_82DF5BB4;
	// lwz r11,132(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 132);
	// stw r11,732(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 732, ctx.r[11].u32);
loc_82DF5BB4:
	// lbz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 80);
	// lwz r10,716(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 716);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// rotlwi r19,r10,0
	ctx.r[19].u64 = std::rotl(ctx.r[10].u32, 0);
	// stb r11,80(r1)
	PPC_STORE_U8(ctx.r[1].u32 + 80, ctx.r[11].u8);
	// stw r10,716(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 716, ctx.r[10].u32);
	// b 0x82df5c74
	goto loc_82DF5C74;
loc_82DF5BD4:
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r14,r14,1
	ctx.r[14].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5bfc
	if (ctx.cr0.lt) goto loc_82DF5BFC;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r23,0(r11)
	ctx.r[23].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5c08
	goto loc_82DF5C08;
loc_82DF5BFC:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5C04;
	sub_82B81360(ctx, base);
	// mr r23,r3
	ctx.r[23].u64 = ctx.r[3].u64;
loc_82DF5C08:
	// lbz r11,0(r19)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// addi r19,r19,1
	ctx.r[19].s64 = ctx.r[19].s64 + 1;
	// cmpw cr6,r11,r23
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[23].s32, ctx.xer);
	// stw r19,716(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 716, ctx.r[19].u32);
	// bne cr6,0x82df5cbc
	if (!ctx.cr6.eq) goto loc_82DF5CBC;
	// clrlwi r3,r23,24
	ctx.r[3].u64 = ctx.r[23].u32 & 0xFF;
	// bl 0x82b7a680
	ctx.lr = 0x82DF5C24;
	sub_82B7A680(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df5c74
	if (ctx.cr0.eq) goto loc_82DF5C74;
	// lwz r11,4(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 4);
	// addi r31,r14,1
	ctx.r[31].s64 = ctx.r[14].s64 + 1;
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 4, ctx.r[11].u32);
	// blt 0x82df5c54
	if (ctx.cr0.lt) goto loc_82DF5C54;
	// lwz r11,0(r26)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[26].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[10].u32);
	// b 0x82df5c5c
	goto loc_82DF5C5C;
loc_82DF5C54:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82b81360
	ctx.lr = 0x82DF5C5C;
	sub_82B81360(ctx, base);
loc_82DF5C5C:
	// lbz r11,0(r19)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// addi r19,r19,1
	ctx.r[19].s64 = ctx.r[19].s64 + 1;
	// cmpw cr6,r11,r3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[3].s32, ctx.xer);
	// stw r19,716(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 716, ctx.r[19].u32);
	// bne cr6,0x82df5cac
	if (!ctx.cr6.eq) goto loc_82DF5CAC;
	// addi r14,r31,-1
	ctx.r[14].s64 = ctx.r[31].s64 + -1;
loc_82DF5C74:
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// bne cr6,0x82df5c9c
	if (!ctx.cr6.eq) goto loc_82DF5C9C;
	// lbz r11,0(r19)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// cmplwi cr6,r11,37
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 37, ctx.xer);
	// bne cr6,0x82df5cd0
	if (!ctx.cr6.eq) goto loc_82DF5CD0;
	// lwz r11,716(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 716);
	// lbz r11,1(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 1);
	// cmplwi cr6,r11,110
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 110, ctx.xer);
	// bne cr6,0x82df5cd0
	if (!ctx.cr6.eq) goto loc_82DF5CD0;
	// lwz r19,716(r1)
	ctx.r[19].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 716);
loc_82DF5C9C:
	// lbz r3,0(r19)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[19].u32 + 0);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// bne 0x82df4c70
	if (!ctx.cr0.eq) goto loc_82DF4C70;
	// b 0x82df5cd0
	goto loc_82DF5CD0;
loc_82DF5CAC:
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df5cbc
	if (ctx.cr6.eq) goto loc_82DF5CBC;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF5CBC;
	sub_82B87CF8(ctx, base);
loc_82DF5CBC:
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// beq cr6,0x82df5cd0
	if (ctx.cr6.eq) goto loc_82DF5CD0;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// mr r3,r23
	ctx.r[3].u64 = ctx.r[23].u64;
	// bl 0x82b87cf8
	ctx.lr = 0x82DF5CD0;
	sub_82B87CF8(ctx, base);
loc_82DF5CD0:
	// lwz r11,100(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 100);
	// cmpwi cr6,r11,1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 1, ctx.xer);
	// bne cr6,0x82df5ce4
	if (!ctx.cr6.eq) goto loc_82DF5CE4;
	// lwz r3,84(r1)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 84);
	// bl 0x823addc0
	ctx.lr = 0x82DF5CE4;
	sub_823ADDC0(ctx, base);
loc_82DF5CE4:
	// cmpwi cr6,r23,-1
	ctx.cr6.compare<int32_t>(ctx.r[23].s32, -1, ctx.xer);
	// bne cr6,0x82df5d08
	if (!ctx.cr6.eq) goto loc_82DF5D08;
	// lwz r3,92(r1)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne cr6,0x82df5d0c
	if (!ctx.cr6.eq) goto loc_82DF5D0C;
	// lbz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[1].u32 + 80);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne 0x82df5d0c
	if (!ctx.cr0.eq) goto loc_82DF5D0C;
	// b 0x82df4b60
	goto loc_82DF4B60;
loc_82DF5D08:
	// lwz r3,92(r1)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
loc_82DF5D0C:
	// addi r1,r1,688
	ctx.r[1].s64 = ctx.r[1].s64 + 688;
	// b 0x82b7a700
	__restgprlr_14(ctx, base);
	return;
}

void Body_82B7CFE0(Context& ctx, Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// rlwinm r10,r3,1,0,30
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 1) & 0xFFFFFFFE;
	// addi r11,r11,21024
	ctx.r[11].s64 = ctx.r[11].s64 + 21024;
	// lwz r11,200(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 200);
	// lhzx r11,r10,r11
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[10].u32 + ctx.r[11].u32);
	// rlwinm r3,r11,0,28,28
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x8;
	// blr
	return;
}

void Body_823588A0(Context& ctx, Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// rlwinm r10,r3,1,0,30
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 1) & 0xFFFFFFFE;
	// addi r11,r11,21024
	ctx.r[11].s64 = ctx.r[11].s64 + 21024;
	// lwz r11,200(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 200);
	// lhzx r11,r10,r11
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[10].u32 + ctx.r[11].u32);
	// rlwinm r3,r11,0,29,29
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
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

void Body_82B7CFC0(Context& ctx, Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// rlwinm r10,r3,1,0,30
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 1) & 0xFFFFFFFE;
	// addi r11,r11,21024
	ctx.r[11].s64 = ctx.r[11].s64 + 21024;
	// lwz r11,200(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 200);
	// lhzx r11,r10,r11
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[10].u32 + ctx.r[11].u32);
	// rlwinm r3,r11,0,24,24
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x80;
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

void Body_82B85420(Context& ctx, Base& base) {
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// lwz r11,24576(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 24576);
	// ori r10,r11,1
	ctx.r[10].u64 = ctx.r[11].u64 | 1;
	// lis r11,-31955
	ctx.r[11].s64 = -2094202880;
	// lwz r11,15592(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 15592);
	// subf r11,r11,r10
	ctx.r[11].s64 = ctx.r[10].s64 - ctx.r[11].s64;
	// cntlzw r11,r11
	ctx.r[11].u64 = ctx.r[11].u32 == 0 ? 32 : std::countl_zero(ctx.r[11].u32);
	// rlwinm r3,r11,27,31,31
	ctx.r[3].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
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

} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (entry != 0x82DF4AF8u) return false;
    Context ctx{};
    FromFull(ctx, state);
    Base base{memory, dependencies, state};
    Body_82DF4AF8(ctx, base);
    ToFull(state, ctx);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_scan_context
