#include "lo_semantics/legacy_config_line_dispatch.h"

#include "lo_semantics/legacy_utf16_line_token_number.h"
#include "lo_semantics/legacy_utf16_prefix_cursor.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/registered_getter_family.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_config_line_dispatch
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
    std::uint8_t u8;
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

void Save27(Context& context, GuestMemory& memory)
{
    for (unsigned reg = 27; reg <= 31; ++reg)
        WriteU64(memory, Address(context.r[1].u64 -
            8u * (33u - reg)), context.r[reg].u64);
    memory.WriteU32(Address(context.r[1].u64 - 8u),
        Address(context.r[12].u64));
}
void Restore27(Context& context, GuestMemory& memory)
{
    for (unsigned reg = 27; reg <= 31; ++reg)
        context.r[reg].u64 = ReadU64(memory, Address(context.r[1].u64 -
            8u * (33u - reg)));
    context.r[12].u64 = memory.ReadU32(Address(context.r[1].u64 - 8u));
    context.lr = context.r[12].u64;
}

void Execute8231A220(GuestMemory& memory, Dependencies dependencies,
    Context& context);

void CallLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Context& context)
{
    if (entry == 0x8231a220u)
    {
        Execute8231A220(memory, dependencies, context);
        return;
    }
    auto state = ToRegisters(context);
    bool handled = false;
    switch (entry)
    {
    case 0x82295ee0u:
        handled = legacy_utf16_line_token_number::Apply(entry, memory,
            dependencies.thread, dependencies.invalid, state);
        break;
    case 0x82296740u:
        handled = legacy_utf16_prefix_cursor::Apply(entry, memory,
            dependencies.thread, dependencies.invalid, state);
        break;
    case 0x822a2700u:
    {
        // The accepted getter returns its value without exposing its PPC
        // prologue/compare scratch. Reconstruct the selected, already
        // initialized path from the complete 822A2700 body.
        const auto initial = memory.ReadU32(0x833181e4u);
        if (initial == 0u)
            throw std::logic_error("lazy config singleton getter is unselected");
        const auto return_lr = state.lr;
        memory.WriteU32(Address(state.sp - 8u), Address(return_lr));
        WriteU64(memory, Address(state.sp - 16u), state.r[31]);
        memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
        std::uint64_t result = 0;
        handled = registered_getter_family::Apply(entry, memory,
            dependencies.manager, dependencies.registration, state.r[3],
            Address(state.sp), result);
        state.r[3] = result;
        state.cr6 = {0u, 1u, 0u, state.xer_so};
        state.r[12] = memory.ReadU32(Address(state.sp - 8u));
        state.lr = state.r[12];
        break;
    }
    default: throw std::logic_error("unknown config line lower");
    }
    if (!handled) throw std::logic_error("accepted config line lower missing");
    context = ToContext(state);
}

void CallVirtual(Context& context, GuestMemory& memory,
    Dependencies dependencies)
{
    auto state = ToRegisters(context);
    dependencies.virtual_calls.Call(Address(context.ctr) & ~3u, memory,
        state);
    context = ToContext(state);
}

#define PPC_LOAD_U8(a) memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(memory, Address(a))
#define PPC_STORE_U32(a, v) memory.WriteU32(Address(a), static_cast<std::uint32_t>(v))
#define PPC_STORE_U64(a, v) WriteU64(memory, Address(a), static_cast<std::uint64_t>(v))

void Execute8231A220(GuestMemory& memory, Dependencies dependencies,
    Context& context)
{
    auto& ctx = context;
    Value temp{};
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
	// beq cr6,0x8231a288
	if (ctx.cr6.eq) goto loc_8231A288;
	// bl 0x822a2700
	ctx.lr = 0x8231A240;
	CallLower(0x822a2700u, memory, dependencies, ctx);
	// lwz r11,52(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 52);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x8231a260
	if (ctx.cr6.eq) goto loc_8231A260;
loc_8231A24C:
	// cmplw cr6,r11,r3
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[3].u32, ctx.xer);
	// beq cr6,0x8231a270
	if (ctx.cr6.eq) goto loc_8231A270;
	// lwz r11,60(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 60);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x8231a24c
	if (!ctx.cr6.eq) goto loc_8231A24C;
loc_8231A260:
	// cntlzw r11,r3
	ctx.r[11].u64 = ctx.r[3].u32 == 0 ? 32 : std::countl_zero(ctx.r[3].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq cr6,0x8231a288
	if (ctx.cr6.eq) goto loc_8231A288;
loc_8231A270:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// addi r1,r1,96
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r31,-16(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// blr
	return;
loc_8231A288:
	// li r3,0
	ctx.r[3].s64 = 0;
	// addi r1,r1,96
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r31,-16(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// blr
	return;
}

void Execute82295DA8(GuestMemory& memory, Dependencies dependencies,
    Context& context)
{
    auto& ctx = context;
    Value temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x82295DB0;
	Save27(ctx, memory);
	// stwu r1,-656(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-656);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// stw r4,684(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 684, ctx.r[4].u32);
	// mr r28,r5
	ctx.r[28].u64 = ctx.r[5].u64;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,96
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
	// addi r3,r1,684
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(684);
	// bl 0x82295ee0
	ctx.lr = 0x82295DD4;
	CallLower(0x82295ee0u, memory, dependencies, ctx);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq cr6,0x82295ed8
	if (ctx.cr6.eq) goto loc_82295ED8;
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// addi r27,r11,-16456
	ctx.r[27].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(-16456);
loc_82295DE4:
	// addi r30,r1,96
	ctx.r[30].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
	// lbz r11,96(r31)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[31].u32 + 96);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// stw r30,80(r1)
	PPC_STORE_U32(ctx.r[1].u32 + 80, ctx.r[30].u32);
	// beq cr6,0x82295e30
	if (ctx.cr6.eq) goto loc_82295E30;
	// cmplwi cr6,r11,2
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 2, ctx.xer);
	// beq cr6,0x82295e30
	if (ctx.cr6.eq) goto loc_82295E30;
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 1, ctx.xer);
	// bne cr6,0x82295e20
	if (!ctx.cr6.eq) goto loc_82295E20;
	// addi r3,r1,80
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(80);
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// bl 0x82296740
	ctx.lr = 0x82295E14;
	CallLower(0x82296740u, memory, dependencies, ctx);
	// lwz r30,80(r1)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 80);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne cr6,0x82295e30
	if (!ctx.cr6.eq) goto loc_82295E30;
loc_82295E20:
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r11,284(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 284);
	// b 0x82295eac
	goto loc_82295EAC;
loc_82295E30:
	// lwz r3,40(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 40);
	// bl 0x8231a220
	ctx.lr = 0x82295E38;
	CallLower(0x8231a220u, memory, dependencies, ctx);
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// mr r6,r31
	ctx.r[6].u64 = ctx.r[31].u64;
	// mr r5,r28
	ctx.r[5].u64 = ctx.r[28].u64;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r11,260(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 260);
	// mtctr r11
	ctx.ctr = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82295E5C;
	CallVirtual(ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne cr6,0x82295ebc
	if (!ctx.cr6.eq) goto loc_82295EBC;
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// mr r5,r28
	ctx.r[5].u64 = ctx.r[28].u64;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r11,284(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 284);
	// mtctr r11
	ctx.ctr = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82295E80;
	CallVirtual(ctx, memory, dependencies);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne cr6,0x82295ebc
	if (!ctx.cr6.eq) goto loc_82295EBC;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82295ebc
	if (ctx.cr6.eq) goto loc_82295EBC;
	// lwz r11,772(r29)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[29].u32 + 772);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x82295ebc
	if (ctx.cr6.eq) goto loc_82295EBC;
	// rotlwi r11,r11,0
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32, 0);
	// addi r3,r11,60
	ctx.r[3].u64 = ctx.r[11].u64 + static_cast<std::uint64_t>(60);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
loc_82295EAC:
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r5,r28
	ctx.r[5].u64 = ctx.r[28].u64;
	// mtctr r11
	ctx.ctr = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82295EBC;
	CallVirtual(ctx, memory, dependencies);
loc_82295EBC:
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,256
	ctx.r[5].s64 = 256;
	// addi r4,r1,96
	ctx.r[4].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(96);
	// addi r3,r1,684
	ctx.r[3].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(684);
	// bl 0x82295ee0
	ctx.lr = 0x82295ED0;
	CallLower(0x82295ee0u, memory, dependencies, ctx);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne cr6,0x82295de4
	if (!ctx.cr6.eq) goto loc_82295DE4;
loc_82295ED8:
	// addi r1,r1,656
	ctx.r[1].u64 = ctx.r[1].u64 + static_cast<std::uint64_t>(656);
	// b 0x82b7a734
	Restore27(ctx, memory);
	return;
}

#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x82295da8u && entry != 0x8231a220u) return false;
    auto context = ToContext(registers);
    if (entry == 0x82295da8u)
        Execute82295DA8(memory, dependencies, context);
    else Execute8231A220(memory, dependencies, context);
    registers = ToRegisters(context);
    return true;
}
} // namespace lo::semantic::gpu::legacy_config_line_dispatch
