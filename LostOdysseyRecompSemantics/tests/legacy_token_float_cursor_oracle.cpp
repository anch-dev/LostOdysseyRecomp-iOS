// Appended after both new PPC bodies, three accepted PPC callees, and exact
// save/restore helper bodies. 82B7D270 remains an explicit PPC guest boundary.
#include "lo_semantics/legacy_token_float_cursor.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_token_float_cursor;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Text = 0x20000u;
constexpr GuestAddress Token = 0x30000u;
constexpr GuestAddress Output = 0x40000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<Region, 1> Regions{{{0u, 0x90000u}}};
constexpr std::uint32_t InitialOutput = 0xdeadbeefu;

enum class Mode { Missing, ExactHit, BoundaryHit, PrefixMismatch,
    EmptyToken, ClearControlHit, DirectTail };
constexpr std::array<Mode, 7> Cases{{Mode::Missing, Mode::ExactHit,
    Mode::BoundaryHit, Mode::PrefixMismatch, Mode::EmptyToken,
    Mode::ClearControlHit, Mode::DirectTail}};

struct BoundaryExpectation
{
    std::uint64_t text = 0;
    double result = 0;
    unsigned calls = 0;
};
BoundaryExpectation original_boundary;

struct Services final : CrtThreadDataServices, InvalidParameterServices,
    family::NumberServices
{
    std::uint64_t expected_text = 0;
    double result = 0;
    unsigned conversions = 0, fp_control_calls = 0;

    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unexpected TLS read"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unexpected TLS write"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data getter"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected thread-data allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data binding"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data free"); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter handler"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter trap"); }
    double Convert(std::uint64_t text, std::uint64_t mode) override
    {
        if (mode != 0u || text != expected_text)
            throw std::runtime_error("recovered PPC conversion boundary arguments");
        ++conversions;
        return result;
    }
    void SetHostFpControl(std::uint32_t control) override
    {
        ++fp_control_calls;
        PPCFPSCRRegister{}.setcsr(control);
    }
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
    state.f0_bits = context.f0.u64;
    state.f1_bits = context.f1.u64;
    state.cached_fp_control = context.fpscr.csr;
    return state;
}

bool Same(const family::Registers& observed,
    const family::Registers& recovered, bool lower_miss)
{
    const auto& a = observed.integer;
    const auto& b = recovered.integer;
    for (unsigned index = 0; index < 32u; ++index)
    {
        if (index == 1u || (lower_miss && (index == 10u || index == 11u)))
            continue;
        if (a.r[index] != b.r[index]) return false;
    }
    if (a.sp != b.sp || a.lr != b.lr || a.ctr != b.ctr ||
        a.xer_so != b.xer_so || a.cr6.lt != b.cr6.lt ||
        a.cr6.gt != b.cr6.gt || a.cr6.eq != b.cr6.eq ||
        a.cr6.so != b.cr6.so) return false;
    if (!lower_miss && (a.xer_ca != b.xer_ca ||
        a.cr0.lt != b.cr0.lt || a.cr0.gt != b.cr0.gt ||
        a.cr0.eq != b.cr0.eq || a.cr0.so != b.cr0.so)) return false;
    return observed.f0_bits == recovered.f0_bits &&
        observed.f1_bits == recovered.f1_bits &&
        observed.cached_fp_control == recovered.cached_fp_control;
}

void WriteText(GuestMemory& memory, GuestAddress address, const char* value)
{
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*value));
        address += 2u;
    } while (*value++);
}

GuestAddress ExpectedText(Mode mode)
{
    switch (mode)
    {
    case Mode::ExactHit: case Mode::ClearControlHit: return Text + 6u;
    case Mode::BoundaryHit: return Text + 16u;
    case Mode::PrefixMismatch: return Text + 14u;
    case Mode::DirectTail: return Text;
    default: return 0u;
    }
}

double Result(Mode mode)
{ return mode == Mode::DirectTail ? -3.25 : 1.23456789012345; }

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Output, InitialOutput);
    switch (mode)
    {
    case Mode::Missing:
        WriteText(memory, Text, "alpha=2");
        WriteText(memory, Token, "beta");
        break;
    case Mode::ExactHit: case Mode::ClearControlHit:
        WriteText(memory, Text, "VAL12.5");
        WriteText(memory, Token, "val");
        break;
    case Mode::BoundaryHit:
        WriteText(memory, Text, "xVAL VAL -3.5");
        WriteText(memory, Token, "vAl");
        break;
    case Mode::PrefixMismatch:
        WriteText(memory, Text, "vax val4");
        WriteText(memory, Token, "VAL");
        break;
    case Mode::EmptyToken:
        WriteText(memory, Text, "VAL12.5");
        WriteText(memory, Token, "");
        break;
    case Mode::DirectTail:
        WriteText(memory, Text, "2.5");
        WriteText(memory, Token, "unused");
        break;
    }
}

PPCContext Initial(Mode mode)
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
    context.r3.u64 = 0x1234567800000000ull | Text;
    context.r4.u64 = 0x8765432100000000ull | Token;
    context.r5.u64 = 0x9988776600000000ull | Output;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    context.f0.u64 = 0x4009000000000000ull;
    context.f1.u64 = 0xc00a000000000000ull;
    context.fpscr.csr = mode == Mode::ClearControlHit ? 0x1f80u : 0x9fc0u;
    return context;
}

bool Check(Mode mode)
{
    RestoreHost restore;
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto raw = Initial(mode);
    auto state = FromPpc(raw);
    const auto expected_text = ExpectedText(mode) ?
        0x1234567800000000ull | ExpectedText(mode) : 0u;
    original_boundary = {expected_text, Result(mode), 0};
    PPCFPSCRRegister{}.setcsr(raw.fpscr.csr);
    if (mode == Mode::DirectTail)
        __imp__sub_822974A8(raw, original.Bytes());
    else
        __imp__sub_82297338(raw, original.Bytes());
    const auto original_host_control = PPCFPSCRRegister{}.getcsr();
    const unsigned expected_calls = expected_text ? 1u : 0u;
    if (original_boundary.calls != expected_calls ||
        (mode != Mode::DirectTail &&
            raw.r3.u64 != (expected_text ? 1u : 0u)))
        throw std::runtime_error("float cursor fixture missed original PPC path");
    Services services;
    services.expected_text = expected_text;
    services.result = Result(mode);
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    auto memory = recovered.Memory();
    if (!family::Apply(mode == Mode::DirectTail ? 0x822974a8u :
        0x82297338u, memory, services, services, services, state))
        throw std::runtime_error("float cursor recovered entry missing");
    const auto recovered_host_control = PPCFPSCRRegister{}.getcsr();
    const auto observed = FromPpc(raw);
    const bool lower_miss = !expected_text;
    const bool ram_equal = original.EqualCommitted(recovered);
    const bool fp_equal = original_host_control == recovered_host_control;
    const unsigned expected_control_calls = expected_text &&
        mode != Mode::DirectTail && mode != Mode::ClearControlHit ? 1u : 0u;
    if (!Same(observed, state, lower_miss) || !ram_equal || !fp_equal ||
        services.conversions != expected_calls ||
        services.fp_control_calls != expected_control_calls)
    {
        std::fprintf(stderr, "FAIL token-float mode=%u r3=%llX/%llX "
            "RAM=%u f0=%llX/%llX f1=%llX/%llX CSR=%X/%X host=%X/%X "
            "calls=%u/%u fp-calls=%u/%u\n", static_cast<unsigned>(mode),
            static_cast<unsigned long long>(observed.integer.r[3]),
            static_cast<unsigned long long>(state.integer.r[3]), ram_equal,
            static_cast<unsigned long long>(observed.f0_bits),
            static_cast<unsigned long long>(state.f0_bits),
            static_cast<unsigned long long>(observed.f1_bits),
            static_cast<unsigned long long>(state.f1_bits),
            observed.cached_fp_control, state.cached_fp_control,
            original_host_control, recovered_host_control,
            services.conversions, expected_calls,
            services.fp_control_calls, expected_control_calls);
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

void LegacyUnrecoveredPpcFloat(PPCContext& context, std::uint8_t*)
{
    if (context.r4.u64 != 0u ||
        context.r3.u64 != original_boundary.text)
        throw std::runtime_error("original PPC conversion boundary arguments");
    ++original_boundary.calls;
    context.f1.f64 = original_boundary.result;
}

void LegacyFloatInvalidArgument(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid argument"); }

void LegacyFloatInvalidParameter(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected original invalid parameter"); }

int main()
{
    try
    {
        for (const auto mode : Cases)
            if (!Check(mode)) return 1;
        std::printf("PASS legacy-token-float-cursor %zu composed PPC cases\n",
            Cases.size());
        std::puts("LIMIT 82B7D270 PPC numeric parse, lower volatile scratch on miss, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
