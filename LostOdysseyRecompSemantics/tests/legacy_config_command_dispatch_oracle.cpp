// Appended after 82296008 and the six reachable accepted lower PPC bodies.
// Five other direct guest callees remain explicit mutable boundaries.
#include "lo_semantics/legacy_config_command_dispatch.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_config_command_dispatch;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Text = 0x30000u;
constexpr GuestAddress Object = 0x50000u;
constexpr GuestAddress Flag = Object + 0x200u;
constexpr GuestAddress Axis = Object + 0x300u;
constexpr GuestAddress Sink = 0x60000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress LocalCursor = Stack + 28u;
constexpr std::array<Region, 5> Regions{{
    {0u, 0x90000u}, {0x82001000u, 0x1000u},
    {0x8220c000u, 0x1000u}, {0x83246000u, 0x1000u},
    {0x8336c000u, 0x1000u}}};

enum class Mode { ButtonSet, ButtonClear, PulseSet, Toggle, AxisGuard,
    Unknown };
constexpr std::array Cases{Mode::ButtonSet, Mode::ButtonClear,
    Mode::PulseSet, Mode::Toggle, Mode::AxisGuard, Mode::Unknown};

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

const PPCRegister* Gpr(const PPCContext& context, unsigned index)
{ return Gpr(const_cast<PPCContext&>(context), index); }

const PPCRegister* Fpr(const PPCContext& context, unsigned index)
{
    const PPCRegister* const fields[] = {&context.f0, &context.f1,
        &context.f2, &context.f3, &context.f4, &context.f5, &context.f6,
        &context.f7, &context.f8, &context.f9, &context.f10, &context.f11,
        &context.f12, &context.f13, &context.f14, &context.f15,
        &context.f16, &context.f17, &context.f18, &context.f19,
        &context.f20, &context.f21, &context.f22, &context.f23,
        &context.f24, &context.f25, &context.f26, &context.f27,
        &context.f28, &context.f29, &context.f30, &context.f31};
    return fields[index];
}

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    for (unsigned i = 0; i < 32; ++i)
    {
        state.integer.r[i] = Gpr(context, i)->u64;
        state.fpr_bits[i] = Fpr(context, i)->u64;
    }
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
    return state;
}

void WriteText(GuestMemory& memory, GuestAddress address,
    const char* text)
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
    WriteText(memory, 0x8200132cu, "BUTTON");
    WriteText(memory, 0x82001204u, "PULSE");
    WriteText(memory, 0x8200137cu, "TOGGLE");
    WriteText(memory, 0x820011f8u, "AXIS");
    WriteText(memory, 0x8220c044u, "COUNT");
    WriteText(memory, 0x8220c07cu, "KEYBINDING");
    const char* input = mode == Mode::ButtonSet ||
        mode == Mode::ButtonClear ? "BUTTON A" :
        mode == Mode::PulseSet ? "PULSE A" :
        mode == Mode::Toggle ? "TOGGLE A" :
        mode == Mode::AxisGuard ? "AXIS A" : "UNKNOWN A";
    WriteText(memory, Text, input);
    memory.WriteU8(Object + 96u, mode == Mode::ButtonClear ? 1u : 0u);
    memory.WriteU8(Flag, mode == Mode::ButtonClear ||
        mode == Mode::Toggle ? 1u : 0u);
    memory.WriteU32(0x8336ceecu, 1u); // Skip the unselected keybinding scan.
}

PPCContext Initial()
{
    PPCContext context{};
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = 0x1122334400000000ull + i;
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.r3.u64 = 0x9988776600000000ull | Object;
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

void GuestCall(GuestAddress entry, GuestMemory& memory,
    family::Registers& state)
{
    const auto token = memory.ReadU16(static_cast<GuestAddress>(
        state.integer.r[4]));
    state.integer.r[11] = token;
    if (entry == 0x82713828u)
        state.integer.r[3] = token == 'A' ?
            0x8877665500000000ull | Flag : 0u;
    else if (entry == 0x82296b00u)
        state.integer.r[3] = token == 'A' ?
            0x8877665500000000ull | Axis : 0u;
    else throw std::runtime_error("unselected original guest callee");
}

struct Services final : CrtThreadDataServices, InvalidParameterServices,
    metadata_name_record::Services, ArrayResizeServices,
    legacy_token_float_cursor::NumberServices, family::GuestBoundaries
{
    void Call(GuestAddress entry, GuestMemory& memory,
        family::Registers& state) override { GuestCall(entry, memory, state); }
    void SetHostFpControl(std::uint32_t) override
    { throw std::runtime_error("unselected FPSCR control change"); }
    double Convert(std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unselected numeric conversion"); }
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
    { throw std::runtime_error("unselected manager allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unselected manager constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unselected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unselected manager method"); }
    std::uint64_t AllocateRecord(GuestAddress, GuestMemory&,
        std::uint64_t, std::uint64_t, std::uint64_t,
        std::uint64_t, metadata_name_record::FrameRegisters&) override
    { throw std::runtime_error("unselected record allocation"); }
    void InitializeManager() override
    { throw std::runtime_error("unselected array manager"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unselected array resize"); }
};

bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.integer;
    const auto& y = b.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr &&
        x.ctr == y.ctr && x.xer_so == y.xer_so &&
        x.xer_ca == y.xer_ca && a.fpr_bits == b.fpr_bits &&
        x.cr0.lt == y.cr0.lt && x.cr0.gt == y.cr0.gt &&
        x.cr0.eq == y.cr0.eq && x.cr0.so == y.cr0.so &&
        x.cr6.lt == y.cr6.lt && x.cr6.gt == y.cr6.gt &&
        x.cr6.eq == y.cr6.eq && x.cr6.so == y.cr6.so;
}

void Expected(Mode mode, const GuestWindow& window,
    const PPCContext& context)
{
    const auto memory = window.Memory();
    const auto expected_status = mode == Mode::Unknown ? 0u : 1u;
    const auto expected_flag = mode == Mode::Toggle ? 129u :
        mode == Mode::ButtonSet || mode == Mode::PulseSet ? 1u : 0u;
    if (context.r3.u64 != expected_status ||
        memory.ReadU8(Flag) != expected_flag)
    {
        std::fprintf(stderr, "EXPECT config case=%u r3=%llX/%X "
            "flag=%X/%X cursor=%08X\n", static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            expected_status, memory.ReadU8(Flag), expected_flag,
            memory.ReadU32(LocalCursor));
        throw std::runtime_error("independent config command result");
    }
}

void Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto context = Initial();
    auto state = FromPpc(context);
    __imp__sub_82296008(context, original.Bytes());
    Expected(mode, original, context);
    Services services;
    auto memory = recovered.Memory();
    family::Dependencies dependencies{services, services, services,
        services, services, services};
    if (!family::Apply(0x82296008u, memory, dependencies, state))
        throw std::runtime_error("command dispatch entry missing");
    const auto observed = FromPpc(context);
    const bool state_equal = Same(observed, state);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!state_equal || !ram_equal)
    {
        std::fprintf(stderr, "FAIL config case=%u state=%u RAM=%u "
            "r3=%llX/%llX r11=%llX/%llX\n",
            static_cast<unsigned>(mode), state_equal, ram_equal,
            static_cast<unsigned long long>(observed.integer.r[3]),
            static_cast<unsigned long long>(state.integer.r[3]),
            static_cast<unsigned long long>(observed.integer.r[11]),
            static_cast<unsigned long long>(state.integer.r[11]));
        for (unsigned i = 0; i < 32; ++i)
            if (observed.integer.r[i] != state.integer.r[i])
                std::fprintf(stderr, " r%u=%llX/%llX", i,
                    static_cast<unsigned long long>(observed.integer.r[i]),
                    static_cast<unsigned long long>(state.integer.r[i]));
        std::fputc('\n', stderr);
        throw std::runtime_error("config command PPC mismatch");
    }
}
} // namespace

void OriginalSave26(PPCContext& context, std::uint8_t* base)
{
    for (unsigned i = 26; i <= 31; ++i)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - i),
            Gpr(context, i)->u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}

void OriginalRestore26(PPCContext& context, std::uint8_t* base)
{
    for (unsigned i = 26; i <= 31; ++i)
        Gpr(context, i)->u64 = PPC_LOAD_U64(
            context.r1.u32 - 8u * (33u - i));
    context.r12.u64 = PPC_LOAD_U32(context.r1.u32 - 8u);
    context.lr = context.r12.u64;
}

void OriginalGuest(lo::semantic::gpu::GuestAddress entry, PPCContext& context,
    std::uint8_t* base)
{
    family::Registers state = FromPpc(context);
    auto memory = lo::semantic::gpu::GuestMemory(0u,
        std::span<std::uint8_t>(base,
        GuestWindow::Space));
    GuestCall(entry, memory, state);
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = state.integer.r[i];
    context.r1.u64 = state.integer.sp;
    context.lr = state.integer.lr;
    context.ctr.u64 = state.integer.ctr;
    context.r11.u64 = state.integer.r[11];
}

void OriginalUnselected(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unselected original PPC lower"); }

int main()
{
    try
    {
        for (const auto mode : Cases) Check(mode);
        std::printf("PASS legacy-config-command-dispatch %zu PPC cases\n",
            Cases.size());
        std::puts("LIMIT five mutable guest callees, unselected registry/numeric paths, faults/MMIO/runtime");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
