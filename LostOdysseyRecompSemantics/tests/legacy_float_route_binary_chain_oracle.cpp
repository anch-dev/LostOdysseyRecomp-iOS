#include "lo_semantics/legacy_float_parse_routes.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_float_parse_routes;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Text = 0x20000u, Destination = 0x40000u;
constexpr GuestAddress Format = 0x83215c50u;
constexpr test::Region Regions[] = {
    {0u, 0x100000u}, {0x83215000u, 0x2000u}
};
constexpr std::uint64_t TextRegister = 0x1234567800000000ull | Text;
constexpr std::uint64_t ModeRegister = 0x0fedcba900050000ull;
struct Case
{
    const char* name;
    std::uint16_t sign_exponent;
    std::uint64_t significand_high;
    std::uint16_t significand_low;
    bool known_output;
    std::uint64_t expected_bits;
};
constexpr Case Cases[] = {
    {"zero", 0x0000u, 0u, 0u, true, 0u},
    {"negative-one-point-five", 0xbfffu, 0xc000000000000000ull, 0u,
        true, 0xbff8000000000000ull},
    {"ties-to-even", 0x3fffu, 0x8000000000000400ull, 0u,
        true, 0x3ff0000000000000ull},
    {"below-subnormal", 0x3bc7u, 0x8000000000000000ull, 0u,
        false, 0u}
};
const Case* active_case = nullptr;
GuestMemory* active_memory = nullptr;
unsigned original_parse_calls = 0;

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
void WriteRecord(GuestMemory& memory, GuestAddress address, const Case& item)
{
    memory.WriteU16(address, item.sign_exponent);
    memory.WriteU32(address + 2u,
        static_cast<std::uint32_t>(item.significand_high >> 32u));
    memory.WriteU32(address + 6u,
        static_cast<std::uint32_t>(item.significand_high));
    memory.WriteU16(address + 10u, item.significand_low);
}
void Seed(test::GuestWindow& window)
{
    window.Fill(0xbd);
    auto memory = window.Memory();
    memory.WriteU16(Text, '1');
    memory.WriteU16(Text + 2u, 0u);
    memory.WriteU32(Destination, 0xaaaaaaaau);
    memory.WriteU32(Destination + 4u, 0xbbbbbbbbu);
    constexpr std::array<std::uint32_t, 6> Parameters = {
        1024u, 0xfffffc01u, 53u, 11u, 64u, 1023u
    };
    for (unsigned index = 0; index < Parameters.size(); ++index)
        memory.WriteU32(Format + index * 4u, Parameters[index]);
}
PPCContext Initial()
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800080000ull;
    c.r3.u64 = 0x9988776600000000ull | Destination;
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
struct Services final : family::PpcBoundaryServices
{
    unsigned parse_calls = 0;
    void Parse822975B0(GuestMemory& memory,
        family::Registers& state) override
    {
        const auto& r = state.integer.r;
        if (r[3] != state.integer.sp + 96u ||
            r[4] != state.integer.sp + 80u ||
            r[5] != TextRegister || r[10] != ModeRegister ||
            r[6] != 0u || r[7] != 0u || r[8] != 0u || r[9] != 0u ||
            state.integer.lr != 0x82297538u)
            throw std::runtime_error("recovered parser boundary inputs");
        memory.WriteU32(static_cast<GuestAddress>(r[4]), Text + 2u);
        WriteRecord(memory, static_cast<GuestAddress>(r[3]), *active_case);
        state.integer.r[3] = 0u;
        state.integer.r[6] = 0x1234u;
        ++parse_calls;
    }
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
    Seed(original); Seed(recovered);
    auto context = Initial();
    auto state = FromPpc(context);
    Services services;
    active_case = &item;
    original_parse_calls = 0;
    auto left = original.Memory();
    active_memory = &left;
    __imp__sub_822974F8(context, original.Bytes());
    active_memory = nullptr;
    auto right = recovered.Memory();
    if (!family::Apply(0x822974f8u, right, services, state))
        throw std::runtime_error("missing legacy float route");
    if (!Same(FromPpc(context), state) ||
        !original.EqualCommitted(recovered) ||
        original_parse_calls != 1u || services.parse_calls != 1u)
        throw std::runtime_error(item.name);
    if (item.known_output)
    {
        const auto actual = (std::uint64_t{left.ReadU32(Destination + 16u)} << 32u) |
            left.ReadU32(Destination + 20u);
        if (actual != item.expected_bits)
            throw std::runtime_error("unexpected composed binary64 value");
    }
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
void OriginalParse(PPCContext& c, std::uint8_t*)
{
    if (c.r3.u64 != c.r1.u64 + 96u ||
        c.r4.u64 != c.r1.u64 + 80u ||
        c.r5.u64 != TextRegister || c.r10.u64 != ModeRegister ||
        c.r6.u64 != 0u || c.r7.u64 != 0u ||
        c.r8.u64 != 0u || c.r9.u64 != 0u ||
        c.lr != 0x82297538u)
        throw std::runtime_error("original parser boundary inputs");
    active_memory->WriteU32(c.r4.u32, Text + 2u);
    WriteRecord(*active_memory, c.r3.u32, *active_case);
    c.r3.u64 = 0u;
    c.r6.u64 = 0x1234u;
    ++original_parse_calls;
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-float-route-binary-chain %zu original PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT 822975B0 parser boundary, ordinary RAM and selected finite inputs; faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
