#include "lo_semantics/metadata_utf16_search.h"
#include "lo_semantics/metadata_utf16_buffer.h"
#include "lo_semantics/manager_object_registration.h"
#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/string_property_initializer.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = metadata_utf16_search;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Data = 0x10000u, Pattern = 0x11000u;
constexpr GuestAddress Replacement = 0x12000u, Destination = 0x20000u;
constexpr GuestAddress Source = 0x21000u, Redirected = 0x22000u;
constexpr GuestAddress Manager = 0x50000u, Table = 0x50100u;
constexpr GuestAddress ResizeMethod = 0x7300u, FreeMethod = 0x7400u;
enum class Mode { SearchEmpty, SearchLate, SearchMiss, PrefixNegative,
    PrefixAlias, ReplaceEmpty, ReplaceMiss, ReplaceTwice, ReplaceAlias };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
    {0x8229d0e8u, __imp__sub_8229D0E8, Mode::SearchEmpty},
    {0x8229d0e8u, __imp__sub_8229D0E8, Mode::SearchLate},
    {0x8229d0e8u, __imp__sub_8229D0E8, Mode::SearchMiss},
    {0x82367160u, __imp__sub_82367160, Mode::PrefixNegative},
    {0x82367160u, __imp__sub_82367160, Mode::PrefixAlias},
    {0x82339d30u, __imp__sub_82339D30, Mode::ReplaceEmpty},
    {0x82339d30u, __imp__sub_82339D30, Mode::ReplaceMiss},
    {0x82339d30u, __imp__sub_82339D30, Mode::ReplaceTwice},
    {0x82339d30u, __imp__sub_82339D30, Mode::ReplaceAlias},
};
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000u, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x821a8000u, 0x1000u, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000u, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve UTF16 search RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 5> args;
    bool operator==(const Event&) const = default;
};
struct Services final : ArrayResizeServices, ManagerFacadeServices,
    metadata_utf16_slice::VirtualServices
{
    GuestMemory memory;
    Mode mode;
    GuestAddress next = 0x40000u;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    void InitializeManager() override { throw std::runtime_error("unexpected init"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager init method"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected allocate method"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes, std::uint32_t argument) override
    {
        events.push_back({'Z', {method, manager, old, bytes, argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8u || bytes > 256u)
            throw std::runtime_error("unexpected search resize boundary");
        if (mode == Mode::PrefixAlias)
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 108u, Redirected);
        if (mode == Mode::ReplaceAlias && bytes == 0u)
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 60u, Redirected);
        if (bytes == 0u) return 0;
        const GuestAddress fresh = next;
        next += 0x100u;
        if (old != 0u)
            for (unsigned i = 0; i < 64u; ++i)
                memory.WriteU8(fresh + i, memory.ReadU8(old + i));
        return fresh;
    }
    std::uint64_t ReleaseStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t storage) override
    {
        events.push_back({'F', {method, manager, storage}});
        if (method != FreeMethod || manager != Manager)
            throw std::runtime_error("unexpected search release boundary");
        if (mode == Mode::ReplaceAlias)
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 60u, Redirected);
        return 0xabcdef0000000025ull;
    }
    std::uint64_t CallMethod(GuestAddress, GuestMemory&, std::uint64_t,
        std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t,
        metadata_utf16_slice::FrameRegisters&) override
    { throw std::runtime_error("unexpected UTF16 virtual method"); }
};
Services* active = nullptr;
void Text(GuestMemory& memory, GuestAddress address, const char* text)
{
    unsigned i = 0;
    do { memory.WriteU16(address + i * 2u, static_cast<std::uint8_t>(text[i])); }
    while (text[i++] != '\0');
}
void Seed(Window& window, Mode mode)
{
    std::memset(window.bytes, 0, 0x90000u);
    std::memset(window.bytes + 0x821a8000u, 0, 0x1000u);
    std::memset(window.bytes + 0x8330b000u, 0, 0x1000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, Table);
    memory.WriteU32(Table + 8u, ResizeMethod | 3u);
    memory.WriteU32(Table + 12u, FreeMethod | 3u);
    Text(memory, Data, "ab--ab");
    Text(memory, Pattern, mode == Mode::SearchEmpty ? "" :
        mode == Mode::SearchLate ? "--ab" :
        mode == Mode::SearchMiss || mode == Mode::ReplaceMiss ? "abc" : "ab");
    Text(memory, Replacement, "X");
    memory.WriteU32(Source, Data);
    memory.WriteU32(Source + 4u, mode == Mode::ReplaceEmpty ? 0u : 7u);
    memory.WriteU32(Source + 8u, 7u);
    Text(memory, 0x821a83d0u, "");
}
PPCContext Initial(const Case& test)
{
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000000000ull |
        (test.address == 0x8229d0e8u ? Data : Destination);
    raw.r4.u64 = 0xfedcba0000000000ull |
        (test.address == 0x8229d0e8u ? Pattern : Source);
    raw.r5.u64 = test.mode == Mode::PrefixNegative ? 0x12345678ffffffffull :
        test.mode == Mode::PrefixAlias ? 99u : 0xaabbcc0000000000ull | Pattern;
    raw.r6.u64 = 0xddeeff0000000000ull | Replacement;
    raw.lr = 0x1234567887654321ull;
    const std::array<PPCRegister*, 7> regs = {&raw.r25, &raw.r26, &raw.r27,
        &raw.r28, &raw.r29, &raw.r30, &raw.r31};
    for (unsigned i = 0; i < regs.size(); ++i)
        regs[i]->u64 = 0xa1b2c3d400000000ull | (25u + i);
    return raw;
}
bool SameMemory(const Window& a, const Window& b, std::uint64_t& first)
{
    constexpr std::array<std::pair<std::uint64_t, std::size_t>, 3> regions = {{
        {0, 0x90000u}, {0x821a8000u, 0x1000u}, {0x8330b000u, 0x1000u}}};
    for (const auto [start, size] : regions)
        for (std::size_t i = 0; i < size; ++i)
            if (a.bytes[start + i] != b.bytes[start + i])
            { first = start + i; return false; }
    return true;
}
bool Compare(const Case& test)
{
    Window original, recovered;
    Seed(original, test.mode); Seed(recovered, test.mode);
    PPCContext raw = Initial(test);
    const auto initial = raw;
    Services expected(original.bytes, test.mode);
    active = &expected;
    test.original(raw, original.bytes);
    Services actual(recovered.bytes, test.mode);
    family::FrameRegisters frame{initial.lr, initial.r25.u64,
        initial.r26.u64, initial.r27.u64, initial.r28.u64,
        initial.r29.u64, initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0;
    if (!family::Apply(test.address, actual.memory, actual, actual, actual,
            initial.r3.u64, initial.r4.u64, initial.r5.u64, initial.r6.u64,
            Stack, frame, result)) throw std::runtime_error("search entry missing");
    std::uint64_t first = 0;
    bool same = SameMemory(original, recovered, first) &&
        expected.events == actual.events && raw.r3.u64 == result &&
        raw.r1.u64 == Stack && raw.lr == frame.lr;
    const std::array<const PPCRegister*, 7> raw_regs = {&raw.r25, &raw.r26,
        &raw.r27, &raw.r28, &raw.r29, &raw.r30, &raw.r31};
    const std::array<const std::uint64_t*, 7> readable = {&frame.r25,
        &frame.r26, &frame.r27, &frame.r28, &frame.r29, &frame.r30, &frame.r31};
    for (unsigned i = 0; i < raw_regs.size(); ++i)
        same &= raw_regs[i]->u64 == *readable[i];
    if (test.mode == Mode::PrefixAlias && raw.r3.u32 != Redirected)
        throw std::runtime_error("nested prefix save-slot alias not reached");
    if (test.mode == Mode::ReplaceAlias && raw.r25.u32 != Redirected)
        throw std::runtime_error("replacement owned save-slot alias not reached");
    if (!same)
        std::fprintf(stderr, "FAIL search %08x mode=%u r3=%llx/%llx first=%llx events=%zu/%zu\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(first), expected.events.size(), actual.events.size());
    return same;
}
} // namespace

void OriginalDirectCall(PPCContext& ctx, std::uint8_t* base, GuestAddress target)
{
    if (target == 0x82296830u)
    {
        ctx.r3.u64 = registered_metadata_string::Utf16Length(active->memory, ctx.r3.u64);
        return;
    }
    if (target == 0x8229f678u)
    {
        ResizeArray(active->memory, *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
        return;
    }
    if (target == 0x8232d318u)
    {
        const auto copy = manager_object_registration::CopyUtf16Padded(
            active->memory, ctx.r3.u64, ctx.r4.u64, ctx.r5.u64);
        ctx.r3.u64 = copy.r3; ctx.r4.u64 = copy.r4; ctx.r5.u64 = copy.r5;
        return;
    }
    if (target == 0x82298938u)
    {
        const auto sp = ctx.r1.u32;
        PPC_STORE_U32(sp - 8u, ctx.lr);
        PPC_STORE_U64(sp - 16u, ctx.r31.u64);
        PPC_STORE_U32(sp - 96u, sp);
        ctx.r3.u64 = ResetTwoByteArray(active->memory, *active, ctx.r3.u32, sp);
        ctx.lr = PPC_LOAD_U32(sp - 8u);
        ctx.r31.u64 = PPC_LOAD_U64(sp - 16u);
        return;
    }
    if (target == 0x822a06c0u || target == 0x8232d378u)
    {
        std::uint64_t result = 0;
        string_property_initializer::FrameRegisters frame{ctx.lr,
            ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
        if (target == 0x822a06c0u)
            (void)string_property_initializer::Apply(target, active->memory,
                *active, *active, ctx.r3.u64, ctx.r4.u64, ctx.r1.u64, frame, result);
        else
        {
            metadata_utf16_buffer::FrameRegisters lower{frame.lr,
                frame.r28, frame.r29, frame.r30, frame.r31};
            (void)metadata_utf16_buffer::Apply(target, active->memory,
                *active, *active, ctx.r3.u64, ctx.r4.u64, 0, ctx.r1.u64, lower, result);
            frame = {lower.lr, lower.r28, lower.r29, lower.r30, lower.r31};
        }
        ctx.r3.u64 = result; ctx.lr = frame.lr;
        ctx.r28.u64 = frame.r28; ctx.r29.u64 = frame.r29;
        ctx.r30.u64 = frame.r30; ctx.r31.u64 = frame.r31;
        return;
    }
    throw std::runtime_error("unexpected search lower target");
}
int main()
{
    try
    {
        for (const auto& test : Cases) if (!Compare(test)) return 1;
        Window window;
        Services service(window.bytes, Mode::SearchEmpty);
        family::FrameRegisters frame{1, 2, 3, 4, 5, 6, 7, 8};
        std::uint64_t result = 9;
        if (family::Apply(0xffffffffu, service.memory, service, service, service,
                1, 2, 3, 4, Stack, frame, result) || result != 9 || frame.lr != 1 ||
            frame.r25 != 2 || frame.r31 != 8 || !service.events.empty())
            throw std::runtime_error("unknown search entry changed state");
        std::printf("PASS metadata-utf16-search %zu original PPC cases + unknown\n", std::size(Cases));
        std::puts("LIMIT real batch search composition and original prefix constructor; known direct lower models with bounded ABI/services, terminating ordinary RAM only");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
