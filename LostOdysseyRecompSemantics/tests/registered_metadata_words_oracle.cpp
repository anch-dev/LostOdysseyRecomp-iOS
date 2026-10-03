#include "lo_semantics/registered_metadata_words.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace metadata = lo::semantic::gpu::registered_metadata_words;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Array = 0x10000u;
constexpr GuestAddress Data = 0x20000u;
constexpr GuestAddress ResizedData = 0x30000u;
constexpr GuestAddress Value = 0x40000u;
constexpr GuestAddress Manager = 0x41000u;
constexpr GuestAddress Vtable = 0x41100u;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Object = 0x70000u;
constexpr GuestAddress Owner = 0x71000u;
constexpr GuestAddress ObjectArray = Object + 364u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress ResizeMethod = 0x82345680u;

struct Entry
{
    GuestAddress address;
    std::uint32_t frame_size;
    std::uint32_t word_count;
    std::int32_t patch_previous_index;
    std::uint32_t clear_object_offset;
    PPCFunc* original;
};
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

enum class Mode { AddNoGrow, AddGrow, AddSigned, AddWrap,
    AppendNoGrow, AppendMutate, AppendHeaderAlias, AppendStackAlias,
    Callback };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit metadata guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 7> arguments;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices
{
    GuestMemory memory;
    Mode mode;
    GuestAddress array;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected, GuestAddress selected_array)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected),
          array(selected_array) {}

    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }

    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({{method, manager, old_storage, bytes, argument,
            memory.ReadU32(array + 4u), memory.ReadU32(array + 8u)}});
        if (method != ResizeMethod || manager != Manager)
            throw std::runtime_error("unexpected resize target");
        if (mode == Mode::AppendMutate)
            memory.WriteU32(array + 4u, 0);
        if (old_storage != 0)
            std::memcpy(memory_bytes + ResizedData,
                memory_bytes + old_storage, 64);
        return ResizedData;
    }

    std::uint8_t* memory_bytes{};
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, Mode mode)
{
    std::memset(bytes, 0xbd, 0x90000);
    std::memset(bytes + 0x8330b000u, 0, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Owner + 52u, Object);
    memory.WriteU32(Object + 508u, 0x12345678u);
    memory.WriteU32(Value, 0x89abcdefu);
    memory.WriteU32(Stack - 32u, 0xdeadbeefu);

    GuestAddress array = mode == Mode::Callback ? ObjectArray : Array;
    std::uint32_t count = 1, capacity = 20;
    if (mode == Mode::AddNoGrow) { count = 2; capacity = 5; }
    if (mode == Mode::AddGrow) { count = 2; capacity = 2; }
    if (mode == Mode::AddSigned) { count = 0x7ffffffeu; capacity = 0x7fffffffu; }
    if (mode == Mode::AddWrap) { count = 0xffffffffu; capacity = 0; }
    if (mode == Mode::AppendNoGrow || mode == Mode::AppendHeaderAlias ||
        mode == Mode::AppendStackAlias) capacity = 3;
    if (mode == Mode::AppendMutate) capacity = 1;
    memory.WriteU32(array, Data);
    memory.WriteU32(array + 4u, count);
    memory.WriteU32(array + 8u, capacity);
    memory.WriteU32(Data, 0x00010000u);
}

bool EqualState(const Services& original, const Services& recovered)
{
    const std::uint8_t* first = original.memory_bytes;
    const std::uint8_t* second = recovered.memory_bytes;
    return original.events == recovered.events &&
        std::memcmp(first, second, 0x50000) == 0 &&
        std::memcmp(first + 0x70000u, second + 0x70000u, 0x2000) == 0 &&
        std::memcmp(first + 0x8330b000u, second + 0x8330b000u, 0x1000) == 0;
}

bool Compare(GuestAddress address, PPCFunc* original_function, Mode mode,
    const Entry* entry, unsigned ordinal, Window& original, Window& recovered)
{
    Initialize(original.bytes, mode);
    Initialize(recovered.bytes, mode);
    const GuestAddress array = mode == Mode::Callback ? ObjectArray : Array;
    Services expected(original.bytes, mode, array);
    Services actual(recovered.bytes, mode, array);
    expected.memory_bytes = original.bytes;
    actual.memory_bytes = recovered.bytes;
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000u;
    context.r30.u64 = 0x1111222233334444ull;
    context.r31.u64 = 0x5555666677778888ull;
    context.r3.u64 = mode == Mode::Callback ?
        0xabcdef0000071000ull : 0x1234567800010000ull;
    const std::uint32_t added_count = mode == Mode::AddSigned ? 2u : 1u;
    context.r4.u64 = mode == Mode::AddSigned ?
        0xdeadbeef00000002ull :
        mode == Mode::AddNoGrow || mode == Mode::AddGrow ||
        mode == Mode::AddWrap ? added_count :
        mode == Mode::AppendHeaderAlias ? Array + 4u :
        mode == Mode::AppendStackAlias ? Stack - 32u : Value;
    context.r5.u64 = 4u;
    context.r6.u64 = 8u;
    active = &expected;
    original_function(context, original.bytes);
    active = &actual;
    std::uint64_t result = 0xfedcba9876543210ull;
    if (address == 0x822c42d8u)
        result = metadata::AddArrayElements(actual.memory, actual, Array,
            added_count, 4, 8);
    else if (address == 0x825f41e8u)
    {
        const GuestAddress value = mode == Mode::AppendHeaderAlias ? Array + 4u :
            mode == Mode::AppendStackAlias ? Stack - 32u : Value;
        result = metadata::AppendMetadataWord(actual.memory, actual, Array, value);
    }
    else if (!entry || !metadata::Apply(address, actual.memory, actual,
                 0xabcdef0000071000ull, Stack, result))
        throw std::runtime_error("metadata callback mapping missing");
    const bool same = context.r3.u64 == result && context.r1.u64 == Stack &&
        context.lr == 0x82200000u &&
        context.r30.u64 == 0x1111222233334444ull &&
        context.r31.u64 == 0x5555666677778888ull &&
        EqualState(expected, actual) &&
        (!entry || expected.memory.ReadU32(Stack - entry->frame_size + 80u) ==
            actual.memory.ReadU32(Stack - entry->frame_size + 80u));
    if (!same)
        std::fprintf(stderr,
            "FAIL metadata %08x case %u mode %u r3 %llx/%llx events %zu/%zu count %08x/%08x\n",
            address, ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result), expected.events.size(),
            actual.events.size(), expected.memory.ReadU32(array + 4u),
            actual.memory.ReadU32(array + 4u));
    return same;
}
} // namespace

PPC_FUNC(sub_8229F678)
{
    (void)base;
    ResizeArray(active->memory, *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}
PPC_FUNC(sub_822C42D8) { __imp__sub_822C42D8(ctx, base); }
PPC_FUNC(sub_825F41E8) { __imp__sub_825F41E8(ctx, base); }

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (Mode mode : {Mode::AddNoGrow, Mode::AddGrow, Mode::AddSigned,
                          Mode::AddWrap})
            if (!Compare(0x822c42d8u, __imp__sub_822C42D8, mode, nullptr,
                         cases++, original, recovered)) return 1;
        for (Mode mode : {Mode::AppendNoGrow, Mode::AppendMutate,
                          Mode::AppendHeaderAlias, Mode::AppendStackAlias})
            if (!Compare(0x825f41e8u, __imp__sub_825F41E8, mode, nullptr,
                         cases++, original, recovered)) return 1;
        for (const Entry& entry : Entries)
            if (!Compare(entry.address, entry.original, Mode::Callback,
                         &entry, cases++, original, recovered)) return 1;
        std::printf("PASS registered-metadata %zu callbacks %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT original 18 cached PPC bodies; ResizeArray uses previously recovered lower semantics; generic ABI volatile state excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
