#include "lo_semantics/object_field_routes.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_field_routes;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Caller = 0x18000u, Parent = 0x20000u;
constexpr GuestAddress ParentList = 0x30000u;
constexpr GuestAddress ChildA = 0x40000u, ChildB = 0x42000u;
constexpr GuestAddress ChildListA = 0x50000u, ChildListB = 0x52000u;
constexpr GuestAddress NodeA = 0x60000u, NodeB = 0x62000u;
constexpr GuestAddress PropertyA = 0x70000u, PropertyB = 0x72000u;
constexpr GuestAddress BaseSingle = 0x82000e50u;
constexpr GuestAddress NegativeSingle = 0x82000e40u;
constexpr GuestAddress OptionalSingle = 0x82007784u;
constexpr test::Region Regions[] = {{0u, 0x100000u}, {0x82000000u, 0x8000u}};
enum class Mode { Null, Empty, Max, Negative };
constexpr Mode Cases[] = {Mode::Null, Mode::Empty, Mode::Max, Mode::Negative};

struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Services final : family::NativeServices
{
    unsigned fp_calls = 0;
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
    void CallGuest(GuestAddress, GuestMemory&, family::Registers&) override
    { throw std::runtime_error("unexpected chain guest boundary"); }
};
GuestMemory* active_memory = nullptr;

family::Registers FromPpc(const PPCContext& c)
{
    return {c.r1.u64, c.r3.u64, c.r4.u64, c.r5.u64,
        c.r10.u64, c.r11.u64, c.r12.u64, c.r31.u64, c.ctr.u64, c.lr,
        c.f0.u64, c.f1.u64, c.f13.u64, c.fpscr.csr, c.xer.so,
        {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un},
        c.r27.u64, c.r28.u64, c.r29.u64, c.r30.u64,
        c.f30.u64, c.f31.u64, c.xer.ca};
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.sp == b.sp && a.r3 == b.r3 && a.r4 == b.r4 && a.r5 == b.r5 &&
        a.r10 == b.r10 && a.r11 == b.r11 && a.r12 == b.r12 &&
        a.r31 == b.r31 && a.ctr == b.ctr && a.lr == b.lr &&
        a.f0_bits == b.f0_bits && a.f1_bits == b.f1_bits &&
        a.f13_bits == b.f13_bits &&
        a.cached_fp_control == b.cached_fp_control &&
        a.xer_so == b.xer_so && a.cr6 == b.cr6 &&
        a.r27 == b.r27 && a.r28 == b.r28 &&
        a.r29 == b.r29 && a.r30 == b.r30 &&
        a.f30_bits == b.f30_bits && a.f31_bits == b.f31_bits &&
        a.xer_ca == b.xer_ca;
}
void Initialize(Mode mode, GuestMemory& memory)
{
    memory.WriteU32(OptionalSingle, 0x40800000u);
    memory.WriteU32(BaseSingle, 0x3f800000u);
    memory.WriteU32(NegativeSingle, 0xbf800000u);
    memory.WriteU32(Caller + 124u, mode == Mode::Null ? 0u : Parent);
    memory.WriteU32(Parent + 116u, ParentList);
    memory.WriteU32(Parent + 120u,
        mode == Mode::Max || mode == Mode::Negative ? 2u : 0u);
    memory.WriteU32(ParentList, ChildA);
    memory.WriteU32(ParentList + 4u, ChildB);
    for (const auto [child, list, node, property] :
        {std::array<GuestAddress, 4>{ChildA, ChildListA, NodeA, PropertyA},
         std::array<GuestAddress, 4>{ChildB, ChildListB, NodeB, PropertyB}})
    {
        memory.WriteU32(child + 216u, list);
        memory.WriteU32(child + 220u, 1u);
        memory.WriteU32(list, node);
        memory.WriteU32(node + 72u, property);
        memory.WriteU32(property + 80u,
            mode == Mode::Max && child == ChildA ? 2u : 0u);
        memory.WriteU32(property + 72u,
            child == ChildB ? (mode == Mode::Negative ? 0xbf000000u : 0x3f000000u) :
            mode == Mode::Max ? 0x3fc00000u : 0x40200000u);
    }
}
void Check(Mode mode)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    auto left = original.Memory(), right = recovered.Memory();
    Initialize(mode, left); Initialize(mode, right);
    PPCContext context{};
    context.r1.u64 = 0x12345678000d0000ull;
    context.r3.u64 = 0x1122334400018000ull;
    context.r4.u64 = 0xaabbccdd00060000ull;
    context.r5.u64 = 0x5566778800040000ull;
    context.r10.u64 = 0x0102030405060708ull;
    context.r11.u64 = 0x1111222233334444ull;
    context.r12.u64 = 0x2222333344445555ull;
    context.r27.u64 = 0x1122334401020304ull;
    context.r28.u64 = 0x2233445502030405ull;
    context.r29.u64 = 0x3344556603040506ull;
    context.r30.u64 = 0x4455667704050607ull;
    context.r31.u64 = 0x778899aabbccddeeull;
    context.ctr.u64 = 0x4444555566667777ull;
    context.lr = 0x8899aabbccddeeffull;
    context.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    context.f1.u64 = std::bit_cast<std::uint64_t>(6.0);
    context.f13.u64 = std::bit_cast<std::uint64_t>(-8.0);
    context.f30.u64 = std::bit_cast<std::uint64_t>(-9.0);
    context.f31.u64 = std::bit_cast<std::uint64_t>(-10.0);
    context.fpscr.csr = 0x9fc0u;
    context.xer.so = 1; context.xer.ca = 1;
    auto state = FromPpc(context);
    Services services;

    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active_memory = &left;
    __imp__sub_822C60D0(context, original.Bytes());
    active_memory = nullptr;
    const auto original_host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(0x822c60d0u, right, services, state))
        throw std::runtime_error("missing object-field child chain");
    const auto recovered_host = PPCFPSCRRegister{}.getcsr();
    if (!Same(FromPpc(context), state) || services.fp_calls != 1u ||
        original_host != recovered_host || !original.EqualCommitted(recovered))
        throw std::runtime_error("object-field child chain original PPC comparison");
}
} // namespace

void OriginalSave27(PPCContext& c, std::uint8_t*)
{
    PPCRegister* fields[] = {&c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
    for (unsigned index = 0; index < 5u; ++index)
        WriteU64(*active_memory, c.r1.u32 - 48u + index * 8u,
            fields[index]->u64);
    active_memory->WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalSave29(PPCContext& c, std::uint8_t*)
{
    PPCRegister* fields[] = {&c.r29, &c.r30, &c.r31};
    for (unsigned index = 0; index < 3u; ++index)
        WriteU64(*active_memory, c.r1.u32 - 32u + index * 8u,
            fields[index]->u64);
    active_memory->WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t*)
{
    PPCRegister* fields[] = {&c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
    for (unsigned index = 0; index < 5u; ++index)
        fields[index]->u64 = ReadU64(*active_memory,
            c.r1.u32 - 48u + index * 8u);
    c.r12.u64 = active_memory->ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalRestore29(PPCContext& c, std::uint8_t*)
{
    PPCRegister* fields[] = {&c.r29, &c.r30, &c.r31};
    for (unsigned index = 0; index < 3u; ++index)
        fields[index]->u64 = ReadU64(*active_memory,
            c.r1.u32 - 32u + index * 8u);
    c.r12.u64 = active_memory->ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalVerify(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected 82384C08 in caller chain"); }
void OriginalIndirect(std::uint32_t, PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected virtual call in caller chain"); }
int main()
{
    try
    {
        for (const auto mode : Cases) Check(mode);
        std::printf("PASS object-field-child-chain %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT unexposed volatile registers at guest-call boundaries; 82384C08, virtual calls, NaNs, exceptions, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
