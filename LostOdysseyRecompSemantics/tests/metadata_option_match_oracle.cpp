#include "lo_semantics/metadata_option_match.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::metadata_option_match;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Left = 0x10000u;
constexpr GuestAddress Right = 0x11000u;
enum class Mode { CompareFold, CompareDifference, FindBoundary,
    ScanDelimiter, ScanRetry, ScanNoDelimiter };
struct Case { GuestAddress address; Mode mode; std::uint64_t expected; };
constexpr Case Cases[] = {
    {0x82296858u, Mode::CompareFold, 0},
    {0x82296858u, Mode::CompareDifference, UINT64_MAX},
    {0x82297390u, Mode::FindBoundary, 0xCAFEBABE00010006ull},
    {0x8247C0C0u, Mode::ScanDelimiter, 1},
    {0x8247C0C0u, Mode::ScanRetry, 1},
    {0x8247C0C0u, Mode::ScanNoDelimiter, 0},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000u, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve guest option RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Services final : CrtThreadDataServices, InvalidParameterServices
{
    GuestMemory memory;
    explicit Services(Window& window)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)) {}
    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unexpected TLS boundary"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unexpected TLS boundary"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data boundary"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected allocation boundary"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data boundary"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unexpected thread-data boundary"); }
    void CallHandler(GuestMemory&, GuestAddress, InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter boundary"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter trap"); }
};

void Text(GuestMemory& memory, GuestAddress address, const char* text)
{
    do
    {
        memory.WriteU16(address, static_cast<std::uint8_t>(*text));
        address += 2u;
    } while (*text++ != 0);
}

void Seed(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xBD, 0x90000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    switch (mode)
    {
    case Mode::CompareFold:
        Text(memory, Left, "Ab"); Text(memory, Right, "aB"); break;
    case Mode::CompareDifference:
        Text(memory, Left, "ab"); Text(memory, Right, "aC"); break;
    case Mode::FindBoundary:
        Text(memory, Left, "xx/Opt"); Text(memory, Right, "opt"); break;
    case Mode::ScanDelimiter:
        Text(memory, Left, "-Opt "); Text(memory, Right, "opt"); break;
    case Mode::ScanRetry:
        Text(memory, Left, "-OptX/Opt\t"); Text(memory, Right, "opt"); break;
    case Mode::ScanNoDelimiter:
        Text(memory, Left, "xOpt "); Text(memory, Right, "opt"); break;
    }
}

family::FrameRegisters Frame(const PPCContext& context)
{
    return {context.lr, {context.r25.u64, context.r26.u64, context.r27.u64,
        context.r28.u64, context.r29.u64, context.r30.u64, context.r31.u64}};
}

InvalidParameterCall Call(const PPCContext& context)
{
    return {{{context.r3.u64, context.r4.u64, context.r5.u64,
        context.r6.u64, context.r7.u64, context.r8.u64,
        context.r9.u64, context.r10.u64}}, context.r13.u64};
}

bool Check(const Case& item)
{
    Window original, recovered;
    Seed(original, item.mode); Seed(recovered, item.mode);
    Services expected(original), actual(recovered);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0xABCDEF0987654321ull;
    context.r3.u64 = 0xCAFEBABE00010000ull;
    context.r4.u64 = 0xDEADC0DE00011000ull;
    context.r5.u64 = 0xFACEB00C00000002ull;
    context.r13.u64 = 0x1234567800009000ull;
    for (unsigned i = 25; i <= 31; ++i)
    {
        // Assign explicitly: PPCContext registers are distinct members.
        const std::uint64_t value = 0x1122334455000000ull + i;
        switch (i)
        {
        case 25: context.r25.u64 = value; break;
        case 26: context.r26.u64 = value; break;
        case 27: context.r27.u64 = value; break;
        case 28: context.r28.u64 = value; break;
        case 29: context.r29.u64 = value; break;
        case 30: context.r30.u64 = value; break;
        case 31: context.r31.u64 = value; break;
        }
    }
    if (item.address != 0x82296858u)
        context.r5.u64 = 0xFACEB00C00000000ull;
    auto frame = Frame(context);
    auto call = Call(context);
    switch (item.address)
    {
    case 0x82296858u: __imp__sub_82296858(context, original.bytes); break;
    case 0x82297390u: __imp__sub_82297390(context, original.bytes); break;
    case 0x8247C0C0u: __imp__sub_8247C0C0(context, original.bytes); break;
    default: throw std::runtime_error("unexpected option entry");
    }
    std::uint64_t result = 0x99999999u;
    if (!family::Apply(item.address, actual.memory, actual, actual, call,
        Stack, frame, result))
        throw std::runtime_error("missing option entry");
    const auto observed = Frame(context);
    const bool direct_volatiles = item.address != 0x82296858u ||
        (call.arguments[1] == context.r4.u64 &&
         call.arguments[2] == context.r5.u64 &&
         call.arguments[5] == context.r8.u64 &&
         call.arguments[6] == context.r9.u64 &&
         call.arguments[7] == context.r10.u64);
    const bool matched = result == item.expected &&
        result == context.r3.u64 &&
        direct_volatiles &&
        frame.lr == observed.lr &&
        frame.r25_through_r31 == observed.r25_through_r31 &&
        context.r1.u64 == Stack &&
        call.thread_environment == context.r13.u64 &&
        std::memcmp(original.bytes, recovered.bytes, 0x90000u) == 0;
    if (!matched)
        std::printf("FAIL %08X mode=%u r3=%llX/%llX lr=%llX/%llX\n",
            item.address, static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(context.lr));
    return matched;
}
} // namespace

void OriginalError(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected errno path"); }
void OriginalInvalid(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected invalid-parameter path"); }

int main()
{
    try
    {
        for (const auto& item : Cases)
            if (!Check(item)) return 1;
        Window window;
        Services services(window);
        family::FrameRegisters frame{0xAABBCCDDu, {1, 2, 3, 4, 5, 6, 7}};
        InvalidParameterCall call{{{0x1234u}}, 0x5678u};
        std::uint64_t result = 0xAABBCCDDu;
        if (family::Apply(0xFFFFFFFFu, services.memory, services, services,
                call, Stack, frame, result) || result != 0xAABBCCDDu ||
            frame.lr != 0xAABBCCDDu || call.arguments[0] != 0x1234u)
            throw std::runtime_error("unknown option address changed state");
        std::printf("PASS metadata-option-match %zu original PPC cases + unknown\n",
            std::size(Cases));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL metadata-option-match: %s\n", error.what());
        return 1;
    }
}
