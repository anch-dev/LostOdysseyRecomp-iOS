#include "lo_semantics/metadata_descriptor_array.h"
#include "lo_semantics/memory_move.h"
#include "lo_semantics/registered_metadata_words.h"

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
namespace family = lo::semantic::gpu::metadata_descriptor_array;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Array = 0x10000u;
constexpr GuestAddress Source = 0x11000u;
constexpr GuestAddress Object = 0x20000u;
constexpr GuestAddress Storage = 0x70000u;
constexpr GuestAddress FreeStorage = 0x71000u;
constexpr GuestAddress CopyStorage = 0x72000u;
constexpr GuestAddress SourceStorage = 0x73000u;
constexpr GuestAddress GrownStorage = 0x76000u;
constexpr GuestAddress Descriptors = 0x833690f4u;
constexpr GuestAddress FreeSlots = 0x83369100u;
constexpr GuestAddress Counter = 0x832383c4u;
constexpr GuestAddress Threshold = 0x83315f48u;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress ManagerTable = 0x51000u;
constexpr GuestAddress ResizeMethod = 0x5235u;
constexpr GuestAddress EmptySource = 0x821a83d0u;
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x821a8000u, 0x1000},
    {0x83238000u, 0x1000}, {0x832f0000u, 0x10000},
    {0x8330b000u, 0x1000}, {0x83315000u, 0x2000},
    {0x83369000u, 0x2000}};
enum class Mode { Pop, AssignSelf, AssignGrow, AssignAlias,
    DirectAlias, FreshCounter, ReuseSlot, AppendSlot };
constexpr Mode Cases[] = {Mode::Pop, Mode::AssignSelf,
    Mode::AssignGrow, Mode::AssignAlias, Mode::DirectAlias,
    Mode::FreshCounter, Mode::ReuseSlot, Mode::AppendSlot};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve descriptor guest RAM");
        for (const Region region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit descriptor guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 5> values;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({{method, manager, old, bytes, argument}});
        if (method != (ResizeMethod & ~3u) || manager != Manager ||
            argument != 8u)
            throw std::runtime_error("incorrect descriptor resize target");
        switch (mode)
        {
        case Mode::AssignGrow:
            if (old != 0u || bytes != 6u)
                throw std::runtime_error("incorrect assign growth");
            return CopyStorage;
        case Mode::AssignAlias:
            if (old != 0u || bytes != 4u)
                throw std::runtime_error("incorrect assign alias growth");
            memory.WriteU32(Array + 8u, 0u); // live source count alias
            return CopyStorage;
        case Mode::ReuseSlot:
            if (old != FreeStorage || bytes != 0u)
                throw std::runtime_error("incorrect free-slot shrink");
            // The nested pop's saved r31 is guest RAM. The original helper
            // restores this new full pointer before the parent resumes.
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 144u,
                0xabcdef00u);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 140u,
                0x00021000u);
            return 0u;
        case Mode::AppendSlot:
            if (old != Storage || bytes != 152u)
                throw std::runtime_error("incorrect slot growth");
            for (std::uint32_t i = 0; i < 16u; ++i)
                memory.WriteU8(GrownStorage + i,
                    memory.ReadU8(Storage + i));
            return GrownStorage;
        default:
            throw std::runtime_error("unexpected descriptor resize");
        }
    }
};
Services* active = nullptr;

void Write64(GuestMemory& memory, GuestAddress where, std::uint64_t value)
{
    memory.WriteU32(where, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(where + 4u, static_cast<std::uint32_t>(value));
}

GuestAddress ObjectFor(Mode mode)
{
    return mode == Mode::DirectAlias ?
        static_cast<GuestAddress>(Stack) - 88u : Object;
}

void Initialize(Window& window, Mode mode)
{
    for (const Region region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 8u, ResizeMethod);
    memory.WriteU32(Threshold, mode == Mode::ReuseSlot ? 1u : 5u);
    memory.WriteU32(Descriptors, Storage);
    memory.WriteU32(Descriptors + 4u, mode == Mode::AppendSlot ? 4u : 3u);
    memory.WriteU32(Descriptors + 8u, mode == Mode::AppendSlot ? 4u : 16u);
    memory.WriteU32(FreeSlots, FreeStorage);
    memory.WriteU32(FreeSlots + 4u, mode == Mode::ReuseSlot ? 1u : 0u);
    memory.WriteU32(FreeSlots + 8u, mode == Mode::ReuseSlot ? 1u : 0u);
    memory.WriteU32(FreeStorage, 7u);
    memory.WriteU32(Counter, 1u);
    const GuestAddress object = ObjectFor(mode);
    Write64(memory, object + 8u,
        mode == Mode::FreshCounter || mode == Mode::ReuseSlot ?
            1ull << 39u : 0x1020304050607080ull);
    memory.WriteU32(object + 40u, 0x88776655u);
    Write64(memory, object + 44u, 0x1122334455667788ull);
    if (mode == Mode::ReuseSlot)
    {
        Write64(memory, 0x21000u + 8u, 1ull << 39u);
        memory.WriteU32(0x21000u + 40u, 0x55667788u);
        Write64(memory, 0x21000u + 44u, 0x99aabbccddeeff00ull);
    }
    if (mode == Mode::Pop)
    {
        memory.WriteU32(Array, CopyStorage);
        memory.WriteU32(Array + 4u, 3u);
        memory.WriteU32(Array + 8u, 3u);
        memory.WriteU32(CopyStorage + 8u, 0xaabbccddu);
    }
    if (mode == Mode::AssignGrow)
    {
        memory.WriteU32(Source, SourceStorage);
        memory.WriteU32(Source + 4u, 3u);
        memory.WriteU16(SourceStorage, 'A');
        memory.WriteU16(SourceStorage + 2u, 'B');
        memory.WriteU16(SourceStorage + 4u, 'C');
    }
    if (mode == Mode::AssignAlias)
    {
        memory.WriteU32(Array + 8u, 2u); // source is Array+4
        memory.WriteU32(EmptySource, 0x00580059u);
    }
}

bool SameMemory(const Window& a, const Window& b,
    std::uint64_t& first)
{
    for (const Region region : Regions)
        for (std::size_t i = 0; i < region.size; ++i)
            if (a.bytes[std::size_t{region.start} + i] !=
                b.bytes[std::size_t{region.start} + i])
            {
                first = std::size_t{region.start} + i;
                return false;
            }
    return true;
}

bool Compare(Mode mode)
{
    Window original, recovered;
    Initialize(original, mode);
    Initialize(recovered, mode);
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xa1b2c3d400000000ull |
        (mode == Mode::Pop || mode == Mode::AssignSelf ||
         mode == Mode::AssignGrow || mode == Mode::AssignAlias ?
            Array : ObjectFor(mode));
    raw.r4.u64 = mode == Mode::AssignSelf ?
        0x8877665500000000ull | Array :
        mode == Mode::AssignGrow ? 0x8877665500000000ull | Source :
        mode == Mode::AssignAlias ? 0x8877665500000000ull | (Array + 4u) :
        mode == Mode::DirectAlias ? 0x8877665500000002ull :
        mode == Mode::FreshCounter || mode == Mode::ReuseSlot ||
        mode == Mode::AppendSlot ? 0x88776655ffffffffull : 0;
    raw.lr = 0x9988776655443322ull;
    raw.r29.u64 = 0x1234567800000029ull;
    raw.r30.u64 = 0x1234567800000030ull;
    raw.r31.u64 = 0x1234567800000031ull;
    const PPCContext initial = raw;
    Services expected(original.bytes, mode);
    active = &expected;
    switch (mode)
    {
    case Mode::Pop: __imp__sub_823B9268(raw, original.bytes); break;
    case Mode::AssignSelf:
    case Mode::AssignGrow:
    case Mode::AssignAlias:
        __imp__sub_822B3F50(raw, original.bytes); break;
    default: __imp__sub_82400BC0(raw, original.bytes); break;
    }
    Services actual(recovered.bytes, mode);
    family::FrameRegisters frame{initial.lr, initial.r29.u64,
        initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    const GuestAddress address = mode == Mode::Pop ? 0x823b9268u :
        mode == Mode::AssignSelf || mode == Mode::AssignGrow ||
        mode == Mode::AssignAlias ? 0x822b3f50u : 0x82400bc0u;
    if (!family::Apply(address, actual.memory, actual,
            initial.r3.u64, initial.r4.u64, initial.r1.u64,
            frame, result))
        throw std::runtime_error("descriptor-array entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        raw.r3.u64 == result && raw.r1.u64 == Stack &&
        raw.lr == frame.lr && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31;
    if (!same)
        std::fprintf(stderr,
            "FAIL descriptor mode=%u r3 %llx/%llx lr %llx/%llx "
            "r29 %llx/%llx events %zu/%zu first %llx:%02x/%02x\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r29.u64),
            static_cast<unsigned long long>(frame.r29),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

PPC_FUNC(sub_82298AF8)
{
    RemoveArrayRange(active->memory, *active, ctx.r3.u32,
        ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32,
        ctx.r1.u32 - 128u);
}

PPC_FUNC(sub_8229F678)
{
    ResizeArray(active->memory, *active, ctx.r3.u32,
        ctx.r4.u32, ctx.r5.u32);
}

PPC_FUNC(sub_82B7A0B0)
{
    ctx.r3.u64 = CopyGuestMemory(active->memory, ctx.r3.u64,
        ctx.r4.u32, ctx.r5.u64, ctx.r1.u32);
}

PPC_FUNC(sub_822C42D8)
{
    ctx.r3.u64 = registered_metadata_words::AddArrayElements(
        active->memory, *active, ctx.r3.u32, ctx.r4.u32,
        ctx.r5.u32, ctx.r6.u32);
}

PPC_FUNC(sub_823B9268)
{
    __imp__sub_823B9268(ctx, base);
}

int main()
{
    try
    {
        for (const Mode mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Pop);
        family::FrameRegisters frame{9u, 10u, 11u, 12u};
        std::uint64_t result = 7u;
        spare.bytes[Array] = 0x5au;
        if (family::Apply(0xffffffffu, service.memory, service,
                1, 2, Stack, frame, result) || result != 7u ||
            frame.lr != 9u || frame.r29 != 10u ||
            frame.r30 != 11u || frame.r31 != 12u ||
            spare.bytes[Array] != 0x5au || !service.events.empty())
            throw std::runtime_error("unknown descriptor entry changed state");
        std::printf("PASS metadata-descriptor-array %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT reused accepted array/copy algorithms; dynamic resize and generic lower ABI/frame/MMIO external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
