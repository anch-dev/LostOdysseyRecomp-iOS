#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::crt_copy_full_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Registers = crt_async_status_transfer::Registers;

union CopyRegister
{
    std::uint64_t u64 = 0;
    std::int64_t s64;
    std::uint32_t u32;
    std::int32_t s32;
    std::uint8_t u8;
};
struct CopyXer { std::uint8_t ca = 0, so = 0; };
struct CopyCondition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    template<class T> void compare(T left, T right, const CopyXer& xer)
    {
        lt = std::uint8_t(left < right);
        gt = std::uint8_t(left > right);
        eq = std::uint8_t(left == right);
        so = xer.so;
    }
};
struct CopyContext
{
    CopyRegister r0, r1, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, ctr;
    CopyCondition cr0, cr1, cr6, cr7;
    CopyXer xer;
};

void Copy(GuestMemory& memory, Registers& state)
{
    CopyContext ctx{};
    ctx.r0.u64 = state.r[0];
    ctx.r1.u64 = state.r[1];
    ctx.r3.u64 = state.r[3];
    ctx.r4.u64 = state.r[4];
    ctx.r5.u64 = state.r[5];
    ctx.r6.u64 = state.r[6];
    ctx.r7.u64 = state.r[7];
    ctx.r8.u64 = state.r[8];
    ctx.r9.u64 = state.r[9];
    ctx.r10.u64 = state.r[10];
    ctx.r11.u64 = state.r[11];
    ctx.r12.u64 = state.r[12];
    ctx.ctr.u64 = state.ctr;
    ctx.xer = {state.xer_ca, state.xer_so};
    ctx.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.so};
    ctx.cr1 = {state.cr1.lt, state.cr1.gt, state.cr1.eq, state.cr1.so};
    ctx.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    ctx.cr7 = {state.cr7.lt, state.cr7.gt, state.cr7.eq, state.cr7.so};
#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(memory, Address(a))
#define PPC_STORE_U8(a,v) memory.WriteU8(Address(a), (v))
#define PPC_STORE_U32(a,v) memory.WriteU32(Address(a), (v))
#define PPC_STORE_U64(a,v) WriteU64(memory, Address(a), (v))
    // Complete 290-instruction 82B7A0B0 selected integer control flow.
    // PPC dcbt/dcbtst only hint at cache state; ordinary RAM is unchanged.
	CopyRegister temp{};
	// std r3,-8(r1)
	PPC_STORE_U64(ctx.r1.u32 + -8, ctx.r3.u64);
	// clrlwi r6,r3,29
	ctx.r6.u64 = ctx.r3.u32 & 0x7;
	// dcbt r0,r4
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// subfic r6,r6,8
	ctx.xer.ca = ctx.r6.u32 <= 8;
	ctx.r6.s64 = 8 - ctx.r6.s64;
	// beq 0x82b7a114
	if (ctx.cr0.eq) goto loc_82B7A114;
	// cmplw r5,r6
	ctx.cr0.compare<uint32_t>(ctx.r5.u32, ctx.r6.u32, ctx.xer);
	// ble 0x82b7a130
	if (!ctx.cr0.gt) goto loc_82B7A130;
	// cmplwi r6,4
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 4, ctx.xer);
	// beq 0x82b7a100
	if (ctx.cr0.eq) goto loc_82B7A100;
	// addi r3,r3,-1
	ctx.r3.s64 = ctx.r3.s64 + -1;
	// addi r4,r4,-1
	ctx.r4.s64 = ctx.r4.s64 + -1;
	// subf r5,r6,r5
	ctx.r5.s64 = ctx.r5.s64 - ctx.r6.s64;
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
loc_82B7A0E8:
	// lbzu r6,1(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(1);
	ctx.r6.u64 = PPC_LOAD_U8(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stbu r6,1(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(1);
	PPC_STORE_U8(temp.u32, ctx.r6.u8);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a0e8
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A0E8;
	// addi r3,r3,1
	ctx.r3.s64 = ctx.r3.s64 + 1;
	// addi r4,r4,1
	ctx.r4.s64 = ctx.r4.s64 + 1;
	// b 0x82b7a114
	goto loc_82B7A114;
loc_82B7A100:
	// subf r5,r6,r5
	ctx.r5.s64 = ctx.r5.s64 - ctx.r6.s64;
	// lwz r6,0(r4)
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r4.u32 + 0);
	// addi r4,r4,4
	ctx.r4.s64 = ctx.r4.s64 + 4;
	// stw r6,0(r3)
	PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r6.u32);
	// addi r3,r3,4
	ctx.r3.s64 = ctx.r3.s64 + 4;
loc_82B7A114:
	// clrlwi r6,r4,29
	ctx.r6.u64 = ctx.r4.u32 & 0x7;
	// cmplwi cr6,r6,4
	ctx.cr6.compare<uint32_t>(ctx.r6.u32, 4, ctx.xer);
	// cmplwi cr1,r6,0
	ctx.cr1.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// cmplwi cr7,r5,128
	ctx.cr7.compare<uint32_t>(ctx.r5.u32, 128, ctx.xer);
	// beq cr6,0x82b7a2f8
	if (ctx.cr6.eq) goto loc_82B7A2F8;
	// bne cr1,0x82b7a428
	if (!ctx.cr1.eq) goto loc_82B7A428;
	// bge cr7,0x82b7a1cc
	if (!ctx.cr7.lt) goto loc_82B7A1CC;
loc_82B7A130:
	// dcbtst r0,r3
	// addi r4,r4,-8
	ctx.r4.s64 = ctx.r4.s64 + -8;
	// addi r3,r3,-8
	ctx.r3.s64 = ctx.r3.s64 + -8;
loc_82B7A13C:
	// rlwinm r7,r5,29,28,31
	ctx.r7.u64 = std::rotl(ctx.r5.u32 | (ctx.r5.u64 << 32), 29) & 0xF;
	// clrlwi r6,r5,29
	ctx.r6.u64 = ctx.r5.u32 & 0x7;
	// cmplwi cr1,r7,0
	ctx.cr1.compare<uint32_t>(ctx.r7.u32, 0, ctx.xer);
	// cmplwi cr6,r6,0
	ctx.cr6.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// beq cr1,0x82b7a160
	if (ctx.cr1.eq) goto loc_82B7A160;
	// mtctr r7
	ctx.ctr.u64 = ctx.r7.u64;
loc_82B7A154:
	// ldu r7,8(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(8);
	ctx.r7.u64 = PPC_LOAD_U64(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stdu r7,8(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(8);
	PPC_STORE_U64(temp.u32, ctx.r7.u64);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a154
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A154;
loc_82B7A160:
	// cmplwi cr1,r6,4
	ctx.cr1.compare<uint32_t>(ctx.r6.u32, 4, ctx.xer);
	// beq cr6,0x82b7a184
	if (ctx.cr6.eq) goto loc_82B7A184;
	// beq cr1,0x82b7a18c
	if (ctx.cr1.eq) goto loc_82B7A18C;
	// addi r3,r3,7
	ctx.r3.s64 = ctx.r3.s64 + 7;
	// addi r4,r4,7
	ctx.r4.s64 = ctx.r4.s64 + 7;
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
loc_82B7A178:
	// lbzu r7,1(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(1);
	ctx.r7.u64 = PPC_LOAD_U8(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stbu r7,1(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(1);
	PPC_STORE_U8(temp.u32, ctx.r7.u8);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a178
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A178;
loc_82B7A184:
	// ld r3,-8(r1)
	ctx.r3.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);
	// blr
	goto copy_exit;
loc_82B7A18C:
	// clrlwi r6,r3,30
	ctx.r6.u64 = ctx.r3.u32 & 0x3;
	// lwz r5,8(r4)
	ctx.r5.u64 = PPC_LOAD_U32(ctx.r4.u32 + 8);
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// bne 0x82b7a1a8
	if (!ctx.cr0.eq) goto loc_82B7A1A8;
	// stw r5,8(r3)
	PPC_STORE_U32(ctx.r3.u32 + 8, ctx.r5.u32);
	// ld r3,-8(r1)
	ctx.r3.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);
	// blr
	goto copy_exit;
loc_82B7A1A8:
	// lbz r8,8(r4)
	ctx.r8.u64 = PPC_LOAD_U8(ctx.r4.u32 + 8);
	// lbz r7,9(r4)
	ctx.r7.u64 = PPC_LOAD_U8(ctx.r4.u32 + 9);
	// lbz r6,10(r4)
	ctx.r6.u64 = PPC_LOAD_U8(ctx.r4.u32 + 10);
	// stb r8,8(r3)
	PPC_STORE_U8(ctx.r3.u32 + 8, ctx.r8.u8);
	// stb r7,9(r3)
	PPC_STORE_U8(ctx.r3.u32 + 9, ctx.r7.u8);
	// stb r6,10(r3)
	PPC_STORE_U8(ctx.r3.u32 + 10, ctx.r6.u8);
	// stb r5,11(r3)
	PPC_STORE_U8(ctx.r3.u32 + 11, ctx.r5.u8);
	// ld r3,-8(r1)
	ctx.r3.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);
	// blr
	goto copy_exit;
loc_82B7A1CC:
	// clrlwi r6,r3,25
	ctx.r6.u64 = ctx.r3.u32 & 0x7F;
	// addi r3,r3,-8
	ctx.r3.s64 = ctx.r3.s64 + -8;
	// addi r4,r4,-8
	ctx.r4.s64 = ctx.r4.s64 + -8;
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// subfic r6,r6,128
	ctx.xer.ca = ctx.r6.u32 <= 128;
	ctx.r6.s64 = 128 - ctx.r6.s64;
	// beq 0x82b7a1fc
	if (ctx.cr0.eq) goto loc_82B7A1FC;
	// rlwinm r7,r6,29,3,31
	ctx.r7.u64 = std::rotl(ctx.r6.u32 | (ctx.r6.u64 << 32), 29) & 0x1FFFFFFF;
	// subf r5,r6,r5
	ctx.r5.s64 = ctx.r5.s64 - ctx.r6.s64;
	// mtctr r7
	ctx.ctr.u64 = ctx.r7.u64;
loc_82B7A1F0:
	// ldu r7,8(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(8);
	ctx.r7.u64 = PPC_LOAD_U64(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stdu r7,8(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(8);
	PPC_STORE_U64(temp.u32, ctx.r7.u64);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a1f0
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A1F0;
loc_82B7A1FC:
	// rlwinm r6,r5,25,7,31
	ctx.r6.u64 = std::rotl(ctx.r5.u32 | (ctx.r5.u64 << 32), 25) & 0x1FFFFFF;
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// beq 0x82b7a13c
	if (ctx.cr0.eq) goto loc_82B7A13C;
	// addi r10,r5,127
	ctx.r10.s64 = ctx.r5.s64 + 127;
	// clrlwi r8,r5,25
	ctx.r8.u64 = ctx.r5.u32 & 0x7F;
	// rlwinm r10,r10,25,7,31
	ctx.r10.u64 = std::rotl(ctx.r10.u32 | (ctx.r10.u64 << 32), 25) & 0x1FFFFFF;
	// cmplwi cr1,r8,0
	ctx.cr1.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
	// addi r10,r10,-1
	ctx.r10.s64 = ctx.r10.s64 + -1;
	// clrlwi r10,r10,29
	ctx.r10.u64 = ctx.r10.u32 & 0x7;
	// addi r10,r10,1
	ctx.r10.s64 = ctx.r10.s64 + 1;
	// li r9,8
	ctx.r9.s64 = 8;
	// mtctr r10
	ctx.ctr.u64 = ctx.r10.u64;
loc_82B7A22C:
	// dcbt r9,r4
	// addi r9,r9,128
	ctx.r9.s64 = ctx.r9.s64 + 128;
	// bdnz 0x82b7a22c
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A22C;
	// add r12,r4,r5
	ctx.r12.u64 = ctx.r4.u64 + ctx.r5.u64;
	// li r10,8
	ctx.r10.s64 = 8;
	// subf r11,r9,r12
	ctx.r11.s64 = ctx.r12.s64 - ctx.r9.s64;
	// add r12,r3,r5
	ctx.r12.u64 = ctx.r3.u64 + ctx.r5.u64;
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
loc_82B7A24C:
	// ld r6,8(r4)
	ctx.r6.u64 = PPC_LOAD_U64(ctx.r4.u32 + 8);
	// ld r7,16(r4)
	ctx.r7.u64 = PPC_LOAD_U64(ctx.r4.u32 + 16);
	// ld r8,24(r4)
	ctx.r8.u64 = PPC_LOAD_U64(ctx.r4.u32 + 24);
	// std r6,8(r3)
	PPC_STORE_U64(ctx.r3.u32 + 8, ctx.r6.u64);
	// ld r6,32(r4)
	ctx.r6.u64 = PPC_LOAD_U64(ctx.r4.u32 + 32);
	// std r7,16(r3)
	PPC_STORE_U64(ctx.r3.u32 + 16, ctx.r7.u64);
	// ld r7,40(r4)
	ctx.r7.u64 = PPC_LOAD_U64(ctx.r4.u32 + 40);
	// std r8,24(r3)
	PPC_STORE_U64(ctx.r3.u32 + 24, ctx.r8.u64);
	// ld r8,48(r4)
	ctx.r8.u64 = PPC_LOAD_U64(ctx.r4.u32 + 48);
	// std r6,32(r3)
	PPC_STORE_U64(ctx.r3.u32 + 32, ctx.r6.u64);
	// ld r6,56(r4)
	ctx.r6.u64 = PPC_LOAD_U64(ctx.r4.u32 + 56);
	// std r7,40(r3)
	PPC_STORE_U64(ctx.r3.u32 + 40, ctx.r7.u64);
	// ld r7,64(r4)
	ctx.r7.u64 = PPC_LOAD_U64(ctx.r4.u32 + 64);
	// std r8,48(r3)
	PPC_STORE_U64(ctx.r3.u32 + 48, ctx.r8.u64);
	// ld r8,72(r4)
	ctx.r8.u64 = PPC_LOAD_U64(ctx.r4.u32 + 72);
	// std r6,56(r3)
	PPC_STORE_U64(ctx.r3.u32 + 56, ctx.r6.u64);
	// ld r6,80(r4)
	ctx.r6.u64 = PPC_LOAD_U64(ctx.r4.u32 + 80);
	// std r7,64(r3)
	PPC_STORE_U64(ctx.r3.u32 + 64, ctx.r7.u64);
	// ld r7,88(r4)
	ctx.r7.u64 = PPC_LOAD_U64(ctx.r4.u32 + 88);
	// std r8,72(r3)
	PPC_STORE_U64(ctx.r3.u32 + 72, ctx.r8.u64);
	// ld r8,96(r4)
	ctx.r8.u64 = PPC_LOAD_U64(ctx.r4.u32 + 96);
	// std r6,80(r3)
	PPC_STORE_U64(ctx.r3.u32 + 80, ctx.r6.u64);
	// ld r6,104(r4)
	ctx.r6.u64 = PPC_LOAD_U64(ctx.r4.u32 + 104);
	// std r7,88(r3)
	PPC_STORE_U64(ctx.r3.u32 + 88, ctx.r7.u64);
	// ld r7,112(r4)
	ctx.r7.u64 = PPC_LOAD_U64(ctx.r4.u32 + 112);
	// std r8,96(r3)
	PPC_STORE_U64(ctx.r3.u32 + 96, ctx.r8.u64);
	// ld r8,120(r4)
	ctx.r8.u64 = PPC_LOAD_U64(ctx.r4.u32 + 120);
	// std r6,104(r3)
	PPC_STORE_U64(ctx.r3.u32 + 104, ctx.r6.u64);
	// ldu r6,128(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(128);
	ctx.r6.u64 = PPC_LOAD_U64(temp.u32);
	ctx.r4.u64 = temp.u64;
	// std r7,112(r3)
	PPC_STORE_U64(ctx.r3.u32 + 112, ctx.r7.u64);
	// std r8,120(r3)
	PPC_STORE_U64(ctx.r3.u32 + 120, ctx.r8.u64);
	// stdu r6,128(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(128);
	PPC_STORE_U64(temp.u32, ctx.r6.u64);
	ctx.r3.u64 = temp.u64;
	// cmplw r4,r11
	ctx.cr0.compare<uint32_t>(ctx.r4.u32, ctx.r11.u32, ctx.xer);
	// bge 0x82b7a2e0
	if (!ctx.cr0.lt) goto loc_82B7A2E0;
	// dcbt r9,r4
	// bdnz 0x82b7a24c
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A24C;
	// b 0x82b7a13c
	goto loc_82B7A13C;
loc_82B7A2E0:
	// beq cr1,0x82b7a2f0
	if (ctx.cr1.eq) goto loc_82B7A2F0;
	// li r8,-1
	ctx.r8.s64 = -1;
	// dcbtst r8,r12
	// cmplwi cr1,r8,0
	ctx.cr1.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
loc_82B7A2F0:
	// bdnz 0x82b7a24c
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A24C;
	// b 0x82b7a13c
	goto loc_82B7A13C;
loc_82B7A2F8:
	// addi r4,r4,-4
	ctx.r4.s64 = ctx.r4.s64 + -4;
	// bge cr7,0x82b7a350
	if (!ctx.cr7.lt) goto loc_82B7A350;
	// dcbtst r0,r3
	// addi r3,r3,-4
	ctx.r3.s64 = ctx.r3.s64 + -4;
loc_82B7A308:
	// rlwinm r7,r5,30,27,31
	ctx.r7.u64 = std::rotl(ctx.r5.u32 | (ctx.r5.u64 << 32), 30) & 0x1F;
	// clrlwi r6,r5,30
	ctx.r6.u64 = ctx.r5.u32 & 0x3;
	// cmplwi cr1,r7,0
	ctx.cr1.compare<uint32_t>(ctx.r7.u32, 0, ctx.xer);
	// cmplwi cr6,r6,0
	ctx.cr6.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// beq cr1,0x82b7a32c
	if (ctx.cr1.eq) goto loc_82B7A32C;
	// mtctr r7
	ctx.ctr.u64 = ctx.r7.u64;
loc_82B7A320:
	// lwzu r7,4(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(4);
	ctx.r7.u64 = PPC_LOAD_U32(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stwu r7,4(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(4);
	PPC_STORE_U32(temp.u32, ctx.r7.u32);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a320
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A320;
loc_82B7A32C:
	// beq cr6,0x82b7a348
	if (ctx.cr6.eq) goto loc_82B7A348;
	// addi r3,r3,3
	ctx.r3.s64 = ctx.r3.s64 + 3;
	// addi r4,r4,3
	ctx.r4.s64 = ctx.r4.s64 + 3;
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
loc_82B7A33C:
	// lbzu r7,1(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(1);
	ctx.r7.u64 = PPC_LOAD_U8(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stbu r7,1(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(1);
	PPC_STORE_U8(temp.u32, ctx.r7.u8);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a33c
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A33C;
loc_82B7A348:
	// ld r3,-8(r1)
	ctx.r3.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);
	// blr
	goto copy_exit;
loc_82B7A350:
	// clrlwi r6,r3,25
	ctx.r6.u64 = ctx.r3.u32 & 0x7F;
	// addi r3,r3,-4
	ctx.r3.s64 = ctx.r3.s64 + -4;
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// subfic r6,r6,128
	ctx.xer.ca = ctx.r6.u32 <= 128;
	ctx.r6.s64 = 128 - ctx.r6.s64;
	// beq 0x82b7a37c
	if (ctx.cr0.eq) goto loc_82B7A37C;
	// rlwinm r7,r6,30,2,31
	ctx.r7.u64 = std::rotl(ctx.r6.u32 | (ctx.r6.u64 << 32), 30) & 0x3FFFFFFF;
	// subf r5,r6,r5
	ctx.r5.s64 = ctx.r5.s64 - ctx.r6.s64;
	// mtctr r7
	ctx.ctr.u64 = ctx.r7.u64;
loc_82B7A370:
	// lwzu r7,4(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(4);
	ctx.r7.u64 = PPC_LOAD_U32(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stwu r7,4(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(4);
	PPC_STORE_U32(temp.u32, ctx.r7.u32);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a370
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A370;
loc_82B7A37C:
	// rlwinm r6,r5,25,7,31
	ctx.r6.u64 = std::rotl(ctx.r5.u32 | (ctx.r5.u64 << 32), 25) & 0x1FFFFFF;
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// beq 0x82b7a308
	if (ctx.cr0.eq) goto loc_82B7A308;
	// addi r10,r5,127
	ctx.r10.s64 = ctx.r5.s64 + 127;
	// clrlwi r8,r5,25
	ctx.r8.u64 = ctx.r5.u32 & 0x7F;
	// rlwinm r10,r10,25,7,31
	ctx.r10.u64 = std::rotl(ctx.r10.u32 | (ctx.r10.u64 << 32), 25) & 0x1FFFFFF;
	// cmplwi cr1,r8,0
	ctx.cr1.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
	// addi r10,r10,-1
	ctx.r10.s64 = ctx.r10.s64 + -1;
	// clrlwi r10,r10,29
	ctx.r10.u64 = ctx.r10.u32 & 0x7;
	// addi r10,r10,1
	ctx.r10.s64 = ctx.r10.s64 + 1;
	// li r9,4
	ctx.r9.s64 = 4;
	// mtctr r10
	ctx.ctr.u64 = ctx.r10.u64;
loc_82B7A3AC:
	// dcbt r9,r4
	// addi r9,r9,128
	ctx.r9.s64 = ctx.r9.s64 + 128;
	// bdnz 0x82b7a3ac
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A3AC;
	// add r12,r4,r5
	ctx.r12.u64 = ctx.r4.u64 + ctx.r5.u64;
	// li r10,8
	ctx.r10.s64 = 8;
	// subf r11,r9,r12
	ctx.r11.s64 = ctx.r12.s64 - ctx.r9.s64;
	// add r12,r3,r5
	ctx.r12.u64 = ctx.r3.u64 + ctx.r5.u64;
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
loc_82B7A3CC:
	// li r6,8
	ctx.r6.s64 = 8;
loc_82B7A3D0:
	// addi r6,r6,-1
	ctx.r6.s64 = ctx.r6.s64 + -1;
	// lwz r0,4(r4)
	ctx.r0.u64 = PPC_LOAD_U32(ctx.r4.u32 + 4);
	// lwz r7,8(r4)
	ctx.r7.u64 = PPC_LOAD_U32(ctx.r4.u32 + 8);
	// lwz r8,12(r4)
	ctx.r8.u64 = PPC_LOAD_U32(ctx.r4.u32 + 12);
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// stw r0,4(r3)
	PPC_STORE_U32(ctx.r3.u32 + 4, ctx.r0.u32);
	// lwzu r0,16(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(16);
	ctx.r0.u64 = PPC_LOAD_U32(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stw r7,8(r3)
	PPC_STORE_U32(ctx.r3.u32 + 8, ctx.r7.u32);
	// stw r8,12(r3)
	PPC_STORE_U32(ctx.r3.u32 + 12, ctx.r8.u32);
	// stwu r0,16(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(16);
	PPC_STORE_U32(temp.u32, ctx.r0.u32);
	ctx.r3.u64 = temp.u64;
	// bne 0x82b7a3d0
	if (!ctx.cr0.eq) goto loc_82B7A3D0;
	// cmplw r4,r11
	ctx.cr0.compare<uint32_t>(ctx.r4.u32, ctx.r11.u32, ctx.xer);
	// bge 0x82b7a410
	if (!ctx.cr0.lt) goto loc_82B7A410;
	// dcbt r9,r4
	// bdnz 0x82b7a3cc
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A3CC;
	// b 0x82b7a308
	goto loc_82B7A308;
loc_82B7A410:
	// beq cr1,0x82b7a420
	if (ctx.cr1.eq) goto loc_82B7A420;
	// li r8,-1
	ctx.r8.s64 = -1;
	// dcbtst r8,r12
	// cmplwi cr1,r8,0
	ctx.cr1.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
loc_82B7A420:
	// bdnz 0x82b7a3cc
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A3CC;
	// b 0x82b7a308
	goto loc_82B7A308;
loc_82B7A428:
	// addi r4,r4,-1
	ctx.r4.s64 = ctx.r4.s64 + -1;
	// bge cr7,0x82b7a45c
	if (!ctx.cr7.lt) goto loc_82B7A45C;
	// dcbtst r0,r3
	// addi r3,r3,-1
	ctx.r3.s64 = ctx.r3.s64 + -1;
loc_82B7A438:
	// clrlwi r6,r5,25
	ctx.r6.u64 = ctx.r5.u32 & 0x7F;
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
	// beq 0x82b7a454
	if (ctx.cr0.eq) goto loc_82B7A454;
loc_82B7A448:
	// lbzu r6,1(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(1);
	ctx.r6.u64 = PPC_LOAD_U8(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stbu r6,1(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(1);
	PPC_STORE_U8(temp.u32, ctx.r6.u8);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a448
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A448;
loc_82B7A454:
	// ld r3,-8(r1)
	ctx.r3.u64 = PPC_LOAD_U64(ctx.r1.u32 + -8);
	// blr
	goto copy_exit;
loc_82B7A45C:
	// clrlwi r6,r3,25
	ctx.r6.u64 = ctx.r3.u32 & 0x7F;
	// addi r3,r3,-1
	ctx.r3.s64 = ctx.r3.s64 + -1;
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// subfic r6,r6,128
	ctx.xer.ca = ctx.r6.u32 <= 128;
	ctx.r6.s64 = 128 - ctx.r6.s64;
	// beq 0x82b7a484
	if (ctx.cr0.eq) goto loc_82B7A484;
	// subf r5,r6,r5
	ctx.r5.s64 = ctx.r5.s64 - ctx.r6.s64;
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
loc_82B7A478:
	// lbzu r6,1(r4)
	temp.u64 = ctx.r4.u64 + uint64_t(1);
	ctx.r6.u64 = PPC_LOAD_U8(temp.u32);
	ctx.r4.u64 = temp.u64;
	// stbu r6,1(r3)
	temp.u64 = ctx.r3.u64 + uint64_t(1);
	PPC_STORE_U8(temp.u32, ctx.r6.u8);
	ctx.r3.u64 = temp.u64;
	// bdnz 0x82b7a478
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A478;
loc_82B7A484:
	// rlwinm r6,r5,25,7,31
	ctx.r6.u64 = std::rotl(ctx.r5.u32 | (ctx.r5.u64 << 32), 25) & 0x1FFFFFF;
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// beq 0x82b7a438
	if (ctx.cr0.eq) goto loc_82B7A438;
	// addi r10,r5,127
	ctx.r10.s64 = ctx.r5.s64 + 127;
	// clrlwi r8,r5,25
	ctx.r8.u64 = ctx.r5.u32 & 0x7F;
	// rlwinm r10,r10,25,7,31
	ctx.r10.u64 = std::rotl(ctx.r10.u32 | (ctx.r10.u64 << 32), 25) & 0x1FFFFFF;
	// cmplwi cr1,r8,0
	ctx.cr1.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
	// addi r10,r10,-1
	ctx.r10.s64 = ctx.r10.s64 + -1;
	// clrlwi r10,r10,29
	ctx.r10.u64 = ctx.r10.u32 & 0x7;
	// addi r10,r10,1
	ctx.r10.s64 = ctx.r10.s64 + 1;
	// li r9,1
	ctx.r9.s64 = 1;
	// mtctr r10
	ctx.ctr.u64 = ctx.r10.u64;
loc_82B7A4B4:
	// dcbt r9,r4
	// addi r9,r9,128
	ctx.r9.s64 = ctx.r9.s64 + 128;
	// bdnz 0x82b7a4b4
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A4B4;
	// add r12,r4,r5
	ctx.r12.u64 = ctx.r4.u64 + ctx.r5.u64;
	// li r10,1
	ctx.r10.s64 = 1;
	// subf r11,r9,r12
	ctx.r11.s64 = ctx.r12.s64 - ctx.r9.s64;
	// add r12,r3,r5
	ctx.r12.u64 = ctx.r3.u64 + ctx.r5.u64;
	// mtctr r6
	ctx.ctr.u64 = ctx.r6.u64;
loc_82B7A4D4:
	// li r6,32
	ctx.r6.s64 = 32;
loc_82B7A4D8:
	// lbz r7,4(r4)
	ctx.r7.u64 = PPC_LOAD_U8(ctx.r4.u32 + 4);
	// lbz r8,3(r4)
	ctx.r8.u64 = PPC_LOAD_U8(ctx.r4.u32 + 3);
	// addi r6,r6,-1
	ctx.r6.s64 = ctx.r6.s64 + -1;
	// rlwimi r7,r8,8,16,23
	ctx.r7.u64 = (std::rotl(ctx.r8.u32 | (ctx.r8.u64 << 32), 8) & 0xFF00) | (ctx.r7.u64 & 0xFFFFFFFFFFFF00FF);
	// lbz r9,2(r4)
	ctx.r9.u64 = PPC_LOAD_U8(ctx.r4.u32 + 2);
	// cmplwi r6,0
	ctx.cr0.compare<uint32_t>(ctx.r6.u32, 0, ctx.xer);
	// rlwimi r7,r9,16,8,15
	ctx.r7.u64 = (std::rotl(ctx.r9.u32 | (ctx.r9.u64 << 32), 16) & 0xFF0000) | (ctx.r7.u64 & 0xFFFFFFFFFF00FFFF);
	// lbz r10,1(r4)
	ctx.r10.u64 = PPC_LOAD_U8(ctx.r4.u32 + 1);
	// addi r4,r4,4
	ctx.r4.s64 = ctx.r4.s64 + 4;
	// rlwimi r7,r10,24,0,7
	ctx.r7.u64 = (std::rotl(ctx.r10.u32 | (ctx.r10.u64 << 32), 24) & 0xFF000000) | (ctx.r7.u64 & 0xFFFFFFFF00FFFFFF);
	// stw r7,1(r3)
	PPC_STORE_U32(ctx.r3.u32 + 1, ctx.r7.u32);
	// addi r3,r3,4
	ctx.r3.s64 = ctx.r3.s64 + 4;
	// bne 0x82b7a4d8
	if (!ctx.cr0.eq) goto loc_82B7A4D8;
	// cmplw r4,r11
	ctx.cr0.compare<uint32_t>(ctx.r4.u32, ctx.r11.u32, ctx.xer);
	// bge 0x82b7a520
	if (!ctx.cr0.lt) goto loc_82B7A520;
	// dcbt r9,r4
	// bdnz 0x82b7a4d4
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A4D4;
	// b 0x82b7a438
	goto loc_82B7A438;
loc_82B7A520:
	// beq cr1,0x82b7a530
	if (ctx.cr1.eq) goto loc_82B7A530;
	// li r8,-1
	ctx.r8.s64 = -1;
	// dcbtst r8,r12
	// cmplwi cr1,r8,0
	ctx.cr1.compare<uint32_t>(ctx.r8.u32, 0, ctx.xer);
loc_82B7A530:
	// bdnz 0x82b7a4d4
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82B7A4D4;
	// b 0x82b7a438
	goto loc_82B7A438;

copy_exit:
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
    state.r[0] = ctx.r0.u64;
    state.r[1] = ctx.r1.u64;
    state.r[3] = ctx.r3.u64;
    state.r[4] = ctx.r4.u64;
    state.r[5] = ctx.r5.u64;
    state.r[6] = ctx.r6.u64;
    state.r[7] = ctx.r7.u64;
    state.r[8] = ctx.r8.u64;
    state.r[9] = ctx.r9.u64;
    state.r[10] = ctx.r10.u64;
    state.r[11] = ctx.r11.u64;
    state.r[12] = ctx.r12.u64;
    state.ctr = ctx.ctr.u64;
    state.xer_ca = ctx.xer.ca;
    state.xer_so = ctx.xer.so;
    state.cr0 = {ctx.cr0.lt, ctx.cr0.gt, ctx.cr0.eq, ctx.cr0.so};
    state.cr1 = {ctx.cr1.lt, ctx.cr1.gt, ctx.cr1.eq, ctx.cr1.so};
    state.cr6 = {ctx.cr6.lt, ctx.cr6.gt, ctx.cr6.eq, ctx.cr6.so};
    state.cr7 = {ctx.cr7.lt, ctx.cr7.gt, ctx.cr7.eq, ctx.cr7.so};
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    if (entry != 0x82b7a0b0u) return false;
    Copy(memory, state);
    return true;
}
} // namespace lo::semantic::gpu::crt_copy_full_context
