#include "lo_semantics/crt_last_error.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_last_error;
struct Case { GuestAddress address; PPCFunc* original; bool blocked; };
constexpr Case Cases[] = {
    {0x822ca108u, __imp__sub_822CA108, false},
    {0x822ca108u, __imp__sub_822CA108, true},
    {0x822ca100u, __imp__sub_822CA100, false},
    {0x822ca100u, __imp__sub_822CA100, true},
};
bool Check(const Case& test)
{
    alignas(32) std::array<std::uint8_t, 0x10000> original{};
    alignas(32) std::array<std::uint8_t, 0x10000> recovered{};
    GuestMemory memory(0, std::span<std::uint8_t>(original));
    memory.WriteU32(0x9000u + 336u, test.blocked ? 1u : 0u);
    memory.WriteU32(0x9000u + 256u, 0x6000u);
    memory.WriteU32(0x6000u + 352u, 0xf1234567u);
    recovered = original;
    PPCContext initial{};
    initial.r1.u64 = 0x1122334400008000ull;
    initial.r3.u64 = 0x5566778899aabbccull;
    initial.r11.u64 = 0xabcdef0123456789ull;
    initial.r13.u64 = 0xcafebabe00009000ull;
    initial.lr = 0x123456789abcdef0ull;
    initial.ctr.u64 = 0x8877665544332211ull;
    initial.xer.so = 1;
    initial.cr6 = {1, 0, 1, {0}};
    PPCContext raw = initial;
    test.original(raw, original.data());
    GuestMemory actual(0, std::span<std::uint8_t>(recovered));
    family::Registers registers{initial.r3.u64, initial.r11.u64,
        initial.r13.u64, initial.xer.so,
        {initial.cr6.lt, initial.cr6.gt, initial.cr6.eq, initial.cr6.so}};
    if (!family::Apply(test.address, actual, registers))
        throw std::runtime_error("missing last-error entry");
    PPCContext expected = initial;
    expected.r3.u64 = registers.r3;
    expected.r11.u64 = registers.r11;
    expected.cr6.lt = registers.cr6.lt;
    expected.cr6.gt = registers.cr6.gt;
    expected.cr6.eq = registers.cr6.eq;
    expected.cr6.so = registers.cr6.so;
    const bool same = raw.r3.u64 == (test.blocked ? 0u : 0xf1234567u) &&
        std::memcmp(&raw, &expected, sizeof(PPCContext)) == 0 && original == recovered;
    if (!same) std::fprintf(stderr, "FAIL last-error %08x blocked=%u\n",
        test.address, static_cast<unsigned>(test.blocked));
    return same;
}
} // namespace

int main()
{
    try
    {
        for (const auto& test : Cases) if (!Check(test)) return 1;
        alignas(32) std::array<std::uint8_t, 32> bytes{};
        GuestMemory memory(0, std::span<std::uint8_t>(bytes));
        family::Registers call{1, 2, 3, 4, {5, 6, 7, 8}};
        const auto saved = call;
        if (family::Apply(0xffffffffu, memory, call) ||
            call.r3 != saved.r3 || call.r11 != saved.r11 ||
            call.r13 != saved.r13 || call.xer_so != saved.xer_so ||
            std::memcmp(&call.cr6, &saved.cr6, sizeof(call.cr6)) != 0)
            throw std::runtime_error("unknown last-error address changed state");
        std::printf("PASS crt-last-error %zu original PPC cases + unknown\n", std::size(Cases));
        std::puts("LIMIT complete PPCContext and unchanged ordinary RAM for getter/tail branches; no fault/MMIO/concurrency/runtime proof");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
