#include "lo_semantics/object_record_update.h"
#include "semantic_oracle_support.h"
#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_record_update;
constexpr GuestAddress Object = 0x20000u, Record = 0x30000u;
constexpr GuestAddress Output = 0x50000u, Delta = 0x60000u;
constexpr std::array<test::Region, 2> Regions{{{0u, 0x90000u}, {0x82000000u, 0x10000u}}};
enum class Mode { NoObjectFlag, NoPartnerFlag, Type10, Type7, OtherType,
    NullRecord, NullSkipped, Active, Clamp, AliasSkipped };
constexpr std::array<Mode, 10> Cases{{Mode::NoObjectFlag, Mode::NoPartnerFlag,
    Mode::Type10, Mode::Type7, Mode::OtherType, Mode::NullRecord,
    Mode::NullSkipped, Mode::Active, Mode::Clamp, Mode::AliasSkipped}};
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Services final : family::NativeServices
{
    unsigned calls = 0;
    void SetHostFpControl(std::uint32_t control) override
    { ++calls; PPCFPSCRRegister{}.setcsr(control); }
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
    state.f0_bits = context.f0.u64;
    state.cached_fp_control = context.fpscr.csr;
    return state;
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.f0_bits == b.f0_bits && a.cached_fp_control == b.cached_fp_control &&
        a.r == b.r && a.sp == b.sp && a.lr == b.lr && a.ctr == b.ctr &&
        a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

bool Predicate(Mode mode) { return mode <= Mode::OtherType; }
void Seed(test::GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Object + 112u, mode == Mode::NoObjectFlag ? 0u : 0x80000u);
    memory.WriteU32(Output + 116u, mode == Mode::NoPartnerFlag ? 0u : 0x4000u);
    memory.WriteU8(Output + 84u, mode == Mode::Type10 ? 10u : mode == Mode::Type7 ? 7u : 3u);
    memory.WriteU32(Object + 528u, mode >= Mode::Active ? Record : 0u);
    memory.WriteU32(Object + 548u,
        (mode == Mode::NullSkipped || mode == Mode::AliasSkipped) ? 0x2000000u : 0u);
    memory.WriteU32(Object + 268u, 0x12345678u);
    memory.WriteU32(Output + 8u, 0xeeeeeeeeu);
    memory.WriteU32(0x82000e50u, 0x80000000u);
    memory.WriteU32(Record + 260u, mode >= Mode::Clamp ? 0xfffffffeu : 1u);
    memory.WriteU32(Record + 264u, 2u);
    memory.WriteU32(Record + 268u, mode == Mode::Clamp ? 0x7fffffffu :
        mode == Mode::AliasSkipped ? 0x80000000u : 3u);
    memory.WriteU32(Delta, 4u);
    memory.WriteU32(Delta + 4u, 5u);
    memory.WriteU32(Delta + 8u, 6u);
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
    for (unsigned index = 0; index < 32; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    context.r1.u64 = 0x1234567800080000ull;
    context.r3.u64 = 0x8765432100000000ull | Object;
    context.r4.u64 = 0x5566778800000000ull | Output;
    context.r5.u64 = 0x778899aa00000000ull |
        (mode == Mode::AliasSkipped ? Record + 260u : Delta);
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x1122334455667788ull;
    context.f0.f64 = 5.5;
    context.fpscr.csr = 0x9fc0u;
    context.xer.so = 1;
    context.xer.ca = 1;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    return context;
}
bool Expected(Mode mode, const PPCContext& context, const test::GuestWindow& window)
{
    if (Predicate(mode))
        return context.r3.u64 == ((mode == Mode::Type10 || mode == Mode::Type7) ? 1u : 0u);
    const auto memory = window.Memory();
    const auto output = (mode == Mode::NullSkipped || mode == Mode::AliasSkipped) ?
        0xeeeeeeeeu : 0x12345678u;
    if (memory.ReadU32(Output + 8u) != output) return false;
    if (mode >= Mode::Active)
    {
        const auto first = mode == Mode::Active ? 5u :
            mode == Mode::Clamp ? 2u : 0xfffffffcu;
        const auto second = mode == Mode::AliasSkipped ? 4u : 7u;
        const auto third = mode == Mode::Active ? 3u : mode == Mode::Clamp ? 0x7fffffffu : 0u;
        if (memory.ReadU32(Record + 260u) != first ||
            memory.ReadU32(Record + 264u) != second ||
            memory.ReadU32(Record + 268u) != third) return false;
    }
    return true;
}
void Check(Mode mode)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode); Seed(recovered, mode);
    auto raw = Initial(mode);
    auto state = FromPpc(raw);
    PPCFPSCRRegister{}.setcsr(0x9fc0u);
    if (Predicate(mode)) __imp__sub_8237DC28(raw, original.Bytes());
    else __imp__sub_8237DC70(raw, original.Bytes());
    const auto original_host = PPCFPSCRRegister{}.getcsr();
    if (!Expected(mode, raw, original))
        throw std::runtime_error("record update fixture missed original path");
    PPCFPSCRRegister{}.setcsr(0x9fc0u);
    Services native;
    auto memory = recovered.Memory();
    const auto entry = Predicate(mode) ? 0x8237dc28u : 0x8237dc70u;
    if (!family::Apply(entry, memory, native, state) ||
        !Same(FromPpc(raw), state) || !original.EqualCommitted(recovered) ||
        native.calls != unsigned(!Predicate(mode)) ||
        original_host != PPCFPSCRRegister{}.getcsr())
    {
        std::fprintf(stderr, "FAIL mode=%u f0=%llX/%llX host=%X/%X calls=%u\n",
            unsigned(mode), static_cast<unsigned long long>(raw.f0.u64),
            static_cast<unsigned long long>(state.f0_bits), original_host,
            PPCFPSCRRegister{}.getcsr(), native.calls);
        throw std::runtime_error("record update original/recovered mismatch");
    }
}
}
int main()
{
    try
    {
        for (auto mode : Cases) Check(mode);
        std::printf("PASS object-record-update %zu focused original PPC cases\n", Cases.size());
        std::puts("LIMIT selected GPR/CR/XER/FPR/cache and x64 host control; other FP/alias inputs, host exceptions, faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
