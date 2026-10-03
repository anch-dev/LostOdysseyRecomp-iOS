#include "cpu/semantic_registered.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint32_t Stack = 0x5f000u;
constexpr std::uint32_t Existing = 0x20000u;
constexpr std::uint32_t Replacement = 0x22000u;
constexpr std::uint64_t Constructed = 0x1234567800010000ull;
constexpr std::uint64_t Failed = 0x1234567800000000ull;
constexpr std::uint32_t AlternateBase = 0x84000000u;
constexpr std::uint32_t AlternatePage = 0x83ff8000u;

struct Entry
{
    std::uint32_t address, global, constructor, registration;
    std::uint64_t owner;
    PPCFunc* original;
    PPCFunc* wrapper;
};
const Entry Entries[] = {
/* ENTRY_TABLE */
};

struct Target
{
    std::uint32_t address;
    PPCFunc* table_stub;
};
const Target Targets[] = {
/* TARGET_TABLE */
};

enum class Mode { Existing, Fresh, Mutated, FailedMutated, AliasR31 };
struct Event
{
    std::array<std::uint64_t, 8> values;
    bool operator==(const Event&) const = default;
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));

    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x70000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83247000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83315000u, 0x5000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + AlternatePage, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit registered getter guest pages");
        for (const Entry& entry : Entries)
            CommitTablePage(entry.address);
        for (const Target& target : Targets)
            CommitTablePage(target.address);
        for (const Entry& entry : Entries)
            PPC_LOOKUP_FUNC(bytes, entry.address) = entry.wrapper;
        for (const Target& target : Targets)
            PPC_LOOKUP_FUNC(bytes, target.address) = target.table_stub;
    }

    void CommitTablePage(std::uint32_t address)
    {
        const auto slot = PPC_IMAGE_BASE + PPC_IMAGE_SIZE +
            (static_cast<std::uint64_t>(address - PPC_CODE_BASE) * 2u);
        if (slot + sizeof(PPCFunc*) > Space ||
            !VirtualAlloc(bytes + (slot & ~std::uint64_t{0xfff}), 0x1000,
                          MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit registered getter function table");
    }

    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Side
{
    const Entry& entry;
    Mode mode;
    std::uint8_t* bytes;
    std::vector<Event> events;
    unsigned direct_calls{};
    unsigned table_calls{};
};
Side* active = nullptr;

std::uint32_t LiveGlobal(const Side& side, const PPCContext& ctx)
{
    const auto low = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(side.entry.global));
    return ctx.r31.u32 + low;
}

void Initialize(Window& window, const Entry& entry, Mode mode)
{
    auto* base = window.bytes;
    std::memset(window.bytes, 0xbd, 0x70000);
    std::memset(window.bytes + 0x83247000u, 0, 0x1000);
    std::memset(window.bytes + 0x83315000u, 0, 0x5000);
    std::memset(window.bytes + AlternatePage, 0, 0x1000);
    PPC_STORE_U32(entry.global, mode == Mode::Existing ? Existing : 0);
}

PPCContext SeedContext()
{
    PPCContext ctx{};
    ctx.r1.u64 = Stack;
    ctx.r3.u64 = 0xabcdef0000007777ull;
    ctx.r4.u64 = 0x1111222233334444ull;
    ctx.r11.u64 = 0x5555666677778888ull;
    ctx.r12.u64 = 0x1234123412341234ull;
    ctx.r31.u64 = 0x9999aaaabbbbccccull;
    ctx.lr = 0x1234000082200000ull;
    ctx.ctr.u64 = 0xdeadbeef00001234ull;
    ctx.xer.ca = 1;
    ctx.cr6.lt = 1;
    return ctx;
}

bool SameMemory(const Window& first, const Window& second)
{
    return std::memcmp(first.bytes, second.bytes, 0x70000) == 0 &&
        std::memcmp(first.bytes + 0x83247000u,
                    second.bytes + 0x83247000u, 0x1000) == 0 &&
        std::memcmp(first.bytes + 0x83315000u,
                    second.bytes + 0x83315000u, 0x5000) == 0 &&
        std::memcmp(first.bytes + AlternatePage,
                    second.bytes + AlternatePage, 0x1000) == 0;
}

bool CheckEvents(const Side& side)
{
    if (side.mode == Mode::Existing)
        return side.events.empty();
    if (side.events.size() != 2)
        return false;
    const auto& construction = side.events[0].values;
    const auto& registration = side.events[1].values;
    return construction[0] == side.entry.constructor &&
        construction[1] == Stack - 96u &&
        construction[2] == side.entry.address + 44u &&
        construction[3] == side.entry.owner &&
        registration[0] == side.entry.registration &&
        registration[1] == Stack - 96u &&
        registration[2] == side.entry.address + 52u &&
        registration[3] == (side.mode == Mode::FailedMutated ? Failed : Constructed);
}

bool Test(const Entry& entry, std::size_t index, Mode mode,
    bool use_table, bool enabled, Window& original, Window& recovered)
{
    Initialize(original, entry, mode);
    Initialize(recovered, entry, mode);
    Side expected{entry, mode, original.bytes};
    Side actual{entry, mode, recovered.bytes};
    PPCContext raw = SeedContext();
    PPCContext wrapper = raw;
    active = &expected;
    entry.original(raw, original.bytes);
    active = &actual;
    const unsigned fallback_before = g_fallback_calls[index];
    PPCFunc* call = use_table ? PPC_LOOKUP_FUNC(recovered.bytes, entry.address) :
                                entry.wrapper;
    if (call != entry.wrapper)
        throw std::runtime_error("registered getter mapping points elsewhere");
    call(wrapper, recovered.bytes);
    const unsigned fallback_delta = g_fallback_calls[index] - fallback_before;
    const unsigned nested = mode == Mode::Existing ? 0u : 2u;
    const bool same = fallback_delta == (enabled ? 0u : 1u) &&
        expected.direct_calls == nested && expected.table_calls == 0 &&
        actual.direct_calls == (enabled ? 0u : nested) &&
        actual.table_calls == (enabled ? nested : 0u) &&
        expected.events == actual.events && CheckEvents(expected) &&
        CheckEvents(actual) &&
        std::memcmp(&raw, &wrapper, sizeof(PPCContext)) == 0 &&
        SameMemory(original, recovered);
    if (!same)
        std::fprintf(stderr,
            "FAIL registered runtime %08x mode %u route %u gate %u r3 %llx/%llx fallback %u events %zu/%zu direct %u/%u table %u/%u\n",
            entry.address, static_cast<unsigned>(mode), use_table, enabled,
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(wrapper.r3.u64), fallback_delta,
            expected.events.size(), actual.events.size(),
            expected.direct_calls, actual.direct_calls,
            expected.table_calls, actual.table_calls);
    return same;
}
} // namespace

unsigned g_fallback_calls[58]{};

void GuestCall(PPCContext& ctx, std::uint8_t* base,
    std::uint32_t target, bool table_call)
{
    if (!active || active->bytes != base)
        throw std::runtime_error("registered getter guest call lost its side");
    Side& side = *active;
    if (table_call) ++side.table_calls;
    else ++side.direct_calls;
    const bool constructor = target == side.entry.constructor;
    if (!constructor && target != side.entry.registration)
        throw std::runtime_error("unexpected registered getter guest target");
    const std::uint32_t global = LiveGlobal(side, ctx);
    side.events.push_back({{target, ctx.r1.u64, ctx.lr, ctx.r3.u64,
        ctx.r11.u64, ctx.r31.u64, ctx.ctr.u64, PPC_LOAD_U32(global)}});
    if (constructor)
    {
        ctx.r3.u64 = side.mode == Mode::FailedMutated ? Failed : Constructed;
        if (side.mode == Mode::AliasR31)
            ctx.r31.s64 = std::bit_cast<std::int32_t>(AlternateBase);
    }
    else
    {
        if (side.mode == Mode::Mutated || side.mode == Mode::FailedMutated ||
            side.mode == Mode::AliasR31)
            PPC_STORE_U32(global, Replacement);
        ctx.r3.u64 = 0x8765432100000033ull;
    }
    ctx.r4.u64 = 0x1111000000000000ull | target;
    ctx.r11.u64 = 0x2222000000000000ull | target;
    ctx.ctr.u64 = 0x3333000000000000ull | target;
    ctx.xer.ca = 0;
    ctx.cr6.lt = 0;
    ctx.cr6.gt = 1;
    ctx.cr6.eq = 0;
}

int main()
{
    try
    {
        const bool enabled = lo::runtime::semantic_registered::Enabled();
        Window original, recovered;
        unsigned comparisons = 0;
        for (std::size_t index = 0; index < std::size(Entries); ++index)
            if (!Test(Entries[index], index, Mode::Existing, index % 2 != 0,
                      enabled, original, recovered)) return 1;
            else ++comparisons;

        struct Selected { std::uint32_t address; Mode mode; bool table; };
        for (const Selected selected : {
                 Selected{0x822a1dd8u, Mode::Mutated, false},
                 Selected{0x824069e8u, Mode::Fresh, true},
                 Selected{0x82463fe0u, Mode::Mutated, true},
                 Selected{0x822a1dd8u, Mode::FailedMutated, true},
                 Selected{0x822a1dd8u, Mode::AliasR31, false},
             })
        {
            std::size_t index = 0;
            while (index < std::size(Entries) &&
                   Entries[index].address != selected.address) ++index;
            if (index == std::size(Entries) ||
                !Test(Entries[index], index, selected.mode, selected.table,
                      enabled, original, recovered)) return 1;
            ++comparisons;
        }
        std::printf("PASS registered-getter-runtime 58 initialized + 5 first-use, %u comparisons, %s\n",
                    comparisons, enabled ? "enabled" : "disabled");
        std::puts("LIMIT exact cached getter PPC bodies, deterministic guest constructor/registration stubs, full context and selected RAM/stack; no runtime scene or downstream constructor validation");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
