#include "lo_semantics/object_child_float.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_child_float;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Parent = 0x20000u, ParentList = 0x30000u;
constexpr GuestAddress ChildA = 0x40000u, ChildB = 0x42000u;
constexpr GuestAddress ChildListA = 0x50000u, ChildListB = 0x52000u;
constexpr GuestAddress NodeA = 0x60000u, NodeB = 0x62000u;
constexpr GuestAddress PropertyA = 0x70000u, PropertyB = 0x72000u;
constexpr GuestAddress Flag = 0x80000u, OptionalList = 0x90000u;
constexpr GuestAddress Candidate = 0xa0000u, VTable = 0xb0000u;
constexpr GuestAddress BaseSingle = 0x82000e50u, NegativeSingle = 0x82000e40u;
constexpr test::Region Regions[] = {{0u, 0x100000u}, {0x82000000u, 0x1000u}};
enum class Mode { ParentFlag, ParentEmpty, ParentMax, ParentNegative,
    ChildScaled, ChildOptional, ChildEarly };
struct Case { GuestAddress entry; Mode mode; };
constexpr Case Cases[] = {
    {0x822c5e58u, Mode::ParentFlag},
    {0x822c5e58u, Mode::ParentEmpty},
    {0x822c5e58u, Mode::ParentMax},
    {0x822c5e58u, Mode::ParentNegative},
    {0x822c5f28u, Mode::ChildScaled},
    {0x822c5f28u, Mode::ChildOptional},
    {0x822c5f28u, Mode::ChildEarly}
};

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
        state.r[index] = fields[index]->u64;
    state.lr = c.lr; state.ctr = c.ctr.u64;
    state.f0_bits = c.f0.u64; state.f1_bits = c.f1.u64;
    state.f13_bits = c.f13.u64; state.f30_bits = c.f30.u64;
    state.f31_bits = c.f31.u64; state.cached_fp_control = c.fpscr.csr;
    state.xer_so = c.xer.so; state.xer_ca = c.xer.ca;
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return state;
}
void ToPpc(PPCContext& c, const family::Registers& state)
{
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = state.r[index];
    c.lr = state.lr; c.ctr.u64 = state.ctr;
    c.f0.u64 = state.f0_bits; c.f1.u64 = state.f1_bits;
    c.f13.u64 = state.f13_bits; c.f30.u64 = state.f30_bits;
    c.f31.u64 = state.f31_bits; c.fpscr.csr = state.cached_fp_control;
    c.xer.so = state.xer_so; c.xer.ca = state.xer_ca;
    c.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, {state.cr6.un}};
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.lr == b.lr && a.ctr == b.ctr &&
        a.f0_bits == b.f0_bits && a.f1_bits == b.f1_bits &&
        a.f13_bits == b.f13_bits && a.f30_bits == b.f30_bits &&
        a.f31_bits == b.f31_bits &&
        a.cached_fp_control == b.cached_fp_control &&
        a.xer_so == b.xer_so && a.xer_ca == b.xer_ca && a.cr6 == b.cr6;
}
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
using Event = std::array<std::uint64_t, 8>;
struct Services final : family::NativeServices
{
    GuestMemory memory;
    unsigned fp_calls = 0;
    std::vector<Event> events;
    explicit Services(test::GuestWindow& window) : memory(window.Memory()) {}
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
    void CallGuest(GuestAddress target, GuestMemory&, family::Registers& state) override
    {
        events.push_back({target, state.r[1], state.lr, state.r[3],
            state.r[4], state.r[5], state.ctr, state.f1_bits});
        if (target == 0x82384c08u)
            state.r[3] = Candidate;
        else if (target == 0xc0000u)
            state.f1_bits = std::bit_cast<std::uint64_t>(0.5);
        else
            throw std::runtime_error("unexpected object-child guest target");
        state.r[10] = 0x1122334400005678ull;
    }
};
Services* active = nullptr;

void Initialize(const Case& item, GuestMemory& memory)
{
    memory.WriteU32(BaseSingle, 0x3f800000u);
    memory.WriteU32(NegativeSingle, 0xbf800000u);
    memory.WriteU32(Parent + 244u, Flag);
    memory.WriteU32(Flag + 60u, item.mode == Mode::ParentFlag ? 0x80000000u : 0u);
    memory.WriteU32(Parent + 116u, ParentList);
    memory.WriteU32(Parent + 120u,
        item.mode == Mode::ParentMax ? 1u :
        item.mode == Mode::ParentNegative ? 2u : 0u);
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
            item.mode == Mode::ChildScaled && child == ChildA ? 2u : 0u);
        const auto word = child == ChildB ? 0xbf000000u :
            item.mode == Mode::ChildScaled || item.mode == Mode::ChildOptional ?
            0x3fc00000u : 0x40200000u;
        memory.WriteU32(property + 72u, word);
    }
    if (item.mode == Mode::ChildEarly)
        memory.WriteU32(PropertyA + 80u, 0u);
    memory.WriteU32(NodeA + 76u, OptionalList);
    memory.WriteU32(NodeA + 80u, 1u);
    memory.WriteU32(OptionalList, Candidate);
    memory.WriteU32(Candidate, VTable);
    memory.WriteU32(VTable + 332u, 0xc0003u);
}

void Check(const Case& item)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    auto left = original.Memory(), right = recovered.Memory();
    Initialize(item, left); Initialize(item, right);
    PPCContext context{};
    context.r1.u64 = 0x12345678000d0000ull;
    context.r3.u64 = 0x1122334400000000ull |
        (item.entry == 0x822c5e58u ? Parent : ChildA);
    context.r4.u64 = item.mode == Mode::ParentFlag || item.mode == Mode::ChildEarly ?
        0u : 1u;
    context.r5.u64 = item.mode == Mode::ChildOptional ? 0u : 1u;
    context.r12.u64 = 0x2233445566778899ull;
    context.r27.u64 = 0x33445566778899aaull;
    context.r28.u64 = 0x445566778899aabbull;
    context.r29.u64 = 0x5566778899aabbccull;
    context.r30.u64 = 0x66778899aabbccddull;
    context.r31.u64 = 0x778899aabbccddeeull;
    context.lr = 0x8899aabbccddeeffull;
    context.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    context.f1.u64 = std::bit_cast<std::uint64_t>(6.0);
    context.f13.u64 = std::bit_cast<std::uint64_t>(-8.0);
    context.f30.u64 = std::bit_cast<std::uint64_t>(-9.0);
    context.f31.u64 = std::bit_cast<std::uint64_t>(-10.0);
    context.fpscr.csr = 0x9fc0u;
    context.xer.so = 1; context.xer.ca = 1;
    context.cr6 = {1, 0, 0, {1}};
    auto state = FromPpc(context);
    Services expected(original), actual(recovered);

    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    if (item.entry == 0x822c5e58u)
        __imp__sub_822C5E58(context, original.Bytes());
    else
        __imp__sub_822C5F28(context, original.Bytes());
    active = nullptr;
    const auto original_host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(item.entry, right, actual, state))
        throw std::runtime_error("missing object-child-float entry");
    const auto recovered_host = PPCFPSCRRegister{}.getcsr();
    const auto wanted_events = item.mode == Mode::ChildOptional ? 2u : 0u;
    if (expected.events.size() != wanted_events ||
        !Same(FromPpc(context), state) || expected.events != actual.events ||
        actual.fp_calls != 1u || original_host != recovered_host ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("object-child-float original PPC comparison");
}
} // namespace

void OriginalSave27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(active->memory, c.r1.u32 - 16u - (31u - index) * 8u,
            fields[index]->u64);
    active->memory.WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalSave29(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 29u; index <= 31u; ++index)
        WriteU64(active->memory, c.r1.u32 - 16u - (31u - index) * 8u,
            fields[index]->u64);
    active->memory.WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        fields[index]->u64 = ReadU64(active->memory,
            c.r1.u32 - 16u - (31u - index) * 8u);
    c.r12.u64 = active->memory.ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalRestore29(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned index = 29u; index <= 31u; ++index)
        fields[index]->u64 = ReadU64(active->memory,
            c.r1.u32 - 16u - (31u - index) * 8u);
    c.r12.u64 = active->memory.ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalVerify(PPCContext& c, std::uint8_t*)
{
    auto state = FromPpc(c);
    active->CallGuest(0x82384c08u, active->memory, state);
    ToPpc(c, state);
}
void OriginalIndirect(std::uint32_t target, PPCContext& c, std::uint8_t*)
{
    auto state = FromPpc(c);
    active->CallGuest(target, active->memory, state);
    ToPpc(c, state);
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS object-child-float %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT 82384C08 and virtual guest callees, selected FP/ABI and ordinary RAM; NaNs, subnormals, exceptions, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
