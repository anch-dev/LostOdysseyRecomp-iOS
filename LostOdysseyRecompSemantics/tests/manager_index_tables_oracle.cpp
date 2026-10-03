#include "lo_semantics/manager_index_tables.h"
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
namespace tables = lo::semantic::gpu::manager_index_tables;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x30000u;
constexpr GuestAddress Entries = 0x40000u;
constexpr GuestAddress OldTable = 0x50000u;
constexpr GuestAddress NewTable = 0x60000u;
constexpr GuestAddress Replacement = 0x70000u;
constexpr GuestAddress Stack = 0x80080u;
constexpr GuestAddress Manager = 0x90000u;
constexpr GuestAddress Vtable = 0x90100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocMethod = 0x82345678u;
constexpr GuestAddress ReleaseMethod = 0x82345688u;
constexpr GuestAddress ResizeMethod = 0x82345680u;

enum class Mode { Empty16, Rebuild28, SourceTableAlias, Append,
                  AppendGrowth, ResizeMutate, SpillAlias, TableFrameAlias,
                  FindHit, FindMiss, FindNoTable };
struct Case
{
    GuestAddress address;
    PPCFunc* original;
    Mode mode;
};
const Case Cases[] = {
    {0x82326b08u, __imp__sub_82326B08, Mode::Empty16},
    {0x823267a0u, __imp__sub_823267A0, Mode::Rebuild28},
    {0x82326b08u, __imp__sub_82326B08, Mode::SourceTableAlias},
    {0x82326890u, __imp__sub_82326890, Mode::Append},
    {0x82326890u, __imp__sub_82326890, Mode::AppendGrowth},
    {0x82326890u, __imp__sub_82326890, Mode::ResizeMutate},
    {0x82326890u, __imp__sub_82326890, Mode::SpillAlias},
    {0x823267a0u, __imp__sub_823267A0, Mode::TableFrameAlias},
    {0x823266a0u, __imp__sub_823266A0, Mode::FindHit},
    {0x823266a0u, __imp__sub_823266A0, Mode::FindMiss},
    {0x823266a0u, __imp__sub_823266A0, Mode::FindNoTable},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0xa0000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("reserve manager-index guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices, ArrayResizeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, {bytes, Space}), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager init"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager construct"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected manager fallback"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    void InitializeManager() override
    { throw std::runtime_error("unexpected array manager init"); }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t buffer) override
    {
        events.push_back({'D', {method, manager, buffer, 0}});
        if (method != ReleaseMethod || manager != Manager)
            throw std::runtime_error("wrong release callback");
        return 0x1234567800000001ull;
    }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        if (method != AllocMethod || manager != Manager || alignment != 8)
            throw std::runtime_error("wrong allocation callback");
        if (mode == Mode::Rebuild28)
            memory.WriteU32(Object + 4u, 1u);
        if (mode == Mode::SourceTableAlias)
            return 0x1234567800040004ull;
        if (mode == Mode::TableFrameAlias)
            return 0x1234567800080070ull;
        return 0x1234567800060000ull;
    }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'R', {method, manager, old_storage,
            (std::uint64_t{bytes} << 32) | argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8)
            throw std::runtime_error("wrong resize callback");
        if (mode == Mode::ResizeMutate)
            memory.WriteU32(Object + 16u, 2u);
        return Replacement;
    }
};

Services* active = nullptr;

void Initialize(Window& window, const Case& test)
{
    std::memset(window.bytes, 0xbd, 0xa0000);
    std::memset(window.bytes + 0x8330b000u, 0xbd, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 4u, AllocMethod | 3u);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Vtable + 12u, ReleaseMethod | 3u);
    const bool append = test.address == 0x82326890u;
    const GuestAddress entry_storage = test.mode == Mode::SpillAlias ?
        Stack + 28u : Entries;
    memory.WriteU32(Object, entry_storage);
    memory.WriteU32(Object + 4u, test.mode == Mode::AppendGrowth ? 10u :
        (test.mode == Mode::Rebuild28 ||
         test.mode == Mode::SourceTableAlias ? 2u :
         (test.mode == Mode::FindHit || test.mode == Mode::FindMiss ? 1u : 0u)));
    memory.WriteU32(Object + 8u,
        test.mode == Mode::ResizeMutate ? 0u : 16u);
    memory.WriteU32(Object + 12u,
        test.mode == Mode::FindNoTable ? 0u : OldTable);
    memory.WriteU32(Object + 16u,
        test.mode == Mode::AppendGrowth ||
        test.mode == Mode::TableFrameAlias ? 1u : 4u);
    for (GuestAddress offset = 0; offset < 80; offset += 4u)
        memory.WriteU32(OldTable + offset, 0xffffffffu);
    const GuestAddress stride = test.address == 0x82326b08u ? 16u : 28u;
    for (GuestAddress i = 0; i < 11u; ++i)
    {
        memory.WriteU32(Entries + i * stride + 4u, i + 1u);
        memory.WriteU32(Entries + i * stride + 8u, i * 7u);
    }
    if (test.mode == Mode::FindHit || test.mode == Mode::FindMiss)
    {
        memory.WriteU32(OldTable, 0u);
        memory.WriteU32(Entries, UINT32_MAX);
        memory.WriteU32(Entries + 4u,
            test.mode == Mode::FindHit ? 0x12345678u : 0x12345679u);
        memory.WriteU32(Entries + 8u, 5u);
    }
    if (!append && test.mode == Mode::Empty16)
        memory.WriteU32(Object + 16u, 0u);
}

bool SameMemory(const Window& a, const Window& b)
{
    return std::memcmp(a.bytes, b.bytes, 0xa0000) == 0 &&
        std::memcmp(a.bytes + 0x8330b000u,
                    b.bytes + 0x8330b000u, 0x1000) == 0;
}

bool Compare(const Case& test)
{
    Window original, recovered;
    Initialize(original, test);
    Initialize(recovered, test);
    PPCContext raw{};
    raw.r1.u64 = 0xfedcba9800000000ull | Stack;
    raw.r3.u64 = 0xabcdef1200000000ull | Object;
    raw.r4.u64 = 0x1234567800000005ull;
    raw.r5.u64 = 0x1111222233334444ull;
    raw.r6.u64 = 0x5555666677778888ull;
    raw.r28.u64 = 0x777788889999aaaaull;
    raw.r29.u64 = 0x9999aaaabbbbccccull;
    raw.r30.u64 = 0xddddeeeeffff0001ull;
    raw.r31.u64 = 0x2222333344445555ull;
    raw.lr = 0x8765432182345678ull;
    PPCContext restored = raw;
    Services expected(original.bytes, test.mode);
    active = &expected;
    test.original(raw, original.bytes);

    Services actual(recovered.bytes, test.mode);
    tables::FrameRegisters frame{restored.lr, restored.r28.u64,
        restored.r29.u64,
        restored.r30.u64, restored.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!tables::Apply(test.address, actual.memory, actual, actual,
            restored.r3.u64, restored.r4.u64, restored.r5.u64,
            restored.r6.u64, restored.r1.u64, frame, result))
        throw std::runtime_error("manager-index entry not mapped");
    const bool same = SameMemory(original, recovered) &&
        expected.events == actual.events && raw.r3.u64 == result &&
        raw.r1.u64 == restored.r1.u64 && raw.lr == frame.lr &&
        raw.r28.u64 == frame.r28 && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL manager-index %08x mode=%u r3 %016llx/%016llx events %zu/%zu\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size());
        for (std::size_t i = 0, shown = 0; i < 0xa0000 && shown < 8; ++i)
            if (original.bytes[i] != recovered.bytes[i])
            {
                std::fprintf(stderr, "  memory %08zx %02x/%02x\n", i,
                    original.bytes[i], recovered.bytes[i]);
                ++shown;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
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
    ctx.r3.u64 = registered_metadata_words::AddArrayElements(active->memory,
        *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
}
PPC_FUNC(sub_823267A0) { __imp__sub_823267A0(ctx, base); }
PPC_FUNC(sub_82326890) { __imp__sub_82326890(ctx, base); }

int main()
{
    try
    {
        for (const Case& test : Cases)
            if (!Compare(test)) return 1;
        std::array<std::uint8_t, 16> scratch{};
        GuestMemory memory(0, scratch);
        Window unknown_window;
        Services unknown(unknown_window.bytes, Mode::Append);
        tables::FrameRegisters frame{1, 2, 3, 4, 5};
        std::uint64_t result = 5;
        if (tables::Apply(0x82326b80u, memory, unknown, unknown,
                6, 7, 8, 9, 10, frame, result) ||
            result != 5 || frame.lr != 1 || frame.r31 != 5 ||
            !unknown.events.empty() ||
            scratch != std::array<std::uint8_t, 16>{})
            return 1;
        std::printf("PASS manager-index 11 original PPC cases + unknown\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "manager-index oracle: %s\n", error.what());
        return 2;
    }
}
