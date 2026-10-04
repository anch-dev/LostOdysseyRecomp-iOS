// Appended after the complete 823ADDC0 and 823ACC98 PPC bodies.
#include "lo_semantics/crt_free_context.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_free_context;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Payload = 0x40000u;
constexpr GuestAddress Heap = 0x50000u;
constexpr GuestAddress Error = 0x60000u;
constexpr GuestAddress Sink = 0x70000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress HeapGlobal = 0x83245708u;
constexpr std::array<Region, 2> Regions{{{0u, 0x90000u},
    {0x83245000u, 0x1000u}}};

enum class Mode { ZeroLowWord, HeapSuccess, HeapFailure };
constexpr std::array Cases{Mode::ZeroLowWord, Mode::HeapSuccess,
    Mode::HeapFailure};

PPCRegister* Gpr(PPCContext& context, unsigned index)
{
    PPCRegister* const fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16,
        &context.r17, &context.r18, &context.r19, &context.r20,
        &context.r21, &context.r22, &context.r23, &context.r24,
        &context.r25, &context.r26, &context.r27, &context.r28,
        &context.r29, &context.r30, &context.r31};
    return fields[index];
}

family::Registers FromPpc(PPCContext& context)
{
    family::Registers state{};
    for (unsigned i = 0; i < 32; ++i) state.r[i] = Gpr(context, i)->u64;
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

void ToPpc(PPCContext& context, const family::Registers& state)
{
    for (unsigned i = 0; i < 32; ++i) Gpr(context, i)->u64 = state.r[i];
    context.r1.u64 = state.sp;
    context.lr = state.lr;
    context.ctr.u64 = state.ctr;
    context.xer.so = state.xer_so;
    context.xer.ca = state.xer_ca;
    context.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq,
        state.cr0.so};
    context.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq,
        state.cr6.so};
}

void Seed(GuestWindow& window)
{
    window.Fill(0);
    window.Memory().WriteU32(HeapGlobal, Heap);
}

PPCContext Initial(Mode mode)
{
    PPCContext context{};
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = 0x1122334400000000ull + i;
    context.r1.u64 = 0x8877665500000000ull | Stack;
    context.r3.u64 = 0xaabbccdd00000000ull |
        (mode == Mode::ZeroLowWord ? 0u : Payload);
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.lt = 1;
    context.cr6.gt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

void LowerAction(GuestAddress entry, Mode mode, GuestMemory& memory,
    family::Registers& state)
{
    if (entry == 0x823ade28u)
    {
        if (state.r[3] != Heap || state.r[4] != 0u ||
            static_cast<GuestAddress>(state.r[5]) != Payload)
            throw std::runtime_error("heap free selected input");
        memory.WriteU32(Sink, Payload);
        state.r[3] = mode == Mode::HeapSuccess ? 1u : 0u;
        state.r[11] = 0x5678u;
        state.cr6 = {1u, 0u, 0u, state.xer_so};
        return;
    }
    if (mode != Mode::HeapFailure)
        throw std::runtime_error("unexpected free error lower");
    switch (entry)
    {
    case 0x82b7fd78u:
        state.r[3] = Error;
        state.r[11] = 0x101u;
        return;
    case 0x822ca100u:
        state.r[3] = 0x20u;
        state.r[10] = 0x102u;
        return;
    case 0x82b7fd10u:
        state.r[3] = 0x31u;
        state.r[9] = 0x103u;
        return;
    default: throw std::runtime_error("unknown free lower");
    }
}

struct Services final : family::LowerCalls
{
    explicit Services(Mode mode) : mode_(mode) {}
    Mode mode_;
    unsigned calls = 0;
    void Call(GuestAddress entry, GuestMemory& memory,
        family::Registers& state) override
    { ++calls; LowerAction(entry, mode_, memory, state); }
};

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

Mode OriginalMode = Mode::ZeroLowWord;
unsigned OriginalCalls = 0;

void Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original);
    Seed(recovered);
    auto context = Initial(mode);
    auto state = FromPpc(context);
    OriginalMode = mode;
    OriginalCalls = 0;
    __imp__sub_823ADDC0(context, original.Bytes());
    const auto expected_result = mode == Mode::ZeroLowWord ?
        0xaabbccdd00000000ull :
        mode == Mode::HeapSuccess ? 1ull : 0x31ull;
    const auto expected_calls = mode == Mode::ZeroLowWord ? 0u :
        mode == Mode::HeapSuccess ? 1u : 4u;
    if (context.r3.u64 != expected_result ||
        OriginalCalls != expected_calls ||
        (mode != Mode::ZeroLowWord &&
            original.Memory().ReadU32(Sink) != Payload) ||
        (mode == Mode::HeapFailure &&
            original.Memory().ReadU32(Error) != 0x31u))
    {
        std::fprintf(stderr, "EXPECT free case=%u r3=%llX/%llX "
            "calls=%u/%u\n", static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(expected_result),
            OriginalCalls, expected_calls);
        throw std::runtime_error("independent free context outcome");
    }
    Services services(mode);
    auto memory = recovered.Memory();
    if (!family::Apply(0x823addc0u, memory, services, state))
        throw std::runtime_error("free context entry missing");
    const auto observed = FromPpc(context);
    const bool state_equal = Same(observed, state);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!state_equal || !ram_equal || OriginalCalls != services.calls)
    {
        std::fprintf(stderr, "FAIL free case=%u state=%u RAM=%u "
            "calls=%u/%u r3=%llX/%llX CR0=%u%u%u%u/%u%u%u%u\n",
            static_cast<unsigned>(mode), state_equal, ram_equal,
            OriginalCalls, services.calls,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            observed.cr0.lt, observed.cr0.gt, observed.cr0.eq,
            observed.cr0.so, state.cr0.lt, state.cr0.gt, state.cr0.eq,
            state.cr0.so);
        for (unsigned i = 0; i < 32; ++i)
            if (observed.r[i] != state.r[i])
                std::fprintf(stderr, " r%u=%llX/%llX", i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fputc('\n', stderr);
        throw std::runtime_error("free context PPC mismatch");
    }
}
} // namespace

void OriginalFreeLower(GuestAddress entry, PPCContext& context,
    std::uint8_t* base)
{
    auto state = FromPpc(context);
    auto memory = GuestMemory(0u,
        std::span<std::uint8_t>(base, GuestWindow::Space));
    LowerAction(entry, OriginalMode, memory, state);
    ++OriginalCalls;
    ToPpc(context, state);
}

int main()
{
    try
    {
        for (const auto mode : Cases) Check(mode);
        std::printf("PASS crt-free-context %zu original PPC cases\n",
            Cases.size());
        std::puts("LIMIT mutable heap free and error lower context; faults/MMIO/runtime");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
