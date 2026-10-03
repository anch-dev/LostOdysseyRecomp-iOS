#include "lo_semantics/metadata_utf16_buffer.h"
#include "lo_semantics/registered_metadata_composed.h"
#include "lo_semantics/registered_metadata_string.h"
#include "lo_semantics/registered_metadata_words.h"
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
namespace family = lo::semantic::gpu::metadata_utf16_buffer;
namespace property = lo::semantic::gpu::string_property_initializer;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Header = 0x20000u;
constexpr GuestAddress Destination = 0x21000u;
constexpr GuestAddress Source = 0x10000u;
constexpr GuestAddress OldStorage = 0x30000u;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress ManagerTable = 0x50100u;
constexpr GuestAddress ResizeMethod = 0x82345680u;
constexpr GuestAddress ReleaseMethod = 0x82345690u;
constexpr std::uint64_t Stack = 0x1234567800070000ull;

enum class Mode { AppendEmpty, AppendFresh, AppendGrowLive,
                  ComposeNormal, ComposeEmpty, ComposeSavedAlias };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
    {0x8232d378u, __imp__sub_8232D378, Mode::AppendEmpty},
    {0x8232d378u, __imp__sub_8232D378, Mode::AppendFresh},
    {0x8232d378u, __imp__sub_8232D378, Mode::AppendGrowLive},
    {0x8232d418u, __imp__sub_8232D418, Mode::ComposeNormal},
    {0x8232d418u, __imp__sub_8232D418, Mode::ComposeEmpty},
    {0x8232d418u, __imp__sub_8232D418, Mode::ComposeSavedAlias},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT,
                PAGE_READWRITE) || !VirtualAlloc(bytes + 0x8330b000u,
                0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve/commit UTF16 buffer RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 6> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices, ManagerFacadeServices
{
    GuestMemory memory;
    Mode mode;
    std::uint32_t next_storage = 0x40000u;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected array manager initialization"); }
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
    { throw std::runtime_error("unexpected standalone allocation"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'Z', {method, manager, old_storage,
            bytes, argument, next_storage}});
        if (method != ResizeMethod || manager != Manager ||
            argument != 8u || bytes > 0x1000u)
            throw std::runtime_error("incorrect UTF16 resize boundary");
        const GuestAddress fresh = next_storage;
        next_storage += 0x1000u;
        if (old_storage != 0)
            for (std::uint32_t i = 0; i < 64; ++i)
                memory.WriteU8(fresh + i, memory.ReadU8(old_storage + i));
        if (mode == Mode::AppendGrowLive)
        {
            memory.WriteU16(Source, 'Q');
            memory.WriteU16(Source + 2u, 0);
        }
        return fresh;
    }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t buffer) override
    {
        events.push_back({'F', {method, manager, buffer}});
        if (method != ReleaseMethod || manager != Manager)
            throw std::runtime_error("incorrect UTF16 release boundary");
        if (mode == Mode::ComposeSavedAlias)
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 16u,
                0x87654321u);
        return 0xabcdef000000f00dull;
    }
};
Services* active = nullptr;

void WriteText(GuestMemory& memory, GuestAddress address, const char* text)
{
    std::size_t index = 0;
    do
    {
        memory.WriteU16(address + static_cast<GuestAddress>(2 * index),
            static_cast<std::uint8_t>(text[index]));
    } while (text[index++] != '\0');
}

void Initialize(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xbd, 0x90000);
    std::memset(window.bytes + 0x8330b000u, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 8u, ResizeMethod | 3u);
    memory.WriteU32(ManagerTable + 12u, ReleaseMethod | 3u);
    const bool empty = mode == Mode::AppendEmpty || mode == Mode::ComposeEmpty;
    WriteText(memory, Source, empty ? "" : "AB");
    const bool fresh = mode == Mode::AppendFresh || mode == Mode::ComposeEmpty;
    memory.WriteU32(Header, fresh ? 0u : OldStorage);
    memory.WriteU32(Header + 4u, fresh ? 0u : 3u);
    memory.WriteU32(Header + 8u,
        mode == Mode::AppendGrowLive ? 3u : fresh ? 0u : 8u);
    WriteText(memory, OldStorage, "XY");
    memory.WriteU32(Destination, 0u);
    memory.WriteU32(Destination + 4u, 0u);
    memory.WriteU32(Destination + 8u, 0u);
}

bool SameMemory(const Window& a, const Window& b,
    std::uint64_t& first)
{
    for (const std::uint64_t start : {0ull, 0x8330b000ull})
    {
        const std::size_t size = start == 0 ? 0x90000u : 0x1000u;
        for (std::size_t i = 0; i < size; ++i)
            if (a.bytes[start + i] != b.bytes[start + i])
            {
                first = start + i;
                return false;
            }
    }
    return true;
}

bool Compare(const Case& test)
{
    Window original, recovered;
    Initialize(original, test.mode);
    Initialize(recovered, test.mode);
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000000000ull |
        (test.address == 0x8232d378u ? Header : Destination);
    raw.r4.u64 = test.address == 0x8232d378u ?
        0xabcdef0000010000ull : 0xabcdef0000020000ull;
    raw.r5.u64 = 0xabcdef0000010000ull;
    raw.lr = 0x1122334455667788ull;
    raw.r28.u64 = 0xabcdef0000000028ull;
    raw.r29.u64 = 0xabcdef0000000029ull;
    raw.r30.u64 = 0xabcdef0000000030ull;
    raw.r31.u64 = 0xabcdef0000000031ull;
    const PPCContext initial = raw;
    Services expected(original.bytes, test.mode);
    active = &expected;
    test.original(raw, original.bytes);

    Services actual(recovered.bytes, test.mode);
    family::FrameRegisters frame{initial.lr, initial.r28.u64,
        initial.r29.u64, initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0;
    if (!family::Apply(test.address, actual.memory, actual, actual,
            initial.r3.u64, initial.r4.u64, initial.r5.u64,
            initial.r1.u64, frame, result))
        throw std::runtime_error("UTF16 buffer entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        raw.r1.u64 == Stack && raw.r3.u64 == result &&
        raw.lr == frame.lr && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31;
    if (!same)
        std::fprintf(stderr,
            "FAIL utf16-buffer %08x mode=%u r3 %llx/%llx "
            "r31 %llx/%llx events %zu/%zu first %llx:%02x/%02x\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(frame.r31),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

void Save29(PPCContext& ctx, std::uint8_t* base)
{
    PPC_STORE_U64(ctx.r1.u32 - 32u, ctx.r29.u64);
    PPC_STORE_U64(ctx.r1.u32 - 24u, ctx.r30.u64);
    PPC_STORE_U64(ctx.r1.u32 - 16u, ctx.r31.u64);
    PPC_STORE_U32(ctx.r1.u32 - 8u, ctx.r12.u32);
}
void Restore29(PPCContext& ctx, std::uint8_t* base)
{
    ctx.r29.u64 = PPC_LOAD_U64(ctx.r1.u32 - 32u);
    ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 - 24u);
    ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 - 16u);
    ctx.lr = PPC_LOAD_U32(ctx.r1.u32 - 8u);
}

void OriginalDirectCall(PPCContext& ctx, std::uint8_t* base, GuestAddress target)
{
    switch (target)
    {
    case 0x82296830u:
        ctx.r3.u64 = registered_metadata_string::Utf16Length(
            active->memory, ctx.r3.u64);
        return;
    case 0x822c42d8u:
        ctx.r3.u64 = registered_metadata_words::AddArrayElements(
            active->memory, *active, ctx.r3.u32, ctx.r4.u32,
            ctx.r5.u32, ctx.r6.u32);
        return;
    case 0x8230bac0u:
    {
        std::uint64_t after = 0;
        ctx.r3.u64 = registered_metadata_composed::CopyUtf16UntilNull(
            active->memory, ctx.r3.u64, ctx.r4.u64, after);
        ctx.r4.u64 = after;
        return;
    }
    case 0x822a06c0u:
    {
        property::FrameRegisters frame{ctx.lr, ctx.r28.u64, ctx.r29.u64,
            ctx.r30.u64, ctx.r31.u64};
        std::uint64_t result = 0;
        (void)property::Apply(target, active->memory, *active, *active,
            ctx.r3.u64, ctx.r4.u64, ctx.r1.u64, frame, result);
        ctx.r3.u64 = result;
        ctx.lr = frame.lr;
        ctx.r28.u64 = frame.r28;
        ctx.r29.u64 = frame.r29;
        ctx.r30.u64 = frame.r30;
        ctx.r31.u64 = frame.r31;
        return;
    }
    case 0x8232d378u:
        __imp__sub_8232D378(ctx, base);
        return;
    case 0x8229f678u:
        ResizeArray(active->memory, *active, ctx.r3.u32,
            ctx.r4.u32, ctx.r5.u32);
        return;
    case 0x82298a98u:
    {
        const GuestAddress sp = ctx.r1.u32;
        PPC_STORE_U32(sp - 8u, ctx.lr);
        PPC_STORE_U64(sp - 16u, ctx.r31.u64);
        PPC_STORE_U32(sp - 96u, sp);
        ctx.r3.u64 = ReleaseTwoByteArray(active->memory, *active,
            ctx.r3.u32, sp);
        ctx.lr = PPC_LOAD_U32(sp - 8u);
        ctx.r31.u64 = PPC_LOAD_U64(sp - 16u);
        return;
    }
    default: throw std::runtime_error("unexpected UTF16 lower call");
    }
}

int main()
{
    try
    {
        for (const Case& test : Cases) if (!Compare(test)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::AppendEmpty);
        family::FrameRegisters frame{1, 2, 3, 4, 5};
        std::uint64_t result = 7;
        if (family::Apply(0xffffffffu, service.memory, service, service,
                1, 2, 3, Stack, frame, result) || result != 7 ||
            frame.lr != 1 || frame.r28 != 2 || frame.r29 != 3 ||
            frame.r30 != 4 || frame.r31 != 5 || !service.events.empty())
            throw std::runtime_error("unknown UTF16 buffer target changed state");
        std::printf("PASS metadata-utf16-buffer %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT reused UTF16/array/header-copy/release lower models; dynamic resize/release and generic lower ABI external, ordinary RAM");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
