#include "lo_semantics/invalid_parameter.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Stack = 0xf000;
constexpr GuestAddress HandlerGlobal = 0x83378e80;
constexpr GuestAddress StateGlobal = 0x83378d64;
constexpr GuestAddress Callback = 0x82200100;

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve guest window");
        if (!VirtualAlloc(bytes, 0x10000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83378000, 0x2000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest pages");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

using Event = std::array<std::uint64_t, 5>;
struct Services : InvalidParameterServices
{
    unsigned variant;
    std::vector<Event> events;
    std::vector<std::array<std::uint64_t, 9>> traps;
    explicit Services(unsigned test) : variant(test) {}
    void CallHandler(GuestMemory& memory, GuestAddress function,
                     InvalidParameterCall& call) override
    {
        events.push_back({function, call.arguments[0], call.arguments[1],
                          call.thread_environment, memory.ReadU32(HandlerGlobal)});
        call.arguments[0] = variant == 3 ? 0 : 0xaabbccdd12345678ull;
        call.arguments[1] = 0x567800009abcdef0ull;
        call.thread_environment = 0x1234567800003456ull;
        memory.WriteU32(HandlerGlobal, 0);
        memory.WriteU32(StateGlobal, 0x13579bdf);
    }
    void Trap(const InvalidParameterCall& call) override
    {
        // The native fixture's generated PPC leaves twi as a comment. A live
        // platform trap can terminate here; it is never invoked on the host.
        traps.push_back({call.arguments[0], call.arguments[1], call.arguments[2],
                         call.arguments[3], call.arguments[4], call.arguments[5],
                         call.arguments[6], call.arguments[7],
                         call.thread_environment});
    }
};

Services* active = nullptr;

InvalidParameterCall FromPpc(const PPCContext& context)
{
    return {{{context.r3.u64, context.r4.u64, context.r5.u64,
              context.r6.u64, context.r7.u64, context.r8.u64,
              context.r9.u64, context.r10.u64}}, context.r13.u64};
}

bool Test(unsigned variant, Window& original, Window& recovered)
{
    std::memset(original.bytes, 0xbd, 0x10000);
    std::memset(recovered.bytes, 0xbd, 0x10000);
    std::memset(original.bytes + 0x83378000, 0xbd, 0x2000);
    std::memset(recovered.bytes + 0x83378000, 0xbd, 0x2000);
    GuestMemory expected_memory(0, std::span<std::uint8_t>(original.bytes, Space));
    GuestMemory actual_memory(0, std::span<std::uint8_t>(recovered.bytes, Space));
    const GuestAddress handler = variant == 0 || variant == 4 ? 0 :
        variant == 2 ? Callback | 3u : Callback;
    expected_memory.WriteU32(HandlerGlobal, handler);
    actual_memory.WriteU32(HandlerGlobal, handler);
    expected_memory.WriteU32(StateGlobal, 0x2468ace0);
    actual_memory.WriteU32(StateGlobal, 0x2468ace0);

    Services expected(variant), actual(variant);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000;
    context.r3.u64 = 0x1122334455667788ull;
    context.r4.u64 = 0x2233445566778899ull;
    context.r5.u64 = 0x33445566778899aaull;
    context.r6.u64 = 0x445566778899aabbull;
    context.r7.u64 = 0x5566778899aabbccull;
    context.r8.u64 = 0x66778899aabbccddull;
    context.r9.u64 = 0x778899aabbccddeeull;
    context.r10.u64 = 0x8899aabbccddeeffull;
    context.r13.u64 = 0x99aabbcc00002000ull;
    auto call = FromPpc(context);
    active = &expected;
    std::uint64_t result;
    if (variant == 4)
    {
        __imp__sub_82B84D88(context, original.bytes);
        ClearInvalidParameterState(actual_memory);
        result = call.arguments[0];
    }
    else
    {
        __imp__sub_82B7FEC0(context, original.bytes);
        result = ReportInvalidParameter(actual_memory, actual, call);
    }
    const auto observed = FromPpc(context);
    const bool same = context.r3.u64 == result &&
        (variant == 4 || observed.arguments == call.arguments) &&
        observed.thread_environment == call.thread_environment &&
        expected.events == actual.events && expected.traps == actual.traps &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83378000,
                    recovered.bytes + 0x83378000, 0x2000) == 0;
    if (!same)
    {
        std::fprintf(stderr, "FAIL invalid-parameter case %u return %llx/%llx events %zu/%zu\n",
                     variant, static_cast<unsigned long long>(context.r3.u64),
                     static_cast<unsigned long long>(result), expected.events.size(),
                     actual.events.size());
        return false;
    }
    return true;
}
} // namespace

PPC_FUNC(sub_82B84D88)
{
    __imp__sub_82B84D88(ctx, base);
    if (active->variant != 4)
        active->Trap(FromPpc(ctx));
}
void InvalidParameterIndirect(PPCContext& context, std::uint8_t* base,
                              std::uint32_t function)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    auto call = FromPpc(context);
    active->CallHandler(memory, function, call);
    context.r3.u64 = call.arguments[0];
    context.r4.u64 = call.arguments[1];
    context.r5.u64 = call.arguments[2];
    context.r6.u64 = call.arguments[3];
    context.r7.u64 = call.arguments[4];
    context.r8.u64 = call.arguments[5];
    context.r9.u64 = call.arguments[6];
    context.r10.u64 = call.arguments[7];
    context.r13.u64 = call.thread_environment;
}

int main()
{
    try
    {
        Window original, recovered;
        for (unsigned variant = 0; variant != 5; ++variant)
            if (!Test(variant, original, recovered)) return 1;
        std::puts("PASS invalid-parameter 4 dispatcher +1 state-clear cases");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
