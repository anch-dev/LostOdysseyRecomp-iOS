#include "lo_semantics/legacy_float_parse_routes.h"
#include "lo_semantics/legacy_numeric_parse_chain.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_float_parse_routes;
namespace chain = legacy_numeric_parse_chain;
namespace digit = legacy_float_digit_accumulator;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Text = 0x20000u, Destination = 0x40000u;
constexpr GuestAddress Format = 0x83215c50u;
constexpr GuestAddress Flags = 0x50000u;
constexpr GuestAddress FlagPointer = 0x83215b40u;
constexpr test::Region Regions[] = {
    {0u, 0x100000u}, {0x820d3000u, 0x1000u},
    {0x83214000u, 0x2000u}
};
constexpr std::uint64_t TextRegister = 0x1234567800000000ull | Text;
constexpr std::uint64_t ModeRegister = 0x0fedcba900050000ull;
struct Case
{
    const char* name;
    const char* text;
    GuestAddress entry;
    std::uint32_t consumed;
    std::uint32_t expected_flags;
    unsigned expected_digits;
    std::uint64_t expected_bits;
};
constexpr Case Cases[] = {
    {"integer-12", "12", 0x822974f8u, 2u, 0u, 2u,
        0x4028000000000000ull},
    {"decimal-1", "1.0", 0x822974f8u, 3u, 0u, 1u,
        0x3ff0000000000000ull},
    {"negative-1", "-1", 0x822974f8u, 2u, 0u, 1u,
        0xbff0000000000000ull},
    {"exponent-1", "1e1", 0x822974f8u, 3u, 0u, 1u,
        0x4024000000000000ull},
    {"no-conversion", "x", 0x822974f8u, 0u, 512u, 0u, 0u},
    {"outer-white-sign", " \t-3.5", 0x82b7d270u, 6u, 0u, 2u,
        0xc00c000000000000ull}
};
const Case* active_case = nullptr;
GuestMemory* active_memory = nullptr;
unsigned original_parse_calls = 0;
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
        state.integer.r[index] = index == 1u ? 0u : fields[index]->u64;
    state.integer.sp = c.r1.u64;
    state.integer.lr = c.lr; state.integer.ctr = c.ctr.u64;
    state.integer.xer_so = c.xer.so; state.integer.xer_ca = c.xer.ca;
    state.integer.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    state.integer.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.f1_bits = c.f1.u64;
    state.cached_fp_control = c.fpscr.csr;
    return state;
}
std::array<digit::Condition, 8> Conditions(PPCContext& c)
{
    const PPCCRRegister* fields[] = {&c.cr0, &c.cr1, &c.cr2, &c.cr3,
        &c.cr4, &c.cr5, &c.cr6, &c.cr7};
    std::array<digit::Condition, 8> result{};
    for (unsigned index = 0; index < result.size(); ++index)
        result[index] = {std::uint8_t(fields[index]->lt),
            std::uint8_t(fields[index]->gt),
            std::uint8_t(fields[index]->eq),
            std::uint8_t(fields[index]->so)};
    return result;
}
bool Same(const family::Registers& left, const family::Registers& right)
{
    const auto& a = left.integer;
    const auto& b = right.integer;
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so &&
        a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so &&
        left.f1_bits == right.f1_bits &&
        left.cached_fp_control == right.cached_fp_control;
}
void WriteText(GuestMemory& memory, const char* value)
{
    auto address = Text;
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
    WriteText(memory, item.text);
    memory.WriteU32(Destination, 0xaaaaaaaau);
    memory.WriteU32(Destination + 4u, 0xbbbbbbbbu);
    memory.WriteU32(FlagPointer, Flags);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>(' '), 8u);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>('\t'), 8u);
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
    // Guest image offset 01215DE0: exact 12-byte 10^-1 extended record.
    // For exponent -1 the parser computes 83215D80 + 84 + 12.
    constexpr std::array<std::uint8_t, 12> Tenth{{
        0x3fu, 0xfbu, 0xccu, 0xccu, 0xccu, 0xccu,
        0xccu, 0xccu, 0xccu, 0xcdu, 0xccu, 0xcdu}};
    for (unsigned index = 0; index < Tenth.size(); ++index)
        memory.WriteU8(0x83215de0u + index, Tenth[index]);
    constexpr std::array<std::uint32_t, 6> Parameters = {
        1024u, 0xfffffc01u, 53u, 11u, 64u, 1023u
    };
    for (unsigned index = 0; index < Parameters.size(); ++index)
        memory.WriteU32(Format + index * 4u, Parameters[index]);
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800080000ull;
    c.r3.u64 = item.entry == 0x82b7d270u ?
        TextRegister : (0x9988776600000000ull | Destination);
    c.r4.u64 = TextRegister;
    c.r8.u64 = ModeRegister;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    c.f1.u64 = 0xc00a000000000000ull;
    c.fpscr.csr = 0x1f80u;
    return c;
}
struct Unresolved final : family::PpcBoundaryServices
{
    void Parse822975B0(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("parser escaped actual-body chain"); }
    void Classify822981C8(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("822981C8 callback must not run"); }
    void InvalidArgument82B7FD78(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("unexpected invalid-argument boundary"); }
    void InvalidParameter82B7FEC0(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("unexpected invalid-parameter boundary"); }
    void SetHostFpControl(std::uint32_t) override
    { throw std::runtime_error("unexpected FP control change"); }
};
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    Seed(original, item); Seed(recovered, item);
    auto context = Initial(item);
    auto state = FromPpc(context);
    Unresolved unresolved;
    chain::Services services(unresolved, Conditions(context));
    active_case = &item;
    original_parse_calls = 0;
    original_digit_calls = 0;
    auto left = original.Memory();
    active_memory = &left;
    if (item.entry == 0x822974f8u)
        __imp__sub_822974F8(context, original.Bytes());
    else
        __imp__sub_82B7D270(context, original.Bytes());
    active_memory = nullptr;
    auto right = recovered.Memory();
    if (!family::Apply(item.entry, right, services, state))
        throw std::runtime_error("missing legacy float route");
    if (!Same(FromPpc(context), state) ||
        !original.EqualCommitted(recovered) ||
        original_parse_calls != 1u || services.ParseCalls() != 1u ||
        original_digit_calls != (item.expected_digits ? 1u : 0u) ||
        services.DigitCalls() != original_digit_calls)
    {
        std::fprintf(stderr, "FAIL %s RAM=%u parse=%u/%u digit=%u/%u "
            "r3=%llX/%llX f1=%llX/%llX\n", item.name,
            original.EqualCommitted(recovered), original_parse_calls,
            services.ParseCalls(), original_digit_calls,
            services.DigitCalls(),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(state.integer.r[3]),
            static_cast<unsigned long long>(context.f1.u64),
            static_cast<unsigned long long>(state.f1_bits));
        throw std::runtime_error("numeric parse actual PPC mismatch");
    }
    if (item.entry == 0x82b7d270u)
    {
        if (context.f1.u64 != item.expected_bits)
        {
            const auto frame = static_cast<GuestAddress>(0x80000u - 128u + 80u);
            std::fprintf(stderr, "outer F1=%llX wanted=%llX "
                "flags=%X consumed=%u value=%llX table-10^-1=%04X/%08X/%08X/%04X\n",
                static_cast<unsigned long long>(context.f1.u64),
                static_cast<unsigned long long>(item.expected_bits),
                left.ReadU32(frame), left.ReadU32(frame + 4u),
                static_cast<unsigned long long>(ReadU64(left, frame + 16u)),
                left.ReadU16(0x83215de0u), left.ReadU32(0x83215de2u),
                left.ReadU32(0x83215de6u), left.ReadU16(0x83215deau));
            throw std::runtime_error("outer independent binary64 value");
        }
    }
    else if (left.ReadU32(Destination) != item.expected_flags ||
        left.ReadU32(Destination + 4u) != item.consumed ||
        ReadU64(left, Destination + 16u) != item.expected_bits)
        throw std::runtime_error("direct independent flags/end/binary64");
}
} // namespace

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
void ChainParsePpc(PPCContext& c, std::uint8_t* base)
{
    const auto expected_text = TextRegister +
        (active_case->entry == 0x82b7d270u ? 4u : 0u);
    const auto expected_mode = active_case->entry == 0x82b7d270u ?
        0xffffffff83215300ull : ModeRegister;
    if (c.r3.u64 != c.r1.u64 + 96u ||
        c.r4.u64 != c.r1.u64 + 80u ||
        c.r5.u64 != expected_text || c.r10.u64 != expected_mode ||
        c.r6.u64 != 0u || c.r7.u64 != 0u ||
        c.r8.u64 != 0u || c.r9.u64 != 0u ||
        c.lr != 0x82297538u)
        throw std::runtime_error("original parser boundary inputs");
    __imp__sub_822975B0(c, base);
    ++original_parse_calls;
}
void ChainDigitPpc(PPCContext& c, std::uint8_t* base)
{
    if (c.r3.u64 != c.r1.u64 + 128u ||
        c.r5.u64 != c.r1.u64 + 96u ||
        c.r4.u64 != active_case->expected_digits ||
        c.lr != 0x82297a94u)
    {
        std::fprintf(stderr, "digit input %s r3=%llX want=%llX r4=%llX want=%u "
            "r5=%llX want=%llX sp=%llX lr=%llX\n", active_case->name,
            static_cast<unsigned long long>(c.r3.u64),
            static_cast<unsigned long long>(c.r1.u64 + 128u),
            static_cast<unsigned long long>(c.r4.u64),
            active_case->expected_digits,
            static_cast<unsigned long long>(c.r5.u64),
            static_cast<unsigned long long>(c.r1.u64 + 96u),
            static_cast<unsigned long long>(c.r1.u64),
            static_cast<unsigned long long>(c.lr));
        throw std::runtime_error("original digit lower call inputs");
    }
    __imp__sub_82297F38(c, base);
    ++original_digit_calls;
}
void OriginalInvalidArgument(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected invalid argument guest call"); }
void OriginalInvalidParameter(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected invalid parameter guest call"); }
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-numeric-parse-chain %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT outer CR1/CR7 unexposed by route ABI, invalid guest callees, other inputs, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
