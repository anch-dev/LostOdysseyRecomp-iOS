// Appended after the complete line loop, manager query, UTF-16 line/prefix
// and registered singleton getter PPC bodies.
#include "lo_semantics/legacy_config_line_dispatch.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_config_line_dispatch;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Text = 0x30000u;
constexpr GuestAddress Object = 0x50000u;
constexpr GuestAddress Vtable = 0x65000u;
constexpr GuestAddress Singleton = 0x60000u;
constexpr GuestAddress Sink = 0x70000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Prefix = 0x8220bfb8u;
constexpr GuestAddress GetterGlobal = 0x833181e4u;
constexpr GuestAddress PrimaryTarget = 0x91000010u;
constexpr GuestAddress FallbackTarget = 0x91000020u;
constexpr std::array<Region, 3> Regions{{{0u, 0x90000u},
    {0x8220b000u, 0x1000u}, {0x83318000u, 0x1000u}}};

enum class Mode { QueryNull, QueryLinked, Empty, Primary,
    Fallback, OnRelease, OtherModeOne, TwoLines };
constexpr std::array Cases{Mode::QueryNull, Mode::QueryLinked,
    Mode::Empty, Mode::Primary, Mode::Fallback, Mode::OnRelease,
    Mode::OtherModeOne, Mode::TwoLines};

bool Query(Mode mode)
{ return mode == Mode::QueryNull || mode == Mode::QueryLinked; }

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
    for (unsigned i = 0; i < 32; ++i)
        state.r[i] = Gpr(context, i)->u64;
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

void WriteText(GuestMemory& memory, GuestAddress address, const char* text)
{
    do
    {
        memory.WriteU16(address, static_cast<unsigned char>(*text));
        address += 2u;
    } while (*text++);
}

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    WriteText(memory, Prefix, "OnRelease");
    const char* line = mode == Mode::Empty ? "" :
        mode == Mode::OnRelease ? "OnRelease arg" :
        mode == Mode::OtherModeOne ? "hello" :
        mode == Mode::TwoLines ? "alpha|beta" : "alpha";
    WriteText(memory, Text, line);
    memory.WriteU32(Object, Vtable);
    memory.WriteU32(Vtable + 260u, PrimaryTarget);
    memory.WriteU32(Vtable + 284u, FallbackTarget);
    memory.WriteU8(Object + 96u,
        mode == Mode::OnRelease || mode == Mode::OtherModeOne ? 1u : 0u);
    if (mode == Mode::QueryLinked)
    {
        memory.WriteU32(GetterGlobal, Singleton);
        memory.WriteU32(Object + 52u, Singleton);
    }
}

PPCContext Initial(Mode mode)
{
    PPCContext context{};
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = 0x1122334400000000ull + i;
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.r3.u64 = Query(mode) ?
        (mode == Mode::QueryNull ? 0u :
            0x9988776600000000ull | Object) :
        0x9988776600000000ull | Object;
    context.r4.u64 = 0x8877665500000000ull | Text;
    context.r5.u64 = 0x7766554400000000ull | Sink;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

std::uint32_t ExpectedCalls(Mode mode)
{
    switch (mode)
    {
    case Mode::QueryNull:
    case Mode::QueryLinked:
    case Mode::Empty: return 0u;
    case Mode::Fallback: return 2u;
    case Mode::TwoLines: return 2u;
    default: return 1u;
    }
}

void VirtualAction(Mode mode, GuestAddress target, GuestMemory& memory,
    family::Registers& state, unsigned ordinal)
{
    const auto first = memory.ReadU16(static_cast<GuestAddress>(state.r[4]));
    memory.WriteU32(Sink + 8u * ordinal, target);
    memory.WriteU32(Sink + 8u * ordinal + 4u, first);
    state.r[11] = target;
    state.r[3] = mode == Mode::Fallback && ordinal == 0u ? 0u : 1u;
}

struct Services final : CrtThreadDataServices, InvalidParameterServices,
    ManagerFacadeServices,
    registered_constructor_family::RegistrationServices,
    family::VirtualCalls
{
    explicit Services(Mode mode) : mode_(mode) {}
    Mode mode_;
    unsigned calls = 0;
    void Call(GuestAddress target, GuestMemory& memory,
        family::Registers& state) override
    { VirtualAction(mode_, target, memory, state, calls++); }
    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unselected TLS read"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unselected TLS write"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unselected thread getter"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unselected thread allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unselected thread binding"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unselected thread free"); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override
    { throw std::runtime_error("unselected invalid handler"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unselected invalid trap"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unselected allocator"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unselected constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unselected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unselected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unselected release"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unselected allocation"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unselected resize"); }
    std::uint64_t Register(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("unselected registration"); }
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

void Expected(Mode mode, const GuestWindow& window,
    const PPCContext& context, unsigned calls)
{
    const auto expected_result = mode == Mode::QueryLinked ?
        (0x9988776600000000ull | Object) : 0u;
    const auto expected_primary = mode != Mode::OtherModeOne;
    const auto memory = window.Memory();
    if (context.r3.u64 != expected_result || calls != ExpectedCalls(mode) ||
        (calls != 0u && memory.ReadU32(Sink) !=
            (expected_primary ? PrimaryTarget : FallbackTarget)))
    {
        std::fprintf(stderr, "EXPECT line case=%u r3=%llX/%llX "
            "calls=%u/%u first=%X cursor=%X\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(expected_result),
            calls, ExpectedCalls(mode), memory.ReadU32(Sink),
            memory.ReadU32(Stack + 28u));
        throw std::runtime_error("independent config line outcome");
    }
    if (mode == Mode::OnRelease && memory.ReadU32(Sink + 4u) != 'a')
        throw std::runtime_error("independent OnRelease cursor");
}

unsigned OriginalCalls = 0;
Mode OriginalMode = Mode::Empty;

void Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto context = Initial(mode);
    auto state = FromPpc(context);
    OriginalMode = mode;
    OriginalCalls = 0;
    if (Query(mode)) __imp__sub_8231A220(context, original.Bytes());
    else __imp__sub_82295DA8(context, original.Bytes());
    Expected(mode, original, context, OriginalCalls);
    Services services(mode);
    auto memory = recovered.Memory();
    family::Dependencies dependencies{services, services, services,
        services, services};
    if (!family::Apply(Query(mode) ? 0x8231a220u : 0x82295da8u,
        memory, dependencies, state))
        throw std::runtime_error("config line entry missing");
    const auto observed = FromPpc(context);
    const bool state_equal = Same(observed, state);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!state_equal || !ram_equal || services.calls != OriginalCalls)
    {
        std::fprintf(stderr, "FAIL line case=%u state=%u RAM=%u "
            "calls=%u/%u r3=%llX/%llX r11=%llX/%llX\n",
            static_cast<unsigned>(mode), state_equal, ram_equal,
            OriginalCalls, services.calls,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[11]),
            static_cast<unsigned long long>(state.r[11]));
        for (unsigned i = 0; i < 32; ++i)
            if (observed.r[i] != state.r[i])
                std::fprintf(stderr, " r%u=%llX/%llX", i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fputc('\n', stderr);
        throw std::runtime_error("config line PPC mismatch");
    }
}
} // namespace

void OriginalSaveLine(PPCContext& context, std::uint8_t* base)
{
    for (unsigned i = 27; i <= 31; ++i)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - i),
            Gpr(context, i)->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void OriginalRestoreLine(PPCContext& context, std::uint8_t* base)
{
    for (unsigned i = 27; i <= 31; ++i)
        Gpr(context, i)->u64 = PPC_LOAD_U64(
            context.r1.u32 - 8u * (33u - i));
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}

void OriginalVirtualLine(PPCContext& context, std::uint8_t* base)
{
    auto state = FromPpc(context);
    auto memory = GuestMemory(0u, std::span<std::uint8_t>(base,
        GuestWindow::Space));
    VirtualAction(OriginalMode, context.ctr.u32 & ~3u, memory,
        state, OriginalCalls++);
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = state.r[i];
    context.r1.u64 = state.sp;
    context.lr = state.lr;
    context.ctr.u64 = state.ctr;
}

void OriginalUnselectedLine(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unselected original config line lower"); }

int main()
{
    try
    {
        for (const auto mode : Cases) Check(mode);
        std::printf("PASS legacy-config-line-dispatch %zu original PPC cases\n",
            Cases.size());
        std::puts("LIMIT three mutable vtable calls, unselected lazy singleton construction, faults/MMIO/runtime");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
