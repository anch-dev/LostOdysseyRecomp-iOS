#include "lo_semantics/metadata_name_registry.h"
#include "lo_semantics/registered_metadata_composed.h"
#include "lo_semantics/registered_metadata_string.h"

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
namespace registry = lo::semantic::gpu::metadata_name_registry;
namespace records = lo::semantic::gpu::metadata_name_record;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Manager = 0x12000u;
constexpr GuestAddress ManagerTable = 0x12100u;
constexpr GuestAddress ArrayStorage = 0x60000u;
constexpr GuestAddress AllocateMethod = 0x7200u;
constexpr GuestAddress ResizeMethod = 0x7300u;
struct Entry { GuestAddress name; std::uint32_t id; };
constexpr Entry Entries[] = {
#include "../src/metadata_name_registry_entries.inc"
};
struct Region { GuestAddress start; std::size_t size; };
constexpr Region Regions[] = {{0, 0x90000}, {0x82000000u, 0x190000},
    {0x83246000u, 0x1000}, {0x832ee000u, 0x6000}, {0x8330b000u, 0x1000},
    {0x83369000u, 0x1000}, {0x83371000u, 0x1000}};
enum class Mode { Cold, ExistingCrc, FinalRecordAliasesCallerLr };
constexpr Mode Cases[] = {Mode::Cold, Mode::ExistingCrc,
    Mode::FinalRecordAliasesCallerLr};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve registry RAM");
        for (const auto region : Regions)
            if (!VirtualAlloc(bytes + region.start, region.size, MEM_COMMIT,
                    PAGE_READWRITE)) throw std::runtime_error("commit registry RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 8> args;
    bool operator==(const Event&) const = default;
};

struct Services final : records::Services, ArrayResizeServices
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
    { throw std::runtime_error("unexpected manager init method"); }
    void InitializeManager() override
    { throw std::runtime_error("unexpected array manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes, std::uint32_t argument) override
    {
        events.push_back({'R', {method, manager, old, bytes, argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8u ||
                (old != 0u && old != ArrayStorage) || bytes > 0x10000u)
            throw std::runtime_error("unexpected registry resize");
        return ArrayStorage;
    }
    std::uint64_t AllocateRecord(GuestAddress method, GuestMemory& caller_memory,
        std::uint64_t manager, std::uint64_t bytes, std::uint64_t align,
        std::uint64_t sp, records::FrameRegisters& frame) override
    {
        events.push_back({'A', {method, manager, bytes, align, sp, frame.lr,
            frame.r29, frame.r30}});
        if (&caller_memory != &memory || method != AllocateMethod || manager != Manager ||
                bytes != 20u || align != 8u || sp != Stack - 96u - 128u ||
                frame.lr != 0x823f7b60u || allocated >= std::size(Entries) ||
                frame.r29 != Entries[allocated].id ||
                frame.r30 != (0xffffffff00000000ull | Entries[allocated].name))
            throw std::runtime_error("incorrect ordered registry allocation");
        const GuestAddress address = mode == Mode::FinalRecordAliasesCallerLr &&
            allocated == std::size(Entries) - 1 ? static_cast<GuestAddress>(Stack) - 8u :
            0x40000u + allocated * 64u;
        ++allocated;
        return 0x1122334400000000ull | address;
    }
};
Services* active = nullptr;

void Initialize(Window& window, Mode mode)
{
    for (const auto region : Regions)
        std::memset(window.bytes + region.start, 0xbd, region.size);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 4u, AllocateMethod | 3u);
    memory.WriteU32(ManagerTable + 8u, ResizeMethod | 3u);
    memory.WriteU32(0x83371a98u, mode == Mode::ExistingCrc ? 0x81u : 0x80u);
    for (unsigned i = 0; i < 256; ++i)
        memory.WriteU32(0x832ee168u + i * 4u, 0x87654321u ^ (i * 0x1234567u));
    for (unsigned i = 0; i < std::size(Entries); ++i)
    {
        memory.WriteU16(Entries[i].name, static_cast<std::uint16_t>('a' + i % 26u));
        memory.WriteU16(Entries[i].name + 2u, 0);
    }
}
bool SameMemory(const Window& a, const Window& b)
{
    for (const auto region : Regions)
        if (std::memcmp(a.bytes + region.start, b.bytes + region.start, region.size))
        {
            for (std::size_t i = 0; i < region.size; ++i)
                if (a.bytes[region.start + i] != b.bytes[region.start + i])
                {
                    std::fprintf(stderr, "registry memory %08zx %02x/%02x\n",
                        std::size_t{region.start} + i, a.bytes[region.start + i],
                        b.bytes[region.start + i]);
                    return false;
                }
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
    raw.lr = 0xabcdef1282345678ull;
    raw.r27.u64 = 0x1111222233334444ull;
    raw.r28.u64 = 0x5555666677778888ull;
    raw.r29.u64 = 0x9999aaaabbbbccccull;
    raw.r30.u64 = 0xddddeeeeffff0001ull;
    raw.r31.u64 = 0x0123456789abcdefull;
    const PPCContext initial = raw;
    Services expected(original.bytes, mode);
    active = &expected;
    __imp__sub_823F4700(raw, original.bytes);
    Services actual(recovered.bytes, mode);
    registry::FrameRegisters frame{initial.lr, initial.r27.u64, initial.r28.u64,
        initial.r29.u64, initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0;
    if (!registry::Apply(0x823f4700u, actual.memory, actual, actual, Stack, frame, result))
        throw std::runtime_error("registry entry missing");
    const bool same = SameMemory(original, recovered) && expected.events == actual.events &&
        expected.allocated == 446u && actual.allocated == 446u &&
        raw.r1.u64 == Stack && raw.r3.u64 == result && raw.lr == frame.lr &&
        raw.r27.u64 == frame.r27 && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31;
    if (!same)
        std::fprintf(stderr, "FAIL registry mode=%u r3 %llx/%llx lr %llx/%llx events %zu/%zu\n",
            static_cast<unsigned>(mode), static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result), static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr), expected.events.size(), actual.events.size());
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
PPC_FUNC(sub_823F7B08) { __imp__sub_823F7B08(ctx, base); }
PPC_FUNC(sub_823F44E8) { __imp__sub_823F44E8(ctx, base); }
PPC_FUNC(sub_82296F68) { __imp__sub_82296F68(ctx, base); }
PPC_FUNC(sub_82296FE8) { __imp__sub_82296FE8(ctx, base); }
PPC_FUNC(sub_82296830)
{ ctx.r3.u64 = registered_metadata_string::Utf16Length(active->memory, ctx.r3.u64); }
PPC_FUNC(sub_8230BAC0)
{
    std::uint64_t after = 0;
    ctx.r3.u64 = registered_metadata_composed::CopyUtf16UntilNull(
        active->memory, ctx.r3.u64, ctx.r4.u64, after);
    ctx.r4.u64 = after;
}
PPC_FUNC(sub_827C5F38) { throw std::runtime_error("unexpected registry manager init"); }
PPC_FUNC(sub_8229F678)
{
    const GuestAddress array = ctx.r3.u32;
    const bool called = active->memory.ReadU32(array) != 0u ||
        active->memory.ReadU32(array + 8u) != 0u;
    ResizeArray(active->memory, *active, array, ctx.r4.u32, ctx.r5.u32);
    if (called) ctx.r3.u64 = active->memory.ReadU32(array);
}
void RegistryAllocate(PPCContext& ctx, std::uint8_t*, GuestAddress method)
{
    records::FrameRegisters frame{ctx.lr, ctx.r27.u64, ctx.r28.u64,
        ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
    ctx.r3.u64 = active->AllocateRecord(method, active->memory, ctx.r3.u64,
        ctx.r4.u64, ctx.r5.u64, ctx.r1.u64, frame);
    ctx.lr = frame.lr;
    ctx.r27.u64 = frame.r27;
    ctx.r28.u64 = frame.r28;
    ctx.r29.u64 = frame.r29;
    ctx.r30.u64 = frame.r30;
    ctx.r31.u64 = frame.r31;
}
int main()
{
    try
    {
        for (const Mode mode : Cases) if (!Compare(mode)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Cold);
        registry::FrameRegisters frame{1, 2, 3, 4, 5, 6};
        std::uint64_t result = 7;
        if (registry::Apply(0xffffffffu, service.memory, service, service, Stack,
                frame, result) || result != 7u || frame.lr != 1u || frame.r27 != 2u ||
                frame.r28 != 3u || frame.r29 != 4u || frame.r30 != 5u || frame.r31 != 6u ||
                !service.events.empty()) throw std::runtime_error("unknown registry target");
        std::puts("PASS metadata-name-registry 3 full original PPC cases with all 446 records + unknown");
        std::puts("LIMIT synthetic UTF16 RAM; real registry/record/index/fold bodies; reused length/copy/resize contracts; selected frame/GPR, not generic lower volatile ABI or dynamic manager internals");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "registry oracle: %s\n", error.what()); return 2; }
}
