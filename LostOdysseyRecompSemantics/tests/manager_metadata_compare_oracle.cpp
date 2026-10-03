#include "lo_semantics/manager_metadata_compare.h"
#include "lo_semantics/allocation_failure.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::manager_metadata_compare;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x5566778800018000ull;
constexpr std::uint64_t Environment = 0x1122334400009000ull;
enum class Mode { Equal, Shorter, NonAscii, NullHandler, ErrnoAlias, NullTrap };
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x40000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83214000u, 0x3000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83378000u, 0x2000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("guest window reserve/commit");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
using Event = std::array<std::uint64_t, 11>;
struct Services final : CrtThreadDataServices, InvalidParameterServices,
    AllocationFailureServices
{
    GuestMemory memory;
    Mode mode;
    CrtThreadDataCall thread{Environment};
    std::vector<Event> events;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}
    static std::uint64_t Unexpected() { throw std::runtime_error("unexpected lower boundary"); }
    std::uint64_t GetTlsValue(std::uint32_t index) override
    { events.push_back({1, index}); return 0x7100u; }
    void SetTlsValue(std::uint32_t, std::uint64_t) override { (void)Unexpected(); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { return Unexpected(); }
    std::uint64_t CallThreadDataGetterWithState(GuestAddress function,
        std::uint64_t context, CrtThreadDataCall& state) override
    {
        events.push_back({2, function, context, state.thread_environment});
        state.thread_environment = 0xAABBCCDD0000A000ull;
        if (mode == Mode::NullTrap)
            return 0x9988776600000000ull;
        return mode == Mode::ErrnoAlias ? 0x9988776600017FF0ull :
            0x9988776600028000ull;
    }
    std::uint64_t AllocateThreadData(std::uint32_t count, std::uint32_t size) override
    { events.push_back({3, count, size}); return 0; }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t, std::uint64_t) override
    { return Unexpected(); }
    void FreeThreadData(std::uint64_t) override { (void)Unexpected(); }
    std::uint64_t GetThreadData() override
    { return GetCrtThreadData(memory, *this, thread); }
    std::uint64_t OutputErrorMessage(GuestAddress) override { return Unexpected(); }
    std::uint64_t BugCheck(std::uint32_t) override { return Unexpected(); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override { return Unexpected(); }
    void CallHandler(GuestMemory& guest, GuestAddress function,
        InvalidParameterCall& call) override
    {
        Event event{4, function, call.thread_environment};
        std::copy(call.arguments.begin(), call.arguments.end(), event.begin() + 3);
        events.push_back(event);
        if (function != 0x7200u || call.arguments[0] != 0 ||
            call.arguments[4] != 0 || call.arguments[7] != 22)
            throw std::runtime_error("invalid callback arguments");
        call.arguments[0] = 0xABCDEF1234567890ull;
        call.arguments[1] = 0xDEADCAFE00023456ull;
        call.arguments[5] = 0x1122334455667788ull;
        call.thread_environment = 0xCAFEBABE0000A000ull;
        if (mode == Mode::NullHandler)
            guest.WriteU32(static_cast<GuestAddress>(Stack) - 8u, 0x12345678u);
    }
    void Trap(const InvalidParameterCall& call) override
    {
        Event event{5, 22, call.thread_environment};
        std::copy(call.arguments.begin(), call.arguments.end(), event.begin() + 3);
        events.push_back(event);
    }
};
Services* active = nullptr;

InvalidParameterCall FromPpc(const PPCContext& context)
{
    return {{{context.r3.u64, context.r4.u64, context.r5.u64, context.r6.u64,
        context.r7.u64, context.r8.u64, context.r9.u64, context.r10.u64}}, context.r13.u64};
}
void ToPpc(PPCContext& context, const InvalidParameterCall& call)
{
    context.r3.u64 = call.arguments[0]; context.r4.u64 = call.arguments[1];
    context.r5.u64 = call.arguments[2]; context.r6.u64 = call.arguments[3];
    context.r7.u64 = call.arguments[4]; context.r8.u64 = call.arguments[5];
    context.r9.u64 = call.arguments[6]; context.r10.u64 = call.arguments[7];
    context.r13.u64 = call.thread_environment;
}
void Initialize(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xBD, 0x40000);
    std::memset(window.bytes + 0x83214000u, 0xBD, 0x3000);
    std::memset(window.bytes + 0x83378000u, 0xBD, 0x2000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x9000u + 336u, 1);
    memory.WriteU32(0xA000u + 336u, 1);
    memory.WriteU32(0x83214D74u, 0x32000u);
    memory.WriteU32(0x83214D78u, 5);
    memory.WriteU32(0x83378E80u, mode == Mode::NullTrap ? 0u : 0x7203u);
    const std::array<std::uint16_t, 4> left = mode == Mode::NonAscii ?
        std::array<std::uint16_t, 4>{0x00C4u, 0, 0, 0} :
        std::array<std::uint16_t, 4>{65, 98, 0, 0};
    const std::array<std::uint16_t, 4> right = mode == Mode::NonAscii ?
        std::array<std::uint16_t, 4>{0x00E4u, 0, 0, 0} : mode == Mode::Shorter ?
        std::array<std::uint16_t, 4>{97, 66, 99, 0} :
        std::array<std::uint16_t, 4>{97, 66, 0, 0};
    for (std::size_t i = 0; i != 4; ++i)
    {
        memory.WriteU16(0x10000u + static_cast<GuestAddress>(i * 2u), left[i]);
        memory.WriteU16(0x11000u + static_cast<GuestAddress>(i * 2u), right[i]);
    }
}
bool Compare(Mode mode, Window& original, Window& recovered)
{
    Initialize(original, mode); Initialize(recovered, mode);
    Services expected(original, mode), actual(recovered, mode);
    InvalidParameterCall call{{{0x2233445500010000ull, 0x3344556600011000ull,
        0x4455667788990011ull, 0x5566778899001122ull, 0x6677889900112233ull,
        0x7788990011223344ull, 0x8899001122334455ull, 0x9900112233445566ull}}, Environment};
    if (mode == Mode::NullHandler || mode == Mode::ErrnoAlias)
        call.arguments[0] = 0x2233445500000000ull;
    if (mode == Mode::NullTrap)
        call.arguments[1] = 0x3344556600000000ull;
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x1234567887654321ull;
    ToPpc(context, call);
    active = &expected;
    __imp__sub_822971E0(context, original.bytes);
    std::uint64_t lr = 0x1234567887654321ull, result = 0;
    if (!family::Apply(0x822971E0u, actual.memory, actual, actual,
            call, Stack, lr, result))
        throw std::runtime_error("missing comparison entry");
    const auto observed = FromPpc(context);
    const bool matched = result == context.r3.u64 && lr == context.lr &&
        context.r1.u64 == Stack && call.arguments == observed.arguments &&
        call.thread_environment == observed.thread_environment &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x40000) == 0 &&
        std::memcmp(original.bytes + 0x83214000u, recovered.bytes + 0x83214000u, 0x3000) == 0 &&
        std::memcmp(original.bytes + 0x83378000u, recovered.bytes + 0x83378000u, 0x2000) == 0;
    if (!matched)
        std::printf("FAIL compare mode=%u result=%llx/%llx lr=%llx/%llx\n",
            static_cast<unsigned>(mode), result, context.r3.u64, lr, context.lr);
    return matched;
}
} // namespace

void OriginalError(PPCContext& context, std::uint8_t*)
{
    active->thread.thread_environment = context.r13.u64;
    context.r3.u64 = GetAllocationErrorAddress(*active);
    context.r13.u64 = active->thread.thread_environment;
}
void OriginalInvalid(PPCContext& context, std::uint8_t*)
{
    auto call = FromPpc(context);
    (void)ReportInvalidParameter(active->memory, *active, call);
    ToPpc(context, call);
}
int main()
{
    try
    {
        Window original, recovered;
        for (auto mode : {Mode::Equal, Mode::Shorter, Mode::NonAscii,
                Mode::NullHandler, Mode::ErrnoAlias, Mode::NullTrap})
            if (!Compare(mode, original, recovered)) return 1;
        Services services(recovered, Mode::Equal);
        InvalidParameterCall call{{{1,2,3,4,5,6,7,8}}, Environment};
        const auto before = call;
        std::uint64_t lr = 99, result = 123;
        if (family::Apply(0xDEADBEEFu, services.memory, services, services,
                call, Stack, lr, result) || call.arguments != before.arguments ||
            call.thread_environment != before.thread_environment || lr != 99 || result != 123)
            return 2;
        std::puts("PASS manager-metadata-compare 1 complete body, 6 original-PPC cases + unknown");
        std::puts("LIMIT reused CRT algorithms, generic lower volatile ABI/frame writes, dynamic target internals, trap termination and faults/MMIO excluded");
        return 0;
    }
    catch (const std::exception& error)
    { std::printf("FAIL %s\n", error.what()); return 3; }
}
