// Appended after the complete 82B7D270, 822974F8, 822974B0 and 82296830
// PPC bodies and the exact save/restore-27 helpers. The 609/453-instruction
// parser/converter callees remain explicit, stateful PPC guest boundaries.
#include "lo_semantics/legacy_float_parse_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_float_parse_routes;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Text = 0x20000u;
constexpr GuestAddress Destination = 0x40000u;
constexpr GuestAddress Flags = 0x50000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress FlagPointer = 0x83215b40u;
constexpr std::uint64_t TextRegister = 0x1234567800000000ull | Text;
constexpr std::uint64_t DirectMode = 0x0fedcba900050000ull;
constexpr std::uint64_t OuterMode = 0xffffffff83215300ull;
constexpr std::array<Region, 2> Regions{{{0u, 0x90000u},
    {0x83214000u, 0x2000u}}};

enum class Mode { Plain, WhiteSign, ParseFailure, ClassTwo,
    DirectFlagTwo, DirectFlagOne, DirectBoth };
constexpr std::array<Mode, 7> Cases{{Mode::Plain, Mode::WhiteSign,
    Mode::ParseFailure, Mode::ClassTwo, Mode::DirectFlagTwo,
    Mode::DirectFlagOne, Mode::DirectBoth}};

struct Spec
{
    Mode mode;
    bool direct;
    const char* text;
    std::uint32_t skip, consumed, parse_flags;
    std::uint32_t classification;
    double value;
    std::uint32_t fp_control;
    std::uint32_t expected_flags;
};
constexpr std::array<Spec, 7> Specs{{
    {Mode::Plain, false, "42", 0u, 2u, 0u, 0u, 42.0, 0x9fc0u, 0u},
    {Mode::WhiteSign, false, " \t-3.5", 2u, 4u, 0u, 1u,
        -3.5, 0x1f80u, 128u},
    {Mode::ParseFailure, false, "x", 0u, 0u, 4u, 0u,
        99.0, 0x9fc0u, 512u},
    {Mode::ClassTwo, false, "+1", 0u, 2u, 0u, 2u,
        1.0, 0x1f80u, 256u},
    {Mode::DirectFlagTwo, true, "2", 0u, 1u, 2u, 0u,
        2.0, 0x9fc0u, 128u},
    {Mode::DirectFlagOne, true, "1", 0u, 1u, 1u, 0u,
        1.0, 0x9fc0u, 256u},
    {Mode::DirectBoth, true, "3", 0u, 1u, 3u, 0u,
        3.0, 0x9fc0u, 384u},
}};

const Spec* active = nullptr;
unsigned original_parse_calls = 0, original_class_calls = 0;

std::uint64_t ExpectedText(const Spec& spec)
{ return TextRegister + spec.skip * 2u; }

std::uint64_t ExpectedMode(const Spec& spec)
{ return spec.direct ? DirectMode : OuterMode; }

GuestAddress ExpectedEnd(const Spec& spec)
{ return Text + (spec.skip + spec.consumed) * 2u; }

struct Services final : family::PpcBoundaryServices
{
    const Spec& spec;
    unsigned parse_calls = 0, class_calls = 0, fp_calls = 0;
    explicit Services(const Spec& value) : spec(value) {}

    void Parse822975B0(GuestMemory& memory,
        family::Registers& state) override
    {
        auto& r = state.integer.r;
        if (r[3] != state.integer.sp + 96u ||
            r[4] != state.integer.sp + 80u ||
            r[5] != ExpectedText(spec) || r[10] != ExpectedMode(spec) ||
            r[6] != 0u || r[7] != 0u || r[8] != 0u || r[9] != 0u ||
            state.integer.lr != 0x82297538u)
            throw std::runtime_error("recovered parse PPC boundary inputs");
        memory.WriteU32(static_cast<GuestAddress>(r[4]), ExpectedEnd(spec));
        memory.WriteU16(static_cast<GuestAddress>(r[3]), 0x4000u);
        memory.WriteU32(static_cast<GuestAddress>(r[3] + 2u), 0x12345678u);
        memory.WriteU32(static_cast<GuestAddress>(r[3] + 6u), 0x9abcdef0u);
        memory.WriteU16(static_cast<GuestAddress>(r[3] + 10u), 0x0102u);
        r[3] = spec.parse_flags;
        r[6] = 0x1234u;
        ++parse_calls;
    }

    void Classify822981C8(GuestMemory& memory,
        family::Registers& state) override
    {
        auto& r = state.integer.r;
        if (r[3] != state.integer.sp + 96u ||
            r[4] != state.integer.sp + 88u ||
            memory.ReadU16(static_cast<GuestAddress>(r[3])) != 0x4000u ||
            state.integer.lr != 0x82297560u)
            throw std::runtime_error("recovered classify PPC boundary inputs");
        const auto bits = std::bit_cast<std::uint64_t>(spec.value);
        memory.WriteU32(static_cast<GuestAddress>(r[4]),
            static_cast<std::uint32_t>(bits >> 32));
        memory.WriteU32(static_cast<GuestAddress>(r[4] + 4u),
            static_cast<std::uint32_t>(bits));
        r[3] = spec.classification;
        r[7] = 0x5678u;
        ++class_calls;
    }

    void InvalidArgument82B7FD78(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("unexpected invalid-argument PPC boundary"); }
    void InvalidParameter82B7FEC0(GuestMemory&,
        family::Registers&) override
    { throw std::runtime_error("unexpected invalid-parameter PPC boundary"); }
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
};

struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
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
        state.integer.r[index] = index == 1u ? 0u : fields[index]->u64;
    state.integer.sp = context.r1.u64;
    state.integer.lr = context.lr;
    state.integer.ctr = context.ctr.u64;
    state.integer.xer_so = context.xer.so;
    state.integer.xer_ca = context.xer.ca;
    state.integer.cr0 = {std::uint8_t(context.cr0.lt),
        std::uint8_t(context.cr0.gt), std::uint8_t(context.cr0.eq),
        std::uint8_t(context.cr0.so)};
    state.integer.cr6 = {std::uint8_t(context.cr6.lt),
        std::uint8_t(context.cr6.gt), std::uint8_t(context.cr6.eq),
        std::uint8_t(context.cr6.so)};
    state.f1_bits = context.f1.u64;
    state.cached_fp_control = context.fpscr.csr;
    return state;
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.integer;
    const auto& y = b.integer;
    if (x.r != y.r || x.sp != y.sp || x.lr != y.lr ||
        x.ctr != y.ctr || x.xer_so != y.xer_so ||
        x.xer_ca != y.xer_ca ||
        x.cr0.lt != y.cr0.lt || x.cr0.gt != y.cr0.gt ||
        x.cr0.eq != y.cr0.eq || x.cr0.so != y.cr0.so ||
        x.cr6.lt != y.cr6.lt || x.cr6.gt != y.cr6.gt ||
        x.cr6.eq != y.cr6.eq || x.cr6.so != y.cr6.so)
        return false;
    return a.f1_bits == b.f1_bits &&
        a.cached_fp_control == b.cached_fp_control;
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
    memory.WriteU32(FlagPointer, Flags);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>(' '), 8u);
    memory.WriteU16(Flags + 2u * static_cast<unsigned>('\t'), 8u);
    memory.WriteU32(Destination, 0xaaaaaaaau);
    memory.WriteU32(Destination + 4u, 0xbbbbbbbbu);
}

PPCContext Initial(const Spec& spec)
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
    context.r3.u64 = spec.direct ?
        (0x9988776600000000ull | Destination) : TextRegister;
    context.r4.u64 = TextRegister;
    context.r5.u64 = spec.consumed;
    context.r8.u64 = DirectMode;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    context.f1.u64 = 0xc00a000000000000ull;
    context.fpscr.csr = spec.fp_control;
    return context;
}

bool Check(const Spec& spec)
{
    RestoreHost restore;
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, spec);
    Seed(recovered, spec);
    active = &spec;
    original_parse_calls = 0;
    original_class_calls = 0;
    auto raw = Initial(spec);
    auto state = FromPpc(raw);
    PPCFPSCRRegister{}.setcsr(raw.fpscr.csr);
    if (spec.direct)
        __imp__sub_822974F8(raw, original.Bytes());
    else
        __imp__sub_82B7D270(raw, original.Bytes());
    const auto original_host_control = PPCFPSCRRegister{}.getcsr();
    Services services(spec);
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    auto memory = recovered.Memory();
    if (!family::Apply(spec.direct ? 0x822974f8u : 0x82b7d270u,
        memory, services, state))
        throw std::runtime_error("float parse routes recovered entry missing");
    const auto recovered_host_control = PPCFPSCRRegister{}.getcsr();
    const auto observed = FromPpc(raw);
    const unsigned expected_class = (spec.parse_flags & 4u) ? 0u : 1u;
    const unsigned expected_fp = !spec.direct &&
        (spec.fp_control & 0x8040u) ? 1u : 0u;
    const auto object = spec.direct ? Destination : Stack - 48u;
    const auto result_memory = original.Memory();
    const auto expected_value = (spec.parse_flags & 4u) ? 0ull :
        std::bit_cast<std::uint64_t>(spec.value);
    const bool fixture_path = original_parse_calls == 1u &&
        original_class_calls == expected_class &&
        result_memory.ReadU32(object) == spec.expected_flags &&
        result_memory.ReadU32(object + 4u) == spec.consumed &&
        (std::uint64_t{result_memory.ReadU32(object + 16u)} << 32 |
            result_memory.ReadU32(object + 20u)) == expected_value;
    if (!fixture_path)
        throw std::runtime_error("float parse routes fixture missed original PPC path");
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!Same(observed, state) || !ram_equal ||
        original_host_control != recovered_host_control ||
        services.parse_calls != original_parse_calls ||
        services.class_calls != original_class_calls ||
        services.fp_calls != expected_fp)
    {
        std::fprintf(stderr, "FAIL float-parse-routes mode=%u r3=%llX/%llX "
            "RAM=%u f1=%llX/%llX CSR=%X/%X host=%X/%X "
            "parse=%u/%u class=%u/%u fp=%u/%u\n",
            static_cast<unsigned>(spec.mode),
            static_cast<unsigned long long>(observed.integer.r[3]),
            static_cast<unsigned long long>(state.integer.r[3]),
            ram_equal, static_cast<unsigned long long>(observed.f1_bits),
            static_cast<unsigned long long>(state.f1_bits),
            observed.cached_fp_control, state.cached_fp_control,
            original_host_control, recovered_host_control,
            original_parse_calls, services.parse_calls,
            original_class_calls, services.class_calls,
            expected_fp, services.fp_calls);
        for (unsigned index = 0; index < 32u; ++index)
            if (observed.integer.r[index] != state.integer.r[index])
                std::fprintf(stderr, " r%u=%llX/%llX", index,
                    static_cast<unsigned long long>(observed.integer.r[index]),
                    static_cast<unsigned long long>(state.integer.r[index]));
        std::fprintf(stderr, "\n");
        return false;
    }
    return true;
}
} // namespace

void LegacyRouteParseBoundary(PPCContext& context, std::uint8_t* base)
{
    const auto& spec = *active;
    if (context.r3.u64 != context.r1.u64 + 96u ||
        context.r4.u64 != context.r1.u64 + 80u ||
        context.r5.u64 != ExpectedText(spec) ||
        context.r10.u64 != ExpectedMode(spec) ||
        context.r6.u64 != 0u || context.r7.u64 != 0u ||
        context.r8.u64 != 0u || context.r9.u64 != 0u ||
        context.lr != 0x82297538u)
        throw std::runtime_error("original parse PPC boundary inputs");
    PPC_STORE_U32(context.r4.u32, ExpectedEnd(spec));
    PPC_STORE_U16(context.r3.u32, 0x4000u);
    PPC_STORE_U32(context.r3.u32 + 2u, 0x12345678u);
    PPC_STORE_U32(context.r3.u32 + 6u, 0x9abcdef0u);
    PPC_STORE_U16(context.r3.u32 + 10u, 0x0102u);
    context.r3.u64 = spec.parse_flags;
    context.r6.u64 = 0x1234u;
    ++original_parse_calls;
}

void LegacyRouteClassifyBoundary(PPCContext& context, std::uint8_t* base)
{
    const auto& spec = *active;
    if (context.r3.u64 != context.r1.u64 + 96u ||
        context.r4.u64 != context.r1.u64 + 88u ||
        PPC_LOAD_U16(context.r3.u32) != 0x4000u ||
        context.lr != 0x82297560u)
        throw std::runtime_error("original classify PPC boundary inputs");
    PPC_STORE_U64(context.r4.u32,
        std::bit_cast<std::uint64_t>(spec.value));
    context.r3.u64 = spec.classification;
    context.r7.u64 = 0x5678u;
    ++original_class_calls;
}

void LegacyRouteInvalidArgument(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid-argument PPC body"); }

void LegacyRouteInvalidParameter(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid-parameter PPC body"); }

int main()
{
    try
    {
        for (const auto& spec : Specs)
            if (!Check(spec)) return 1;
        std::printf("PASS legacy-float-parse-routes %zu composed PPC cases\n",
            Specs.size());
        std::puts("LIMIT 822975B0/822981C8 PPC internals, null/error path, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
