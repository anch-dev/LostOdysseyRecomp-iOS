#include "lo_semantics/crt_stream_state.h"
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
namespace family = lo::semantic::gpu::crt_stream_state;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Stream = 0x10000u;
constexpr GuestAddress HeapGlobal = 0x83245708u;
constexpr GuestAddress RetryGlobal = 0x832D3AECu;
constexpr GuestAddress BufferCounter = 0x832D3AB8u;
constexpr GuestAddress OnceTarget = 0x832D3CA8u;
constexpr GuestAddress Handler = 0x83378E80u;
enum class Mode { ReadValid, ReadInvalid, BufferSuccess, BufferFailure,
    BufferAlias, NativeWrapper, OnceDefault, OnceOverride };
struct Case { GuestAddress address; Mode mode; std::uint64_t expected; };
constexpr Case Cases[] = {
    {0x82B81648u, Mode::ReadValid, 0xCAFEBABEu},
    {0x82B81648u, Mode::ReadInvalid, UINT64_MAX},
    {0x82B85C40u, Mode::BufferSuccess, 0x1234567800040000ull},
    {0x82B85C40u, Mode::BufferFailure, 0},
    {0x82B85C40u, Mode::BufferAlias, 0x1234567800040000ull},
    {0x82B82180u, Mode::NativeWrapper, 1},
    {0x82B821B0u, Mode::OnceDefault, 1},
    {0x82B821B0u, Mode::OnceOverride, 0xAABBCCDD00000007ull},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83214000u, 0x3000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83245000u, 0x2000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x832D3000u, 0x2000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83378000u, 0x2000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve CRT stream guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

using Event = std::array<std::uint64_t, 6>;
void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

struct Services final : CrtThreadDataServices, InvalidParameterServices,
    RawAllocationServices, AllocationFailureServices, family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::uint64_t thread_environment = 0x1122334400009000ull;
    std::vector<Event> events;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}
    static std::uint64_t Unexpected()
    { throw std::runtime_error("unexpected lower boundary"); }
    std::uint64_t GetTlsValue(std::uint32_t index) override
    { events.push_back({1, index}); return 0x7100u; }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { (void)Unexpected(); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { return Unexpected(); }
    std::uint64_t CallThreadDataGetterWithState(GuestAddress target,
        std::uint64_t context, CrtThreadDataCall& state) override
    {
        events.push_back({2, target, context, state.thread_environment});
        return 0xAABBCCDD00022000ull;
    }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { return Unexpected(); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t, std::uint64_t) override
    { return Unexpected(); }
    void FreeThreadData(std::uint64_t) override { (void)Unexpected(); }
    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall state{thread_environment};
        const auto result = GetCrtThreadData(memory, *this, state);
        thread_environment = state.thread_environment;
        return result;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { return Unexpected(); }
    std::uint64_t BugCheck(std::uint32_t) override { return Unexpected(); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { return Unexpected(); }
    void CallHandler(GuestMemory& guest, GuestAddress target,
        InvalidParameterCall& call) override
    {
        events.push_back({3, target, call.thread_environment,
            call.arguments[0], call.arguments[7]});
        if (mode != Mode::ReadInvalid || call.arguments[7] != 22)
            throw std::runtime_error("unexpected invalid-parameter arguments");
        guest.WriteU32(static_cast<GuestAddress>(Stack) - 8u, 0x1234ABCDu);
    }
    void Trap(const InvalidParameterCall&) override { (void)Unexpected(); }

    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags,
        std::uint64_t bytes) override
    {
        events.push_back({4, heap, flags, bytes});
        if (mode == Mode::BufferAlias)
            WriteU64(memory, static_cast<GuestAddress>(Stack) - 16u,
                0xAABBCCDDEEFF0011ull);
        return mode == Mode::BufferFailure ? 0 : 0x1234567800040000ull;
    }
    void EnterMissingHeapPath() override { (void)Unexpected(); }
    void ReportMissingHeap(std::uint32_t) override { (void)Unexpected(); }
    void TerminateMissingHeap(std::uint32_t) override { (void)Unexpected(); }
    std::int32_t RetryAllocation(std::uint64_t) override
    { return static_cast<std::int32_t>(Unexpected()); }
    GuestAddress GetErrorAddress() override
    { events.push_back({5, 0x25000u}); return 0x25000u; }

    std::uint64_t InitializeCriticalSection(GuestMemory& guest,
        InvalidParameterCall& call, family::FrameRegisters& frame) override
    {
        events.push_back({6, frame.sp, frame.lr, frame.r31, call.arguments[0]});
        if (mode == Mode::NativeWrapper)
            guest.WriteU32(static_cast<GuestAddress>(frame.sp + 88u),
                0x2468ACE0u);
        return 0x1122334455667788ull; // The PPC wrapper then sets r3 to 1.
    }
    std::uint64_t CallIndirect(GuestAddress target, GuestMemory& guest,
        InvalidParameterCall& call, family::FrameRegisters& frame) override
    {
        events.push_back({7, target, frame.sp, frame.lr, call.arguments[0]});
        if (target != 0x7100u || mode != Mode::OnceOverride)
            throw std::runtime_error("unexpected indirect target");
        WriteU64(guest, static_cast<GuestAddress>(Stack) - 16u,
            0xAABBCCDDEEFF0011ull);
        return 0xAABBCCDD00000007ull;
    }
};
Services* active = nullptr;

family::FrameRegisters Frame(const PPCContext& context)
{ return {context.lr, context.r31.u64, context.r1.u64}; }

InvalidParameterCall Call(const PPCContext& context)
{
    return {{{context.r3.u64, context.r4.u64, context.r5.u64,
        context.r6.u64, context.r7.u64, context.r8.u64,
        context.r9.u64, context.r10.u64}}, context.r13.u64};
}

void ToPpc(PPCContext& context, const InvalidParameterCall& call,
    const family::FrameRegisters& frame)
{
    context.r3.u64 = call.arguments[0]; context.r4.u64 = call.arguments[1];
    context.r5.u64 = call.arguments[2]; context.r6.u64 = call.arguments[3];
    context.r7.u64 = call.arguments[4]; context.r8.u64 = call.arguments[5];
    context.r9.u64 = call.arguments[6]; context.r10.u64 = call.arguments[7];
    context.r13.u64 = call.thread_environment;
    context.r31.u64 = frame.r31; context.r1.u64 = frame.sp;
    context.lr = frame.lr;
}

void Seed(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xBD, 0x90000u);
    std::memset(window.bytes + 0x83214000u, 0, 0x3000u);
    std::memset(window.bytes + 0x83245000u, 0, 0x2000u);
    std::memset(window.bytes + 0x832D3000u, 0, 0x2000u);
    std::memset(window.bytes + 0x83378000u, 0, 0x2000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Stream + 16u, 0xCAFEBABEu);
    memory.WriteU32(Stream + 12u, 0x40u);
    memory.WriteU32(BufferCounter, 9);
    memory.WriteU32(HeapGlobal, 0x30000u);
    memory.WriteU32(RetryGlobal, 0);
    memory.WriteU32(OnceTarget, mode == Mode::OnceOverride ? 0x7103u : 0);
    memory.WriteU32(Handler, 0x7203u);
    memory.WriteU32(0x9000u + 336u, 1);
    memory.WriteU32(0x83214D74u, 0x32000u);
    memory.WriteU32(0x83214D78u, 5);
}

bool Check(const Case& item)
{
    Window original, recovered;
    Seed(original, item.mode); Seed(recovered, item.mode);
    Services expected(original, item.mode), actual(recovered, item.mode);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x1234567887654321ull;
    context.r31.u64 = 0x9988776600001234ull;
    context.r13.u64 = 0x1122334400009000ull;
    context.r3.u64 = item.mode == Mode::ReadInvalid ?
        0xAABBCCDD00000000ull : 0xAABBCCDD00010000ull;
    context.r4.u64 = 0x5566778800002000ull;
    auto frame = Frame(context);
    auto call = Call(context);
    active = &expected;
    switch (item.address)
    {
    case 0x82B81648u: __imp__sub_82B81648(context, original.bytes); break;
    case 0x82B85C40u: __imp__sub_82B85C40(context, original.bytes); break;
    case 0x82B82180u: __imp__sub_82B82180(context, original.bytes); break;
    case 0x82B821B0u: __imp__sub_82B821B0(context, original.bytes); break;
    default: throw std::runtime_error("unexpected stream entry");
    }
    std::uint64_t result = 0xDEADBEEFu;
    if (!family::Apply(item.address, actual.memory, actual, actual, actual,
            actual, call, Stack, frame, result))
        throw std::runtime_error("missing stream entry");
    const bool matched = result == item.expected &&
        result == context.r3.u64 && frame.lr == context.lr &&
        frame.r31 == context.r31.u64 && frame.sp == context.r1.u64 &&
        call.thread_environment == context.r13.u64 &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x90000u) == 0 &&
        std::memcmp(original.bytes + 0x83214000u,
            recovered.bytes + 0x83214000u, 0x3000u) == 0 &&
        std::memcmp(original.bytes + 0x83245000u,
            recovered.bytes + 0x83245000u, 0x2000u) == 0 &&
        std::memcmp(original.bytes + 0x832D3000u,
            recovered.bytes + 0x832D3000u, 0x2000u) == 0 &&
        std::memcmp(original.bytes + 0x83378000u,
            recovered.bytes + 0x83378000u, 0x2000u) == 0;
    if (!matched)
        std::printf("FAIL %08X mode=%u r3=%llX/%llX sp=%llX/%llX lr=%llX/%llX r31=%llX/%llX events=%zu/%zu\n",
            item.address, static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(frame.sp),
            static_cast<unsigned long long>(context.r1.u64),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(frame.r31),
            static_cast<unsigned long long>(context.r31.u64),
            expected.events.size(), actual.events.size());
    return matched;
}
} // namespace

void OriginalError(PPCContext& context, std::uint8_t*)
{
    active->thread_environment = context.r13.u64;
    context.r3.u64 = GetAllocationErrorAddress(*active);
    context.r13.u64 = active->thread_environment;
}
void OriginalInvalid(PPCContext& context, std::uint8_t*)
{
    auto call = Call(context);
    call.arguments[0] = ReportInvalidParameter(active->memory, *active, call);
    const auto frame = Frame(context);
    ToPpc(context, call, frame);
}
void OriginalRawAllocate(PPCContext& context, std::uint8_t*)
{ context.r3.u64 = AllocateRawMemory(active->memory, *active, context.r3.u64); }
void OriginalNative(PPCContext& context, std::uint8_t*)
{
    auto call = Call(context);
    auto frame = Frame(context);
    call.arguments[0] = active->InitializeCriticalSection(active->memory, call, frame);
    ToPpc(context, call, frame);
}
void OriginalIndirect(PPCContext& context, std::uint8_t* base,
    std::uint32_t target)
{
    if (target == 0x82B82180u)
    {
        __imp__sub_82B82180(context, base);
        return;
    }
    auto call = Call(context);
    auto frame = Frame(context);
    call.arguments[0] = active->CallIndirect(target, active->memory, call, frame);
    ToPpc(context, call, frame);
}
void OriginalUnexpected(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unreachable direct call executed"); }

int main()
{
    try
    {
        for (const auto& item : Cases)
            if (!Check(item)) return 1;
        Window window;
        Services services(window, Mode::ReadValid);
        family::FrameRegisters frame{1, 2, 3};
        InvalidParameterCall call{{{4}}, 5};
        std::uint64_t result = 6;
        if (family::Apply(0xFFFFFFFFu, services.memory, services, services,
                services, services, call, Stack, frame, result) ||
            frame.lr != 1 || frame.r31 != 2 || frame.sp != 3 ||
            call.arguments[0] != 4 || result != 6)
            throw std::runtime_error("unknown address changed state");
        std::printf("PASS crt-stream-state %zu original PPC cases + unknown\n",
            std::size(Cases));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL crt-stream-state: %s\n", error.what());
        return 1;
    }
}
