#include "lo_semantics/registered_metadata_range.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace range = lo::semantic::gpu::registered_metadata_range;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Owner = 0x20000u;
constexpr GuestAddress Array = Owner + 364u;
constexpr GuestAddress Data = 0x30000u;
constexpr GuestAddress ResizedData = 0x40000u;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Manager = 0x70000u;
constexpr GuestAddress Vtable = 0x70100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress ResizeMethod = 0x82345680u;

struct Entry { GuestAddress address; PPCFunc* original; };
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

enum class Mode { Ordinary, Growth, SaveAlias };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT,
                                    PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit metadata-range guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 5> arguments;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices
{
    GuestMemory memory;
    std::uint8_t* bytes;
    std::vector<Event> events;
    Services(std::uint8_t* guest)
        : memory(0, std::span<std::uint8_t>(guest, Space)), bytes(guest) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t size,
        std::uint32_t argument) override
    {
        events.push_back({{method, manager, old_storage, size, argument}});
        if (method != ResizeMethod || manager != Manager ||
                old_storage != Data || argument != 8u)
            throw std::runtime_error("unexpected resize service");
        std::memcpy(bytes + ResizedData, bytes + old_storage, 512);
        return ResizedData;
    }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, Mode mode)
{
    std::memset(bytes, 0xbd, 0x90000);
    std::memset(bytes + 0x8330b000u, 0, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(Object + 52u, Owner);
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Array, mode == Mode::SaveAlias ? Stack - 24u : Data);
    memory.WriteU32(Array + 4u, mode == Mode::SaveAlias ? 0u : 1u);
    memory.WriteU32(Array + 8u, mode == Mode::Growth ? 2u : 24u);
    memory.WriteU32(Data, 0x01010000u);
}

bool Compare(GuestAddress address, PPCFunc* original_body, Mode mode,
    Window& original, Window& recovered)
{
    Initialize(original.bytes, mode);
    Initialize(recovered.bytes, mode);
    Services expected(original.bytes), actual(recovered.bytes);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0xabcdef0082200000ull;
    context.r30.u64 = 0x1111222233334444ull;
    context.r31.u64 = 0x5555666677778888ull;
    context.r3.u64 = address == 0x8249b130u ?
        0x1234567800020000ull : 0xabcdef0000010000ull;
    context.r4.u64 = 0x99999999000000B8ull;
    context.r5.u64 = 0xaaaaaaaa00000028ull;
    active = &expected;
    original_body(context, original.bytes);

    range::Result result{};
    const range::EntryAbi abi{Stack, 0xabcdef0082200000ull,
        0x1111222233334444ull, 0x5555666677778888ull};
    active = &actual;
    if (!range::Apply(address, actual.memory, actual,
        address == 0x8249b130u ? 0x1234567800020000ull :
                                0xabcdef0000010000ull,
        0x99999999000000B8ull, 0xaaaaaaaa00000028ull, abi, result))
        throw std::runtime_error("metadata-range entry missing");
    const auto stack_same = [&](GuestAddress at, std::size_t size)
    { return std::memcmp(original.bytes + at, recovered.bytes + at, size) == 0; };
    // AppendMetadataWord, AddArrayElements, and ResizeArray retain their own
    // deeper adapter frames. This family owns its entry frame, helper frame,
    // and both outgoing words.
    const bool owned_stack = stack_same(Stack - 24u, 16) &&
        stack_same(Stack - 8u, 4) && stack_same(Stack - 112u, 4) &&
        stack_same(Stack - 32u, 4) &&
        (address == 0x8249b130u || stack_same(Stack - 144u, 4));
    const bool same = context.r3.u64 == result.r3 &&
        context.lr == result.lr && context.r30.u64 == result.r30 &&
        context.r31.u64 == result.r31 && context.r1.u64 == Stack &&
        expected.events == actual.events && owned_stack &&
        std::memcmp(original.bytes, recovered.bytes, 0x50000) == 0 &&
        std::memcmp(original.bytes + 0x60000u,
                    recovered.bytes + 0x60000u, 0x30000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
                    recovered.bytes + 0x8330b000u, 0x1000) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL metadata-range %08x mode %u r3 %llx/%llx lr %llx/%llx r30 %llx/%llx r31 %llx/%llx events %zu/%zu\n",
            address, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result.r3),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(result.lr),
            static_cast<unsigned long long>(context.r30.u64),
            static_cast<unsigned long long>(result.r30),
            static_cast<unsigned long long>(context.r31.u64),
            static_cast<unsigned long long>(result.r31),
            expected.events.size(), actual.events.size());
        for (std::size_t i = 0, shown = 0; i < 0x90000 && shown < 8; ++i)
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

PPC_FUNC(sub_8229F678)
{
    (void)base;
    ResizeArray(active->memory, *active, ctx.r3.u32, ctx.r4.u32,
                ctx.r5.u32);
}
PPC_FUNC(sub_822C42D8) { __imp__sub_822C42D8(ctx, base); }
PPC_FUNC(sub_825F41E8) { __imp__sub_825F41E8(ctx, base); }
PPC_FUNC(sub_8249B130) { __imp__sub_8249B130(ctx, base); }

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (const Entry& entry : Entries)
        {
            if (!Compare(entry.address, entry.original, Mode::Ordinary,
                         original, recovered)) return 1;
            ++cases;
        }
        if (!Compare(0x8249b130u, __imp__sub_8249B130, Mode::Growth,
                     original, recovered)) return 1;
        ++cases;
        if (!Compare(0x8249b130u, __imp__sub_8249B130, Mode::SaveAlias,
                     original, recovered)) return 1;
        ++cases;
        if (!Compare(0x825f45f0u, __imp__sub_825F45F0, Mode::SaveAlias,
                     original, recovered)) return 1;
        ++cases;
        Services untouched(recovered.bytes);
        range::Result result{0x1234, 0x2345, 0x3456, 0x4567};
        const range::EntryAbi abi{Stack, 0, 0, 0};
        if (range::Apply(0x8249b134u, untouched.memory, untouched,
                         Owner, 184, 40, abi, result) ||
                result.r3 != 0x1234 || result.lr != 0x2345 ||
                result.r30 != 0x3456 || result.r31 != 0x4567 ||
                !untouched.events.empty()) return 1;
        std::printf("PASS metadata-range %u PPC comparisons and 1 unknown\n",
                    cases);
        std::puts("LIMIT original five full PPC bodies; lower ResizeArray uses the previously recovered service model; generic volatile register ABI excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
