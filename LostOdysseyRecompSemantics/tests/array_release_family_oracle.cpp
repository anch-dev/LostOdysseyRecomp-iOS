#include "lo_semantics/array_release_family.h"

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
constexpr GuestAddress Stack = 0x4f000;
constexpr GuestAddress Array = 0x10000;
constexpr GuestAddress Data = 0x12000;
constexpr GuestAddress Replacement = 0x21000;
constexpr GuestAddress Manager = 0x35000;
constexpr GuestAddress Vtable = 0x35100;
constexpr GuestAddress Global = 0x8330b608;
constexpr GuestAddress GlobalPage = Global & ~0xfffu;
constexpr GuestAddress ReleaseMethod = 0x82345670;
constexpr std::uint64_t CallbackResult = 0xabcdef0000000077ull;

struct Entry
{
    GuestAddress address;
    std::uint32_t element_size;
    std::uint32_t resize_argument;
    PPCFunc* original;
};
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

enum class Mode { Direct, ReplaceAfterRemove, NullAfterRemove, LazyAndMutateRelease };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve guest window");
        if (!VirtualAlloc(bytes, 0x50000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + GlobalPage, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest pages");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 6> arguments;
    std::array<std::uint32_t, 3> header;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}

    void Record(char kind, std::array<std::uint64_t, 6> arguments)
    {
        events.push_back({kind, arguments, {memory.ReadU32(Array),
            memory.ReadU32(Array + 4), memory.ReadU32(Array + 8)}});
    }

    std::uint64_t ReleaseStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t buffer) override
    {
        Record('F', {method, manager, buffer, 0, 0, 0});
        if (method != ReleaseMethod || manager != Manager)
            throw std::runtime_error("unexpected manager release method");
        if (mode == Mode::LazyAndMutateRelease)
        {
            memory.WriteU32(Array, Replacement);
            memory.WriteU32(Array + 4, 0xaaaaaaaau);
            memory.WriteU32(Array + 8, 0xbbbbbbbbu);
        }
        return CallbackResult;
    }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected allocate"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocate"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construct"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construct"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager init method"); }
};

struct RemoveServices final : ArrayResizeServices
{
    void InitializeManager() override
    { throw std::runtime_error("unexpected remove initialization"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected remove resize"); }
};

Services* active = nullptr;
RemoveServices remove_services;

void Initialize(std::uint8_t* bytes, Mode mode)
{
    std::memset(bytes, 0xbd, 0x50000);
    std::memset(bytes + GlobalPage, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(Global, mode == Mode::LazyAndMutateRelease ? 0 : Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 12, ReleaseMethod | 3u);
    memory.WriteU32(Array, Data);
    memory.WriteU32(Array + 4, 3);
    memory.WriteU32(Array + 8, 3);
}

bool Test(const Entry& entry, Mode mode, unsigned index,
    Window& original, Window& recovered)
{
    Initialize(original.bytes, mode);
    Initialize(recovered.bytes, mode);
    Services expected(original.bytes, mode);
    Services actual(recovered.bytes, mode);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.r3.u64 = 0x1234567800010000ull;
    context.r31.u64 = 0x1122334455667788ull;
    context.lr = 0x82200000;
    active = &expected;
    entry.original(context, original.bytes);
    std::uint64_t result = 0xfedcba9876543210ull;
    active = &actual;
    if (!array_release_family::Apply(entry.address, actual.memory, actual,
                                     Array, Stack, result))
        throw std::runtime_error("missing array-release mapping");
    const bool same = context.r3.u64 == result &&
        context.r1.u64 == Stack && context.r31.u64 == 0x1122334455667788ull &&
        context.lr == 0x82200000 && expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0 &&
        std::memcmp(original.bytes + GlobalPage,
                    recovered.bytes + GlobalPage, 0x1000) == 0;
    if (!same)
    {
        std::size_t first = 0;
        while (first < Stack - 0x1000u &&
               original.bytes[first] == recovered.bytes[first]) ++first;
        std::fprintf(stderr,
            "FAIL array-release %08x case %u mode %u r3 %llx/%llx events %zu/%zu first %zx\n",
            entry.address, index, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(), first);
        return false;
    }
    return true;
}
} // namespace

namespace lo::semantic::gpu
{
std::uint64_t InitializeManager(GuestMemory& memory, ManagerInitServices&,
    GuestAddress frame_base)
{
    active->Record('I', {frame_base, 0, 0, 0, 0, 0});
    memory.WriteU32(Global, Manager);
    return Manager;
}

void RemoveArrayRange(GuestMemory& memory, ArrayResizeServices&,
    GuestAddress array, std::uint32_t first, std::uint32_t count,
    std::uint32_t element_size, std::uint32_t argument, GuestAddress frame_base)
{
    active->Record('N', {array, first, count, element_size, argument, frame_base});
    if (active->mode == Mode::ReplaceAfterRemove)
        memory.WriteU32(array, Replacement);
    if (active->mode == Mode::NullAfterRemove)
        memory.WriteU32(array, 0);
}

void ResizeArray(GuestMemory&, ArrayResizeServices&, GuestAddress,
    std::uint32_t, std::uint32_t)
{ throw std::runtime_error("unexpected direct resize"); }
} // namespace lo::semantic::gpu

PPC_FUNC(sub_82298AF8)
{
    lo::semantic::gpu::RemoveArrayRange(active->memory, remove_services,
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
        ctx.r1.u32 - 128u);
    ctx.r3.u64 = 0xfeedcafe00000000ull;
}

PPC_FUNC(sub_823F3340)
{
    ctx.r3.u64 = lo::semantic::gpu::ReleaseManagerBuffer(
        active->memory, *active, ctx.r3.u64, ctx.r1.u32);
}

int main()
{
    try
    {
        Window original, recovered;
        for (unsigned index = 0; index != std::size(Entries); ++index)
        {
            const auto mode = static_cast<Mode>(index % 4u);
            if (!Test(Entries[index], mode, index, original, recovered))
                return 1;
        }
        std::printf("PASS array-release %zu new entries\n", std::size(Entries));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
