#include "lo_semantics/legacy_float_binary_conversion.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::legacy_float_binary_conversion
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

// The register overlay preserves the recompiler's low-word PPC operations.
// It is local to this self-contained lower; no game PPC runtime is linked.
union Word
{
    std::uint64_t u64 = 0;
    std::int64_t s64;
    std::uint32_t u32;
    std::int32_t s32;
    std::uint8_t u8;
};
struct Cr
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    template<typename T>
    void compare(T left, T right, const Xer& xer)
    {
        lt = left < right;
        gt = left > right;
        eq = left == right;
        so = xer.so;
    }
};
struct Context
{
    std::array<Word, 32> r{};
    Word ctr{};
    std::uint64_t lr = 0;
    Xer xer{};
    Cr cr0{}, cr6{};
};
Context ToContext(const Registers& state)
{
    Context result{};
    for (unsigned i = 0; i < result.r.size(); ++i)
        result.r[i].u64 = state.r[i];
    result.ctr.u64 = state.ctr;
    result.lr = state.lr;
    result.xer = state.xer;
    result.cr0 = {state.cr0.lt, state.cr0.gt,
        state.cr0.eq, state.cr0.so};
    result.cr6 = {state.cr6.lt, state.cr6.gt,
        state.cr6.eq, state.cr6.so};
    return result;
}
void FromContext(const Context& source, Registers& state)
{
    for (unsigned i = 0; i < source.r.size(); ++i)
        state.r[i] = source.r[i].u64;
    state.ctr = source.ctr.u64;
    state.lr = source.lr;
    state.xer = source.xer;
    state.cr0 = {source.cr0.lt, source.cr0.gt,
        source.cr0.eq, source.cr0.so};
    state.cr6 = {source.cr6.lt, source.cr6.gt,
        source.cr6.eq, source.cr6.so};
}
void SaveGprs(GuestMemory& memory, Context& state)
{
    const auto sp = Address(state.r[1].u64);
    for (unsigned index = 19u; index <= 31u; ++index)
        WriteU64(memory, sp - 16u - (31u - index) * 8u,
            state.r[index].u64);
    memory.WriteU32(sp - 8u, Address(state.r[12].u64));
}
void RestoreGprs(GuestMemory& memory, Context& state)
{
    const auto sp = Address(state.r[1].u64);
    for (unsigned index = 19u; index <= 31u; ++index)
        state.r[index].u64 = ReadU64(memory,
            sp - 16u - (31u - index) * 8u);
    state.r[12].u64 = memory.ReadU32(sp - 8u);
    state.lr = state.r[12].u64;
}

// The original accesses are big-endian; these adapters retain their order and
// low-32-bit guest addresses. Fault and MMIO behavior is outside this lower.
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_STORE_U32(a, v) memory.WriteU32(Address(a), (v))

void Convert(GuestMemory& memory, Registers& registers)
{
    Context ctx = ToContext(registers);
    // 822981C8, ppc_recomp.0.cpp:19506. Keep each PPC operation and branch.
	Word temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6c4
	ctx.lr = 0x822981D0;
	SaveGprs(memory, ctx);
	// lhz r10,10(r3)
	ctx.r[10].u64 = PPC_LOAD_U16(ctx.r[3].u32 + 10);
	// li r22,0
	ctx.r[22].s64 = 0;
	// lhz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[3].u32 + 0);
	// rotlwi r10,r10,16
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32, 16);
	// lwz r9,2(r3)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 2);
	// rlwinm r19,r11,0,0,16
	ctx.r[19].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFF8000;
	// stw r10,-144(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -144, ctx.r[10].u32);
	// clrlwi r10,r11,17
	ctx.r[10].u64 = ctx.r[11].u32 & 0x7FFF;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// stw r9,-152(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -152, ctx.r[9].u32);
	// lwz r9,6(r3)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 6);
	// addi r30,r10,-16383
	ctx.r[30].s64 = ctx.r[10].s64 + -16383;
	// addi r20,r11,23632
	ctx.r[20].s64 = ctx.r[11].s64 + 23632;
	// cmpwi cr6,r30,-16383
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, -16383, ctx.xer);
	// stw r9,-148(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -148, ctx.r[9].u32);
	// lwz r21,12(r20)
	ctx.r[21].u64 = PPC_LOAD_U32(ctx.r[20].u32 + 12);
	// bne cr6,0x82298258
	if (!ctx.cr6.eq) goto loc_82298258;
	// mr r31,r22
	ctx.r[31].u64 = ctx.r[22].u64;
	// mr r11,r22
	ctx.r[11].u64 = ctx.r[22].u64;
	// addi r10,r1,-152
	ctx.r[10].s64 = ctx.r[1].s64 + -152;
loc_82298220:
	// lwz r9,0(r10)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// bne cr6,0x82298240
	if (!ctx.cr6.eq) goto loc_82298240;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// blt cr6,0x82298220
	if (ctx.cr6.lt) goto loc_82298220;
	// b 0x8229888c
	goto loc_8229888C;
loc_82298240:
	// addi r11,r1,-152
	ctx.r[11].s64 = ctx.r[1].s64 + -152;
	// li r3,2
	ctx.r[3].s64 = 2;
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
	// stw r22,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[22].u32);
	// stw r22,8(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 8, ctx.r[22].u32);
	// b 0x82298890
	goto loc_82298890;
loc_82298258:
	// addi r11,r1,-152
	ctx.r[11].s64 = ctx.r[1].s64 + -152;
	// lwz r25,8(r20)
	ctx.r[25].u64 = PPC_LOAD_U32(ctx.r[20].u32 + 8);
	// addi r10,r1,-136
	ctx.r[10].s64 = ctx.r[1].s64 + -136;
	// addi r26,r25,-1
	ctx.r[26].s64 = ctx.r[25].s64 + -1;
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r27,r1,-152
	ctx.r[27].s64 = ctx.r[1].s64 + -152;
	// lwz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// mr r24,r30
	ctx.r[24].u64 = ctx.r[30].u64;
	// lwz r8,4(r11)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// mr r5,r22
	ctx.r[5].u64 = ctx.r[22].u64;
	// lwz r11,8(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 8);
	// li r23,-1
	ctx.r[23].s64 = -1;
	// stw r9,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[9].u32);
	// stw r8,4(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 4, ctx.r[8].u32);
	// stw r11,8(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 8, ctx.r[11].u32);
	// addi r11,r26,1
	ctx.r[11].s64 = ctx.r[26].s64 + 1;
	// srawi r10,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[10].s64 = ctx.r[11].s32 >> 5;
	// addze r31,r10
	temp.s64 = ctx.r[10].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[10].u32;
	ctx.r[31].s64 = temp.s64;
	// srawi r10,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[10].s64 = ctx.r[11].s32 >> 5;
	// rlwinm r28,r31,2,0,29
	ctx.r[28].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 2) & 0xFFFFFFFC;
	// addze r10,r10
	temp.s64 = ctx.r[10].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[10].u32;
	ctx.r[10].s64 = temp.s64;
	// rlwinm r10,r10,5,0,26
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 5) & 0xFFFFFFE0;
	// subf r11,r10,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[10].s64;
	// lwzx r10,r28,r27
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[27].u32);
	// subfic r29,r11,31
	ctx.xer.ca = ctx.r[11].u32 <= 31;
	ctx.r[29].s64 = 31 - ctx.r[11].s64;
	// slw r11,r3,r29
	ctx.r[11].u64 = ctx.r[29].u8 & 0x20 ? 0 : (ctx.r[3].u32 << (ctx.r[29].u8 & 0x3F));
	// and. r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 & ctx.r[10].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x822983b0
	if (ctx.cr0.eq) goto loc_822983B0;
	// addi r11,r1,-152
	ctx.r[11].s64 = ctx.r[1].s64 + -152;
	// rlwinm r9,r31,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 2) & 0xFFFFFFFC;
	// slw r10,r23,r29
	ctx.r[10].u64 = ctx.r[29].u8 & 0x20 ? 0 : (ctx.r[23].u32 << (ctx.r[29].u8 & 0x3F));
	// lwzx r11,r9,r11
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[9].u32 + ctx.r[11].u32);
	// andc. r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 & ~ctx.r[10].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82298318
	if (!ctx.cr0.eq) goto loc_82298318;
	// addi r11,r31,1
	ctx.r[11].s64 = ctx.r[31].s64 + 1;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// bge cr6,0x822983b0
	if (!ctx.cr6.lt) goto loc_822983B0;
	// rlwinm r10,r11,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
loc_822982F8:
	// lwz r9,0(r10)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// bne cr6,0x82298318
	if (!ctx.cr6.eq) goto loc_82298318;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// blt cr6,0x822982f8
	if (ctx.cr6.lt) goto loc_822982F8;
	// b 0x822983b0
	goto loc_822983B0;
loc_82298318:
	// srawi r11,r26,5
	ctx.xer.ca = (ctx.r[26].s32 < 0) && ((ctx.r[26].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[26].s32 >> 5;
	// addi r7,r1,-152
	ctx.r[7].s64 = ctx.r[1].s64 + -152;
	// addze r9,r11
	temp.s64 = ctx.r[11].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[11].u32;
	ctx.r[9].s64 = temp.s64;
	// srawi r11,r26,5
	ctx.xer.ca = (ctx.r[26].s32 < 0) && ((ctx.r[26].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[26].s32 >> 5;
	// rlwinm r8,r9,2,0,29
	ctx.r[8].u64 = std::rotl(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 2) & 0xFFFFFFFC;
	// addze r11,r11
	temp.s64 = ctx.r[11].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[11].u32;
	ctx.r[11].s64 = temp.s64;
	// mr r5,r22
	ctx.r[5].u64 = ctx.r[22].u64;
	// rlwinm r11,r11,5,0,26
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 5) & 0xFFFFFFE0;
	// subf r11,r11,r26
	ctx.r[11].s64 = ctx.r[26].s64 - ctx.r[11].s64;
	// subfic r11,r11,31
	ctx.xer.ca = ctx.r[11].u32 <= 31;
	ctx.r[11].s64 = 31 - ctx.r[11].s64;
	// slw r6,r3,r11
	ctx.r[6].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[3].u32 << (ctx.r[11].u8 & 0x3F));
	// lwzx r11,r8,r7
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[8].u32 + ctx.r[7].u32);
	// add r10,r11,r6
	ctx.r[10].u64 = ctx.r[11].u64 + ctx.r[6].u64;
	// cmplw cr6,r10,r11
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x8229835c
	if (ctx.cr6.lt) goto loc_8229835C;
	// cmplw cr6,r10,r6
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[6].u32, ctx.xer);
	// bge cr6,0x82298360
	if (!ctx.cr6.lt) goto loc_82298360;
loc_8229835C:
	// mr r5,r3
	ctx.r[5].u64 = ctx.r[3].u64;
loc_82298360:
	// addic. r11,r9,-1
	ctx.xer.ca = ctx.r[9].u32 > 0;
	ctx.r[11].s64 = ctx.r[9].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stwx r10,r8,r7
	PPC_STORE_U32(ctx.r[8].u32 + ctx.r[7].u32, ctx.r[10].u32);
	// blt 0x822983b0
	if (ctx.cr0.lt) goto loc_822983B0;
	// rlwinm r10,r11,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
loc_82298378:
	// cmpwi cr6,r5,0
	ctx.cr6.compare<int32_t>(ctx.r[5].s32, 0, ctx.xer);
	// beq cr6,0x822983b0
	if (ctx.cr6.eq) goto loc_822983B0;
	// lwz r9,0(r10)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// mr r5,r22
	ctx.r[5].u64 = ctx.r[22].u64;
	// addi r8,r9,1
	ctx.r[8].s64 = ctx.r[9].s64 + 1;
	// cmplw cr6,r8,r9
	ctx.cr6.compare<uint32_t>(ctx.r[8].u32, ctx.r[9].u32, ctx.xer);
	// blt cr6,0x8229839c
	if (ctx.cr6.lt) goto loc_8229839C;
	// cmplwi cr6,r8,1
	ctx.cr6.compare<uint32_t>(ctx.r[8].u32, 1, ctx.xer);
	// bge cr6,0x822983a0
	if (!ctx.cr6.lt) goto loc_822983A0;
loc_8229839C:
	// mr r5,r3
	ctx.r[5].u64 = ctx.r[3].u64;
loc_822983A0:
	// stw r8,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[8].u32);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// addi r10,r10,-4
	ctx.r[10].s64 = ctx.r[10].s64 + -4;
	// bge 0x82298378
	if (!ctx.cr0.lt) goto loc_82298378;
loc_822983B0:
	// lwzx r10,r28,r27
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[27].u32);
	// slw r9,r23,r29
	ctx.r[9].u64 = ctx.r[29].u8 & 0x20 ? 0 : (ctx.r[23].u32 << (ctx.r[29].u8 & 0x3F));
	// addi r11,r31,1
	ctx.r[11].s64 = ctx.r[31].s64 + 1;
	// and r10,r9,r10
	ctx.r[10].u64 = ctx.r[9].u64 & ctx.r[10].u64;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// stwx r10,r28,r27
	PPC_STORE_U32(ctx.r[28].u32 + ctx.r[27].u32, ctx.r[10].u32);
	// bge cr6,0x822983f8
	if (!ctx.cr6.lt) goto loc_822983F8;
	// rlwinm r10,r11,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// subfic r11,r11,3
	ctx.xer.ca = ctx.r[11].u32 <= 3;
	ctx.r[11].s64 = 3 - ctx.r[11].s64;
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// mr r9,r22
	ctx.r[9].u64 = ctx.r[22].u64;
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq 0x822983f8
	if (ctx.cr0.eq) goto loc_822983F8;
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
loc_822983EC:
	// stw r9,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[9].u32);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// bdnz 0x822983ec
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_822983EC;
loc_822983F8:
	// cmpwi cr6,r5,0
	ctx.cr6.compare<int32_t>(ctx.r[5].s32, 0, ctx.xer);
	// beq cr6,0x82298404
	if (ctx.cr6.eq) goto loc_82298404;
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
loc_82298404:
	// lwz r11,4(r20)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[20].u32 + 4);
	// subf r10,r25,r11
	ctx.r[10].s64 = ctx.r[11].s64 - ctx.r[25].s64;
	// cmpw cr6,r30,r10
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, ctx.r[10].s32, ctx.xer);
	// bge cr6,0x82298430
	if (!ctx.cr6.lt) goto loc_82298430;
	// addi r11,r1,-152
	ctx.r[11].s64 = ctx.r[1].s64 + -152;
	// mr r31,r22
	ctx.r[31].u64 = ctx.r[22].u64;
	// li r3,2
	ctx.r[3].s64 = 2;
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
	// stw r22,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[22].u32);
	// stw r22,8(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 8, ctx.r[22].u32);
	// b 0x82298890
	goto loc_82298890;
loc_82298430:
	// cmpw cr6,r30,r11
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, ctx.r[11].s32, ctx.xer);
	// bgt cr6,0x8229870c
	if (ctx.cr6.gt) goto loc_8229870C;
	// subf r11,r24,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[24].s64;
	// addi r10,r1,-136
	ctx.r[10].s64 = ctx.r[1].s64 + -136;
	// srawi r5,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[5].s64 = ctx.r[11].s32 >> 5;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// addze r31,r5
	temp.s64 = ctx.r[5].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[5].u32;
	ctx.r[31].s64 = temp.s64;
	// srawi r29,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[29].s64 = ctx.r[11].s32 >> 5;
	// lwz r30,0(r10)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// li r27,-1
	ctx.r[27].s64 = -1;
	// addze r29,r29
	temp.s64 = ctx.r[29].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[29].u32;
	ctx.r[29].s64 = temp.s64;
	// lwz r5,4(r10)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 4);
	// lwz r10,8(r10)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 8);
	// mr r6,r22
	ctx.r[6].u64 = ctx.r[22].u64;
	// addi r8,r1,-152
	ctx.r[8].s64 = ctx.r[1].s64 + -152;
	// li r7,3
	ctx.r[7].s64 = 3;
	// stw r30,0(r9)
	PPC_STORE_U32(ctx.r[9].u32 + 0, ctx.r[30].u32);
	// rlwinm r30,r29,5,0,26
	ctx.r[30].u64 = std::rotl(ctx.r[29].u32 | (ctx.r[29].u64 << 32), 5) & 0xFFFFFFE0;
	// stw r5,4(r9)
	PPC_STORE_U32(ctx.r[9].u32 + 4, ctx.r[5].u32);
	// subf r11,r30,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[30].s64;
	// stw r10,8(r9)
	PPC_STORE_U32(ctx.r[9].u32 + 8, ctx.r[10].u32);
	// subfic r10,r11,32
	ctx.xer.ca = ctx.r[11].u32 <= 32;
	ctx.r[10].s64 = 32 - ctx.r[11].s64;
	// slw r9,r27,r11
	ctx.r[9].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[27].u32 << (ctx.r[11].u8 & 0x3F));
	// not r9,r9
	ctx.r[9].u64 = ~ctx.r[9].u64;
loc_82298490:
	// lwz r5,0(r8)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[8].u32 + 0);
	// addic. r7,r7,-1
	ctx.xer.ca = ctx.r[7].u32 > 0;
	ctx.r[7].s64 = ctx.r[7].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[7].s32, 0, ctx.xer);
	// and r30,r5,r9
	ctx.r[30].u64 = ctx.r[5].u64 & ctx.r[9].u64;
	// stw r30,-160(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -160, ctx.r[30].u32);
	// srw r5,r5,r11
	ctx.r[5].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[5].u32 >> (ctx.r[11].u8 & 0x3F));
	// or r6,r5,r6
	ctx.r[6].u64 = ctx.r[5].u64 | ctx.r[6].u64;
	// stw r6,0(r8)
	PPC_STORE_U32(ctx.r[8].u32 + 0, ctx.r[6].u32);
	// addi r8,r8,4
	ctx.r[8].s64 = ctx.r[8].s64 + 4;
	// lwz r6,-160(r1)
	ctx.r[6].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -160);
	// slw r6,r6,r10
	ctx.r[6].u64 = ctx.r[10].u8 & 0x20 ? 0 : (ctx.r[6].u32 << (ctx.r[10].u8 & 0x3F));
	// bne 0x82298490
	if (!ctx.cr0.eq) goto loc_82298490;
	// rlwinm r9,r31,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r8,r1,-144
	ctx.r[8].s64 = ctx.r[1].s64 + -144;
	// li r10,2
	ctx.r[10].s64 = 2;
	// addi r11,r1,-144
	ctx.r[11].s64 = ctx.r[1].s64 + -144;
	// subf r9,r9,r8
	ctx.r[9].s64 = ctx.r[8].s64 - ctx.r[9].s64;
loc_822984D0:
	// cmpw cr6,r10,r31
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, ctx.r[31].s32, ctx.xer);
	// blt cr6,0x822984e4
	if (ctx.cr6.lt) goto loc_822984E4;
	// lwz r8,0(r9)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[9].u32 + 0);
	// stw r8,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[8].u32);
	// b 0x822984e8
	goto loc_822984E8;
loc_822984E4:
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
loc_822984E8:
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r[10].u32 > 0;
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// addi r9,r9,-4
	ctx.r[9].s64 = ctx.r[9].s64 + -4;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
	// bge 0x822984d0
	if (!ctx.cr0.lt) goto loc_822984D0;
	// addi r11,r26,1
	ctx.r[11].s64 = ctx.r[26].s64 + 1;
	// addi r28,r1,-152
	ctx.r[28].s64 = ctx.r[1].s64 + -152;
	// srawi r10,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[10].s64 = ctx.r[11].s32 >> 5;
	// addze r31,r10
	temp.s64 = ctx.r[10].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[10].u32;
	ctx.r[31].s64 = temp.s64;
	// srawi r10,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[10].s64 = ctx.r[11].s32 >> 5;
	// rlwinm r29,r31,2,0,29
	ctx.r[29].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 2) & 0xFFFFFFFC;
	// addze r10,r10
	temp.s64 = ctx.r[10].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[10].u32;
	ctx.r[10].s64 = temp.s64;
	// rlwinm r10,r10,5,0,26
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 5) & 0xFFFFFFE0;
	// subf r11,r10,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[10].s64;
	// lwzx r10,r29,r28
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[28].u32);
	// subfic r30,r11,31
	ctx.xer.ca = ctx.r[11].u32 <= 31;
	ctx.r[30].s64 = 31 - ctx.r[11].s64;
	// slw r11,r3,r30
	ctx.r[11].u64 = ctx.r[30].u8 & 0x20 ? 0 : (ctx.r[3].u32 << (ctx.r[30].u8 & 0x3F));
	// and. r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 & ctx.r[10].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8229861c
	if (ctx.cr0.eq) goto loc_8229861C;
	// addi r11,r1,-152
	ctx.r[11].s64 = ctx.r[1].s64 + -152;
	// rlwinm r9,r31,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 2) & 0xFFFFFFFC;
	// slw r10,r23,r30
	ctx.r[10].u64 = ctx.r[30].u8 & 0x20 ? 0 : (ctx.r[23].u32 << (ctx.r[30].u8 & 0x3F));
	// lwzx r11,r9,r11
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[9].u32 + ctx.r[11].u32);
	// andc. r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 & ~ctx.r[10].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82298580
	if (!ctx.cr0.eq) goto loc_82298580;
	// addi r11,r31,1
	ctx.r[11].s64 = ctx.r[31].s64 + 1;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// bge cr6,0x8229861c
	if (!ctx.cr6.lt) goto loc_8229861C;
	// rlwinm r10,r11,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
loc_82298560:
	// lwz r9,0(r10)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// bne cr6,0x82298580
	if (!ctx.cr6.eq) goto loc_82298580;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// blt cr6,0x82298560
	if (ctx.cr6.lt) goto loc_82298560;
	// b 0x8229861c
	goto loc_8229861C;
loc_82298580:
	// srawi r11,r26,5
	ctx.xer.ca = (ctx.r[26].s32 < 0) && ((ctx.r[26].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[26].s32 >> 5;
	// addi r7,r1,-152
	ctx.r[7].s64 = ctx.r[1].s64 + -152;
	// addze r9,r11
	temp.s64 = ctx.r[11].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[11].u32;
	ctx.r[9].s64 = temp.s64;
	// srawi r11,r26,5
	ctx.xer.ca = (ctx.r[26].s32 < 0) && ((ctx.r[26].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[26].s32 >> 5;
	// rlwinm r8,r9,2,0,29
	ctx.r[8].u64 = std::rotl(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 2) & 0xFFFFFFFC;
	// addze r11,r11
	temp.s64 = ctx.r[11].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[11].u32;
	ctx.r[11].s64 = temp.s64;
	// mr r5,r22
	ctx.r[5].u64 = ctx.r[22].u64;
	// rlwinm r11,r11,5,0,26
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 5) & 0xFFFFFFE0;
	// subf r11,r11,r26
	ctx.r[11].s64 = ctx.r[26].s64 - ctx.r[11].s64;
	// subfic r11,r11,31
	ctx.xer.ca = ctx.r[11].u32 <= 31;
	ctx.r[11].s64 = 31 - ctx.r[11].s64;
	// slw r6,r3,r11
	ctx.r[6].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[3].u32 << (ctx.r[11].u8 & 0x3F));
	// lwzx r11,r8,r7
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[8].u32 + ctx.r[7].u32);
	// add r10,r11,r6
	ctx.r[10].u64 = ctx.r[11].u64 + ctx.r[6].u64;
	// cmplw cr6,r10,r11
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x822985c4
	if (ctx.cr6.lt) goto loc_822985C4;
	// cmplw cr6,r10,r6
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[6].u32, ctx.xer);
	// bge cr6,0x822985c8
	if (!ctx.cr6.lt) goto loc_822985C8;
loc_822985C4:
	// mr r5,r3
	ctx.r[5].u64 = ctx.r[3].u64;
loc_822985C8:
	// addic. r11,r9,-1
	ctx.xer.ca = ctx.r[9].u32 > 0;
	ctx.r[11].s64 = ctx.r[9].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stwx r10,r8,r7
	PPC_STORE_U32(ctx.r[8].u32 + ctx.r[7].u32, ctx.r[10].u32);
	// blt 0x8229861c
	if (ctx.cr0.lt) goto loc_8229861C;
	// rlwinm r10,r11,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
loc_822985E0:
	// cmpwi cr6,r5,0
	ctx.cr6.compare<int32_t>(ctx.r[5].s32, 0, ctx.xer);
	// beq cr6,0x8229861c
	if (ctx.cr6.eq) goto loc_8229861C;
	// lwz r9,0(r10)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// mr r7,r22
	ctx.r[7].u64 = ctx.r[22].u64;
	// addi r8,r9,1
	ctx.r[8].s64 = ctx.r[9].s64 + 1;
	// cmplw cr6,r8,r9
	ctx.cr6.compare<uint32_t>(ctx.r[8].u32, ctx.r[9].u32, ctx.xer);
	// blt cr6,0x82298604
	if (ctx.cr6.lt) goto loc_82298604;
	// cmplwi cr6,r8,1
	ctx.cr6.compare<uint32_t>(ctx.r[8].u32, 1, ctx.xer);
	// bge cr6,0x82298608
	if (!ctx.cr6.lt) goto loc_82298608;
loc_82298604:
	// mr r7,r3
	ctx.r[7].u64 = ctx.r[3].u64;
loc_82298608:
	// stw r8,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[8].u32);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// mr r5,r7
	ctx.r[5].u64 = ctx.r[7].u64;
	// addi r10,r10,-4
	ctx.r[10].s64 = ctx.r[10].s64 + -4;
	// bge 0x822985e0
	if (!ctx.cr0.lt) goto loc_822985E0;
loc_8229861C:
	// lwzx r10,r29,r28
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[28].u32);
	// slw r9,r23,r30
	ctx.r[9].u64 = ctx.r[30].u8 & 0x20 ? 0 : (ctx.r[23].u32 << (ctx.r[30].u8 & 0x3F));
	// addi r11,r31,1
	ctx.r[11].s64 = ctx.r[31].s64 + 1;
	// and r10,r9,r10
	ctx.r[10].u64 = ctx.r[9].u64 & ctx.r[10].u64;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// stwx r10,r29,r28
	PPC_STORE_U32(ctx.r[29].u32 + ctx.r[28].u32, ctx.r[10].u32);
	// bge cr6,0x82298664
	if (!ctx.cr6.lt) goto loc_82298664;
	// rlwinm r10,r11,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// subfic r11,r11,3
	ctx.xer.ca = ctx.r[11].u32 <= 3;
	ctx.r[11].s64 = 3 - ctx.r[11].s64;
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// mr r9,r22
	ctx.r[9].u64 = ctx.r[22].u64;
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq 0x82298664
	if (ctx.cr0.eq) goto loc_82298664;
	// mtctr r11
	ctx.ctr.u64 = ctx.r[11].u64;
loc_82298658:
	// stw r9,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[9].u32);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// bdnz 0x82298658
	--ctx.ctr.u64;
	if (ctx.ctr.u32 != 0) goto loc_82298658;
loc_82298664:
	// addi r11,r21,1
	ctx.r[11].s64 = ctx.r[21].s64 + 1;
	// mr r8,r22
	ctx.r[8].u64 = ctx.r[22].u64;
	// srawi r7,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[7].s64 = ctx.r[11].s32 >> 5;
	// addi r10,r1,-152
	ctx.r[10].s64 = ctx.r[1].s64 + -152;
	// addze r3,r7
	temp.s64 = ctx.r[7].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[7].u32;
	ctx.r[3].s64 = temp.s64;
	// srawi r7,r11,5
	ctx.xer.ca = (ctx.r[11].s32 < 0) && ((ctx.r[11].u32 & 0x1F) != 0);
	ctx.r[7].s64 = ctx.r[11].s32 >> 5;
	// li r9,3
	ctx.r[9].s64 = 3;
	// addze r7,r7
	temp.s64 = ctx.r[7].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[7].u32;
	ctx.r[7].s64 = temp.s64;
	// rlwinm r7,r7,5,0,26
	ctx.r[7].u64 = std::rotl(ctx.r[7].u32 | (ctx.r[7].u64 << 32), 5) & 0xFFFFFFE0;
	// subf r11,r7,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[7].s64;
	// subfic r6,r11,32
	ctx.xer.ca = ctx.r[11].u32 <= 32;
	ctx.r[6].s64 = 32 - ctx.r[11].s64;
	// slw r7,r27,r11
	ctx.r[7].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[27].u32 << (ctx.r[11].u8 & 0x3F));
	// not r5,r7
	ctx.r[5].u64 = ~ctx.r[7].u64;
loc_82298698:
	// lwz r7,0(r10)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// addic. r9,r9,-1
	ctx.xer.ca = ctx.r[9].u32 > 0;
	ctx.r[9].s64 = ctx.r[9].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// and r31,r7,r5
	ctx.r[31].u64 = ctx.r[7].u64 & ctx.r[5].u64;
	// stw r31,-160(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -160, ctx.r[31].u32);
	// srw r7,r7,r11
	ctx.r[7].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[7].u32 >> (ctx.r[11].u8 & 0x3F));
	// or r8,r7,r8
	ctx.r[8].u64 = ctx.r[7].u64 | ctx.r[8].u64;
	// stw r8,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[8].u32);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// lwz r8,-160(r1)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -160);
	// slw r8,r8,r6
	ctx.r[8].u64 = ctx.r[6].u8 & 0x20 ? 0 : (ctx.r[8].u32 << (ctx.r[6].u8 & 0x3F));
	// bne 0x82298698
	if (!ctx.cr0.eq) goto loc_82298698;
	// rlwinm r9,r3,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r8,r1,-144
	ctx.r[8].s64 = ctx.r[1].s64 + -144;
	// li r10,2
	ctx.r[10].s64 = 2;
	// addi r11,r1,-144
	ctx.r[11].s64 = ctx.r[1].s64 + -144;
	// subf r9,r9,r8
	ctx.r[9].s64 = ctx.r[8].s64 - ctx.r[9].s64;
loc_822986D8:
	// cmpw cr6,r10,r3
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, ctx.r[3].s32, ctx.xer);
	// blt cr6,0x822986ec
	if (ctx.cr6.lt) goto loc_822986EC;
	// lwz r8,0(r9)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[9].u32 + 0);
	// stw r8,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[8].u32);
	// b 0x822986f0
	goto loc_822986F0;
loc_822986EC:
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
loc_822986F0:
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r[10].u32 > 0;
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// addi r9,r9,-4
	ctx.r[9].s64 = ctx.r[9].s64 + -4;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
	// bge 0x822986d8
	if (!ctx.cr0.lt) goto loc_822986D8;
	// mr r31,r22
	ctx.r[31].u64 = ctx.r[22].u64;
	// li r3,2
	ctx.r[3].s64 = 2;
	// b 0x82298890
	goto loc_82298890;
loc_8229870C:
	// lwz r29,0(r20)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[20].u32 + 0);
	// cmpw cr6,r30,r29
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, ctx.r[29].s32, ctx.xer);
	// blt cr6,0x822987dc
	if (ctx.cr6.lt) goto loc_822987DC;
	// srawi r10,r21,5
	ctx.xer.ca = (ctx.r[21].s32 < 0) && ((ctx.r[21].u32 & 0x1F) != 0);
	ctx.r[10].s64 = ctx.r[21].s32 >> 5;
	// addi r11,r1,-152
	ctx.r[11].s64 = ctx.r[1].s64 + -152;
	// addze r31,r10
	temp.s64 = ctx.r[10].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[10].u32;
	ctx.r[31].s64 = temp.s64;
	// srawi r10,r21,5
	ctx.xer.ca = (ctx.r[21].s32 < 0) && ((ctx.r[21].u32 & 0x1F) != 0);
	ctx.r[10].s64 = ctx.r[21].s32 >> 5;
	// li r6,-1
	ctx.r[6].s64 = -1;
	// addze r10,r10
	temp.s64 = ctx.r[10].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[10].u32;
	ctx.r[10].s64 = temp.s64;
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
	// mr r7,r22
	ctx.r[7].u64 = ctx.r[22].u64;
	// stw r22,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[22].u32);
	// rlwinm r10,r10,5,0,26
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 5) & 0xFFFFFFE0;
	// stw r22,8(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 8, ctx.r[22].u32);
	// lwz r11,-152(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -152);
	// subf r10,r10,r21
	ctx.r[10].s64 = ctx.r[21].s64 - ctx.r[10].s64;
	// addi r9,r1,-152
	ctx.r[9].s64 = ctx.r[1].s64 + -152;
	// oris r11,r11,32768
	ctx.r[11].u64 = ctx.r[11].u64 | 2147483648;
	// li r8,3
	ctx.r[8].s64 = 3;
	// stw r11,-152(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -152, ctx.r[11].u32);
	// slw r11,r6,r10
	ctx.r[11].u64 = ctx.r[10].u8 & 0x20 ? 0 : (ctx.r[6].u32 << (ctx.r[10].u8 & 0x3F));
	// subfic r6,r10,32
	ctx.xer.ca = ctx.r[10].u32 <= 32;
	ctx.r[6].s64 = 32 - ctx.r[10].s64;
	// not r5,r11
	ctx.r[5].u64 = ~ctx.r[11].u64;
loc_82298768:
	// lwz r11,0(r9)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[9].u32 + 0);
	// addic. r8,r8,-1
	ctx.xer.ca = ctx.r[8].u32 > 0;
	ctx.r[8].s64 = ctx.r[8].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[8].s32, 0, ctx.xer);
	// and r30,r11,r5
	ctx.r[30].u64 = ctx.r[11].u64 & ctx.r[5].u64;
	// stw r30,-160(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -160, ctx.r[30].u32);
	// srw r11,r11,r10
	ctx.r[11].u64 = ctx.r[10].u8 & 0x20 ? 0 : (ctx.r[11].u32 >> (ctx.r[10].u8 & 0x3F));
	// or r11,r11,r7
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[7].u64;
	// stw r11,0(r9)
	PPC_STORE_U32(ctx.r[9].u32 + 0, ctx.r[11].u32);
	// addi r9,r9,4
	ctx.r[9].s64 = ctx.r[9].s64 + 4;
	// lwz r11,-160(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -160);
	// slw r7,r11,r6
	ctx.r[7].u64 = ctx.r[6].u8 & 0x20 ? 0 : (ctx.r[11].u32 << (ctx.r[6].u8 & 0x3F));
	// bne 0x82298768
	if (!ctx.cr0.eq) goto loc_82298768;
	// rlwinm r9,r31,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r8,r1,-144
	ctx.r[8].s64 = ctx.r[1].s64 + -144;
	// li r10,2
	ctx.r[10].s64 = 2;
	// addi r11,r1,-144
	ctx.r[11].s64 = ctx.r[1].s64 + -144;
	// subf r9,r9,r8
	ctx.r[9].s64 = ctx.r[8].s64 - ctx.r[9].s64;
loc_822987A8:
	// cmpw cr6,r10,r31
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, ctx.r[31].s32, ctx.xer);
	// blt cr6,0x822987bc
	if (ctx.cr6.lt) goto loc_822987BC;
	// lwz r8,0(r9)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[9].u32 + 0);
	// stw r8,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[8].u32);
	// b 0x822987c0
	goto loc_822987C0;
loc_822987BC:
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
loc_822987C0:
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r[10].u32 > 0;
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// addi r9,r9,-4
	ctx.r[9].s64 = ctx.r[9].s64 + -4;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
	// bge 0x822987a8
	if (!ctx.cr0.lt) goto loc_822987A8;
	// lwz r11,20(r20)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[20].u32 + 20);
	// add r31,r11,r29
	ctx.r[31].u64 = ctx.r[11].u64 + ctx.r[29].u64;
	// b 0x82298890
	goto loc_82298890;
loc_822987DC:
	// srawi r11,r21,5
	ctx.xer.ca = (ctx.r[21].s32 < 0) && ((ctx.r[21].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[21].s32 >> 5;
	// li r7,-1
	ctx.r[7].s64 = -1;
	// addze r3,r11
	temp.s64 = ctx.r[11].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[11].u32;
	ctx.r[3].s64 = temp.s64;
	// lwz r11,20(r20)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[20].u32 + 20);
	// srawi r10,r21,5
	ctx.xer.ca = (ctx.r[21].s32 < 0) && ((ctx.r[21].u32 & 0x1F) != 0);
	ctx.r[10].s64 = ctx.r[21].s32 >> 5;
	// add r31,r11,r30
	ctx.r[31].u64 = ctx.r[11].u64 + ctx.r[30].u64;
	// addze r11,r10
	temp.s64 = ctx.r[10].s64 + ctx.xer.ca;
	ctx.xer.ca = temp.u32 < ctx.r[10].u32;
	ctx.r[11].s64 = temp.s64;
	// lwz r10,-152(r1)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -152);
	// mr r8,r22
	ctx.r[8].u64 = ctx.r[22].u64;
	// rlwinm r11,r11,5,0,26
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 5) & 0xFFFFFFE0;
	// clrlwi r10,r10,1
	ctx.r[10].u64 = ctx.r[10].u32 & 0x7FFFFFFF;
	// subf r11,r11,r21
	ctx.r[11].s64 = ctx.r[21].s64 - ctx.r[11].s64;
	// li r9,3
	ctx.r[9].s64 = 3;
	// subfic r6,r11,32
	ctx.xer.ca = ctx.r[11].u32 <= 32;
	ctx.r[6].s64 = 32 - ctx.r[11].s64;
	// stw r10,-152(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -152, ctx.r[10].u32);
	// addi r10,r1,-152
	ctx.r[10].s64 = ctx.r[1].s64 + -152;
	// slw r7,r7,r11
	ctx.r[7].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[7].u32 << (ctx.r[11].u8 & 0x3F));
	// not r5,r7
	ctx.r[5].u64 = ~ctx.r[7].u64;
loc_82298824:
	// lwz r7,0(r10)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// addic. r9,r9,-1
	ctx.xer.ca = ctx.r[9].u32 > 0;
	ctx.r[9].s64 = ctx.r[9].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// and r30,r7,r5
	ctx.r[30].u64 = ctx.r[7].u64 & ctx.r[5].u64;
	// stw r30,-160(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -160, ctx.r[30].u32);
	// srw r7,r7,r11
	ctx.r[7].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[7].u32 >> (ctx.r[11].u8 & 0x3F));
	// or r8,r7,r8
	ctx.r[8].u64 = ctx.r[7].u64 | ctx.r[8].u64;
	// stw r8,0(r10)
	PPC_STORE_U32(ctx.r[10].u32 + 0, ctx.r[8].u32);
	// addi r10,r10,4
	ctx.r[10].s64 = ctx.r[10].s64 + 4;
	// lwz r8,-160(r1)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -160);
	// slw r8,r8,r6
	ctx.r[8].u64 = ctx.r[6].u8 & 0x20 ? 0 : (ctx.r[8].u32 << (ctx.r[6].u8 & 0x3F));
	// bne 0x82298824
	if (!ctx.cr0.eq) goto loc_82298824;
	// rlwinm r9,r3,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r8,r1,-144
	ctx.r[8].s64 = ctx.r[1].s64 + -144;
	// li r10,2
	ctx.r[10].s64 = 2;
	// addi r11,r1,-144
	ctx.r[11].s64 = ctx.r[1].s64 + -144;
	// subf r9,r9,r8
	ctx.r[9].s64 = ctx.r[8].s64 - ctx.r[9].s64;
loc_82298864:
	// cmpw cr6,r10,r3
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, ctx.r[3].s32, ctx.xer);
	// blt cr6,0x82298878
	if (ctx.cr6.lt) goto loc_82298878;
	// lwz r8,0(r9)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[9].u32 + 0);
	// stw r8,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[8].u32);
	// b 0x8229887c
	goto loc_8229887C;
loc_82298878:
	// stw r22,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[22].u32);
loc_8229887C:
	// addic. r10,r10,-1
	ctx.xer.ca = ctx.r[10].u32 > 0;
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// addi r9,r9,-4
	ctx.r[9].s64 = ctx.r[9].s64 + -4;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
	// bge 0x82298864
	if (!ctx.cr0.lt) goto loc_82298864;
loc_8229888C:
	// mr r3,r22
	ctx.r[3].u64 = ctx.r[22].u64;
loc_82298890:
	// subfic r11,r21,31
	ctx.xer.ca = ctx.r[21].u32 <= 31;
	ctx.r[11].s64 = 31 - ctx.r[21].s64;
	// cmpwi cr6,r19,0
	ctx.cr6.compare<int32_t>(ctx.r[19].s32, 0, ctx.xer);
	// lis r10,-32768
	ctx.r[10].s64 = -2147483648;
	// bne cr6,0x822988a4
	if (!ctx.cr6.eq) goto loc_822988A4;
	// mr r10,r22
	ctx.r[10].u64 = ctx.r[22].u64;
loc_822988A4:
	// slw r9,r31,r11
	ctx.r[9].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[31].u32 << (ctx.r[11].u8 & 0x3F));
	// lwz r11,16(r20)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[20].u32 + 16);
	// or r10,r9,r10
	ctx.r[10].u64 = ctx.r[9].u64 | ctx.r[10].u64;
	// lwz r9,-152(r1)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -152);
	// cmpwi cr6,r11,64
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 64, ctx.xer);
	// or r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 | ctx.r[9].u64;
	// bne cr6,0x822988cc
	if (!ctx.cr6.eq) goto loc_822988CC;
	// lwz r11,-148(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -148);
	// stw r11,4(r4)
	PPC_STORE_U32(ctx.r[4].u32 + 4, ctx.r[11].u32);
	// b 0x822988d4
	goto loc_822988D4;
loc_822988CC:
	// cmpwi cr6,r11,32
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 32, ctx.xer);
	// bne cr6,0x822988d8
	if (!ctx.cr6.eq) goto loc_822988D8;
loc_822988D4:
	// stw r10,0(r4)
	PPC_STORE_U32(ctx.r[4].u32 + 0, ctx.r[10].u32);
loc_822988D8:
	// b 0x82b7a714
	RestoreGprs(memory, ctx);
	FromContext(ctx, registers);
	return;
}
#undef PPC_LOAD_U16
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    if (entry != 0x822981c8u)
        return false;
    Convert(memory, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_float_binary_conversion
