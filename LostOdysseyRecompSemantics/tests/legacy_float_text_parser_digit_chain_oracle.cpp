// Composes actual 822975B0, 82297F38 and accepted 82B7A0B0 PPC bodies.
// Invalid-parameter callees remain explicit unselected guest boundaries.
#include "lo_semantics/legacy_float_text_parser.h"
#include "lo_semantics/legacy_float_text_parser_digit_chain.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_float_text_parser;
namespace chain = legacy_float_text_parser_digit_chain;
namespace digit = legacy_float_digit_accumulator;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Text = 0x20000u;
constexpr GuestAddress Output = 0x30000u;
constexpr GuestAddress EndPointer = 0x40000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::uint64_t TextRegister = 0x1234567800000000ull | Text;
constexpr std::array<Region, 3> Regions{{{0u, 0x90000u},
    {0x820d3000u, 0x1000u}, {0x83215000u, 0x1000u}}};

struct Spec
{
    const char* text;
    const char* digits;
    GuestAddress end;
    std::uint32_t status;
    std::uint16_t exponent;
    std::uint32_t mantissa_high, mantissa_low;
    std::uint16_t tail;
};
constexpr std::array<Spec, 5> Cases{{
    {"12", "12", Text + 4u, 0u, 0x4002u, 0xc0000000u, 0u, 0u},
    {"1.0", "1", Text + 6u, 0u, 0x3fffu, 0x80000000u, 0u, 0u},
    {"-1", "1", Text + 4u, 0u, 0xbfffu, 0x80000000u, 0u, 0u},
    {"1e5201", "1", Text + 12u, 2u,
        0x7fffu, 0x80000000u, 0u, 0u},
    {" \t1x", "1", Text + 6u, 0u,
        0x3fffu, 0x80000000u, 0u, 0u},
}};

// One extra focused nonzero-exponent path is selected below; the actual
// power-of-ten record is pinned in Seed from image_disc1.bin guest VA 83215C80.
constexpr Spec FactorTen{"1e1", "1", Text + 6u,
    0u, 0x4002u, 0xa0000000u, 0u, 0u};

const Spec* active = nullptr;
unsigned original_digit_calls = 0;
digit::Registers original_digit_return{};

void CheckDigits(const Spec& spec, GuestAddress digit_buffer,
    std::uint64_t count, const std::uint8_t* bytes)
{
    unsigned expected = 0;
    while (spec.digits[expected]) ++expected;
    if (count != expected)
        throw std::runtime_error("digit-conversion PPC boundary count");
    for (unsigned index = 0; index < expected; ++index)
        if (bytes[digit_buffer + index] !=
            static_cast<unsigned char>(spec.digits[index] - '0'))
            throw std::runtime_error("digit-conversion PPC boundary digits");
}

struct InvalidServices final : family::PpcBoundaryServices
{
    void DigitsToExtended82297F38(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("digit call escaped actual-body chain"); }
    void InvalidArgument82B7FD78(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("unexpected invalid-argument PPC boundary"); }
    void InvalidParameter82B7FEC0(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("unexpected invalid-parameter PPC boundary"); }
};

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    const PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
        state.r[index] = index == 1u ? 0u : fields[index]->u64;
    state.sp = context.r1.u64;
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    state.cr0 = {std::uint8_t(context.cr0.lt),
        std::uint8_t(context.cr0.gt), std::uint8_t(context.cr0.eq),
        std::uint8_t(context.cr0.so)};
    state.cr6 = {std::uint8_t(context.cr6.lt),
        std::uint8_t(context.cr6.gt), std::uint8_t(context.cr6.eq),
        std::uint8_t(context.cr6.so)};
    return state;
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so &&
        a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

digit::Registers DigitFromPpc(const PPCContext& context)
{
    digit::Registers state{};
    const PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    const PPCCRRegister* cr[] = {&context.cr0, &context.cr1, &context.cr2,
        &context.cr3, &context.cr4, &context.cr5, &context.cr6, &context.cr7};
    for (unsigned index = 0; index < 32u; ++index)
        state.r[index] = fields[index]->u64;
    for (unsigned index = 0; index < 8u; ++index)
        state.cr[index] = {std::uint8_t(cr[index]->lt),
            std::uint8_t(cr[index]->gt), std::uint8_t(cr[index]->eq),
            std::uint8_t(cr[index]->so)};
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    return state;
}

bool SameDigit(const digit::Registers& a, const digit::Registers& b)
{
    return a.r == b.r && a.cr == b.cr && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so && a.xer_ca == b.xer_ca;
}

void WriteText(GuestMemory& memory, GuestAddress address,
    const char* value)
{
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*value));
        address += 2u;
    } while (*value++);
}

void Seed(GuestWindow& window, const Spec& spec)
{
    window.Fill(0);
    auto memory = window.Memory();
    WriteText(memory, Text, spec.text);
    memory.WriteU32(Output, 0xaaaaaaaau);
    memory.WriteU32(EndPointer, 0xbbbbbbbbu);
    // Exact 12 guest bytes at 820D3A30 from image_disc1.bin: 12-state
    // dispatch offsets, multiplied by four and added to 822976A8.
    constexpr std::array<std::uint8_t, 12> JumpOffsets{{
        0x00u, 0x1du, 0x3cu, 0x5bu, 0x78u, 0x96u,
        0x9eu, 0xbfu, 0xb0u, 0xd5u, 0xd2u, 0xc5u}};
    for (unsigned index = 0; index < JumpOffsets.size(); ++index)
        memory.WriteU8(0x820d3a30u + index, JumpOffsets[index]);
    memory.WriteU32(0x83215648u, 0x83215618u);
    memory.WriteU32(0x83215618u, 0x83215614u);
    memory.WriteU8(0x83215614u, static_cast<std::uint8_t>('.'));
    // Exact extended 10^1 record at guest VA 83215C80, image offset
    // 01215C80. This is read on the FactorTen multiplication path.
    constexpr std::array<std::uint8_t, 12> Ten{{
        0x40u, 0x02u, 0xa0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}};
    for (unsigned index = 0; index < Ten.size(); ++index)
        memory.WriteU8(0x83215c80u + index, Ten[index]);
}

PPCContext Initial()
{
    PPCContext context{};
    PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.r3.u64 = 0x9988776600000000ull | Output;
    context.r4.u64 = 0x8877665500000000ull | EndPointer;
    context.r5.u64 = TextRegister;
    context.r6.u64 = 0u;
    context.r7.u64 = 0u;
    context.r8.u64 = 0u;
    context.r9.u64 = 0u;
    context.r10.u64 = 0xffffffff83215300ull;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

bool Check(const Spec& spec)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, spec);
    Seed(recovered, spec);
    active = &spec;
    original_digit_calls = 0;
    auto raw = Initial();
    auto state = FromPpc(raw);
    InvalidServices unresolved;
    chain::Services services(unresolved, DigitFromPpc(raw).cr);
    __imp__sub_822975B0(raw, original.Bytes());
    const auto fixture = original.Memory();
    if (original_digit_calls != 1u ||
        raw.r3.u64 != spec.status ||
        fixture.ReadU32(EndPointer) != spec.end ||
        fixture.ReadU16(Output) != spec.exponent ||
        fixture.ReadU32(Output + 2u) != spec.mantissa_high ||
        fixture.ReadU32(Output + 6u) != spec.mantissa_low ||
        fixture.ReadU16(Output + 10u) != spec.tail)
        throw std::runtime_error("float text parser fixture missed original PPC path");
    auto memory = recovered.Memory();
    if (!family::Apply(0x822975b0u, memory, services, state))
        throw std::runtime_error("float text parser recovered entry missing");
    const auto observed = FromPpc(raw);
    const bool ram_equal = original.EqualCommitted(recovered);
    const bool digit_equal = SameDigit(original_digit_return,
        services.LastDigitReturn());
    if (!Same(observed, state) || !ram_equal || !digit_equal ||
        services.DigitCalls() != original_digit_calls)
    {
        std::fprintf(stderr, "FAIL float-text-parser text=%s r3=%llX/%llX "
            "RAM=%u digit=%u calls=%u/%u CR6=%u%u%u%u/%u%u%u%u CA=%u/%u\n",
            spec.text,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]), ram_equal,
            digit_equal, original_digit_calls, services.DigitCalls(),
            observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
            observed.cr6.so, state.cr6.lt, state.cr6.gt,
            state.cr6.eq, state.cr6.so,
            observed.xer_ca, state.xer_ca);
        for (unsigned index = 0; index < 32u; ++index)
            if (observed.r[index] != state.r[index])
                std::fprintf(stderr, " r%u=%llX/%llX", index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        std::fprintf(stderr, "\n");
        return false;
    }
    return true;
}
} // namespace

void ChainDigitPpc(PPCContext& context, std::uint8_t* base)
{
    const auto& spec = *active;
    if (context.r3.u64 != context.r1.u64 + 128u ||
        context.r5.u64 != context.r1.u64 + 96u ||
        context.lr != 0x82297a94u)
        throw std::runtime_error("original digit PPC boundary inputs");
    CheckDigits(spec, context.r3.u32, context.r4.u64, base);
    __imp__sub_82297F38(context, base);
    original_digit_return = DigitFromPpc(context);
    ++original_digit_calls;
}

void OriginalSave27(PPCContext& context, std::uint8_t* base)
{
    PPCRegister* fields[] = {&context.r27, &context.r28, &context.r29,
        &context.r30, &context.r31};
    for (unsigned index = 0; index < 5u; ++index)
        PPC_STORE_U64(context.r1.u32 - 48u + index * 8u,
            fields[index]->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}

void OriginalRestore27(PPCContext& context, std::uint8_t* base)
{
    PPCRegister* fields[] = {&context.r27, &context.r28, &context.r29,
        &context.r30, &context.r31};
    for (unsigned index = 0; index < 5u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(context.r1.u32 - 48u + index * 8u);
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}

void LegacyParserInvalidArgument(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid-argument PPC body"); }

void LegacyParserInvalidParameter(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid-parameter PPC body"); }

int main()
{
    try
    {
        for (const auto& spec : Cases)
            if (!Check(spec)) return 1;
        if (!Check(FactorTen)) return 1;
        std::printf("PASS legacy-float-text-parser-digit-chain %zu actual PPC cases\n",
            Cases.size() + 1u);
        std::puts("LIMIT invalid guest callees, unselected parser inputs, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
