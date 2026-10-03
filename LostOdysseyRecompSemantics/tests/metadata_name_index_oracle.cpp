#include "lo_semantics/metadata_name_index.h"

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
namespace names = lo::semantic::gpu::metadata_name_index;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Name = 0x20000u;
constexpr GuestAddress Record = 0x30000u;
constexpr GuestAddress Storage = 0x40000u;
constexpr GuestAddress Replacement = 0x50000u;
constexpr GuestAddress Stack = 0x80080u;
constexpr GuestAddress Manager = 0x90000u;
constexpr GuestAddress Vtable = 0x90100u;
constexpr GuestAddress ResizeMethod = 0x82345680u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress FoldTable = 0x82297010u;
constexpr GuestAddress HashTable = 0x832ee168u;
constexpr GuestAddress Buckets = 0x832ee568u;
constexpr GuestAddress IndexArray = 0x833690d0u;

enum class Mode { FoldAscii, FoldFirst, FoldException, FoldLast,
    HashEmpty, HashMixed, HashStackAlias, InsertNoGrowth, InsertGrowth,
    InsertCallbackMutation, InsertFrameAlias };
struct Case
{
    GuestAddress address;
    PPCFunc* original;
    Mode mode;
};
const Case Cases[] = {
    {0x82296fe8u, __imp__sub_82296FE8, Mode::FoldAscii},
    {0x82296fe8u, __imp__sub_82296FE8, Mode::FoldFirst},
    {0x82296fe8u, __imp__sub_82296FE8, Mode::FoldException},
    {0x82296fe8u, __imp__sub_82296FE8, Mode::FoldLast},
    {0x82296f68u, __imp__sub_82296F68, Mode::HashEmpty},
    {0x82296f68u, __imp__sub_82296F68, Mode::HashMixed},
    {0x82296f68u, __imp__sub_82296F68, Mode::HashStackAlias},
    {0x823f44e8u, __imp__sub_823F44E8, Mode::InsertNoGrowth},
    {0x823f44e8u, __imp__sub_823F44E8, Mode::InsertGrowth},
    {0x823f44e8u, __imp__sub_823F44E8, Mode::InsertCallbackMutation},
    {0x823f44e8u, __imp__sub_823F44E8, Mode::InsertFrameAlias},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x100000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x82297000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x832ee000u, 0x5000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83369000u, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve name-index guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    GuestAddress method, manager, storage, bytes, argument;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, {bytes, Space}), mode(selected) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({method, manager, storage, bytes, argument});
        if (method != ResizeMethod || manager != Manager || argument != 8u)
            throw std::runtime_error("wrong name-index resize callback");
        if (mode == Mode::InsertCallbackMutation)
        {
            memory.WriteU32(IndexArray + 4u, 2u);
            memory.WriteU32(Record, 1u);
        }
        return Replacement;
    }
};

Services* active = nullptr;

void Initialize(Window& window, Mode mode)
{
    std::memset(window.bytes, 0, 0x100000);
    std::memset(window.bytes + 0x82297000u, 0, 0x1000);
    std::memset(window.bytes + 0x832ee000u, 0, 0x5000);
    std::memset(window.bytes + 0x8330b000u, 0, 0x1000);
    std::memset(window.bytes + 0x83369000u, 0, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    for (std::uint32_t index = 0; index < 100u; ++index)
    {
        GuestAddress target = 0x822971b0u;
        if (index == 0u) target = 0x822971a8u;
        if (index == 99u) target = 0x822971a0u;
        if (index == 52u || index == 67u || index == 84u || index == 91u)
            target = 0x822971dcu;
        memory.WriteU32(FoldTable + index * 4u, target);
    }
    for (std::uint32_t index = 0; index < 256u; ++index)
    {
        std::uint32_t value = index << 24u;
        for (unsigned bit = 0; bit < 8u; ++bit)
            value = (value << 1u) ^
                ((value & 0x80000000u) != 0u ? 0x04c11db7u : 0u);
        memory.WriteU32(HashTable + index * 4u, value);
    }
    for (std::uint32_t index = 0; index < 4096u; ++index)
        memory.WriteU32(Buckets + index * 4u, 0x11110000u | index);
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU16(Name, 0x0061u);
    memory.WriteU16(Name + 2u, 0x00f0u);
    memory.WriteU16(Name + 4u, 0u);
    if (mode == Mode::HashEmpty)
        memory.WriteU16(Name, 0u);
    memory.WriteU32(Record, mode == Mode::InsertCallbackMutation ? 2u : 1u);
    memory.WriteU16(Record + 16u, 0x0061u);
    memory.WriteU16(Record + 18u, 0u);
    const bool growth = mode == Mode::InsertGrowth ||
        mode == Mode::InsertCallbackMutation;
    memory.WriteU32(IndexArray, growth ? 0u : Storage);
    memory.WriteU32(IndexArray + 4u, growth ? 0u : 3u);
    memory.WriteU32(IndexArray + 8u, growth ? 0u : 4u);
    for (std::uint32_t index = 0; index < 4u; ++index)
        memory.WriteU32(Storage + index * 4u, 0xfeed0000u | index);
}

bool SameMemory(const Window& a, const Window& b)
{
    return std::memcmp(a.bytes, b.bytes, 0x100000) == 0 &&
        std::memcmp(a.bytes + 0x82297000u,
            b.bytes + 0x82297000u, 0x1000) == 0 &&
        std::memcmp(a.bytes + 0x832ee000u,
            b.bytes + 0x832ee000u, 0x5000) == 0 &&
        std::memcmp(a.bytes + 0x8330b000u,
            b.bytes + 0x8330b000u, 0x1000) == 0 &&
        std::memcmp(a.bytes + 0x83369000u,
            b.bytes + 0x83369000u, 0x1000) == 0;
}

bool Compare(const Case& test)
{
    Window original, recovered;
    Initialize(original, test.mode);
    Initialize(recovered, test.mode);
    PPCContext raw{};
    raw.r1.u64 = 0xfedcba9800000000ull | Stack;
    raw.r3.u64 = 0x1234567800000000ull | Record;
    if (test.mode == Mode::FoldAscii)
        raw.r3.u64 = 0x1234567800000061ull;
    else if (test.mode == Mode::FoldFirst)
        raw.r3.u64 = 0x123456780000009cull;
    else if (test.mode == Mode::FoldException)
        raw.r3.u64 = 0x12345678000000f0ull;
    else if (test.mode == Mode::FoldLast)
        raw.r3.u64 = 0x12345678000000ffull;
    else if (test.mode == Mode::HashEmpty || test.mode == Mode::HashMixed)
        raw.r3.u64 = 0x1234567800000000ull | Name;
    else if (test.mode == Mode::HashStackAlias)
        raw.r3.u64 = 0x1234567800000000ull | (Stack - 32u);
    else if (test.mode == Mode::InsertFrameAlias)
        raw.r3.u64 = 0x1234567800000000ull | (Stack - 48u);
    raw.lr = 0xabcdef1282345678ull;
    raw.r27.u64 = test.mode == Mode::InsertFrameAlias ?
        0x0000000100000000ull : 0x1111222233334444ull;
    raw.r28.u64 = 0x5555666677778888ull;
    raw.r29.u64 = test.mode == Mode::HashStackAlias ||
        test.mode == Mode::InsertFrameAlias ?
        0x0061006200000000ull : 0x9999aaaabbbbccccull;
    raw.r30.u64 = 0xddddeeeeffff0001ull;
    raw.r31.u64 = 0x0123456789abcdefull;
    raw.r0.u64 = 0xaaaabbbbccccddddull;
    raw.ctr.u64 = 0x1111222233334444ull;
    PPCContext restored = raw;
    Services expected(original.bytes, test.mode);
    active = &expected;
    test.original(raw, original.bytes);
    Services actual(recovered.bytes, test.mode);
    names::FrameRegisters frame{restored.lr, restored.r27.u64,
        restored.r28.u64, restored.r29.u64, restored.r30.u64,
        restored.r31.u64, restored.r0.u64, restored.ctr.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!names::Apply(test.address, actual.memory, actual,
            restored.r3.u64, restored.r1.u64, frame, result))
        throw std::runtime_error("name-index entry not mapped");
    const bool same = SameMemory(original, recovered) &&
        expected.events == actual.events && raw.r3.u64 == result &&
        raw.r1.u64 == restored.r1.u64 && raw.lr == frame.lr &&
        raw.r27.u64 == frame.r27 && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31 &&
        (test.mode == Mode::InsertGrowth ||
         test.mode == Mode::InsertCallbackMutation ||
         (raw.r0.u64 == frame.r0 && raw.ctr.u64 == frame.ctr));
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL metadata-name-index %08x mode=%u r3 %016llx/%016llx lr %016llx/%016llx events %zu/%zu\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size());
        for (std::size_t i = 0, shown = 0; i < 0x100000 && shown < 8; ++i)
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

PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
PPC_FUNC(sub_82296FE8) { __imp__sub_82296FE8(ctx, base); }
PPC_FUNC(sub_82296F68) { __imp__sub_82296F68(ctx, base); }
PPC_FUNC(sub_8229F678)
{
    const GuestAddress array = ctx.r3.u32;
    const GuestAddress old_storage = active->memory.ReadU32(array);
    const std::uint32_t capacity = active->memory.ReadU32(array + 8u);
    ResizeArray(active->memory, *active, array, ctx.r4.u32, ctx.r5.u32);
    if (old_storage != 0u || capacity != 0u)
        ctx.r3.u64 = active->memory.ReadU32(array);
}

int main()
{
    try
    {
        for (const Case& test : Cases)
            if (!Compare(test)) return 1;
        std::array<std::uint8_t, 16> scratch{};
        GuestMemory memory(0, scratch);
        Window window;
        Services services(window.bytes, Mode::FoldAscii);
        names::FrameRegisters frame{1, 2, 3, 4, 5, 6, 7, 8};
        std::uint64_t result = 9;
        if (names::Apply(0x823f44ecu, memory, services, 10, 11,
                frame, result) || result != 9 || frame.lr != 1 ||
            frame.r27 != 2 || frame.ctr != 8 ||
            scratch != std::array<std::uint8_t, 16>{} ||
            !services.events.empty())
            return 1;
        std::printf("PASS metadata-name-index 11 original PPC cases + unknown\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "metadata-name-index oracle: %s\n", error.what());
        return 2;
    }
}
