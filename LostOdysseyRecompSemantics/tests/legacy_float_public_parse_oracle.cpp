#include "lo_semantics/legacy_float_public_parse.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_float_public_parse;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

constexpr GuestAddress Text = 0x20000u, End = 0x30000u;
constexpr GuestAddress Flags = 0x50000u, Errno = 0x60000u;
constexpr GuestAddress Stack = 0x80000u;
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
    std::uint32_t skip, consumed;
    std::uint16_t sign_exponent;
    std::uint64_t significand;
    std::uint32_t parse_flags;
    std::uint32_t expected_flags;
    std::uint64_t expected_f1;
    bool end_pointer;
    bool range_error;
};
constexpr std::array<Case, 5> Cases{{
    {"plain-end", 0x82b7e098u, "1", 0u, 1u,
        0x3fffu, 0x8000000000000000ull, 0u, 0u,
        0x3ff0000000000000ull, true, false},
    {"tail-whitespace-negative", 0x82b7e200u, " \t-1.5", 2u, 4u,
        0xbfffu, 0xc000000000000000ull, 0u, 0u,
        0xbff8000000000000ull, true, false},
    {"parse-failure-resets-end", 0x82b7e098u, "x", 0u, 0u,
        0x0000u, 0u, 4u, 512u, 0u, true, false},
    {"tail-negative-range-error", 0x82b7e200u, "-9", 0u, 2u,
        0xbfffu, 0x8000000000000000ull, 2u, 128u,
        0xfff0000000000000ull, false, true},
    {"no-end-pointer-finite", 0x82b7e098u, "1.5", 0u, 3u,
        0x3fffu, 0xc000000000000000ull, 0u, 0u,
        0x3ff8000000000000ull, false, false}
}};
const Case* active_case = nullptr;
GuestMemory* active_memory = nullptr;
unsigned original_parse_calls = 0, original_invalid_calls = 0;

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
    memory.WriteU32(FlagPointer, Flags);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>(' '), 8u);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>('\t'), 8u);
    memory.WriteU32(End, 0xaaaaaaaau);
    WriteU64(memory, Zero, 0u);
    WriteU64(memory, Infinity, 0x7ff0000000000000ull);
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
void WriteRecord(GuestMemory& memory, GuestAddress address,
    const Case& item)
{
    memory.WriteU16(address, item.sign_exponent);
    memory.WriteU32(address + 2u,
        static_cast<std::uint32_t>(item.significand >> 32u));
    memory.WriteU32(address + 6u,
        static_cast<std::uint32_t>(item.significand));
    memory.WriteU16(address + 10u, 0u);
}

struct Services final : family::NativeServices
{
    unsigned parse_calls = 0, invalid_calls = 0, fp_calls = 0;
    void Parse822975B0(GuestMemory& memory,
        family::Registers& state) override
    {
        const auto& integer = state.route.integer;
        const auto& r = integer.r;
        const auto expected_text = TextRegister + active_case->skip * 2u;
        if (r[3] != integer.sp + 96u || r[4] != integer.sp + 80u ||
            r[5] != expected_text || r[10] != 0xffffffff83215300ull ||
            r[6] != 0u || r[7] != 0u || r[8] != 0u || r[9] != 0u ||
            integer.lr != 0x82297538u)
            throw std::runtime_error("recovered parser boundary inputs");
        memory.WriteU32(static_cast<GuestAddress>(r[4]),
            Text + (active_case->skip + active_case->consumed) * 2u);
        WriteRecord(memory, static_cast<GuestAddress>(r[3]), *active_case);
        state.route.integer.r[3] = active_case->parse_flags;
        state.route.integer.r[6] = 0x1234u;
        ++parse_calls;
    }
    void InvalidArgument82B7FD78(GuestMemory&,
        family::Registers& state) override
    {
        if (!active_case->range_error ||
            state.route.integer.lr != 0x82b7e1dcu)
            throw std::runtime_error("unexpected invalid-argument boundary");
        state.route.integer.r[3] = Errno;
        ++invalid_calls;
    }
    void InvalidParameter82B7FEC0(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("unexpected invalid-parameter boundary"); }
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
    active_case = &item;
    original_parse_calls = 0; original_invalid_calls = 0;
    auto context = Initial(item);
    auto state = FromPpc(context);
    auto left = original.Memory();
    active_memory = &left;
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    if (item.entry == 0x82b7e200u)
        __imp__sub_82B7E200(context, original.Bytes());
    else
        __imp__sub_82B7E098(context, original.Bytes());
    const auto original_control = PPCFPSCRRegister{}.getcsr();
    active_memory = nullptr;

    Services services;
    PPCFPSCRRegister{}.setcsr(state.route.cached_fp_control);
    auto right = recovered.Memory();
    if (!family::Apply(item.entry, right, services, state))
        throw std::runtime_error("public parse entry missing");
    const auto recovered_control = PPCFPSCRRegister{}.getcsr();
    const auto observed = FromPpc(context);
    const GuestAddress object = Stack - 80u;
    const GuestAddress expected_end = item.parse_flags & 4u ? Text :
        Text + (item.skip + item.consumed) * 2u;
    if (left.ReadU32(object) != item.expected_flags ||
        (item.end_pointer && left.ReadU32(End) != expected_end) ||
        left.ReadU32(Errno) != (item.range_error ? 34u : 0u) ||
        observed.route.f1_bits != item.expected_f1 ||
        original_parse_calls != 1u ||
        original_invalid_calls != unsigned(item.range_error))
        throw std::runtime_error("original public parse fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered) ||
        original_control != recovered_control ||
        services.parse_calls != original_parse_calls ||
        services.invalid_calls != original_invalid_calls ||
        services.fp_calls != 1u)
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
void OriginalParse(PPCContext& c, std::uint8_t*)
{
    const auto expected_text = TextRegister + active_case->skip * 2u;
    if (c.r3.u64 != c.r1.u64 + 96u || c.r4.u64 != c.r1.u64 + 80u ||
        c.r5.u64 != expected_text || c.r10.u64 != 0xffffffff83215300ull ||
        c.r6.u64 != 0u || c.r7.u64 != 0u || c.r8.u64 != 0u ||
        c.r9.u64 != 0u || c.lr != 0x82297538u)
        throw std::runtime_error("original parser boundary inputs");
    active_memory->WriteU32(c.r4.u32,
        Text + (active_case->skip + active_case->consumed) * 2u);
    WriteRecord(*active_memory, c.r3.u32, *active_case);
    c.r3.u64 = active_case->parse_flags;
    c.r6.u64 = 0x1234u;
    ++original_parse_calls;
}
void OriginalInvalid(PPCContext& c, std::uint8_t*)
{
    if (!active_case->range_error || c.lr != 0x82b7e1dcu)
        throw std::runtime_error("unexpected original invalid-argument call");
    c.r3.u64 = Errno;
    ++original_invalid_calls;
}
void OriginalInvalidParameter(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid-parameter call"); }

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-float-public-parse %zu original PPC cases\n",
            Cases.size());
        std::puts("LIMIT 822975B0 parser and exceptional native boundaries; ordinary RAM and selected finite inputs only, faults/MMIO/runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
