#include "lo_semantics/legacy_float_public_numeric_parse_chain.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_float_public_parse;
namespace chain = legacy_float_public_numeric_parse_chain;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

constexpr GuestAddress Text = 0x20000u, End = 0x30000u;
constexpr GuestAddress Flags = 0x50000u, Stack = 0x80000u;
constexpr GuestAddress FlagPointer = 0x83215b40u;
constexpr GuestAddress Format = 0x83215c50u;
constexpr GuestAddress Zero = 0x82000fe8u;
constexpr GuestAddress Infinity = 0x820d3af0u;
constexpr std::array<test::Region, 4> Regions{{
    {0u, 0x90000u}, {0x83215000u, 0x2000u},
    {0x82000000u, 0x2000u}, {0x820d3000u, 0x2000u}
}};
constexpr std::uint64_t TextRegister = 0x1234567800000000ull | Text;
constexpr std::uint64_t EndRegister = 0xaabbccdd00000000ull | End;

struct Case
{
    const char* name;
    GuestAddress entry;
    const char* text;
    GuestAddress expected_end;
    std::uint32_t expected_flags;
    std::uint64_t expected_f1;
    bool end_pointer;
    unsigned digit_calls;
};
constexpr std::array<Case, 5> Cases{{
    {"plain-twelve", 0x82b7e098u, "12", Text + 4u, 0u,
        0x4028000000000000ull, true, 1u},
    {"tail-whitespace-negative", 0x82b7e200u, " \t-1x",
        Text + 8u, 0u, 0xbff0000000000000ull, true, 1u},
    {"no-conversion-end-reset", 0x82b7e098u, "x", Text,
        512u, 0u, true, 0u},
    {"power-ten", 0x82b7e098u, "1e1", Text + 6u, 0u,
        0x4024000000000000ull, true, 1u},
    {"tail-no-end-pointer", 0x82b7e200u, "1", Text + 2u,
        0u, 0x3ff0000000000000ull, false, 1u}
}};
GuestMemory* active_memory = nullptr;
unsigned original_digit_calls = 0;

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers state{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        state.route.integer.r[index] = index == 1u ? 0u : fields[index]->u64;
    auto& integer = state.route.integer;
    integer.sp = c.r1.u64;
    integer.lr = c.lr; integer.ctr = c.ctr.u64;
    integer.xer_so = c.xer.so; integer.xer_ca = c.xer.ca;
    integer.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    integer.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.route.f1_bits = c.f1.u64;
    state.route.cached_fp_control = c.fpscr.csr;
    state.f0_bits = c.f0.u64;
    state.f31_bits = c.f31.u64;
    return state;
}
bool Same(const family::Registers& left, const family::Registers& right)
{
    const auto& a = left.route.integer;
    const auto& b = right.route.integer;
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so &&
        left.route.f1_bits == right.route.f1_bits &&
        left.route.cached_fp_control == right.route.cached_fp_control &&
        left.f0_bits == right.f0_bits && left.f31_bits == right.f31_bits;
}
chain::Services::Conditions CrFromPpc(const PPCContext& c)
{
    const PPCCRRegister* fields[] = {&c.cr0, &c.cr1, &c.cr2,
        &c.cr3, &c.cr4, &c.cr5, &c.cr6, &c.cr7};
    chain::Services::Conditions result{};
    for (unsigned index = 0; index < 8u; ++index)
        result[index] = {fields[index]->lt, fields[index]->gt,
            fields[index]->eq, fields[index]->so};
    return result;
}
void WriteText(GuestMemory& memory, GuestAddress address, const char* value)
{
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*value));
        address += 2u;
    } while (*value++);
}
void Seed(test::GuestWindow& window, const Case& item)
{
    window.Fill(0);
    auto memory = window.Memory();
    WriteText(memory, Text, item.text);
    memory.WriteU32(End, 0xaaaaaaaau);
    memory.WriteU32(FlagPointer, Flags);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>(' '), 8u);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>('\t'), 8u);
    WriteU64(memory, Zero, 0u);
    WriteU64(memory, Infinity, 0x7ff0000000000000ull);
    constexpr std::array<std::uint32_t, 6> Parameters = {
        1024u, 0xfffffc01u, 53u, 11u, 64u, 1023u
    };
    for (unsigned index = 0; index < Parameters.size(); ++index)
        memory.WriteU32(Format + index * 4u, Parameters[index]);
    constexpr std::array<std::uint8_t, 12> JumpOffsets{{
        0x00u, 0x1du, 0x3cu, 0x5bu, 0x78u, 0x96u,
        0x9eu, 0xbfu, 0xb0u, 0xd5u, 0xd2u, 0xc5u}};
    for (unsigned index = 0; index < JumpOffsets.size(); ++index)
        memory.WriteU8(0x820d3a30u + index, JumpOffsets[index]);
    memory.WriteU32(0x83215648u, 0x83215618u);
    memory.WriteU32(0x83215618u, 0x83215614u);
    memory.WriteU8(0x83215614u, static_cast<std::uint8_t>('.'));
    constexpr std::array<std::uint8_t, 12> Ten{{
        0x40u, 0x02u, 0xa0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u}};
    for (unsigned index = 0; index < Ten.size(); ++index)
        memory.WriteU8(0x83215c80u + index, Ten[index]);
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = TextRegister;
    c.r4.u64 = item.end_pointer ? EndRegister : 0u;
    c.r5.u64 = 0xfaceface00001234ull;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    c.f0.u64 = 0xc00a000000000000ull;
    c.f1.u64 = 0xc00b000000000000ull;
    c.f31.u64 = 0x400c000000000000ull;
    c.fpscr.csr = 0x9fc0u;
    return c;
}
struct Unresolved final : family::NativeServices
{
    unsigned fp_calls = 0;
    void Parse822975B0(GuestMemory&, family::Registers&) override
    { throw std::runtime_error("numeric parser escaped actual lower"); }
    void InvalidArgument82B7FD78(GuestMemory&, family::Registers&) override
    { throw std::runtime_error("unexpected invalid-argument call"); }
    void InvalidParameter82B7FEC0(GuestMemory&, family::Registers&) override
    { throw std::runtime_error("unexpected invalid-parameter call"); }
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
};
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
void Check(const Case& item)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    Seed(original, item); Seed(recovered, item);
    original_digit_calls = 0;
    auto context = Initial(item);
    auto state = FromPpc(context);
    const auto initial_cr = CrFromPpc(context);
    auto left = original.Memory();
    active_memory = &left;
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    if (item.entry == 0x82b7e200u)
        __imp__sub_82B7E200(context, original.Bytes());
    else
        __imp__sub_82B7E098(context, original.Bytes());
    const auto original_control = PPCFPSCRRegister{}.getcsr();
    active_memory = nullptr;

    Unresolved unresolved;
    chain::Services services(unresolved, initial_cr);
    PPCFPSCRRegister{}.setcsr(state.route.cached_fp_control);
    auto right = recovered.Memory();
    if (!family::Apply(item.entry, right, services, state))
        throw std::runtime_error("public parse entry missing");
    const auto recovered_control = PPCFPSCRRegister{}.getcsr();
    const auto observed = FromPpc(context);
    if (left.ReadU32(Stack - 80u) != item.expected_flags ||
        (item.end_pointer && left.ReadU32(End) != item.expected_end) ||
        observed.route.f1_bits != item.expected_f1 ||
        original_digit_calls != item.digit_calls)
        throw std::runtime_error("original numeric public fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered) ||
        original_control != recovered_control ||
        services.ParserCalls() != 1u ||
        services.DigitCalls() != original_digit_calls ||
        unresolved.fp_calls != 1u)
        throw std::runtime_error(item.name);
}
} // namespace

void OriginalSave28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 28u; index <= 31u; ++index)
        WriteU64(*active_memory, c.r1.u32 - 40u + (index - 28u) * 8u,
            fields[index]->u64);
    active_memory->WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 28u; index <= 31u; ++index)
        fields[index]->u64 = ReadU64(*active_memory,
            c.r1.u32 - 40u + (index - 28u) * 8u);
    c.r12.u64 = active_memory->ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalSave27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(*active_memory, c.r1.u32 - 16u - (31u - index) * 8u,
            fields[index]->u64);
    active_memory->WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        fields[index]->u64 = ReadU64(*active_memory,
            c.r1.u32 - 16u - (31u - index) * 8u);
    c.r12.u64 = active_memory->ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalSave19(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 19u; index <= 31u; ++index)
        WriteU64(*active_memory, c.r1.u32 - 16u - (31u - index) * 8u,
            fields[index]->u64);
    active_memory->WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore19(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 19u; index <= 31u; ++index)
        fields[index]->u64 = ReadU64(*active_memory,
            c.r1.u32 - 16u - (31u - index) * 8u);
    c.r12.u64 = active_memory->ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalDigit(PPCContext& c, std::uint8_t* base)
{
    if (c.r3.u64 != c.r1.u64 + 128u ||
        c.r5.u64 != c.r1.u64 + 96u || c.lr != 0x82297a94u)
        throw std::runtime_error("original digit call inputs");
    __imp__sub_82297F38(c, base);
    ++original_digit_calls;
}
void OriginalInvalid(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid-argument call"); }
void OriginalInvalidParameter(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid-parameter call"); }

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-float-public-numeric-parse-chain %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected ordinary RAM inputs, invalid native calls unselected; faults/MMIO/runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
