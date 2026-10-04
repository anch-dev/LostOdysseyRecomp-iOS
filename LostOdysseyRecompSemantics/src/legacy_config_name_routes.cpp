#include "lo_semantics/legacy_config_name_routes.h"

#include "lo_semantics/legacy_token_lookup_flags.h"
#include "lo_semantics/manager_metadata_compare.h"
#include "lo_semantics/manager_metadata_parsing.h"
#include "lo_semantics/metadata_name_index.h"
#include "lo_semantics/metadata_name_lookup.h"
#include "lo_semantics/metadata_name_registry.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/registered_metadata_string.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_config_name_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

union Value
{
    std::uint64_t u64;
    std::int64_t s64;
    std::uint32_t u32;
    std::int32_t s32;
    std::uint16_t u16;
};

struct Xer { std::uint8_t so = 0, ca = 0; };
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    template<class T> void compare(T left, T right, const Xer& xer)
    {
        lt = std::uint8_t(left < right);
        gt = std::uint8_t(left > right);
        eq = std::uint8_t(left == right);
        so = xer.so;
    }
};

struct Context
{
    Value r[32]{};
    std::uint64_t lr = 0, ctr = 0;
    Xer xer{};
    Condition cr0{}, cr6{};
};

Context ToContext(const Registers& state)
{
    Context context{};
    for (unsigned i = 0; i < 32; ++i)
        context.r[i].u64 = state.r[i];
    context.r[1].u64 = state.sp;
    context.lr = state.lr;
    context.ctr = state.ctr;
    context.xer = {state.xer_so, state.xer_ca};
    context.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq,
        state.cr0.so};
    context.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq,
        state.cr6.so};
    return context;
}

Registers ToRegisters(const Context& context)
{
    Registers state{};
    for (unsigned i = 0; i < 32; ++i)
        state.r[i] = context.r[i].u64;
    state.sp = context.r[1].u64;
    state.lr = context.lr;
    state.ctr = context.ctr;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    state.cr0 = {context.cr0.lt, context.cr0.gt, context.cr0.eq,
        context.cr0.so};
    state.cr6 = {context.cr6.lt, context.cr6.gt, context.cr6.eq,
        context.cr6.so};
    return state;
}

void Save25(Context& context, GuestMemory& memory)
{
    for (unsigned reg = 25; reg <= 31; ++reg)
        WriteU64(memory, Address(context.r[1].u64 -
            8u * (33u - reg)), context.r[reg].u64);
    memory.WriteU32(Address(context.r[1].u64 - 8u),
        Address(context.r[12].u64));
}

void Restore25(Context& context, GuestMemory& memory)
{
    for (unsigned reg = 25; reg <= 31; ++reg)
        context.r[reg].u64 = ReadU64(memory, Address(context.r[1].u64 -
            8u * (33u - reg)));
    context.r[12].u64 = memory.ReadU32(Address(context.r[1].u64 - 8u));
    context.lr = context.r[12].u64;
}

void HashVolatileBytes(GuestMemory& memory, std::uint64_t source,
    Registers& state)
{
    constexpr GuestAddress table = 0x832ee168u;
    auto address = Address(source);
    auto unit = memory.ReadU16(address);
    std::uint32_t crc = 0u;
    while (unit != 0u)
    {
        metadata_name_index::FrameRegisters fold_frame{};
        const auto folded = static_cast<std::uint32_t>(
            metadata_name_index::FoldUtf16(memory, unit, fold_frame));
        auto carry = std::rotl(crc, 24) & 0xffffffu;
        const auto first_index = ((folded ^ crc) << 2u) & 0x3fcu;
        carry ^= memory.ReadU32(table + first_index);
        const auto high_byte = std::rotl(folded, 24) & 0xffu;
        const auto second_index = ((high_byte ^ carry) << 2u) & 0x3fcu;
        const auto second_carry = std::rotl(carry, 24) & 0xffffffu;
        const auto second_word = memory.ReadU32(table + second_index);
        state.r[8] = high_byte;
        state.r[9] = second_word;
        crc = second_word ^ second_carry;
        address += 2u;
        unit = memory.ReadU16(address);
    }
}

void CallLower(GuestAddress entry, Context& context, GuestMemory& memory,
    Dependencies dependencies)
{
    auto state = ToRegisters(context);
    bool handled = false;
    switch (entry)
    {
    case 0x822972a8u:
        handled = legacy_token_lookup_flags::Apply(entry, memory, state);
        break;
    case 0x82296e80u:
    {
        const auto source = state.r[3];
        manager_metadata_parsing::FrameRegisters frame{};
        frame.lr = state.lr;
        frame.r13 = state.r[13];
        frame.r8 = state.r[8];
        frame.r9 = state.r[9];
        for (unsigned i = 0; i < 9; ++i)
            frame.r23_through_r31[i] = state.r[i + 23];
        std::uint64_t result = 0;
        handled = manager_metadata_parsing::Apply(entry, memory,
            dependencies.thread, dependencies.invalid, state.r[3],
            state.r[4], state.r[5], state.r[6], state.r[7], state.sp,
            frame, result);
        state.r[3] = result;
        // 82296E80 computes the full-width address of the last UTF-16 unit
        // into r10 before rejecting a name without a numeric suffix. The
        // accepted parser interface exposes the result but not this scratch.
        if (result == 0u)
        {
            state.r[10] = source + 2u *
                registered_metadata_string::Utf16Length(memory, source) - 2u;
            // 82296830's terminal NUL comparison leaves CR0.eq set; its
            // positive even-byte srawi clears XER.CA. The no-suffix branch
            // of 82296E80 does not touch either field before returning.
            state.cr0 = {0u, 0u, 1u, state.xer_so};
            state.xer_ca = 0u;
        }
        state.r[13] = frame.r13;
        state.r[8] = frame.r8;
        state.r[9] = frame.r9;
        for (unsigned i = 0; i < 9; ++i)
            state.r[i + 23] = frame.r23_through_r31[i];
        state.lr = frame.lr;
        state.r[12] = frame.lr;
        break;
    }
    case 0x82296f68u:
    {
        const auto source = state.r[3];
        metadata_name_index::FrameRegisters frame{};
        frame.lr = state.lr;
        frame.r27 = state.r[27];
        frame.r28 = state.r[28];
        frame.r29 = state.r[29];
        frame.r30 = state.r[30];
        frame.r31 = state.r[31];
        frame.r0 = state.r[0];
        frame.ctr = state.ctr;
        state.r[3] = metadata_name_index::HashName(memory, state.r[3],
            state.sp, frame);
        state.lr = frame.lr;
        state.r[27] = frame.r27;
        state.r[28] = frame.r28;
        state.r[29] = frame.r29;
        state.r[30] = frame.r30;
        state.r[31] = frame.r31;
        state.r[0] = frame.r0;
        state.ctr = frame.ctr;
        state.r[12] = frame.lr;
        // The accepted hash API exposes its result and frame but not the two
        // last table-lookup scratch bytes, which remain live in this caller.
        HashVolatileBytes(memory, source, state);
        handled = true;
        break;
    }
    case 0x822971e0u:
    {
        InvalidParameterCall call{};
        for (unsigned i = 0; i < 8; ++i)
            call.arguments[i] = state.r[i + 3];
        call.thread_environment = state.r[13];
        std::uint64_t result = 0;
        handled = manager_metadata_compare::Apply(entry, memory,
            dependencies.thread, dependencies.invalid, call, state.sp,
            state.lr, result);
        for (unsigned i = 0; i < 8; ++i)
            state.r[i + 3] = call.arguments[i];
        state.r[3] = result;
        state.r[13] = call.thread_environment;
        state.r[12] = state.lr;
        break;
    }
    case 0x823f4700u:
    {
        metadata_name_registry::FrameRegisters frame{};
        frame.lr = state.lr;
        frame.r27 = state.r[27];
        frame.r28 = state.r[28];
        frame.r29 = state.r[29];
        frame.r30 = state.r[30];
        frame.r31 = state.r[31];
        std::uint64_t result = 0;
        handled = metadata_name_registry::Apply(entry, memory,
            dependencies.records, dependencies.arrays, state.sp,
            frame, result);
        state.r[3] = result;
        state.lr = frame.lr;
        state.r[12] = frame.lr;
        state.r[27] = frame.r27;
        state.r[28] = frame.r28;
        state.r[29] = frame.r29;
        state.r[30] = frame.r30;
        state.r[31] = frame.r31;
        break;
    }
    case 0x82296d30u:
    {
        metadata_name_lookup::FrameRegisters frame{};
        frame.lr = state.lr;
        frame.r13 = state.r[13];
        frame.r23 = state.r[23]; frame.r24 = state.r[24];
        frame.r25 = state.r[25]; frame.r26 = state.r[26];
        frame.r27 = state.r[27]; frame.r28 = state.r[28];
        frame.r29 = state.r[29]; frame.r30 = state.r[30];
        frame.r31 = state.r[31];
        frame.r0 = state.r[0]; frame.ctr = state.ctr;
        frame.r8 = state.r[8]; frame.r9 = state.r[9];
        frame.r10 = state.r[10];
        std::uint64_t result = 0;
        handled = metadata_name_lookup::Apply(entry, memory,
            dependencies.records, dependencies.arrays, dependencies.thread,
            dependencies.invalid, state.r[3], state.r[4], state.r[5],
            state.r[6], state.r[7], state.sp, frame, result);
        state.r[3] = result;
        state.lr = frame.lr;
        state.r[13] = frame.r13;
        state.r[23] = frame.r23; state.r[24] = frame.r24;
        state.r[25] = frame.r25; state.r[26] = frame.r26;
        state.r[27] = frame.r27; state.r[28] = frame.r28;
        state.r[29] = frame.r29; state.r[30] = frame.r30;
        state.r[31] = frame.r31;
        state.r[0] = frame.r0; state.ctr = frame.ctr;
        state.r[8] = frame.r8; state.r[9] = frame.r9;
        state.r[10] = frame.r10;
        state.r[12] = frame.lr;
        break;
    }
    case 0x8240bca8u:
    case 0x8240c400u:
    {
        std::uint64_t result = 0;
        handled = registered_constructor_family::Apply(entry, memory,
            dependencies.manager, dependencies.registration, state.r[3],
            Address(state.sp), result);
        state.r[3] = result;
        state.r[12] = state.lr;
        break;
    }
    case 0x82713cf8u:
    {
        manager_index_operations::FrameRegisters frame{};
        frame.lr = state.lr;
        frame.r28 = state.r[28]; frame.r29 = state.r[29];
        frame.r30 = state.r[30]; frame.r31 = state.r[31];
        std::uint64_t result = 0;
        handled = manager_index_operations::Apply(entry, memory,
            dependencies.manager, dependencies.arrays, dependencies.fp,
            state.r[3], state.r[4], state.r[5], 0u, state.sp,
            frame, result);
        state.r[3] = result;
        state.lr = frame.lr;
        state.r[12] = frame.lr;
        state.r[28] = frame.r28; state.r[29] = frame.r29;
        state.r[30] = frame.r30; state.r[31] = frame.r31;
        break;
    }
    default: throw std::logic_error("unknown config name lower");
    }
    if (!handled) throw std::logic_error("accepted config name lower unavailable");
    context = ToContext(state);
}

#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(memory, Address(a))
#define PPC_STORE_U32(a, v) memory.WriteU32(Address(a), static_cast<std::uint32_t>(v))
#define PPC_STORE_U64(a, v) WriteU64(memory, Address(a), static_cast<std::uint64_t>(v))

void Execute82296B00(GuestMemory& memory, Dependencies dependencies,
    Context& context)
{
    auto& ctx = context;
    Value temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x82296B08;
	Save25(ctx, memory);
	// stwu r1,-432(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-432);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lis r11,-31964
	ctx.r[11].s64 = -2094792704;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// li r26,0
	ctx.r[26].s64 = 0;
	// lwz r11,25184(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 25184);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82296b2c
	if (!ctx.cr6.eq) goto loc_82296B2C;
	// bl 0x823f4700
	ctx.lr = 0x82296B2C;
	CallLower(0x823f4700u, ctx, memory, dependencies);
loc_82296B2C:
	// addi r6,r1,80
	ctx.r[6].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(80);
	// li r5,128
	ctx.r[5].s64 = 128;
	// addi r4,r1,112
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(112);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82296e80
	ctx.lr = 0x82296B40;
	CallLower(0x82296e80u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x82296b54
	if (ctx.cr6.eq) goto loc_82296B54;
	// lwz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// addi r30,r1,112
	ctx.r[30].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(112);
	// addi r26,r11,1
	ctx.r[26].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(1);
loc_82296B54:
	// lhz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[30].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82296bc8
	if (ctx.cr6.eq) goto loc_82296BC8;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// stw r26,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[26].u32);
	// bl 0x82296f68
	ctx.lr = 0x82296B6C;
	CallLower(0x82296f68u, ctx, memory, dependencies);
	// lis r11,-31953
	ctx.r[11].s64 = -2094071808;
	// rlwinm r10,r3,2,18,29
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 2) & 0x3FFC;
	// addi r11,r11,-6808
	ctx.r[11].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-6808);
	// lwzx r31,r10,r11
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[11].u32);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x82296bc8
	if (ctx.cr6.eq) goto loc_82296BC8;
loc_82296B84:
	// addi r4,r31,16
	ctx.r[4].u64 = ctx.r[31].u64 + static_cast<std::uint64_t>(16);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x822971e0
	ctx.lr = 0x82296B90;
	CallLower(0x822971e0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x82296bb0
	if (ctx.cr6.eq) goto loc_82296BB0;
	// lwz r31,12(r31)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82296b84
	if (!ctx.cr6.eq) goto loc_82296B84;
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
loc_82296BB0:
	// lwz r28,0(r31)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// cmpwi cr6,r28,0
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 0, ctx.xer);
	// stw r28,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[28].u32);
	// bne cr6,0x82296bd4
	if (!ctx.cr6.eq) goto loc_82296BD4;
	// cmpwi cr6,r26,0
	ctx.cr6.compare<int32_t>(ctx.r[26].s32, 0, ctx.xer);
	// bne cr6,0x82296bd4
	if (!ctx.cr6.eq) goto loc_82296BD4;
loc_82296BC8:
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
loc_82296BD4:
	// addi r27,r29,108
	ctx.r[27].u64 = ctx.r[29].u64 + static_cast<std::uint64_t>(108);
	// addi r4,r1,80
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(80);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x822972a8
	ctx.lr = 0x82296BE4;
	CallLower(0x822972a8u, ctx, memory, dependencies);
	// mr r25,r3
	ctx.r[25].u64 = ctx.r[3].u64;
	// cmplwi cr6,r25,0
	ctx.cr6.compare<uint32_t>(ctx.r[25].u32, 0, ctx.xer);
	// bne cr6,0x82296cc0
	if (!ctx.cr6.eq) goto loc_82296CC0;
	// mr r30,r29
	ctx.r[30].u64 = ctx.r[29].u64;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82296cc0
	if (ctx.cr6.eq) goto loc_82296CC0;
	// lis r11,-32231
	ctx.r[11].s64 = -2112290816;
	// addi r29,r11,-10128
	ctx.r[29].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-10128);
loc_82296C04:
	// lwz r11,52(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 52);
	// lwz r31,112(r11)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 112);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x82296cb4
	if (ctx.cr6.eq) goto loc_82296CB4;
loc_82296C14:
	// ld r11,76(r31)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[31].u32 + 76);
	// rlwinm r11,r11,0,29,29
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	// cmpldi cr6,r11,0
	ctx.cr6.compare<uint64_t>(ctx.r[11].u64, 0, ctx.xer);
	// beq cr6,0x82296ca8
	if (ctx.cr6.eq) goto loc_82296CA8;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, -1, ctx.xer);
	// beq cr6,0x82296c38
	if (ctx.cr6.eq) goto loc_82296C38;
	// addi r11,r31,44
	ctx.r[11].u64 = ctx.r[31].u64 + static_cast<std::uint64_t>(44);
	// b 0x82296c54
	goto loc_82296C54;
loc_82296C38:
	// li r7,1
	ctx.r[7].s64 = 1;
	// li r6,1
	ctx.r[6].s64 = 1;
	// li r5,0
	ctx.r[5].s64 = 0;
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// addi r3,r1,96
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
	// bl 0x82296d30
	ctx.lr = 0x82296C50;
	CallLower(0x82296d30u, ctx, memory, dependencies);
	// addi r11,r1,96
	ctx.r[11].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
loc_82296C54:
	// ld r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[11].u32 + 0);
	// std r11,88(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 88, ctx.r[11].u64);
	// lwz r11,88(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// cmpw cr6,r11,r28
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[28].s32, ctx.xer);
	// bne cr6,0x82296ca8
	if (!ctx.cr6.eq) goto loc_82296CA8;
	// lwz r11,92(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
	// cmpw cr6,r11,r26
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[26].s32, ctx.xer);
	// bne cr6,0x82296ca8
	if (!ctx.cr6.eq) goto loc_82296CA8;
	// bl 0x8240bca8
	ctx.lr = 0x82296C78;
	CallLower(0x8240bca8u, ctx, memory, dependencies);
	// lwz r11,52(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 52);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82296c98
	if (ctx.cr6.eq) goto loc_82296C98;
loc_82296C84:
	// cmplw cr6,r11,r3
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[3].u32, ctx.xer);
	// beq cr6,0x82296ccc
	if (ctx.cr6.eq) goto loc_82296CCC;
	// lwz r11,60(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 60);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82296c84
	if (!ctx.cr6.eq) goto loc_82296C84;
loc_82296C98:
	// cntlzw r11,r3
	ctx.r[11].u64 = ctx.r[3].u32 == 0 ? 32 : std::countl_zero(ctx.r[3].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82296ccc
	if (!ctx.cr6.eq) goto loc_82296CCC;
loc_82296CA8:
	// lwz r31,104(r31)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 104);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82296c14
	if (!ctx.cr6.eq) goto loc_82296C14;
loc_82296CB4:
	// lwz r30,40(r30)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 40);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// bne cr6,0x82296c04
	if (!ctx.cr6.eq) goto loc_82296C04;
loc_82296CC0:
	// mr r3,r25
	ctx.r[3].u64 = ctx.r[25].u64;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
loc_82296CCC:
	// lwz r11,100(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 100);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// ld r4,80(r1)
	ctx.r[4].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 80);
	// add r31,r11,r30
	ctx.r[31].u64 = ctx.r[11].u64 + ctx.r[30].u64;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// bl 0x82713cf8
	ctx.lr = 0x82296CE4;
	CallLower(0x82713cf8u, ctx, memory, dependencies);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
}

void Execute82713828(GuestMemory& memory, Dependencies dependencies,
    Context& context)
{
    auto& ctx = context;
    Value temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x82713830;
	Save25(ctx, memory);
	// stwu r1,-432(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-432);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lis r11,-31964
	ctx.r[11].s64 = -2094792704;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// li r26,0
	ctx.r[26].s64 = 0;
	// lwz r11,25184(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 25184);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82713854
	if (!ctx.cr6.eq) goto loc_82713854;
	// bl 0x823f4700
	ctx.lr = 0x82713854;
	CallLower(0x823f4700u, ctx, memory, dependencies);
loc_82713854:
	// addi r6,r1,80
	ctx.r[6].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(80);
	// li r5,128
	ctx.r[5].s64 = 128;
	// addi r4,r1,112
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(112);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82296e80
	ctx.lr = 0x82713868;
	CallLower(0x82296e80u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x8271387c
	if (ctx.cr6.eq) goto loc_8271387C;
	// lwz r11,80(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// addi r30,r1,112
	ctx.r[30].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(112);
	// addi r26,r11,1
	ctx.r[26].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(1);
loc_8271387C:
	// lhz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[30].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x827138f0
	if (ctx.cr6.eq) goto loc_827138F0;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// stw r26,84(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 84, ctx.r[26].u32);
	// bl 0x82296f68
	ctx.lr = 0x82713894;
	CallLower(0x82296f68u, ctx, memory, dependencies);
	// lis r11,-31953
	ctx.r[11].s64 = -2094071808;
	// rlwinm r10,r3,2,18,29
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 2) & 0x3FFC;
	// addi r11,r11,-6808
	ctx.r[11].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-6808);
	// lwzx r31,r10,r11
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[11].u32);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x827138f0
	if (ctx.cr6.eq) goto loc_827138F0;
loc_827138AC:
	// addi r4,r31,16
	ctx.r[4].u64 = ctx.r[31].u64 + static_cast<std::uint64_t>(16);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x822971e0
	ctx.lr = 0x827138B8;
	CallLower(0x822971e0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x827138d8
	if (ctx.cr6.eq) goto loc_827138D8;
	// lwz r31,12(r31)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x827138ac
	if (!ctx.cr6.eq) goto loc_827138AC;
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
loc_827138D8:
	// lwz r28,0(r31)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// cmpwi cr6,r28,0
	ctx.cr6.compare<int32_t>(ctx.r[28].s32, 0, ctx.xer);
	// stw r28,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[28].u32);
	// bne cr6,0x827138fc
	if (!ctx.cr6.eq) goto loc_827138FC;
	// cmpwi cr6,r26,0
	ctx.cr6.compare<int32_t>(ctx.r[26].s32, 0, ctx.xer);
	// bne cr6,0x827138fc
	if (!ctx.cr6.eq) goto loc_827138FC;
loc_827138F0:
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
loc_827138FC:
	// addi r27,r29,108
	ctx.r[27].u64 = ctx.r[29].u64 + static_cast<std::uint64_t>(108);
	// addi r4,r1,80
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(80);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x822972a8
	ctx.lr = 0x8271390C;
	CallLower(0x822972a8u, ctx, memory, dependencies);
	// mr r25,r3
	ctx.r[25].u64 = ctx.r[3].u64;
	// cmplwi cr6,r25,0
	ctx.cr6.compare<uint32_t>(ctx.r[25].u32, 0, ctx.xer);
	// bne cr6,0x827139e8
	if (!ctx.cr6.eq) goto loc_827139E8;
	// mr r30,r29
	ctx.r[30].u64 = ctx.r[29].u64;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x827139e8
	if (ctx.cr6.eq) goto loc_827139E8;
	// lis r11,-32231
	ctx.r[11].s64 = -2112290816;
	// addi r29,r11,-10128
	ctx.r[29].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-10128);
loc_8271392C:
	// lwz r11,52(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 52);
	// lwz r31,112(r11)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 112);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x827139dc
	if (ctx.cr6.eq) goto loc_827139DC;
loc_8271393C:
	// ld r11,76(r31)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[31].u32 + 76);
	// rlwinm r11,r11,0,29,29
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	// cmpldi cr6,r11,0
	ctx.cr6.compare<uint64_t>(ctx.r[11].u64, 0, ctx.xer);
	// beq cr6,0x827139d0
	if (ctx.cr6.eq) goto loc_827139D0;
	// lwz r11,4(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, -1, ctx.xer);
	// beq cr6,0x82713960
	if (ctx.cr6.eq) goto loc_82713960;
	// addi r11,r31,44
	ctx.r[11].u64 = ctx.r[31].u64 + static_cast<std::uint64_t>(44);
	// b 0x8271397c
	goto loc_8271397C;
loc_82713960:
	// li r7,1
	ctx.r[7].s64 = 1;
	// li r6,1
	ctx.r[6].s64 = 1;
	// li r5,0
	ctx.r[5].s64 = 0;
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// addi r3,r1,96
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
	// bl 0x82296d30
	ctx.lr = 0x82713978;
	CallLower(0x82296d30u, ctx, memory, dependencies);
	// addi r11,r1,96
	ctx.r[11].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
loc_8271397C:
	// ld r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U64(ctx.r[11].u32 + 0);
	// std r11,88(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 88, ctx.r[11].u64);
	// lwz r11,88(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 88);
	// cmpw cr6,r11,r28
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[28].s32, ctx.xer);
	// bne cr6,0x827139d0
	if (!ctx.cr6.eq) goto loc_827139D0;
	// lwz r11,92(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 92);
	// cmpw cr6,r11,r26
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, ctx.r[26].s32, ctx.xer);
	// bne cr6,0x827139d0
	if (!ctx.cr6.eq) goto loc_827139D0;
	// bl 0x8240c400
	ctx.lr = 0x827139A0;
	CallLower(0x8240c400u, ctx, memory, dependencies);
	// lwz r11,52(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 52);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x827139c0
	if (ctx.cr6.eq) goto loc_827139C0;
loc_827139AC:
	// cmplw cr6,r11,r3
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[3].u32, ctx.xer);
	// beq cr6,0x827139f4
	if (ctx.cr6.eq) goto loc_827139F4;
	// lwz r11,60(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 60);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x827139ac
	if (!ctx.cr6.eq) goto loc_827139AC;
loc_827139C0:
	// cntlzw r11,r3
	ctx.r[11].u64 = ctx.r[3].u32 == 0 ? 32 : std::countl_zero(ctx.r[3].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x827139f4
	if (!ctx.cr6.eq) goto loc_827139F4;
loc_827139D0:
	// lwz r31,104(r31)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 104);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x8271393c
	if (!ctx.cr6.eq) goto loc_8271393C;
loc_827139DC:
	// lwz r30,40(r30)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 40);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// bne cr6,0x8271392c
	if (!ctx.cr6.eq) goto loc_8271392C;
loc_827139E8:
	// mr r3,r25
	ctx.r[3].u64 = ctx.r[25].u64;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
loc_827139F4:
	// lwz r11,100(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 100);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// ld r4,80(r1)
	ctx.r[4].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 80);
	// add r31,r11,r30
	ctx.r[31].u64 = ctx.r[11].u64 + ctx.r[30].u64;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// bl 0x82713cf8
	ctx.lr = 0x82713A0C;
	CallLower(0x82713cf8u, ctx, memory, dependencies);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// addi r1,r1,432
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(432);
	// b 0x82b7a72c
	Restore25(ctx, memory);
	return;
}

#undef PPC_LOAD_U16
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x82296b00u && entry != 0x82713828u) return false;
    auto context = ToContext(registers);
    if (entry == 0x82296b00u)
        Execute82296B00(memory, dependencies, context);
    else Execute82713828(memory, dependencies, context);
    registers = ToRegisters(context);
    return true;
}

bool ApplyParsingLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x82296e80u && entry != 0x82296f68u && entry != 0x822971e0u)
        return false;
    auto context = ToContext(registers);
    CallLower(entry, context, memory, dependencies);
    registers = ToRegisters(context);
    return true;
}
} // namespace lo::semantic::gpu::legacy_config_name_routes
