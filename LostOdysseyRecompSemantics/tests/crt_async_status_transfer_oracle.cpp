#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_async_status_transfer;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x80000u, Async = 0x30000u;
constexpr GuestAddress Output = 0x50000u, Environment = 0x60000u;
constexpr GuestAddress ErrorState = 0x61000u, Table = 0x70000u;
constexpr GuestAddress VirtualTarget = 0x2400u;
constexpr std::array<test::Region, 2> Regions{{{0u, 0x120000u},
    {0x831e0000u, 0x10000u}}};
enum class Mode { ExternalSuccess, ExternalFailure, LocalSuccess,
    LocalPending, LocalSpecial, LocalFailure };
constexpr Mode Cases[] = {Mode::ExternalSuccess, Mode::ExternalFailure,
    Mode::LocalSuccess, Mode::LocalPending, Mode::LocalSpecial,
    Mode::LocalFailure};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
std::array<PPCRegister*, 32> FprFields(PPCContext& c)
{
    return {&c.f0, &c.f1, &c.f2, &c.f3, &c.f4, &c.f5, &c.f6, &c.f7,
        &c.f8, &c.f9, &c.f10, &c.f11, &c.f12, &c.f13, &c.f14, &c.f15,
        &c.f16, &c.f17, &c.f18, &c.f19, &c.f20, &c.f21, &c.f22, &c.f23,
        &c.f24, &c.f25, &c.f26, &c.f27, &c.f28, &c.f29, &c.f30, &c.f31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers s{};
    const auto r = Fields(c), f = FprFields(c);
    for (unsigned i = 0; i < 32u; ++i)
    { s.r[i] = r[i]->u64; s.fpr_bits[i] = f[i]->u64; }
    s.lr = c.lr; s.ctr = c.ctr.u64;
    s.cached_fp_control = c.fpscr.csr;
    s.xer_so = c.xer.so; s.xer_ca = c.xer.ca;
    s.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    return s;
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    const auto r = Fields(c), f = FprFields(c);
    for (unsigned i = 0; i < 32u; ++i)
    { r[i]->u64 = s.r[i]; f[i]->u64 = s.fpr_bits[i]; }
    c.lr = s.lr; c.ctr.u64 = s.ctr;
    c.fpscr.csr = s.cached_fp_control;
    c.xer.so = s.xer_so; c.xer.ca = s.xer_ca;
    c.cr0 = {s.cr0.lt, s.cr0.gt, s.cr0.eq, {s.cr0.so}};
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, {s.cr6.so}};
}
std::array<std::uint64_t, 70> Snapshot(const family::Registers& s)
{
    std::array<std::uint64_t, 70> out{};
    for (unsigned i = 0; i < 32u; ++i)
    { out[i] = s.r[i]; out[i + 32u] = s.fpr_bits[i]; }
    out[64] = s.lr; out[65] = s.ctr;
    out[66] = s.cached_fp_control;
    out[67] = s.xer_so | (std::uint64_t(s.xer_ca) << 8u);
    const auto pack = [](family::Condition c)
    { return c.lt | (std::uint64_t(c.gt) << 8u) |
        (std::uint64_t(c.eq) << 16u) | (std::uint64_t(c.so) << 24u); };
    out[68] = pack(s.cr0); out[69] = pack(s.cr6);
    return out;
}
struct Event
{
    unsigned kind;
    GuestAddress target;
    std::array<std::uint64_t, 70> state;
    bool operator==(const Event&) const = default;
};
struct Services final : family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(test::GuestWindow& window, Mode value)
        : memory(window.Memory()), mode(value) {}
    void CallIndirect(GuestAddress target, GuestMemory& m,
        family::Registers& s) override
    {
        if (target != VirtualTarget ||
            (s.lr != 0x82be2e60u && s.lr != 0x82be2ed8u))
            throw std::runtime_error("incorrect CRT dynamic target");
        events.push_back({0u, target, Snapshot(s)});
        if (s.lr == 0x82be2ed8u)
        {
            m.WriteU32(Address(s.r[1] + 80u), 0u);
            m.WriteU32(Address(s.r[1] + 84u), 23u);
        }
        s.r[3] = mode == Mode::ExternalFailure ? 0xffffffffc0000017ull :
            mode == Mode::LocalPending ? 259u :
            mode == Mode::LocalSpecial ? 0xffffffffc0000011ull :
            mode == Mode::LocalFailure ? 0xffffffff80000001ull : 0u;
        s.r[10] = 0x12345678u;
        s.fpr_bits[3] = 0x4008000000000000ull;
    }
    void NtWaitForSingleObjectEx(GuestMemory& m,
        family::Registers& s) override
    {
        if (mode != Mode::LocalPending || s.lr != 0x82be2ef4u)
            throw std::runtime_error("unexpected CRT wait");
        events.push_back({1u, 0x830d9cfcu, Snapshot(s)});
        m.WriteU32(Address(s.r[1] + 80u), 0u);
        s.r[3] = 0u;
    }
    void NtStatusToDosError(GuestMemory&,
        family::Registers& s) override
    {
        if (mode != Mode::ExternalFailure && mode != Mode::LocalFailure)
            throw std::runtime_error("unexpected status conversion");
        if (s.lr != 0x827ca638u)
            throw std::runtime_error("status lower LR mismatch");
        events.push_back({2u, 0x830d9efcu, Snapshot(s)});
        s.r[3] = 5u;
    }
};
Services* active = nullptr;
void Seed(GuestMemory& m)
{
    m.WriteU32(0x831e7df4u, Table);
    m.WriteU32(Table + 16u, VirtualTarget | 3u);
    m.WriteU32(Async + 4u, 42u);
    m.WriteU32(Async + 8u, 0x123u);
    m.WriteU32(Async + 12u, 0x456u);
    m.WriteU32(Async + 16u, 0u);
    m.WriteU32(Environment + 336u, 0u);
    m.WriteU32(Environment + 256u, ErrorState);
    m.WriteU32(Output, 0xbdbdbdbdu);
}
PPCContext MakeContext(Mode mode)
{
    PPCContext c{};
    const auto r = Fields(c), f = FprFields(c);
    for (unsigned i = 0; i < 32u; ++i)
    {
        r[i]->u64 = 0x1234000000000000ull + i * 0x100000001ull;
        f[i]->u64 = 0x4000000000000000ull + i;
    }
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1111000000000000ull | 0x7777u;
    c.r6.u64 = 0x2222000000000000ull | Output;
    c.r7.u64 = mode == Mode::ExternalSuccess ||
        mode == Mode::ExternalFailure ? Async : 0u;
    c.r13.u64 = 0x5555000000000000ull | Environment;
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.fpscr.csr = 0x9fc0u;
    c.xer.so = 1; c.xer.ca = 1;
    c.cr0 = {0, 1, 0, {1}};
    c.cr6 = {1, 0, 0, {1}};
    return c;
}
bool Check(Mode mode, unsigned ordinal)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    Services expected(original, mode), actual(recovered, mode);
    Seed(expected.memory); Seed(actual.memory);
    auto context = MakeContext(mode);
    auto state = FromPpc(context);
    active = &expected;
    __imp__sub_82BE2DD8(context, original.Bytes());
    active = nullptr;
    if (!family::Apply(0x82be2dd8u, actual.memory, actual, state))
        throw std::runtime_error("missing async status entry");
    bool covered = expected.events.size() ==
        (mode == Mode::ExternalFailure || mode == Mode::LocalFailure ||
        mode == Mode::LocalPending ? 2u : 1u);
    if (mode == Mode::ExternalSuccess)
        covered &= expected.memory.ReadU32(Output) == 42u;
    if (mode == Mode::LocalSuccess || mode == Mode::LocalPending ||
        mode == Mode::LocalFailure)
        covered &= expected.memory.ReadU32(Output) == 23u;
    if (mode == Mode::LocalSpecial)
        covered &= expected.memory.ReadU32(Output) == 0u;
    if (mode == Mode::ExternalFailure || mode == Mode::LocalFailure)
        covered &= expected.memory.ReadU32(ErrorState + 352u) == 5u;
    bool same = original.EqualCommitted(recovered) &&
        expected.events == actual.events;
    const auto x = Snapshot(FromPpc(context)), y = Snapshot(state);
    for (unsigned i = 0; i < x.size(); ++i)
        if (x[i] != y[i])
        {
            std::fprintf(stderr, "case %u state[%u] %llx/%llx\n", ordinal,
                i, static_cast<unsigned long long>(x[i]),
                static_cast<unsigned long long>(y[i]));
            same = false;
        }
    if (expected.events != actual.events)
        std::fprintf(stderr, "case %u events %zu/%zu\n", ordinal,
            expected.events.size(), actual.events.size());
    if (!original.EqualCommitted(recovered))
        for (const auto region : Regions)
            for (std::size_t i = 0; i < region.size; ++i)
                if (original.Bytes()[region.base + i] !=
                    recovered.Bytes()[region.base + i])
                {
                    std::fprintf(stderr, "case %u RAM %08llx %02x/%02x\n",
                        ordinal,
                        static_cast<unsigned long long>(region.base + i),
                        original.Bytes()[region.base + i],
                        recovered.Bytes()[region.base + i]);
                    break;
                }
    if (!covered)
        std::fprintf(stderr, "case %u missed requested CRT status path\n", ordinal);
    return covered && same;
}
} // namespace

void OriginalSave28(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u), r[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore28(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
        r[i]->u64 = ReadU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalIndirect(std::uint32_t target, PPCContext& c, std::uint8_t*)
{
    auto s = FromPpc(c);
    active->CallIndirect(target, active->memory, s);
    ToPpc(c, s);
}
void OriginalWait(PPCContext& c, std::uint8_t*)
{
    auto s = FromPpc(c);
    active->NtWaitForSingleObjectEx(active->memory, s);
    ToPpc(c, s);
}
void OriginalStatus(PPCContext& c, std::uint8_t*)
{
    auto s = FromPpc(c);
    active->NtStatusToDosError(active->memory, s);
    ToPpc(c, s);
}
int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS crt-async-status-transfer %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT imported wait/status and function-table target internals, faults and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
