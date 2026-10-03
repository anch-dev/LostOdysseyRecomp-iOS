#include "lo_semantics/caller_frame_unlock.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = caller_frame_unlock;
constexpr test::Region Regions[] = {{0, 0x100000u}};
constexpr GuestAddress CallerFrame = 0x70000u, DirectRecord = 0x60000u;
struct Case { GuestAddress entry; std::uint32_t size, field; GuestAddress critical; };
constexpr Case Cases[] = {
    {0x82290878u, 0, 0, 0x50000u}, {0x82290708u, 512, 88, 0x50100u},
    {0x8229587cu, 176, 80, 0x50200u}, {0x822958a4u, 176, 80, 0x50300u},
    {0x822958ccu, 176, 80, 0x50400u}, {0x8229a8dcu, 128, 80, 0x50500u},
    {0x8229a904u, 128, 80, 0x50600u}, {0x82290878u, 0, 0, 0x50700u}
};
family::Registers FromPpc(const PPCContext& c)
{
    return {c.r1.u64, c.lr, c.ctr.u64, c.r3.u64, c.r4.u64,
        c.r10.u64, c.r11.u64, c.r12.u64, c.r31.u64,
        c.xer.ca, c.xer.so,
        {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so},
        {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so}};
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    c.r1.u64 = s.sp; c.lr = s.lr; c.ctr.u64 = s.ctr;
    c.r3.u64 = s.r3; c.r4.u64 = s.r4; c.r10.u64 = s.r10;
    c.r11.u64 = s.r11; c.r12.u64 = s.r12; c.r31.u64 = s.r31;
    c.xer.ca = s.xer_ca; c.xer.so = s.xer_so;
    c.cr0 = {bool(s.cr0.lt), bool(s.cr0.gt), bool(s.cr0.eq), {bool(s.cr0.so)}};
    c.cr6 = {bool(s.cr6.lt), bool(s.cr6.gt), bool(s.cr6.eq), {bool(s.cr6.so)}};
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.sp == b.sp && a.lr == b.lr && a.ctr == b.ctr && a.r3 == b.r3 &&
        a.r4 == b.r4 && a.r10 == b.r10 && a.r11 == b.r11 && a.r12 == b.r12 &&
        a.r31 == b.r31 && a.xer_ca == b.xer_ca && a.xer_so == b.xer_so &&
        a.cr0 == b.cr0 && a.cr6 == b.cr6;
}
struct Services final : family::NativeServices
{
    GuestMemory memory;
    std::vector<std::array<std::uint64_t, 4>> events;
    explicit Services(test::GuestWindow& window) : memory(window.Memory()) {}
    void LeaveCriticalSection(GuestMemory& guest, family::Registers& state) override
    {
        events.push_back({state.sp, state.lr, state.r3, state.r31});
        guest.WriteU32(static_cast<GuestAddress>(state.r3) + 8u, 0xabcdef01u);
        state.r3 = 0x1122334400000042ull;
        state.r10 = 0x8899aabbccddeeffull;
        state.r11 = 0x9988776655443322ull;
        state.ctr = 0x123456789abcdef0ull;
        state.xer_so = 1; state.xer_ca = 1;
        state.cr0 = {0, 1, 0, 1}; state.cr6 = {1, 0, 0, 1};
    }
};
Services* active = nullptr;
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0); recovered.Fill(0);
    const auto record = item.size ? CallerFrame - item.size + item.field : DirectRecord;
    for (auto* window : {&original, &recovered})
        window->Memory().WriteU32(record, item.critical);
    PPCContext context{};
    context.r1.u64 = 0x1234567800080000ull;
    context.lr = 0x99887766abcdef01ull;
    context.r3.u64 = 0x1122334400060000ull;
    context.r4.u64 = 0x8877665544332211ull;
    context.r10.u64 = 0x5555666677778888ull;
    context.r11.u64 = 0x1111222233334444ull;
    context.r12.u64 = 0x5566778800070000ull;
    context.r31.u64 = 0x99aabbccddeeff00ull;
    auto state = FromPpc(context);
    Services expected(original), actual(recovered);
    active = &expected;
    switch (item.entry)
    {
    case 0x82290878u: __imp__sub_82290878(context, original.Bytes()); break;
    case 0x82290708u: __imp__sub_82290708(context, original.Bytes()); break;
    case 0x8229587cu: __imp__sub_8229587C(context, original.Bytes()); break;
    case 0x822958a4u: __imp__sub_822958A4(context, original.Bytes()); break;
    case 0x822958ccu: __imp__sub_822958CC(context, original.Bytes()); break;
    case 0x8229a8dcu: __imp__sub_8229A8DC(context, original.Bytes()); break;
    case 0x8229a904u: __imp__sub_8229A904(context, original.Bytes()); break;
    }
    active = nullptr;
    if (expected.events.size() != 1 || expected.events[0][2] != item.critical + 4u)
        throw std::runtime_error("unlock original fixture missed native target");
    if (!family::Apply(item.entry, actual.memory, actual, state) ||
        !Same(FromPpc(context), state) || expected.events != actual.events ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("caller frame unlock original PPC comparison");
}
}
void OriginalLeave(PPCContext& context, std::uint8_t*)
{
    auto state = FromPpc(context);
    active->LeaveCriticalSection(active->memory, state);
    ToPpc(context, state);
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS caller-frame-unlock %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT selected ABI/RAM and native boundary; native internals, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
