#include "lo_semantics/legacy_config_command_dispatch.h"

#include "lo_semantics/legacy_character_cursor.h"
#include "lo_semantics/legacy_utf16_line_token_number.h"
#include "lo_semantics/legacy_utf16_prefix_cursor.h"
#include "lo_semantics/manager_metadata_compare.h"
#include "lo_semantics/manager_metadata_parsing.h"
#include "lo_semantics/metadata_name_index.h"
#include "lo_semantics/metadata_name_registry.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace lo::semantic::gpu::legacy_config_command_dispatch
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

// The original body uses both halves of GPRs and FPSCR-visible single
// operations. This local representation keeps their exact bit patterns while
// the existing accepted callees use their own selected-state interfaces.
union Value
{
    std::uint64_t u64;
    std::int64_t s64;
    std::uint32_t u32;
    std::int32_t s32;
    std::uint16_t u16;
    std::uint8_t u8;
    double f64;
    float f32;
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
        if constexpr (std::is_floating_point_v<T>)
            so = std::uint8_t(std::isnan(left) || std::isnan(right));
        else so = xer.so;
    }
    void compare(double left, double right)
    {
        const Xer unused{};
        compare(left, right, unused);
    }
};

struct Context
{
    Value r[32]{};
    Value f[32]{};
    std::uint64_t lr = 0, ctr = 0;
    Xer xer{};
    Condition cr0{}, cr6{};
    std::uint32_t cached_fp_control = 0;
};

Context ToContext(const Registers& state)
{
    Context context{};
    for (unsigned i = 0; i < 32; ++i)
    {
        context.r[i].u64 = state.integer.r[i];
        context.f[i].u64 = state.fpr_bits[i];
    }
    context.r[1].u64 = state.integer.sp;
    context.lr = state.integer.lr;
    context.ctr = state.integer.ctr;
    context.xer = {state.integer.xer_so, state.integer.xer_ca};
    context.cr0 = {state.integer.cr0.lt, state.integer.cr0.gt,
        state.integer.cr0.eq, state.integer.cr0.so};
    context.cr6 = {state.integer.cr6.lt, state.integer.cr6.gt,
        state.integer.cr6.eq, state.integer.cr6.so};
    context.cached_fp_control = state.cached_fp_control;
    return context;
}

Registers ToRegisters(const Context& context)
{
    Registers state{};
    for (unsigned i = 0; i < 32; ++i)
    {
        state.integer.r[i] = context.r[i].u64;
        state.fpr_bits[i] = context.f[i].u64;
    }
    state.integer.sp = context.r[1].u64;
    state.integer.lr = context.lr;
    state.integer.ctr = context.ctr;
    state.integer.xer_so = context.xer.so;
    state.integer.xer_ca = context.xer.ca;
    state.integer.cr0 = {context.cr0.lt, context.cr0.gt,
        context.cr0.eq, context.cr0.so};
    state.integer.cr6 = {context.cr6.lt, context.cr6.gt,
        context.cr6.eq, context.cr6.so};
    state.cached_fp_control = context.cached_fp_control;
    return state;
}

void DisableFlushMode(Context& context, GuestBoundaries& guests)
{
    constexpr std::uint32_t flush_mask = 0x8040u;
    if ((context.cached_fp_control & flush_mask) == 0u) return;
    context.cached_fp_control &= ~flush_mask;
    guests.SetHostFpControl(context.cached_fp_control);
}

void Save26(Context& context, GuestMemory& memory)
{
    for (unsigned reg = 26; reg <= 31; ++reg)
        WriteU64(memory, Address(context.r[1].u64 -
            8u * (33u - reg)), context.r[reg].u64);
    memory.WriteU32(Address(context.r[1].u64 - 8u),
        Address(context.r[12].u64));
}

void Restore26(Context& context, GuestMemory& memory)
{
    for (unsigned reg = 26; reg <= 31; ++reg)
        context.r[reg].u64 = ReadU64(memory, Address(context.r[1].u64 -
            8u * (33u - reg)));
    context.r[12].u64 = memory.ReadU32(Address(context.r[1].u64 - 8u));
    context.lr = context.r[12].u64;
}

void CallLower(GuestAddress entry, Context& context, GuestMemory& memory,
    Dependencies dependencies)
{
    auto state = ToRegisters(context);
    if (dependencies.guests.TryApplyLower(entry, memory, state))
    {
        context = ToContext(state);
        return;
    }
    bool handled = false;
    switch (entry)
    {
    case 0x82296740u:
        handled = legacy_utf16_prefix_cursor::Apply(entry, memory,
            dependencies.thread, dependencies.invalid, state.integer);
        break;
    case 0x822969a0u:
        handled = legacy_character_cursor::Apply(entry, memory,
            state.integer);
        break;
    case 0x822988e0u:
        handled = legacy_utf16_line_token_number::Apply(entry, memory,
            dependencies.thread, dependencies.invalid, state.integer);
        break;
    case 0x82297338u:
    {
        legacy_token_float_cursor::Registers floating{};
        floating.integer = state.integer;
        floating.f0_bits = state.fpr_bits[0];
        floating.f1_bits = state.fpr_bits[1];
        floating.cached_fp_control = state.cached_fp_control;
        handled = legacy_token_float_cursor::Apply(entry, memory,
            dependencies.thread, dependencies.invalid,
            dependencies.numbers, floating);
        state.integer = floating.integer;
        state.fpr_bits[0] = floating.f0_bits;
        state.fpr_bits[1] = floating.f1_bits;
        state.cached_fp_control = floating.cached_fp_control;
        break;
    }
    case 0x82296e80u:
    {
        manager_metadata_parsing::FrameRegisters frame{};
        frame.lr = state.integer.lr;
        frame.r13 = state.integer.r[13];
        frame.r8 = state.integer.r[8];
        frame.r9 = state.integer.r[9];
        for (unsigned i = 0; i < 9; ++i)
            frame.r23_through_r31[i] = state.integer.r[i + 23];
        std::uint64_t result = 0;
        handled = manager_metadata_parsing::Apply(entry, memory,
            dependencies.thread, dependencies.invalid,
            state.integer.r[3], state.integer.r[4], state.integer.r[5],
            state.integer.r[6], state.integer.r[7], state.integer.sp,
            frame, result);
        state.integer.r[3] = result;
        state.integer.r[13] = frame.r13;
        state.integer.r[8] = frame.r8;
        state.integer.r[9] = frame.r9;
        for (unsigned i = 0; i < 9; ++i)
            state.integer.r[i + 23] = frame.r23_through_r31[i];
        state.integer.lr = frame.lr;
        state.integer.r[12] = frame.lr;
        break;
    }
    case 0x82296f68u:
    {
        metadata_name_index::FrameRegisters frame{};
        frame.lr = state.integer.lr;
        frame.r27 = state.integer.r[27];
        frame.r28 = state.integer.r[28];
        frame.r29 = state.integer.r[29];
        frame.r30 = state.integer.r[30];
        frame.r31 = state.integer.r[31];
        frame.r0 = state.integer.r[0];
        frame.ctr = state.integer.ctr;
        state.integer.r[3] = metadata_name_index::HashName(memory,
            state.integer.r[3], state.integer.sp, frame);
        state.integer.lr = frame.lr;
        state.integer.r[27] = frame.r27;
        state.integer.r[28] = frame.r28;
        state.integer.r[29] = frame.r29;
        state.integer.r[30] = frame.r30;
        state.integer.r[31] = frame.r31;
        state.integer.r[0] = frame.r0;
        state.integer.ctr = frame.ctr;
        state.integer.r[12] = frame.lr;
        handled = true;
        break;
    }
    case 0x822971e0u:
    {
        InvalidParameterCall call{};
        for (unsigned i = 0; i < 8; ++i)
            call.arguments[i] = state.integer.r[i + 3];
        call.thread_environment = state.integer.r[13];
        std::uint64_t result = 0;
        handled = manager_metadata_compare::Apply(entry, memory,
            dependencies.thread, dependencies.invalid, call,
            state.integer.sp, state.integer.lr, result);
        for (unsigned i = 0; i < 8; ++i)
            state.integer.r[i + 3] = call.arguments[i];
        state.integer.r[3] = result;
        state.integer.r[13] = call.thread_environment;
        state.integer.r[12] = state.integer.lr;
        break;
    }
    case 0x823f4700u:
    {
        metadata_name_registry::FrameRegisters frame{};
        frame.lr = state.integer.lr;
        frame.r27 = state.integer.r[27];
        frame.r28 = state.integer.r[28];
        frame.r29 = state.integer.r[29];
        frame.r30 = state.integer.r[30];
        frame.r31 = state.integer.r[31];
        std::uint64_t result = 0;
        handled = metadata_name_registry::Apply(entry, memory,
            dependencies.records, dependencies.arrays, state.integer.sp,
            frame, result);
        state.integer.r[3] = result;
        state.integer.lr = frame.lr;
        state.integer.r[12] = frame.lr;
        state.integer.r[27] = frame.r27;
        state.integer.r[28] = frame.r28;
        state.integer.r[29] = frame.r29;
        state.integer.r[30] = frame.r30;
        state.integer.r[31] = frame.r31;
        break;
    }
    case 0x82713828u:
    case 0x82479058u:
    case 0x824790a8u:
    case 0x82296b00u:
    case 0x82295da8u:
        dependencies.guests.Call(entry, memory, state);
        handled = true;
        break;
    default: throw std::logic_error("unknown command lower");
    }
    if (!handled) throw std::logic_error("accepted command lower unavailable");
    context = ToContext(state);
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U16(a) memory.ReadU16(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(memory, Address(a))
#define PPC_STORE_U8(a, v) memory.WriteU8(Address(a), static_cast<std::uint8_t>(v))
#define PPC_STORE_U16(a, v) memory.WriteU16(Address(a), static_cast<std::uint16_t>(v))
#define PPC_STORE_U32(a, v) memory.WriteU32(Address(a), static_cast<std::uint32_t>(v))
#define PPC_STORE_U64(a, v) WriteU64(memory, Address(a), static_cast<std::uint64_t>(v))

void Execute(GuestMemory& memory, Dependencies dependencies,
    Context& context)
{
    auto& ctx = context;
    Value temp{};
    // Exact 461-instruction 82296008 control flow follows.
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x82296010;
	Save26(ctx, memory);
	// stfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	PPC_STORE_U64(ctx.r[1].u32 + -72, ctx.f[30].u64);
	// stfd f31,-64(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -64, ctx.f[31].u64);
	// stwu r1,-992(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-992);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// stw r4,1020(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 1020, ctx.r[4].u32);
	// mr r27,r3
	ctx.r[27].u64 = ctx.r[3].u64;
	// addi r4,r11,4908
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(4908);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// mr r26,r5
	ctx.r[26].u64 = ctx.r[5].u64;
	// bl 0x82296740
	ctx.lr = 0x82296038;
	CallLower(0x82296740u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// beq cr6,0x822960d0
	if (ctx.cr6.eq) goto loc_822960D0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// bl 0x822969a0
	ctx.lr = 0x82296054;
	CallLower(0x822969a0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x822960ac
	if (ctx.cr6.eq) goto loc_822960AC;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82713828
	ctx.lr = 0x82296068;
	CallLower(0x82713828u, ctx, memory, dependencies);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x822960ac
	if (ctx.cr6.eq) goto loc_822960AC;
	// lbz r11,96(r27)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[27].u32 + 96);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82296120
	if (ctx.cr6.eq) goto loc_82296120;
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 1, ctx.xer);
	// bne cr6,0x822960bc
	if (!ctx.cr6.eq) goto loc_822960BC;
	// lbz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[3].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x822960bc
	if (ctx.cr6.eq) goto loc_822960BC;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_822960AC:
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// addi r4,r11,-16436
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-16436);
	// bl 0x82479058
	ctx.lr = 0x822960BC;
	CallLower(0x82479058u, ctx, memory, dependencies);
loc_822960BC:
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_822960D0:
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// addi r4,r11,4612
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(4612);
	// bl 0x82296740
	ctx.lr = 0x822960DC;
	CallLower(0x82296740u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// beq cr6,0x8229613c
	if (ctx.cr6.eq) goto loc_8229613C;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// bl 0x822969a0
	ctx.lr = 0x822960F8;
	CallLower(0x822969a0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x822960ac
	if (ctx.cr6.eq) goto loc_822960AC;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82713828
	ctx.lr = 0x8229610C;
	CallLower(0x82713828u, ctx, memory, dependencies);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x822960ac
	if (ctx.cr6.eq) goto loc_822960AC;
	// lbz r11,96(r27)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[27].u32 + 96);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x822960bc
	if (!ctx.cr6.eq) goto loc_822960BC;
loc_82296120:
	// li r11,1
	ctx.r[11].s64 = 1;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_8229613C:
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// addi r4,r11,4988
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(4988);
	// bl 0x82296740
	ctx.lr = 0x82296148;
	CallLower(0x82296740u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// beq cr6,0x822961d0
	if (ctx.cr6.eq) goto loc_822961D0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// bl 0x822969a0
	ctx.lr = 0x82296164;
	CallLower(0x822969a0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x822961ac
	if (ctx.cr6.eq) goto loc_822961AC;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82713828
	ctx.lr = 0x82296178;
	CallLower(0x82713828u, ctx, memory, dependencies);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x822961ac
	if (ctx.cr6.eq) goto loc_822961AC;
	// lbz r11,96(r27)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[27].u32 + 96);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x822961bc
	if (!ctx.cr6.eq) goto loc_822961BC;
	// lbz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[3].u32 + 0);
	// xori r11,r11,128
	ctx.r[11].u64 = ctx.r[11].u64 ^ 128;
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_822961AC:
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// addi r4,r11,-16396
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-16396);
	// bl 0x82479058
	ctx.lr = 0x822961BC;
	CallLower(0x82479058u, ctx, memory, dependencies);
loc_822961BC:
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_822961D0:
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// addi r4,r11,4600
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(4600);
	// bl 0x82296740
	ctx.lr = 0x822961DC;
	CallLower(0x82296740u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// beq cr6,0x82296380
	if (ctx.cr6.eq) goto loc_82296380;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// bl 0x822969a0
	ctx.lr = 0x822961F8;
	CallLower(0x822969a0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x82296358
	if (ctx.cr6.eq) goto loc_82296358;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82296b00
	ctx.lr = 0x8229620C;
	CallLower(0x82296b00u, ctx, memory, dependencies);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x82296358
	if (ctx.cr6.eq) goto loc_82296358;
	// lbz r11,96(r27)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[27].u32 + 96);
	// cmplwi cr6,r11,4
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 4, ctx.xer);
	// bne cr6,0x8229636c
	if (!ctx.cr6.eq) goto loc_8229636C;
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// lwz r31,1020(r1)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 1020);
	// addi r5,r1,120
	ctx.r[5].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(120);
	// addi r4,r11,5036
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(5036);
	// lis r11,-32231
	ctx.r[11].s64 = -2112290816;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lfs f30,-27252(r11)
	DisableFlushMode(ctx, dependencies.guests);
	temp.u32 = PPC_LOAD_U32(ctx.r[11].u32 + -27252);
	ctx.f[30].f64 = double(temp.f32);
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// stfs f30,120(r1)
	temp.f32 = float(ctx.f[30].f64);
	PPC_STORE_U32(ctx.r[1].u32 + 120, temp.u32);
	// lfs f31,3664(r11)
	temp.u32 = PPC_LOAD_U32(ctx.r[11].u32 + 3664);
	ctx.f[31].f64 = double(temp.f32);
	// li r11,1
	ctx.r[11].s64 = 1;
	// stfs f31,128(r1)
	temp.f32 = float(ctx.f[31].f64);
	PPC_STORE_U32(ctx.r[1].u32 + 128, temp.u32);
	// stfs f31,124(r1)
	temp.f32 = float(ctx.f[31].f64);
	PPC_STORE_U32(ctx.r[1].u32 + 124, temp.u32);
	// stw r11,112(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 112, ctx.r[11].u32);
	// bl 0x82297338
	ctx.lr = 0x82296260;
	CallLower(0x82297338u, ctx, memory, dependencies);
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// addi r5,r1,112
	ctx.r[5].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(112);
	// addi r4,r11,5084
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(5084);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x822988e0
	ctx.lr = 0x82296274;
	CallLower(0x822988e0u, ctx, memory, dependencies);
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// addi r5,r1,128
	ctx.r[5].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(128);
	// addi r4,r11,5232
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(5232);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82297338
	ctx.lr = 0x82296288;
	CallLower(0x82297338u, ctx, memory, dependencies);
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// addi r5,r1,124
	ctx.r[5].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(124);
	// addi r4,r11,6956
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(6956);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82297338
	ctx.lr = 0x8229629C;
	CallLower(0x82297338u, ctx, memory, dependencies);
	// lfs f0,128(r1)
	DisableFlushMode(ctx, dependencies.guests);
	temp.u32 = PPC_LOAD_U32(ctx.r[1].u32 + 128);
	ctx.f[0].f64 = double(temp.f32);
	// fcmpu cr6,f0,f31
	ctx.cr6.compare(ctx.f[0].f64, ctx.f[31].f64);
	// ble cr6,0x822962f4
	if (!ctx.cr6.gt) goto loc_822962F4;
	// fcmpu cr6,f0,f30
	ctx.cr6.compare(ctx.f[0].f64, ctx.f[30].f64);
	// bge cr6,0x822962f4
	if (!ctx.cr6.lt) goto loc_822962F4;
	// lfs f13,100(r27)
	temp.u32 = PPC_LOAD_U32(ctx.r[27].u32 + 100);
	ctx.f[13].f64 = double(temp.f32);
	// fcmpu cr6,f13,f31
	ctx.cr6.compare(ctx.f[13].f64, ctx.f[31].f64);
	// ble cr6,0x822962d4
	if (!ctx.cr6.gt) goto loc_822962D4;
	// fsubs f13,f13,f0
	ctx.f[13].f64 = double(float(ctx.f[13].f64 - ctx.f[0].f64));
	// fsubs f0,f30,f0
	ctx.f[0].f64 = double(float(ctx.f[30].f64 - ctx.f[0].f64));
	// fneg f12,f13
	ctx.f[12].u64 = ctx.f[13].u64 ^ 0x8000000000000000;
	// fsel f13,f12,f31,f13
	ctx.f[13].f64 = ctx.f[12].f64 >= 0.0 ? ctx.f[31].f64 : ctx.f[13].f64;
	// fdivs f0,f13,f0
	ctx.f[0].f64 = double(float(ctx.f[13].f64 / ctx.f[0].f64));
	// b 0x822962f0
	goto loc_822962F0;
loc_822962D4:
	// fneg f13,f13
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[13].u64 = ctx.f[13].u64 ^ 0x8000000000000000;
	// fsubs f12,f30,f0
	ctx.f[12].f64 = double(float(ctx.f[30].f64 - ctx.f[0].f64));
	// fsubs f0,f13,f0
	ctx.f[0].f64 = double(float(ctx.f[13].f64 - ctx.f[0].f64));
	// fneg f13,f0
	ctx.f[13].u64 = ctx.f[0].u64 ^ 0x8000000000000000;
	// fsel f0,f13,f31,f0
	ctx.f[0].f64 = ctx.f[13].f64 >= 0.0 ? ctx.f[31].f64 : ctx.f[0].f64;
	// fdivs f0,f0,f12
	ctx.f[0].f64 = double(float(ctx.f[0].f64 / ctx.f[12].f64));
	// fneg f0,f0
	ctx.f[0].u64 = ctx.f[0].u64 ^ 0x8000000000000000;
loc_822962F0:
	// stfs f0,100(r27)
	DisableFlushMode(ctx, dependencies.guests);
	temp.f32 = float(ctx.f[0].f64);
	PPC_STORE_U32(ctx.r[27].u32 + 100, temp.u32);
loc_822962F4:
	// lfs f0,124(r1)
	DisableFlushMode(ctx, dependencies.guests);
	temp.u32 = PPC_LOAD_U32(ctx.r[1].u32 + 124);
	ctx.f[0].f64 = double(temp.f32);
	// fcmpu cr6,f0,f31
	ctx.cr6.compare(ctx.f[0].f64, ctx.f[31].f64);
	// beq cr6,0x82296314
	if (ctx.cr6.eq) goto loc_82296314;
	// lfs f13,104(r27)
	temp.u32 = PPC_LOAD_U32(ctx.r[27].u32 + 104);
	ctx.f[13].f64 = double(temp.f32);
	// fmuls f0,f13,f0
	ctx.f[0].f64 = double(float(ctx.f[13].f64 * ctx.f[0].f64));
	// lfs f13,120(r1)
	temp.u32 = PPC_LOAD_U32(ctx.r[1].u32 + 120);
	ctx.f[13].f64 = double(temp.f32);
	// fmuls f0,f0,f13
	ctx.f[0].f64 = double(float(ctx.f[0].f64 * ctx.f[13].f64));
	// b 0x82296318
	goto loc_82296318;
loc_82296314:
	// lfs f0,120(r1)
	DisableFlushMode(ctx, dependencies.guests);
	temp.u32 = PPC_LOAD_U32(ctx.r[1].u32 + 120);
	ctx.f[0].f64 = double(temp.f32);
loc_82296318:
	// lwz r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 112);
	// lfs f13,100(r27)
	DisableFlushMode(ctx, dependencies.guests);
	temp.u32 = PPC_LOAD_U32(ctx.r[27].u32 + 100);
	ctx.f[13].f64 = double(temp.f32);
	// lfs f12,0(r30)
	temp.u32 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	ctx.f[12].f64 = double(temp.f32);
	// li r3,1
	ctx.r[3].s64 = 1;
	// extsw r11,r11
	ctx.r[11].s64 = ctx.r[11].s32;
	// std r11,112(r1)
	PPC_STORE_U64(ctx.r[1].u32 + 112, ctx.r[11].u64);
	// lfd f11,112(r1)
	ctx.f[11].u64 = PPC_LOAD_U64(ctx.r[1].u32 + 112);
	// fcfid f11,f11
	ctx.f[11].f64 = double(ctx.f[11].s64);
	// frsp f11,f11
	ctx.f[11].f64 = double(float(ctx.f[11].f64));
	// fmuls f13,f13,f11
	ctx.f[13].f64 = double(float(ctx.f[13].f64 * ctx.f[11].f64));
	// fmadds f0,f13,f0,f12
	ctx.f[0].f64 = double(float(ctx.f[13].f64 * ctx.f[0].f64 + ctx.f[12].f64));
	// stfs f0,0(r30)
	temp.f32 = float(ctx.f[0].f64);
	PPC_STORE_U32(ctx.r[30].u32 + 0, temp.u32);
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_82296358:
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// lwz r5,1020(r1)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 1020);
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// addi r4,r11,-16356
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-16356);
	// bl 0x824790a8
	ctx.lr = 0x8229636C;
	CallLower(0x824790a8u, ctx, memory, dependencies);
loc_8229636C:
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_82296380:
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// addi r4,r11,-16316
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-16316);
	// bl 0x82296740
	ctx.lr = 0x8229638C;
	CallLower(0x82296740u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// beq cr6,0x8229640c
	if (ctx.cr6.eq) goto loc_8229640C;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// bl 0x822969a0
	ctx.lr = 0x822963A8;
	CallLower(0x822969a0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x822963e4
	if (ctx.cr6.eq) goto loc_822963E4;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82713828
	ctx.lr = 0x822963BC;
	CallLower(0x82713828u, ctx, memory, dependencies);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq cr6,0x822963e4
	if (ctx.cr6.eq) goto loc_822963E4;
	// lbz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[3].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(1);
	// stb r11,0(r3)
	PPC_STORE_U8(ctx.r[3].u32 + 0, ctx.r[11].u8);
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_822963E4:
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// lwz r5,1020(r1)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 1020);
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// addi r4,r11,-16304
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-16304);
	// bl 0x824790a8
	ctx.lr = 0x822963F8;
	CallLower(0x824790a8u, ctx, memory, dependencies);
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_8229640C:
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// addi r4,r11,-16260
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-16260);
	// bl 0x82296740
	ctx.lr = 0x82296418;
	CallLower(0x82296740u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x8229659c
	if (ctx.cr6.eq) goto loc_8229659C;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// bl 0x822969a0
	ctx.lr = 0x82296434;
	CallLower(0x822969a0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x8229659c
	if (ctx.cr6.eq) goto loc_8229659C;
	// lis r11,-31964
	ctx.r[11].s64 = -2094792704;
	// li r29,0
	ctx.r[29].s64 = 0;
	// addi r30,r1,144
	ctx.r[30].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// lwz r11,25184(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 25184);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82296458
	if (!ctx.cr6.eq) goto loc_82296458;
	// bl 0x823f4700
	ctx.lr = 0x82296458;
	CallLower(0x823f4700u, ctx, memory, dependencies);
loc_82296458:
	// addi r6,r1,112
	ctx.r[6].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(112);
	// li r5,128
	ctx.r[5].s64 = 128;
	// addi r4,r1,656
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(656);
	// addi r3,r1,144
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// bl 0x82296e80
	ctx.lr = 0x8229646C;
	CallLower(0x82296e80u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x82296480
	if (ctx.cr6.eq) goto loc_82296480;
	// lwz r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 112);
	// addi r30,r1,656
	ctx.r[30].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(656);
	// addi r29,r11,1
	ctx.r[29].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(1);
loc_82296480:
	// lhz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[30].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82296588
	if (ctx.cr6.eq) goto loc_82296588;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82296f68
	ctx.lr = 0x82296494;
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
	// beq cr6,0x82296588
	if (ctx.cr6.eq) goto loc_82296588;
loc_822964AC:
	// addi r4,r31,16
	ctx.r[4].u64 = ctx.r[31].u64 + static_cast<std::uint64_t>(16);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x822971e0
	ctx.lr = 0x822964B8;
	CallLower(0x822971e0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x822964e0
	if (ctx.cr6.eq) goto loc_822964E0;
	// lwz r31,12(r31)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x822964ac
	if (!ctx.cr6.eq) goto loc_822964AC;
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_822964E0:
	// lwz r8,0(r31)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// cmpwi cr6,r8,0
	ctx.cr6.compare<int32_t>(ctx.r[8].s32, 0, ctx.xer);
	// bne cr6,0x822964f4
	if (!ctx.cr6.eq) goto loc_822964F4;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// beq cr6,0x82296588
	if (ctx.cr6.eq) goto loc_82296588;
loc_822964F4:
	// lwz r11,76(r27)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 76);
	// li r10,0
	ctx.r[10].s64 = 0;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// ble cr6,0x82296588
	if (!ctx.cr6.gt) goto loc_82296588;
	// lwz r9,72(r27)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 72);
	// mr r11,r9
	ctx.r[11].u64 = ctx.r[9].u64;
loc_8229650C:
	// lwz r7,0(r11)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// cmpw cr6,r7,r8
	ctx.cr6.compare<int32_t>(ctx.r[7].s32, ctx.r[8].s32, ctx.xer);
	// bne cr6,0x82296524
	if (!ctx.cr6.eq) goto loc_82296524;
	// lwz r7,4(r11)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 4);
	// cmpw cr6,r7,r29
	ctx.cr6.compare<int32_t>(ctx.r[7].s32, ctx.r[29].s32, ctx.xer);
	// beq cr6,0x8229654c
	if (ctx.cr6.eq) goto loc_8229654C;
loc_82296524:
	// lwz r7,76(r27)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 76);
	// addi r10,r10,1
	ctx.r[10].u64 = ctx.r[10].u64 + static_cast<std::uint64_t>(1);
	// addi r11,r11,24
	ctx.r[11].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(24);
	// cmplw cr6,r10,r7
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[7].u32, ctx.xer);
	// blt cr6,0x8229650c
	if (ctx.cr6.lt) goto loc_8229650C;
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_8229654C:
	// rlwinm r11,r10,1,0,30
	ctx.r[11].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 1) & 0xFFFFFFFE;
	// add r11,r10,r11
	ctx.r[11].u64 = ctx.r[10].u64 + ctx.r[11].u64;
	// rlwinm r11,r11,3,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r9
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[9].u64;
	// lwz r10,12(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq cr6,0x82296570
	if (ctx.cr6.eq) goto loc_82296570;
	// lwz r5,8(r11)
	ctx.r[5].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 8);
	// b 0x82296578
	goto loc_82296578;
loc_82296570:
	// lis r11,-32229
	ctx.r[11].s64 = -2112159744;
	// addi r5,r11,-31792
	ctx.r[5].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-31792);
loc_82296578:
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// addi r4,r11,3960
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(3960);
	// bl 0x824790a8
	ctx.lr = 0x82296588;
	CallLower(0x824790a8u, ctx, memory, dependencies);
loc_82296588:
	// li r3,1
	ctx.r[3].s64 = 1;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_8229659C:
	// lis r28,-31945
	ctx.r[28].s64 = -2093547520;
	// lwz r11,-12564(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + -12564);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82296658
	if (!ctx.cr6.eq) goto loc_82296658;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,144
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// addi r3,r1,1020
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(1020);
	// bl 0x822969a0
	ctx.lr = 0x822965C0;
	CallLower(0x822969a0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x82296658
	if (ctx.cr6.eq) goto loc_82296658;
	// lis r11,-31964
	ctx.r[11].s64 = -2094792704;
	// li r29,0
	ctx.r[29].s64 = 0;
	// addi r30,r1,144
	ctx.r[30].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// lwz r11,25184(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 25184);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x822965e4
	if (!ctx.cr6.eq) goto loc_822965E4;
	// bl 0x823f4700
	ctx.lr = 0x822965E4;
	CallLower(0x823f4700u, ctx, memory, dependencies);
loc_822965E4:
	// addi r6,r1,112
	ctx.r[6].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(112);
	// li r5,128
	ctx.r[5].s64 = 128;
	// addi r4,r1,656
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(656);
	// addi r3,r1,144
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(144);
	// bl 0x82296e80
	ctx.lr = 0x822965F8;
	CallLower(0x82296e80u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x8229660c
	if (ctx.cr6.eq) goto loc_8229660C;
	// lwz r11,112(r1)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 112);
	// addi r30,r1,656
	ctx.r[30].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(656);
	// addi r29,r11,1
	ctx.r[29].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(1);
loc_8229660C:
	// lhz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U16(ctx.r[30].u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82296658
	if (ctx.cr6.eq) goto loc_82296658;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82296f68
	ctx.lr = 0x82296620;
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
	// beq cr6,0x82296658
	if (ctx.cr6.eq) goto loc_82296658;
loc_82296638:
	// addi r4,r31,16
	ctx.r[4].u64 = ctx.r[31].u64 + static_cast<std::uint64_t>(16);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x822971e0
	ctx.lr = 0x82296644;
	CallLower(0x822971e0u, ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x8229666c
	if (ctx.cr6.eq) goto loc_8229666C;
	// lwz r31,12(r31)
	ctx.r[31].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82296638
	if (!ctx.cr6.eq) goto loc_82296638;
loc_82296658:
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_8229666C:
	// lwz r8,0(r31)
	ctx.r[8].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// cmpwi cr6,r8,0
	ctx.cr6.compare<int32_t>(ctx.r[8].s32, 0, ctx.xer);
	// bne cr6,0x82296680
	if (!ctx.cr6.eq) goto loc_82296680;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// beq cr6,0x82296658
	if (ctx.cr6.eq) goto loc_82296658;
loc_82296680:
	// lwz r11,76(r27)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 76);
	// addi r11,r11,-1
	ctx.r[11].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-1);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// blt cr6,0x82296658
	if (ctx.cr6.lt) goto loc_82296658;
	// rlwinm r10,r11,1,0,30
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 1) & 0xFFFFFFFE;
	// lwz r9,72(r27)
	ctx.r[9].u64 = PPC_LOAD_U32(ctx.r[27].u32 + 72);
	// add r10,r11,r10
	ctx.r[10].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// rlwinm r10,r10,3,0,28
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 3) & 0xFFFFFFF8;
	// add r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 + ctx.r[9].u64;
loc_822966A4:
	// lwz r7,0(r10)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 0);
	// cmpw cr6,r7,r8
	ctx.cr6.compare<int32_t>(ctx.r[7].s32, ctx.r[8].s32, ctx.xer);
	// bne cr6,0x822966bc
	if (!ctx.cr6.eq) goto loc_822966BC;
	// lwz r7,4(r10)
	ctx.r[7].u64 = PPC_LOAD_U32(ctx.r[10].u32 + 4);
	// cmpw cr6,r7,r29
	ctx.cr6.compare<int32_t>(ctx.r[7].s32, ctx.r[29].s32, ctx.xer);
	// beq cr6,0x822966e0
	if (ctx.cr6.eq) goto loc_822966E0;
loc_822966BC:
	// addi r11,r11,-1
	ctx.r[11].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-1);
	// addi r10,r10,-24
	ctx.r[10].u64 = ctx.r[10].u64 + static_cast<std::uint64_t>(-24);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bge cr6,0x822966a4
	if (!ctx.cr6.lt) goto loc_822966A4;
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_822966E0:
	// li r10,1
	ctx.r[10].s64 = 1;
	// stw r10,-12564(r28)
	PPC_STORE_U32(ctx.r[28].u32 + -12564, ctx.r[10].u32);
	// rlwinm r10,r11,1,0,30
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 1) & 0xFFFFFFFE;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// rlwinm r11,r11,3,0,28
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 3) & 0xFFFFFFF8;
	// add r11,r11,r9
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[9].u64;
	// lwz r10,12(r11)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 12);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq cr6,0x8229670c
	if (ctx.cr6.eq) goto loc_8229670C;
	// lwz r4,8(r11)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 8);
	// b 0x82296714
	goto loc_82296714;
loc_8229670C:
	// lis r11,-32229
	ctx.r[11].s64 = -2112159744;
	// addi r4,r11,-31792
	ctx.r[4].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-31792);
loc_82296714:
	// mr r5,r26
	ctx.r[5].u64 = ctx.r[26].u64;
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82295da8
	ctx.lr = 0x82296720;
	CallLower(0x82295da8u, ctx, memory, dependencies);
	// li r11,0
	ctx.r[11].s64 = 0;
	// li r3,1
	ctx.r[3].s64 = 1;
	// stw r11,-12564(r28)
	PPC_STORE_U32(ctx.r[28].u32 + -12564, ctx.r[11].u32);
	// addi r1,r1,992
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(992);
	// lfd f30,-72(r1)
	DisableFlushMode(ctx, dependencies.guests);
	ctx.f[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -72);
	// lfd f31,-64(r1)
	ctx.f[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -64);
	// b 0x82b7a730
	Restore26(ctx, memory);
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
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x82296008u) return false;
    auto context = ToContext(registers);
    Execute(memory, dependencies, context);
    registers = ToRegisters(context);
    return true;
}
} // namespace lo::semantic::gpu::legacy_config_command_dispatch
