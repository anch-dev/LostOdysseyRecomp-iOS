#include "lo_semantics/metadata_descriptor_index.h"
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
namespace family = lo::semantic::gpu::metadata_descriptor_index;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Table = 0x10000u;
constexpr GuestAddress Destination = 0x12000u;
constexpr GuestAddress RedirectedDestination = 0x13000u;
constexpr GuestAddress RedirectedTable = 0x14000u;
constexpr GuestAddress Key = 0x20000u;
constexpr GuestAddress Key2 = 0x21000u;
constexpr GuestAddress Key3 = 0x22000u;
constexpr GuestAddress Owner = 0x23000u;
constexpr GuestAddress RedirectedOwner = 0x24000u;
constexpr GuestAddress Entries = 0x30000u;
constexpr GuestAddress GrownEntries = 0x31000u;
constexpr GuestAddress RedirectedEntries = 0x32000u;
constexpr GuestAddress Buckets = 0x50000u;
constexpr GuestAddress NewBuckets = 0x60000u;
constexpr GuestAddress Manager = 0x70000u;
constexpr GuestAddress Vtable = 0x71000u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress FeatureGlobal = 0x8330b62cu;
constexpr GuestAddress ReleaseMethod = 0x7055u;
constexpr GuestAddress AllocateMethod = 0x7045u;
constexpr GuestAddress ResizeMethod = 0x7065u;
enum class Mode { Rebuild, RebuildAlias, AppendGrow, InsertHit,
    InsertMiss, DefaultKey, ExplicitKey };
constexpr Mode Cases[] = {Mode::Rebuild, Mode::RebuildAlias,
    Mode::AppendGrow, Mode::InsertHit, Mode::InsertMiss,
    Mode::DefaultKey, Mode::ExplicitKey};
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x8330b000u, 0x1000}};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve index guest RAM");
        for (const Region region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit index guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 6> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw manager allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected initializer method"); }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t buffer) override
    {
        events.push_back({'R', {method, manager, buffer}});
        if (method != (ReleaseMethod & ~3u) || manager != Manager ||
            buffer != (mode == Mode::ExplicitKey ? 0u : Buckets))
            throw std::runtime_error("incorrect bucket release boundary");
        return 0xfedcba9876543210ull;
    }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        const std::uint64_t expected = mode == Mode::RebuildAlias ?
            ~std::uint64_t{0} : mode == Mode::Rebuild ? 16u : 8u;
        if (method != (AllocateMethod & ~3u) || manager != Manager ||
            bytes != expected || alignment != 8u ||
            (mode != Mode::Rebuild && mode != Mode::RebuildAlias &&
             mode != Mode::AppendGrow && mode != Mode::ExplicitKey))
            throw std::runtime_error("incorrect bucket allocation boundary");
        if (mode == Mode::RebuildAlias)
        {
            memory.WriteU32(Table + 16u, 1u); // live loop bound
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 16u,
                0x22334455u);
            memory.WriteU32(static_cast<GuestAddress>(Stack) - 12u,
                0x66778899u);
        }
        if (mode == Mode::AppendGrow)
            memory.WriteU32(Table + 4u, 2u); // live rehash item count
        if (mode == Mode::ExplicitKey)
        {
            // Redirect Rebuild's restored r31, then Insert's restored
            // r30/r31. Both callers must use the resulting live registers.
            const GuestAddress stack = static_cast<GuestAddress>(Stack);
            memory.WriteU32(stack - 240u, 0xabcdef00u);
            memory.WriteU32(stack - 236u, RedirectedTable);
            memory.WriteU32(stack - 136u, 0x12345678u);
            memory.WriteU32(stack - 132u, RedirectedDestination);
            memory.WriteU32(stack - 128u, 0x88776655u);
            memory.WriteU32(stack - 124u, RedirectedOwner);
        }
        return 0xabcdef0000060000ull;
    }
    GuestAddress ResizeStorage(GuestAddress method,
        GuestAddress manager, GuestAddress old,
        std::uint32_t bytes, std::uint32_t argument) override
    {
        events.push_back({'Z', {method, manager, old, bytes, argument}});
        if (mode != Mode::AppendGrow || method != (ResizeMethod & ~3u) ||
            manager != Manager || old != Entries || bytes != 564u ||
            argument != 8u)
            throw std::runtime_error("incorrect entry-array resize boundary");
        for (std::uint32_t i = 0; i < 120u; ++i)
            memory.WriteU8(GrownEntries + i,
                memory.ReadU8(Entries + i));
        return GrownEntries;
    }
};
Services* active = nullptr;

void Write64(GuestMemory& memory, GuestAddress where, std::uint64_t value)
{
    memory.WriteU32(where, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(where + 4u, static_cast<std::uint32_t>(value));
}

GuestAddress TableFor(Mode mode)
{
    return mode == Mode::DefaultKey || mode == Mode::ExplicitKey ?
        Destination + 32u : Table;
}

void Initialize(Window& window, Mode mode)
{
    for (const Region region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(FeatureGlobal, 4u);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 4u, AllocateMethod);
    memory.WriteU32(Vtable + 8u, ResizeMethod);
    memory.WriteU32(Vtable + 12u, ReleaseMethod);
    memory.WriteU32(Key + 4u, 1u);
    memory.WriteU32(Key2 + 4u, 2u);
    memory.WriteU32(Key3 + 4u, 0u);
    const GuestAddress table = TableFor(mode);
    memory.WriteU32(table, Entries);
    memory.WriteU32(table + 8u,
        mode == Mode::AppendGrow ? 10u : 2u);
    memory.WriteU32(table + 12u, Buckets);
    if (mode == Mode::ExplicitKey)
        memory.WriteU32(table + 12u, 0u);
    memory.WriteU32(table + 16u,
        mode == Mode::RebuildAlias ? 0x40000000u :
        mode == Mode::AppendGrow ? 1u :
        mode == Mode::Rebuild ? 4u : 2u);
    memory.WriteU32(Buckets, 0xffffffffu);
    memory.WriteU32(Buckets + 4u, 0xffffffffu);
    if (mode == Mode::Rebuild)
    {
        memory.WriteU32(table + 4u, 2u);
        memory.WriteU32(Entries + 4u, Key);
        memory.WriteU32(Entries + 12u + 4u, Key2);
    }
    if (mode == Mode::AppendGrow)
    {
        memory.WriteU32(table + 4u, 10u);
        for (std::uint32_t i = 0; i < 10u; ++i)
            memory.WriteU32(Entries + i * 12u + 4u,
                i == 0 ? Key : i == 1 ? Key2 : 0u);
    }
    if (mode == Mode::InsertHit || mode == Mode::InsertMiss)
    {
        memory.WriteU32(table + 4u, 1u);
        memory.WriteU32(Buckets + 4u, 0u);
        memory.WriteU32(Entries, 0xffffffffu);
        memory.WriteU32(Entries + 4u,
            mode == Mode::InsertHit ? Key : Key2);
    }
    if (mode == Mode::DefaultKey || mode == Mode::ExplicitKey)
    {
        memory.WriteU32(Owner + 56u, Key);
        Write64(memory, Owner + 8u,
            mode == Mode::DefaultKey ? 0x400u : 0u);
    }
    if (mode == Mode::ExplicitKey)
    {
        memory.WriteU32(RedirectedTable, RedirectedEntries);
        memory.WriteU32(RedirectedTable + 8u, 2u);
        memory.WriteU32(RedirectedTable + 12u, Buckets);
        memory.WriteU32(RedirectedTable + 16u, 2u);
        Write64(memory, RedirectedOwner + 8u, 0x400u);
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
    raw.r3.u64 = 0xabcddcba00000000ull |
        (mode == Mode::DefaultKey || mode == Mode::ExplicitKey ?
            Destination : Table);
    raw.r4.u64 = 0x9988776600000000ull |
        (mode == Mode::DefaultKey || mode == Mode::ExplicitKey ?
            Owner : mode == Mode::AppendGrow ? Key3 : Key);
    raw.r5.u64 = mode == Mode::DefaultKey ?
        0x1234567800000000ull :
        mode == Mode::ExplicitKey ? 0x1234567800020000ull :
        0x123456780000a5a5ull;
    raw.lr = 0x8877665544332211ull;
    raw.r28.u64 = 0x1234567800000028ull;
    raw.r29.u64 = 0x1234567800000029ull;
    raw.r30.u64 = 0x1234567800000030ull;
    raw.r31.u64 = 0x1234567800000031ull;
    const PPCContext initial = raw;
    Services expected(original.bytes, mode);
    active = &expected;
    switch (mode)
    {
    case Mode::Rebuild:
    case Mode::RebuildAlias:
        __imp__sub_82523C48(raw, original.bytes); break;
    case Mode::AppendGrow:
        __imp__sub_8256B910(raw, original.bytes); break;
    case Mode::InsertHit:
    case Mode::InsertMiss:
        __imp__sub_826BD860(raw, original.bytes); break;
    default:
        __imp__sub_82408D28(raw, original.bytes); break;
    }
    Services actual(recovered.bytes, mode);
    family::FrameRegisters frame{initial.lr, initial.r28.u64,
        initial.r29.u64, initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    const GuestAddress address = mode == Mode::Rebuild ||
        mode == Mode::RebuildAlias ? 0x82523c48u :
        mode == Mode::AppendGrow ? 0x8256b910u :
        mode == Mode::InsertHit || mode == Mode::InsertMiss ?
            0x826bd860u : 0x82408d28u;
    if (!family::Apply(address, actual.memory, actual,
            initial.r3.u64, initial.r4.u64, initial.r5.u64,
            initial.r1.u64, frame, result))
        throw std::runtime_error("descriptor-index entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        raw.r3.u64 == result && raw.r1.u64 == Stack &&
        raw.lr == frame.lr && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31;
    if (!same)
        std::fprintf(stderr,
            "FAIL index mode=%u r3 %llx/%llx lr %llx/%llx "
            "r31 %llx/%llx events %zu/%zu first %llx:%02x/%02x\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(frame.r31),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

class OriginalArrayServices final : public ArrayResizeServices
{
public:
    OriginalArrayServices(GuestMemory& memory, ManagerFacadeServices& services,
        GuestAddress init_frame)
        : memory_(memory), services_(services), init_frame_(init_frame) {}
    void InitializeManager() override
    { (void)InitializeManagerImpl(); }
    GuestAddress ResizeStorage(GuestAddress target, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes,
        std::uint32_t argument) override
    { return services_.ResizeStorage(target, manager, old, bytes, argument); }
private:
    std::uint64_t InitializeManagerImpl()
    { return lo::semantic::gpu::InitializeManager(
        memory_, services_, init_frame_); }
    GuestMemory& memory_;
    ManagerFacadeServices& services_;
    GuestAddress init_frame_;
};

PPC_FUNC(sub_823F3340)
{
    ctx.r3.u64 = ReleaseManagerBuffer(active->memory, *active,
        ctx.r3.u64, ctx.r1.u32);
}

PPC_FUNC(sub_82486C88)
{
    ctx.r3.u64 = AllocateManagerBuffer(active->memory, *active,
        ctx.r3.u64, ctx.r1.u32);
}

PPC_FUNC(sub_822C42D8)
{
    OriginalArrayServices adapter(active->memory, *active,
        ctx.r1.u32 - 96u - 128u - 112u);
    ctx.r3.u64 = registered_metadata_words::AddArrayElements(
        active->memory, adapter, ctx.r3.u32, ctx.r4.u32,
        ctx.r5.u32, ctx.r6.u32);
}

PPC_FUNC(sub_82523C48) { __imp__sub_82523C48(ctx, base); }
PPC_FUNC(sub_8256B910) { __imp__sub_8256B910(ctx, base); }
PPC_FUNC(sub_826BD860) { __imp__sub_826BD860(ctx, base); }

int main()
{
    try
    {
        for (const Mode mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Rebuild);
        family::FrameRegisters frame{9u, 10u, 11u, 12u, 13u};
        std::uint64_t result = 7u;
        spare.bytes[Table] = 0x5au;
        if (family::Apply(0xffffffffu, service.memory, service,
                1, 2, 3, Stack, frame, result) || result != 7u ||
            frame.lr != 9u || frame.r28 != 10u || frame.r29 != 11u ||
            frame.r30 != 12u || frame.r31 != 13u ||
            spare.bytes[Table] != 0x5au || !service.events.empty())
            throw std::runtime_error("unknown descriptor-index changed state");
        std::printf("PASS metadata-descriptor-index %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT reused accepted manager/array lower models; dynamic manager methods and generic lower ABI/frame/MMIO external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
