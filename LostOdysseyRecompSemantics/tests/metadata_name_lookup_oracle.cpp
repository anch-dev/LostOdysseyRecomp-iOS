#include "lo_semantics/metadata_name_lookup.h"
#include "lo_semantics/metadata_name_index.h"
#include "lo_semantics/metadata_name_registry.h"
#include "lo_semantics/manager_metadata_compare.h"
#include "lo_semantics/manager_metadata_parsing.h"
#include "lo_semantics/registered_metadata_composed.h"

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
namespace family = lo::semantic::gpu::metadata_name_lookup;
namespace record = lo::semantic::gpu::metadata_name_record;
namespace index = lo::semantic::gpu::metadata_name_index;
namespace registry = lo::semantic::gpu::metadata_name_registry;
namespace parsing = lo::semantic::gpu::manager_metadata_parsing;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Source = 0x10000u;
constexpr GuestAddress Output = 0x20000u;
constexpr GuestAddress Node = 0x30000u;
constexpr GuestAddress Manager = 0x12000u;
constexpr GuestAddress ManagerTable = 0x12100u;
constexpr GuestAddress ArrayStorage = 0x60000u;
constexpr GuestAddress ClassTable = 0x50000u;
constexpr GuestAddress Buckets = 0x832ee568u;
constexpr GuestAddress IdIndex = 0x833690d0u;
constexpr GuestAddress Ready = 0x83246260u;
constexpr GuestAddress AllocateMethod = 0x7200u;
constexpr GuestAddress ResizeMethod = 0x7300u;
struct Entry { GuestAddress name; std::uint32_t id; };
constexpr Entry RegistryEntries[] = {
#include "../src/metadata_name_registry_entries.inc"
};
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x82000000u, 0x190000},
    {0x83214000u, 0x2000}, {0x83246000u, 0x1000},
    {0x832ee000u, 0x6000}, {0x8330b000u, 0x1000},
    {0x83369000u, 0x1000}, {0x83371000u, 0x1000}};

enum class Mode { Empty, MissingDisabled, FoundCopy, CollisionFound,
    StackAliasCreate, NumericSuffix, ColdRegistry };
constexpr Mode Cases[] = {Mode::Empty, Mode::MissingDisabled,
    Mode::FoundCopy, Mode::CollisionFound, Mode::StackAliasCreate,
    Mode::NumericSuffix, Mode::ColdRegistry};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve lookup guest RAM");
        for (const Region region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size,
                    MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit lookup guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 8> args;
    bool operator==(const Event&) const = default;
};

struct Services final : record::Services, ArrayResizeServices,
    CrtThreadDataServices, InvalidParameterServices
{
    GuestMemory memory;
    Mode mode;
    unsigned allocated = 0;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected manager fallback"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    void InitializeManager() override
    { throw std::runtime_error("unexpected array manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'R', {method, manager, old, bytes, argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8u ||
            (old != 0u && old != ArrayStorage) || bytes > 0x10000u)
            throw std::runtime_error("incorrect lookup resize boundary");
        if (mode == Mode::StackAliasCreate)
            memory.WriteU16(Source, 'Z');
        return ArrayStorage;
    }
    std::uint64_t AllocateRecord(GuestAddress method, GuestMemory& caller_memory,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment, std::uint64_t sp,
        record::FrameRegisters& frame) override
    {
        events.push_back({'A', {method, manager, bytes, alignment, sp,
            frame.lr, frame.r29, frame.r30}});
        if (&caller_memory != &memory || method != AllocateMethod ||
            manager != Manager || bytes != 20u || alignment != 8u ||
            frame.lr != 0x823f7b60u)
            throw std::runtime_error("incorrect lookup record allocation");
        if (mode == Mode::ColdRegistry)
        {
            if (sp != Stack - 416u - 96u - 128u ||
                allocated >= std::size(RegistryEntries) ||
                frame.r29 != RegistryEntries[allocated].id ||
                frame.r30 != (0xffffffff00000000ull |
                    RegistryEntries[allocated].name))
                throw std::runtime_error("incorrect cold registry record");
            const GuestAddress result = 0x40000u + allocated * 64u;
            ++allocated;
            return 0x1122334400000000ull | result;
        }
        if (mode != Mode::StackAliasCreate || sp != Stack - 416u - 128u ||
            allocated++ != 0u)
            throw std::runtime_error("unexpected ready record allocation");
        return 0xabcdef000007ffe0ull; // aliases outer caller's saved r29
    }
    std::uint64_t GetTlsValue(std::uint32_t) override
    { throw std::runtime_error("unexpected CRT TLS read"); }
    void SetTlsValue(std::uint32_t, std::uint64_t) override
    { throw std::runtime_error("unexpected CRT TLS write"); }
    std::uint64_t CallThreadDataGetter(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected CRT getter"); }
    std::uint64_t AllocateThreadData(std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected CRT allocation"); }
    std::uint64_t BindThreadData(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected CRT bind"); }
    void FreeThreadData(std::uint64_t) override
    { throw std::runtime_error("unexpected CRT free"); }
    void CallHandler(GuestMemory&, GuestAddress,
        InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter handler"); }
    void Trap(const InvalidParameterCall&) override
    { throw std::runtime_error("unexpected invalid-parameter trap"); }
};
Services* active = nullptr;

void WriteText(GuestMemory& memory, GuestAddress where, const char* text)
{
    std::size_t index = 0;
    do
    {
        memory.WriteU16(where + static_cast<GuestAddress>(index * 2u),
            static_cast<std::uint8_t>(text[index]));
    } while (text[index++] != '\0');
}

void Initialize(Window& window, Mode mode)
{
    for (const Region region : Regions)
        std::memset(window.bytes + region.start, 0, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Ready, mode == Mode::ColdRegistry ? 0u : 1u);
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 4u, AllocateMethod | 3u);
    memory.WriteU32(ManagerTable + 8u, ResizeMethod | 3u);
    memory.WriteU32(IdIndex, ArrayStorage);
    memory.WriteU32(IdIndex + 4u, 2u);
    memory.WriteU32(IdIndex + 8u,
        mode == Mode::StackAliasCreate ? 2u : 16u);
    memory.WriteU32(0x83215b40u, ClassTable);
    for (const std::uint16_t unit : {' ', '\t', '\n', '\r'})
        memory.WriteU16(ClassTable + 2u * unit, 8u);
    for (std::uint32_t index = 0; index < 256u; ++index)
        memory.WriteU32(0x832ee168u + 4u * index,
            0x87654321u ^ (index * 0x1234567u));
    if (mode == Mode::ColdRegistry)
    {
        memory.WriteU32(0x83371a98u, 0x80u);
        for (unsigned i = 0; i < std::size(RegistryEntries); ++i)
        {
            memory.WriteU16(RegistryEntries[i].name,
                static_cast<std::uint16_t>('a' + i % 26u));
            memory.WriteU16(RegistryEntries[i].name + 2u, 0);
        }
    }
    WriteText(memory, Source, mode == Mode::Empty ||
        mode == Mode::ColdRegistry ? "" :
        mode == Mode::NumericSuffix ? "Map_3" : "A");
    memory.WriteU32(Output, 0xabcdef01u);
    memory.WriteU32(Output + 4u, 0xabcdef02u);
    if (mode == Mode::FoundCopy || mode == Mode::CollisionFound)
    {
        index::FrameRegisters frame{};
        const std::uint64_t hash = index::HashName(memory, Source,
            Stack - 0x1000u, frame);
        const GuestAddress bucket = Buckets +
            ((static_cast<std::uint32_t>(hash) << 2u) & 0x3ffcu);
        if (mode == Mode::FoundCopy)
            memory.WriteU32(bucket, Node);
        else
        {
            memory.WriteU32(bucket, Node + 0x100u);
            memory.WriteU32(Node + 0x100u + 12u, Node);
            WriteText(memory, Node + 0x100u + 16u, "Q");
        }
        memory.WriteU32(Node, 0x55aau);
        WriteText(memory, Node + 16u, "a");
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
    raw.r3.u64 = 0xabcdef0000020000ull;
    raw.r4.u64 = 0xabcdef0000010000ull;
    raw.r5.u64 = mode == Mode::NumericSuffix ? 0u :
        0xabcdef0000000007ull;
    raw.r6.u64 = mode == Mode::MissingDisabled ||
        mode == Mode::NumericSuffix ? 0u :
        mode == Mode::FoundCopy ? 2u : 1u;
    raw.r7.u64 = mode == Mode::NumericSuffix ? 1u : 0u;
    raw.lr = 0x1122334455667788ull;
    raw.r13.u64 = 0xabcdef0000013000ull;
    raw.r23.u64 = 0x1234567800000023ull;
    raw.r24.u64 = 0x1234567800000024ull;
    raw.r25.u64 = 0x1234567800000025ull;
    raw.r26.u64 = 0x1234567800000026ull;
    raw.r27.u64 = 0x1234567800000027ull;
    raw.r28.u64 = 0x1234567800000028ull;
    raw.r29.u64 = 0x1234567800000029ull;
    raw.r30.u64 = 0x1234567800000030ull;
    raw.r31.u64 = 0x1234567800000031ull;
    raw.r0.u64 = 0x1234567800000000ull;
    raw.ctr.u64 = 0x12345678000000ccull;
    raw.r8.u64 = 0x1234567800000008ull;
    raw.r9.u64 = 0x1234567800000009ull;
    raw.r10.u64 = 0x1234567800000010ull;
    const PPCContext initial = raw;
    Services expected(original.bytes, mode);
    active = &expected;
    __imp__sub_82296D30(raw, original.bytes);

    Services actual(recovered.bytes, mode);
    family::FrameRegisters frame{initial.lr, initial.r13.u64,
        initial.r23.u64, initial.r24.u64, initial.r25.u64,
        initial.r26.u64, initial.r27.u64, initial.r28.u64,
        initial.r29.u64, initial.r30.u64, initial.r31.u64,
        initial.r0.u64, initial.ctr.u64,
        initial.r8.u64, initial.r9.u64, initial.r10.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(0x82296d30u, actual.memory, actual, actual,
            actual, actual, initial.r3.u64, initial.r4.u64,
            initial.r5.u64, initial.r6.u64, initial.r7.u64,
            initial.r1.u64, frame, result))
        throw std::runtime_error("lookup entry missing");
    std::uint64_t first = 0;
    const bool same_memory = SameMemory(original, recovered, first);
    const bool same = same_memory && expected.events == actual.events &&
        expected.allocated == actual.allocated && raw.r3.u64 == result &&
        raw.r1.u64 == Stack && raw.lr == frame.lr &&
        raw.r13.u64 == frame.r13 && raw.r23.u64 == frame.r23 &&
        raw.r24.u64 == frame.r24 && raw.r25.u64 == frame.r25 &&
        raw.r26.u64 == frame.r26 && raw.r27.u64 == frame.r27 &&
        raw.r28.u64 == frame.r28 && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31;
    if (!same)
        std::fprintf(stderr,
            "FAIL lookup mode=%u r3 %llx/%llx lr %llx/%llx "
            "r30 %llx/%llx allocated %u/%u events %zu/%zu "
            "first %llx:%02x/%02x\n", static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r30.u64),
            static_cast<unsigned long long>(frame.r30),
            expected.allocated, actual.allocated,
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            original.bytes[first], recovered.bytes[first]);
    return same;
}
} // namespace

void SaveGprs(PPCContext& context, std::uint8_t* base)
{
    for (unsigned reg = 26u; reg <= 31u; ++reg)
        PPC_STORE_U64(context.r1.u32 - 8u * (33u - reg),
            (&context.r26)[reg - 26u].u64);
    PPC_STORE_U32(context.r1.u32 - 8u, context.r12.u32);
}
void RestoreGprs(PPCContext& context, std::uint8_t* base)
{
    for (unsigned reg = 26u; reg <= 31u; ++reg)
        (&context.r26)[reg - 26u].u64 =
            PPC_LOAD_U64(context.r1.u32 - 8u * (33u - reg));
    context.lr = PPC_LOAD_U32(context.r1.u32 - 8u);
}

void OriginalDirectCall(PPCContext& ctx, std::uint8_t*, GuestAddress target)
{
    switch (target)
    {
    case 0x823f4700u:
    {
        record::FrameRegisters frame{ctx.lr, ctx.r27.u64, ctx.r28.u64,
            ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
        std::uint64_t result = 0;
        (void)registry::Apply(target, active->memory, *active,
            *active, ctx.r1.u64, frame, result);
        ctx.r3.u64 = result;
        ctx.lr = frame.lr;
        ctx.r27.u64 = frame.r27;
        ctx.r28.u64 = frame.r28;
        ctx.r29.u64 = frame.r29;
        ctx.r30.u64 = frame.r30;
        ctx.r31.u64 = frame.r31;
        return;
    }
    case 0x82296e80u:
    {
        parsing::FrameRegisters frame{};
        frame.lr = ctx.lr;
        frame.r13 = ctx.r13.u64;
        frame.r8 = ctx.r8.u64;
        frame.r9 = ctx.r9.u64;
        for (unsigned i = 23; i <= 31; ++i)
            frame.r23_through_r31[i - 23] = (&ctx.r23)[i - 23].u64;
        std::uint64_t result = 0;
        (void)parsing::Apply(target, active->memory, *active,
            *active, ctx.r3.u64, ctx.r4.u64, ctx.r5.u64,
            ctx.r6.u64, ctx.r7.u64, ctx.r1.u64, frame, result);
        ctx.r3.u64 = result;
        ctx.lr = frame.lr;
        ctx.r13.u64 = frame.r13;
        ctx.r8.u64 = frame.r8;
        ctx.r9.u64 = frame.r9;
        for (unsigned i = 23; i <= 31; ++i)
            (&ctx.r23)[i - 23].u64 = frame.r23_through_r31[i - 23];
        return;
    }
    case 0x82296f68u:
    {
        index::FrameRegisters frame{ctx.lr, ctx.r27.u64, ctx.r28.u64,
            ctx.r29.u64, ctx.r30.u64, ctx.r31.u64,
            ctx.r0.u64, ctx.ctr.u64};
        ctx.r3.u64 = index::HashName(active->memory,
            ctx.r3.u64, ctx.r1.u64, frame);
        ctx.lr = frame.lr;
        ctx.r0.u64 = frame.r0;
        ctx.ctr.u64 = frame.ctr;
        ctx.r27.u64 = frame.r27;
        ctx.r28.u64 = frame.r28;
        ctx.r29.u64 = frame.r29;
        ctx.r30.u64 = frame.r30;
        ctx.r31.u64 = frame.r31;
        return;
    }
    case 0x822971e0u:
    {
        InvalidParameterCall call{};
        std::uint64_t* const registers[] = {&ctx.r3.u64, &ctx.r4.u64,
            &ctx.r5.u64, &ctx.r6.u64, &ctx.r7.u64, &ctx.r8.u64,
            &ctx.r9.u64, &ctx.r10.u64};
        for (unsigned i = 0; i < 8; ++i)
            call.arguments[i] = *registers[i];
        call.thread_environment = ctx.r13.u64;
        std::uint64_t result = 0;
        std::uint64_t lr = ctx.lr;
        (void)manager_metadata_compare::Apply(target, active->memory,
            *active, *active, call, ctx.r1.u64, lr, result);
        for (unsigned i = 0; i < 8; ++i)
            *registers[i] = call.arguments[i];
        ctx.r3.u64 = result;
        ctx.lr = lr;
        ctx.r13.u64 = call.thread_environment;
        return;
    }
    case 0x822c42d8u:
        ctx.r3.u64 = registered_metadata_words::AddArrayElements(
            active->memory, *active, ctx.r3.u32, ctx.r4.u32,
            ctx.r5.u32, ctx.r6.u32);
        return;
    case 0x823f7b08u:
    {
        record::FrameRegisters frame{ctx.lr, ctx.r27.u64, ctx.r28.u64,
            ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
        std::uint64_t result = 0;
        (void)record::Apply(target, active->memory, *active,
            ctx.r3.u64, ctx.r4.u64, ctx.r5.u64, ctx.r6.u64,
            ctx.r1.u64, frame, result);
        ctx.r3.u64 = result;
        ctx.lr = frame.lr;
        ctx.r27.u64 = frame.r27;
        ctx.r28.u64 = frame.r28;
        ctx.r29.u64 = frame.r29;
        ctx.r30.u64 = frame.r30;
        ctx.r31.u64 = frame.r31;
        return;
    }
    case 0x8230bac0u:
    {
        std::uint64_t after = 0;
        ctx.r3.u64 = registered_metadata_composed::CopyUtf16UntilNull(
            active->memory, ctx.r3.u64, ctx.r4.u64, after);
        ctx.r4.u64 = after;
        return;
    }
    default: throw std::runtime_error("unexpected original lookup callee");
    }
}

int main()
{
    try
    {
        for (const Mode mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Empty);
        family::FrameRegisters frame{};
        frame.lr = 9;
        std::uint64_t result = 7;
        if (family::Apply(0xffffffffu, service.memory, service, service,
                service, service, 1, 2, 3, 4, 5, Stack, frame, result) ||
            result != 7 || frame.lr != 9 || !service.events.empty())
            throw std::runtime_error("unknown lookup target changed state");
        std::printf("PASS metadata-name-lookup %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT reused validated registry/parser/hash/compare/record/array/copy lower models; dynamic allocator/resize/CRT services and generic lower volatile ABI external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
