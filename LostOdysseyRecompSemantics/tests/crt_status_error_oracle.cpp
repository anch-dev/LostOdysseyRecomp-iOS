#include "lo_semantics/crt_status_error.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_status_error;
constexpr GuestAddress Stack = 0x8000, Environment = 0x1000, OtherEnvironment = 0x2000;
constexpr GuestAddress ErrorState = 0x3000, OtherErrorState = 0x4000;
constexpr std::uint64_t Converted = 0x98abcdef12345678ull;
unsigned variant;
unsigned native_calls;

family::Registers ReadState(const PPCContext& ctx)
{
    return {ctx.r1.u64, ctx.lr, ctx.r3.u64, ctx.r11.u64, ctx.r12.u64,
        ctx.r13.u64, ctx.xer.so,
        {ctx.cr6.lt, ctx.cr6.gt, ctx.cr6.eq, ctx.cr6.so}};
}

void WriteState(PPCContext& ctx, const family::Registers& state)
{
    ctx.r1.u64 = state.sp;
    ctx.lr = state.lr;
    ctx.r3.u64 = state.r3;
    ctx.r11.u64 = state.r11;
    ctx.r12.u64 = state.r12;
    ctx.r13.u64 = state.r13;
    ctx.xer.so = state.xer_so;
    ctx.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, {state.cr6.so}};
}

struct Native : family::NativeServices
{
    void NtStatusToDosError(GuestMemory& memory, family::Registers& state) override
    {
        ++native_calls;
        if (state.lr != 0x827ca638u || state.r3 != 0x76543210c0000017ull)
            throw std::runtime_error("native status arguments/LR");
        state.r3 = Converted;
        if (variant == 2)
        {
            state.r13 = 0xaabbccdd00000000ull | OtherEnvironment;
            state.sp = 0x5566778800006000ull;
            state.xer_so = 0;
            memory.WriteU32(0x6058, 0xa1b2c3d4);
        }
        if (variant == 3)
            memory.WriteU32(Environment + 256u, Stack - 8u - 352u);
    }
};

bool Check(unsigned test)
{
    variant = test;
    alignas(32) std::array<std::uint8_t, 0x10000> original{};
    GuestMemory memory(0, std::span<std::uint8_t>(original));
    memory.WriteU32(Environment + 336, test == 1 ? 1u : 0u);
    memory.WriteU32(Environment + 256, ErrorState);
    memory.WriteU32(OtherEnvironment + 256, OtherErrorState);
    memory.WriteU32(ErrorState + 352, 0xeeeeeeee);
    memory.WriteU32(OtherErrorState + 352, 0xdddddddd);
    auto recovered = original;
    PPCContext initial{};
    initial.r1.u64 = 0x1122334400000000ull | Stack;
    initial.lr = 0x123456789abcdef0ull;
    initial.r3.u64 = 0x76543210c0000017ull;
    initial.r11.u64 = 0x8877665544332211ull;
    initial.r12.u64 = 0xabcdef1234567890ull;
    initial.r13.u64 = 0xcafebabe00000000ull | Environment;
    initial.xer.so = 1;
    initial.cr6 = {1, 0, 1, {0}};
    auto raw = initial;
    native_calls = 0;
    __imp__sub_827CA628(raw, original.data());
    if (native_calls != 1) throw std::runtime_error("original native count");
    auto state = ReadState(initial);
    GuestMemory actual(0, std::span<std::uint8_t>(recovered));
    Native native;
    native_calls = 0;
    if (!family::Apply(0x827ca628u, actual, native, state) || native_calls != 1)
        throw std::runtime_error("recovered status entry/native count");
    auto expected = initial;
    WriteState(expected, state);
    if (raw.r3.u64 != Converted || std::memcmp(&raw, &expected, sizeof(raw)) || original != recovered)
    {
        std::fprintf(stderr, "FAIL status-error case %u\n", test);
        return false;
    }
    return true;
}
} // namespace

void OriginalConvert(PPCContext& ctx, std::uint8_t* bytes)
{
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, 0x10000));
    auto state = ReadState(ctx);
    Native native;
    native.NtStatusToDosError(memory, state);
    WriteState(ctx, state);
}

int main()
{
    try
    {
        for (unsigned test = 0; test != 4; ++test) if (!Check(test)) return 1;
        std::array<std::uint8_t, 32> bytes{};
        const auto before = bytes;
        GuestMemory memory(0, std::span<std::uint8_t>(bytes));
        family::Registers state{1, 2, 3, 4, 5, 6, 1, {1, 2, 3, 4}};
        const auto saved = state;
        Native native;
        native_calls = 0;
        if (family::Apply(0xffffffffu, memory, native, state) || native_calls || bytes != before ||
            state.sp != saved.sp || state.lr != saved.lr || state.r3 != saved.r3 ||
            state.r11 != saved.r11 || state.r12 != saved.r12 || state.r13 != saved.r13 ||
            state.xer_so != saved.xer_so || std::memcmp(&state.cr6, &saved.cr6, sizeof(state.cr6)))
            throw std::runtime_error("unknown status-error entry changed state");
        std::puts("PASS crt-status-error 4 original PPC cases + unknown");
        std::puts("LIMIT own frame/selected native callback profile and ordinary RAM; native internals, faults/MMIO/concurrency/runtime unverified");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
