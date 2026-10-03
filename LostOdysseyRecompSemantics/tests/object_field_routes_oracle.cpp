#include "lo_semantics/object_field_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_field_routes;
constexpr GuestAddress Object = 0x20000u, Link = 0x30000u;
constexpr GuestAddress Record = 0x30100u, Target = 0x40000u;
constexpr GuestAddress VTable = 0x50000u, AlternateVTable = 0x51000u;
constexpr GuestAddress Reference = 0x82000e50u, OptionalValue = 0x82007784u;
constexpr test::Region Regions[] = {{0u, 0x100000u}, {0x82000000u, 0x8000u}};
struct Case { GuestAddress entry; unsigned mode; std::uint32_t control; };
constexpr Case Cases[] = {
    {0x822c4ba8u, 0u, 0x9fc0u},
    {0x822c4bf8u, 0u, 0x1f80u},
    {0x822c4bf8u, 1u, 0x9fc0u},
    {0x822c4bf8u, 2u, 0x1f80u},
    {0x822c5c68u, 0u, 0x9fc0u},
    {0x822c5c68u, 1u, 0x1f80u},
    {0x822c60d0u, 0u, 0x9fc0u},
    {0x822c60d0u, 1u, 0x9fc0u}
};

family::Registers FromPpc(const PPCContext& c)
{
    return {c.r1.u64, c.r3.u64, c.r4.u64, c.r5.u64,
        c.r10.u64, c.r11.u64, c.r12.u64, c.r31.u64, c.ctr.u64, c.lr,
        c.f0.u64, c.f1.u64, c.f13.u64, c.fpscr.csr, c.xer.so,
        {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un}};
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    c.r1.u64 = s.sp; c.r3.u64 = s.r3; c.r4.u64 = s.r4; c.r5.u64 = s.r5;
    c.r10.u64 = s.r10; c.r11.u64 = s.r11; c.r12.u64 = s.r12;
    c.r31.u64 = s.r31; c.ctr.u64 = s.ctr; c.lr = s.lr;
    c.f0.u64 = s.f0_bits; c.f1.u64 = s.f1_bits; c.f13.u64 = s.f13_bits;
    c.fpscr.csr = s.cached_fp_control; c.xer.so = s.xer_so;
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, {s.cr6.un}};
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.sp == b.sp && a.r3 == b.r3 && a.r4 == b.r4 && a.r5 == b.r5 &&
        a.r10 == b.r10 && a.r11 == b.r11 && a.r12 == b.r12 &&
        a.r31 == b.r31 && a.ctr == b.ctr && a.lr == b.lr &&
        a.f0_bits == b.f0_bits && a.f1_bits == b.f1_bits &&
        a.f13_bits == b.f13_bits &&
        a.cached_fp_control == b.cached_fp_control &&
        a.xer_so == b.xer_so && a.cr6 == b.cr6;
}
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
using Event = std::array<std::uint64_t, 8>;
struct Services final : family::NativeServices
{
    Case item;
    GuestMemory memory;
    unsigned fp_calls = 0;
    std::vector<Event> events;
    Services(const Case& value, test::GuestWindow& window)
        : item(value), memory(window.Memory()) {}
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
    void CallGuest(GuestAddress target, GuestMemory& guest,
        family::Registers& state) override
    {
        events.push_back({target, state.sp, state.lr, state.r3,
            state.r4, state.r5, state.ctr, state.r31});
        if (item.entry == 0x822c4ba8u)
        {
            guest.WriteU32(Target + 928u, 0x3fc00000u);
            state.f1_bits = std::bit_cast<std::uint64_t>(2.0);
        }
        state.r3 = 0x1234567800000055ull;
        state.r10 = 0xfeedcafe11223344ull;
        state.r11 = 0x8877665544332211ull;
        state.lr = 0x99887766aabbccdduLL;
    }
};
Services* active = nullptr;

void Initialize(const Case& item, GuestMemory& memory)
{
    memory.WriteU32(Reference, 0x3f800000u);
    memory.WriteU32(OptionalValue, 0x40000000u);
    if (item.entry == 0x822c4ba8u)
    {
        memory.WriteU32(Object + 80u, Link);
        memory.WriteU32(Link + 60u, Record);
        memory.WriteU32(Record, Target);
        memory.WriteU32(Target, VTable);
        memory.WriteU32(VTable + 288u, 0x70003u);
        memory.WriteU32(Target + 928u, 0x3f800000u);
    }
    else if (item.entry == 0x822c4bf8u)
    {
        memory.WriteU32(Object + 916u, item.mode == 0u ? 0x40000000u : 0x3f800000u);
        memory.WriteU32(Object + 924u, item.mode == 2u ? 0x3f800000u : 0xc0400000u);
        memory.WriteU32(Object + 920u, 0x40800000u);
    }
    else if (item.entry == 0x822c5c68u)
    {
        memory.WriteU32(Object + 84u, item.mode == 0u ? 0u : 0x20000000u);
        memory.WriteU32(Object, VTable);
        memory.WriteU32(VTable + 308u, 0x71003u);
        memory.WriteU32(Target, AlternateVTable);
        memory.WriteU32(AlternateVTable + 268u, 0x72003u);
    }
    else
        memory.WriteU32(Object + 124u, item.mode == 0u ? 0u : Target);
}

void Check(const Case& item)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    auto left = original.Memory(), right = recovered.Memory();
    Initialize(item, left); Initialize(item, right);
    PPCContext context{};
    context.r1.u64 = 0x1234567800080000ull;
    context.r3.u64 = 0x1122334400020000ull;
    context.r4.u64 = 0xaabbccdd00060000ull;
    context.r5.u64 = 0x5566778800040000ull;
    context.r10.u64 = 0x0102030405060708ull;
    context.r11.u64 = 0x1111222233334444ull;
    context.r12.u64 = 0x2222333344445555ull;
    context.r31.u64 = 0x3333444455556666ull;
    context.ctr.u64 = 0x4444555566667777ull;
    context.lr = 0x5555666677778888ull;
    context.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    context.f1.u64 = std::bit_cast<std::uint64_t>(5.0);
    context.f13.u64 = std::bit_cast<std::uint64_t>(-9.0);
    context.fpscr.csr = item.control;
    context.xer.so = 1;
    context.cr6 = {1, 0, 0, {1}};
    auto state = FromPpc(context);
    Services expected(item, original), actual(item, recovered);

    PPCFPSCRRegister{}.setcsr(item.control);
    active = &expected;
    switch (item.entry)
    {
    case 0x822c4ba8u: __imp__sub_822C4BA8(context, original.Bytes()); break;
    case 0x822c4bf8u: __imp__sub_822C4BF8(context, original.Bytes()); break;
    case 0x822c5c68u: __imp__sub_822C5C68(context, original.Bytes()); break;
    case 0x822c60d0u: __imp__sub_822C60D0(context, original.Bytes()); break;
    }
    active = nullptr;
    const auto original_host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(item.control);
    if (!family::Apply(item.entry, right, actual, state))
        throw std::runtime_error("missing object-field-route entry");
    const auto recovered_host = PPCFPSCRRegister{}.getcsr();
    if (!Same(FromPpc(context), state) || expected.events != actual.events ||
        actual.fp_calls != unsigned(item.entry != 0x822c5c68u &&
            (item.control & PPCFPSCRRegister::FlushMask) != 0u) ||
        original_host != recovered_host || !original.EqualCommitted(recovered))
        throw std::runtime_error("object-field-route original PPC comparison");
}
} // namespace

void OriginalIndirect(std::uint32_t target, PPCContext& context, std::uint8_t*)
{
    auto state = FromPpc(context);
    active->CallGuest(target, active->memory, state);
    ToPpc(context, state);
}
void OriginalTail(PPCContext& context, std::uint8_t*)
{
    auto state = FromPpc(context);
    active->CallGuest(0x822c5e58u, active->memory, state);
    ToPpc(context, state);
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS object-field-routes %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT selected ABI/FPR/FPSCR and ordinary RAM; guest callees, signaling NaNs, subnormals, host exceptions, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
