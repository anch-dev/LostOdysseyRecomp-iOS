#include "lo_semantics/metadata_descriptor_cursor.h"

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
namespace family = lo::semantic::gpu::metadata_descriptor_cursor;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Header = 0x20000u;
constexpr GuestAddress LiveHeader = 0x21000u;
constexpr GuestAddress SavedHeader = 0x22000u;
constexpr GuestAddress Node1 = 0x30000u;
constexpr GuestAddress Node2 = 0x31000u;
constexpr GuestAddress Node3 = 0x32000u;
constexpr GuestAddress Link1 = 0x40000u;
constexpr GuestAddress Link2 = 0x41000u;
constexpr GuestAddress Descriptor1 = 0x50000u;
constexpr GuestAddress Descriptor2 = 0x51000u;
constexpr GuestAddress Vtable = 0x60000u;
constexpr GuestAddress Method = 0x7303u;
enum class Mode { Empty, Flag, Chain, MultiDynamic, ParentNull, ParentAlias };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
    {0x822a6ef8u, __imp__sub_822A6EF8, Mode::Empty},
    {0x822a6ef8u, __imp__sub_822A6EF8, Mode::Flag},
    {0x822a6ef8u, __imp__sub_822A6EF8, Mode::Chain},
    {0x822a6ef8u, __imp__sub_822A6EF8, Mode::MultiDynamic},
    {0x82406568u, __imp__sub_82406568, Mode::ParentNull},
    {0x82406568u, __imp__sub_82406568, Mode::ParentAlias},
};
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000u, MEM_COMMIT,
                PAGE_READWRITE))
            throw std::runtime_error("reserve cursor guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    std::array<std::uint64_t, 8> values;
    bool operator==(const Event&) const = default;
};
struct Services final : family::VirtualServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    std::uint64_t CallNext(GuestAddress target, GuestMemory& caller_memory,
        std::uint64_t node_r3, std::uint64_t r4,
        std::uint64_t caller_sp, family::FrameRegisters& frame) override
    {
        events.push_back({{target, node_r3, r4, caller_sp,
            frame.lr, frame.r31, frame.ctr, events.size()}});
        if (&caller_memory != &memory || target != (Method & ~3u) ||
            frame.ctr != Method || frame.lr != 0x822a6f64u ||
            frame.r31 != 0xabcdef0000020000ull ||
            r4 != (mode == Mode::ParentAlias ?
                0x1234567800030000ull : 0x1234567800000044ull) ||
            caller_sp != Stack - (mode == Mode::ParentAlias ? 192u : 96u) ||
            node_r3 != (mode == Mode::MultiDynamic ?
                (events.size() == 1 ? Node1 :
                 events.size() == 2 ? Node2 : Node3) : Node1))
            throw std::runtime_error("incorrect cursor virtual boundary");
        if (mode == Mode::ParentAlias)
        {
            frame.r31 = 0xabcdef0000021000ull;
            const GuestAddress saved = static_cast<GuestAddress>(Stack) - 112u;
            memory.WriteU32(saved, 0x88776655u);
            memory.WriteU32(saved + 4u, SavedHeader);
        }
        if (mode == Mode::MultiDynamic)
            return events.size() == 1 ?
                0xabcdef0000031000ull : events.size() == 2 ?
                0xabcdef0000032000ull : 0x9999888800000000ull;
        return 0x9999888800000000ull;
    }
};
Services* active = nullptr;

void Seed(Window& window, Mode mode)
{
    std::memset(window.bytes, 0, 0x90000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Header, mode == Mode::Empty ? 0u : Node1);
    memory.WriteU32(Header + 4u,
        mode == Mode::Flag || mode == Mode::Chain ? Link1 : 0u);
    memory.WriteU32(LiveHeader, 0xeeeeeeeeu);
    memory.WriteU32(Node1, Vtable);
    memory.WriteU32(Node2, Vtable);
    memory.WriteU32(Node3, Vtable);
    memory.WriteU32(Vtable + 284u, Method);
    memory.WriteU32(Node1 + 76u, 0u);
    memory.WriteU32(Node2 + 76u, 0u);
    memory.WriteU32(Node3 + 76u, 0u);
    memory.WriteU32(Link1 + 52u, Descriptor1);
    memory.WriteU32(Link1 + 64u, Link2);
    memory.WriteU32(Link2 + 52u, Descriptor2);
    memory.WriteU32(Link2 + 64u, 0u);
    memory.WriteU32(Descriptor1 + 184u,
        mode == Mode::Flag ? 0x8000u : 0u);
    memory.WriteU32(Descriptor2 + 184u, 0u);
}
PPCContext Initial(const Case& test)
{
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000020000ull;
    raw.r4.u64 = test.mode == Mode::ParentNull ?
        0x1234567800000000ull :
        test.mode == Mode::ParentAlias ?
        0x1234567800030000ull : 0x1234567800000044ull;
    raw.r31.u64 = 0x8877665500000031ull;
    raw.lr = 0x1122334455667788ull;
    raw.ctr.u64 = 0x8899aabbccddeeffull;
    return raw;
}
bool Compare(const Case& test)
{
    Window original, recovered;
    Seed(original, test.mode);
    Seed(recovered, test.mode);
    PPCContext raw = Initial(test);
    const PPCContext initial = raw;
    Services expected(original.bytes, test.mode);
    active = &expected;
    test.original(raw, original.bytes);
    Services actual(recovered.bytes, test.mode);
    family::FrameRegisters frame{initial.lr, initial.r31.u64,
        initial.ctr.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(test.address, actual.memory, actual,
            initial.r3.u64, initial.r4.u64, initial.r1.u64,
            frame, result))
        throw std::runtime_error("descriptor-cursor entry missing");
    GuestAddress first = 0;
    while (first < 0x90000u && original.bytes[first] == recovered.bytes[first])
        ++first;
    const bool same = first == 0x90000u &&
        expected.events == actual.events && raw.r3.u64 == result &&
        raw.r1.u64 == Stack && raw.lr == frame.lr &&
        raw.r31.u64 == frame.r31 && raw.ctr.u64 == frame.ctr;
    if (!same)
        std::fprintf(stderr,
            "FAIL cursor mode=%u r3 %llx/%llx r31 %llx/%llx "
            "ctr %llx/%llx events %zu/%zu first %x:%02x/%02x\n",
            static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(frame.r31),
            static_cast<unsigned long long>(raw.ctr.u64),
            static_cast<unsigned long long>(frame.ctr),
            expected.events.size(), actual.events.size(), first,
            first == 0x90000u ? 0u : original.bytes[first],
            first == 0x90000u ? 0u : recovered.bytes[first]);
    return same;
}
} // namespace

void OriginalVirtualCall(PPCContext& ctx, std::uint8_t*, GuestAddress target)
{
    family::FrameRegisters frame{ctx.lr, ctx.r31.u64, ctx.ctr.u64};
    ctx.r3.u64 = active->CallNext(target, active->memory,
        ctx.r3.u64, ctx.r4.u64, ctx.r1.u64, frame);
    ctx.lr = frame.lr;
    ctx.r31.u64 = frame.r31;
    ctx.ctr.u64 = frame.ctr;
}
PPC_FUNC(sub_822A6EF8) { __imp__sub_822A6EF8(ctx, base); }

int main()
{
    try
    {
        for (const Case& test : Cases) if (!Compare(test)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Empty);
        family::FrameRegisters frame{9u, 10u, 11u};
        std::uint64_t result = 7u;
        spare.bytes[Header] = 0x5au;
        if (family::Apply(0xffffffffu, service.memory, service,
                1, 2, Stack, frame, result) || result != 7u ||
            frame.lr != 9u || frame.r31 != 10u || frame.ctr != 11u ||
            spare.bytes[Header] != 0x5au || !service.events.empty())
            throw std::runtime_error("unknown cursor changed state");
        std::printf("PASS metadata-descriptor-cursor %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT dynamic vtable+284 target internal, other volatile ABI, MMIO and infinite lists external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
